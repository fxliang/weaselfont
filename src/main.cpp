#include "resource.h"
#include "utils.h"
#include <ShellScalingApi.h>
#include <algorithm>
#include <atomic>
#include <d2d1_2.h>
#include <dwrite_2.h>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <map>
#include <regex>
#include <string>
#include <thread>
#include <vector>
#include <windows.h>
#include <wrl.h>

using namespace Microsoft::WRL;
using namespace std;
namespace fs = filesystem;
using json = nlohmann::json;

#define STYLEORWEIGHT (L":[^:]*[^a-f0-9:]+[^:]*")
#define DEBUG                                                                  \
  (DebugStream() << "[file: " << __FILE__ << ", line: " << __LINE__ << "] ")

typedef ComPtr<IDWriteTextFormat1> PtTextFormat;

const wstring patStyle(L"(\\s*:\\s*italic|\\s*:\\s*oblique|\\s*:\\s*normal)");

const wstring patWeight(
    L"(\\s*:\\s*thin|\\s*:\\s*extra_light|\\s*:\\s*ultra_light|\\s*:\\s*light|"
    L"\\s*:\\s*semi_light|\\s*:\\s*medium|\\s*:\\s*demi_"
    L"bold|"
    L"\\s*:\\s*semi_bold|\\s*:\\s*bold|\\s*:\\s*extra_bold|\\s*:\\s*ultra_bold|"
    L"\\s*:\\s*black|\\s*:\\s*heavy|\\s*:\\s*extra_black|\\s*:\\s*"
    L"ultra_black)");

const map<wstring, DWRITE_FONT_STYLE> _mapStyle = {
    {L"italic", DWRITE_FONT_STYLE_ITALIC},
    {L"oblique", DWRITE_FONT_STYLE_OBLIQUE},
    {L"normal", DWRITE_FONT_STYLE_NORMAL},
};

const map<wstring, float> _mapScale = {
    {L"100%", 1.0f}, {L"110%", 1.1f}, {L"120%", 1.2f}, {L"150%", 1.5f},
    {L"200%", 2.0f}, {L"250%", 2.5f}, {L"400%", 4.0f},
};

const map<wstring, DWRITE_FONT_WEIGHT> _mapWeight = {
    {L"thin", DWRITE_FONT_WEIGHT_THIN},
    {L"extra_light", DWRITE_FONT_WEIGHT_EXTRA_LIGHT},
    {L"ultra_light", DWRITE_FONT_WEIGHT_ULTRA_LIGHT},
    {L"light", DWRITE_FONT_WEIGHT_LIGHT},
    {L"semi_light", DWRITE_FONT_WEIGHT_SEMI_LIGHT},
    {L"medium", DWRITE_FONT_WEIGHT_MEDIUM},
    {L"demi_bold", DWRITE_FONT_WEIGHT_DEMI_BOLD},
    {L"semi_bold", DWRITE_FONT_WEIGHT_SEMI_BOLD},
    {L"bold", DWRITE_FONT_WEIGHT_BOLD},
    {L"extra_bold", DWRITE_FONT_WEIGHT_EXTRA_BOLD},
    {L"ultra_bold", DWRITE_FONT_WEIGHT_ULTRA_BOLD},
    {L"black", DWRITE_FONT_WEIGHT_BLACK},
    {L"heavy", DWRITE_FONT_WEIGHT_HEAVY},
    {L"extra_black", DWRITE_FONT_WEIGHT_EXTRA_BLACK},
    {L"regular", DWRITE_FONT_WEIGHT_NORMAL},
    {L"ultra_black", DWRITE_FONT_WEIGHT_ULTRA_BLACK}};

const wstring patStretch(L"(:undefined|:ultra_condensed|:extra_condensed|"
                         L":condensed|:semi_condensed|"
                         L":normal_stretch|:medium_stretch|:semi_expanded|"
                         L":expanded|:extra_expanded|:ultra_expanded)");

const map<wstring, DWRITE_FONT_STRETCH> _mapStretch = {
    {L"undefined", DWRITE_FONT_STRETCH_UNDEFINED},
    {L"ultra_condensed", DWRITE_FONT_STRETCH_ULTRA_CONDENSED},
    {L"extra_condensed", DWRITE_FONT_STRETCH_EXTRA_CONDENSED},
    {L"condensed", DWRITE_FONT_STRETCH_CONDENSED},
    {L"semi_condensed", DWRITE_FONT_STRETCH_SEMI_CONDENSED},
    {L"normal_stretch", DWRITE_FONT_STRETCH_NORMAL},
    {L"medium_stretch", DWRITE_FONT_STRETCH_MEDIUM},
    {L"semi_expanded", DWRITE_FONT_STRETCH_SEMI_EXPANDED},
    {L"expanded", DWRITE_FONT_STRETCH_EXPANDED},
    {L"extra_expanded", DWRITE_FONT_STRETCH_EXTRA_EXPANDED},
    {L"ultra_expanded", DWRITE_FONT_STRETCH_ULTRA_EXPANDED}};
// ----------------------------------------------------------------------------
vector<wstring> WstrSplit(const wstring &in, const wstring &delim) {
  wregex re{delim};
  return vector<wstring>{wsregex_token_iterator(in.begin(), in.end(), re, -1),
                         wsregex_token_iterator()};
}

static wstring MatchWordsOutLowerCaseTrim1st(const wstring &wstr,
                                             const wstring &pat) {
  wstring mat = L"";
  wsmatch mc;
  wregex pattern(pat, wregex::icase);
  wstring::const_iterator iter = wstr.cbegin();
  wstring::const_iterator end = wstr.cend();
  while (regex_search(iter, end, mc, pattern)) {
    for (const auto &m : mc) {
      mat = m;
      mat = mat.substr(1);
      break;
    }
    iter = mc.suffix().first;
  }
  wstring res;
  transform(mat.begin(), mat.end(), back_inserter(res), ::tolower);
  return res;
}

void ParseFontFace(const wstring &fontFaceStr, DWRITE_FONT_WEIGHT &fontWeight,
                   DWRITE_FONT_STYLE &fontStyle,
                   DWRITE_FONT_STRETCH &fontStretch) {
  const wstring weight = MatchWordsOutLowerCaseTrim1st(fontFaceStr, patWeight);
  const auto it = _mapWeight.find(weight);
  fontWeight =
      (it != _mapWeight.end()) ? it->second : DWRITE_FONT_WEIGHT_NORMAL;

  const wstring style = MatchWordsOutLowerCaseTrim1st(fontFaceStr, patStyle);
  const auto it2 = _mapStyle.find(style);
  fontStyle = (it2 != _mapStyle.end()) ? it2->second : DWRITE_FONT_STYLE_NORMAL;
  const wstring stretch =
      MatchWordsOutLowerCaseTrim1st(fontFaceStr, patStretch);
  const auto it3 = _mapStretch.find(stretch);
  fontStretch =
      (it3 != _mapStretch.end()) ? it3->second : DWRITE_FONT_STRETCH_NORMAL;
  if (DWRITE_FONT_STRETCH_UNDEFINED)
    fontStretch = DWRITE_FONT_STRETCH_NORMAL;
}

void RemoveSpaceAround(wstring &str) {
  str = regex_replace(str, wregex(L",$"), L"");
  str = regex_replace(str, wregex(L"\\s*(,|:|^|$)\\s*"), L"$1");
}
// ----------------------------------------------------------------------------

template <class Derived> class BaseWin {
public:
  BaseWin() : m_hWnd(nullptr) {}
  virtual ~BaseWin() {
    if (m_hWnd)
      DestroyWindow(m_hWnd);
  }
  HWND GetHandle() const { return m_hWnd; }
  static LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam,
                                     LPARAM lParam) {
    Derived *pThis = nullptr;
    if (msg == WM_NCCREATE) {
      CREATESTRUCT *pCreate = reinterpret_cast<CREATESTRUCT *>(lParam);
      pThis = reinterpret_cast<Derived *>(pCreate->lpCreateParams);
      SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
    } else {
      pThis =
          reinterpret_cast<Derived *>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }
    if (pThis)
      return pThis->HandleMessage(hwnd, msg, wParam, lParam);
    return DefWindowProc(hwnd, msg, wParam, lParam);
  }
  virtual LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam,
                                LPARAM lParam) = 0;
  HWND m_hWnd;
  RECT m_rect;
  HINSTANCE m_hInstance;
};

class FontPreviewControl : public BaseWin<FontPreviewControl> {
public:
  FontPreviewControl(HWND parent, HINSTANCE hInstance, const RECT &rect,
                     wstring &fontFace, int &fontPoint, wstring &text)
      : m_hParent(parent), m_fontFace(fontFace), m_fontPoint(fontPoint),
        m_text(text) {
    m_hInstance = hInstance;
    m_rect = rect;
    Initialize();
  }

  void Refresh() {
    HR(InitFontFormat(m_fontFace, m_fontPoint));
    InvalidateRect(m_hWnd, nullptr, TRUE);
  }
  void Redraw() { InvalidateRect(m_hWnd, nullptr, TRUE); }

  LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT: {
      PAINTSTRUCT ps;
      BeginPaint(hwnd, &ps);

      if (!m_pRenderTarget)
        InitializeDirect2D();

      m_pRenderTarget->BeginDraw();
      COLORREF color = GetSysColor(COLOR_BTNFACE);
      auto d2dColor =
          D2D1::ColorF(GetRValue(color) / 255.0f, GetGValue(color) / 255.0f,
                       GetBValue(color) / 255.0f);
      m_pRenderTarget->Clear(d2dColor);
      if (m_pTextFormat && !m_text.empty()) {
        m_pRenderTarget->DrawText(
            m_text.c_str(), static_cast<UINT32>(m_text.length()),
            m_pTextFormat.Get(),
            D2D1::RectF(10, 0, m_rect.right - 10, m_rect.bottom),
            m_pBrush.Get(), D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
      }
      m_pRenderTarget->EndDraw();
      EndPaint(hwnd, &ps);
      return 0;
    }
    case WM_SIZE: {
      if (m_pRenderTarget) {
        GetClientRect(hwnd, &m_rect);
        m_pRenderTarget->Resize(D2D1::SizeU(m_rect.right, m_rect.bottom));
        InvalidateRect(hwnd, nullptr, FALSE);
      }
      return 0;
    }
    case WM_DESTROY:
      return 0;
    default:
      return DefWindowProc(hwnd, msg, wParam, lParam);
    }
  }

private:
  HRESULT InitFontFormat(const wstring &font_face, const int font_point) {
    DWRITE_WORD_WRAPPING wrapping = DWRITE_WORD_WRAPPING_WHOLE_WORD;
    DWRITE_FLOW_DIRECTION flow = DWRITE_FLOW_DIRECTION_LEFT_TO_RIGHT;
    m_dpiScaleFontPoint = GetDpi(m_hWnd);

    auto init_font = [&](const wstring &font_face, int font_point,
                         PtTextFormat &_pTextFormat,
                         DWRITE_WORD_WRAPPING wrap) {
      vector<wstring> fontFaceStrVector;
      // set main font a invalid font name, to make every font range
      // customizable
      const wstring _mainFontFace = L"_InvalidFontName_";
      DWRITE_FONT_WEIGHT fontWeight = DWRITE_FONT_WEIGHT_NORMAL;
      DWRITE_FONT_STYLE fontStyle = DWRITE_FONT_STYLE_NORMAL;
      DWRITE_FONT_STRETCH fontStretch = DWRITE_FONT_STRETCH_NORMAL;

      ParseFontFace(font_face, fontWeight, fontStyle, fontStretch);
      // text font text format set up
      fontFaceStrVector = WstrSplit(font_face, L",");
      fontFaceStrVector[0] = regex_replace(
          fontFaceStrVector[0], wregex(STYLEORWEIGHT, wregex::icase), L"");
      if (font_point <= 0) {
        _pTextFormat.Reset();
        return S_FALSE;
      }
      HR(m_pDWriteFactory->CreateTextFormat(
          _mainFontFace.c_str(), NULL, fontWeight, fontStyle, fontStretch,
          font_point * m_dpiScaleFontPoint, L"",
          reinterpret_cast<IDWriteTextFormat **>(
              _pTextFormat.ReleaseAndGetAddressOf())));

      _pTextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
      _pTextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

      _pTextFormat->SetWordWrapping(wrap);
      HR(SetFontFallback(_pTextFormat, fontFaceStrVector));
      decltype(fontFaceStrVector)().swap(fontFaceStrVector);
      return S_OK;
    };
    init_font(font_face, font_point, m_pTextFormat, wrapping);
    return S_OK;
  }

  void Initialize() {
    RegisterWindowClass();
    CreateControl();
    InitializeDirect2D();
  }

  void RegisterWindowClass() {
    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = m_hInstance;
    wc.lpszClassName = m_className.c_str();
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    RegisterClass(&wc);
  }

  void CreateControl() {
    m_hWnd = CreateWindowExW(
        0, m_className.c_str(), L"", WS_VISIBLE | WS_CHILD | WS_CLIPSIBLINGS,
        m_rect.left, m_rect.top, m_rect.right - m_rect.left,
        m_rect.bottom - m_rect.top, m_hParent, nullptr, m_hInstance, this);
    if (!m_hWnd) {
      throw std::runtime_error("Failed to create control");
    }
  }

  HRESULT SetFontFallback(PtTextFormat textFormat,
                          const vector<wstring> &fontVector) {
    ComPtr<IDWriteFontFallback> pSysFallback;
    HR(m_pDWriteFactory->GetSystemFontFallback(
        pSysFallback.ReleaseAndGetAddressOf()));
    ComPtr<IDWriteFontFallback> pFontFallback = NULL;
    ComPtr<IDWriteFontFallbackBuilder> pFontFallbackBuilder = NULL;
    HR(m_pDWriteFactory->CreateFontFallbackBuilder(
        pFontFallbackBuilder.ReleaseAndGetAddressOf()));
    vector<wstring> fallbackFontsVector;
    for (UINT32 i = 0; i < fontVector.size(); i++) {
      fallbackFontsVector = WstrSplit(fontVector[i], L":");
      wstring _fontFaceWstr, firstWstr, lastWstr;
      if (fallbackFontsVector.size() == 3) {
        _fontFaceWstr = fallbackFontsVector[0];
        firstWstr = fallbackFontsVector[1];
        lastWstr = fallbackFontsVector[2];
        if (lastWstr.empty())
          lastWstr = L"10ffff";
        if (firstWstr.empty())
          firstWstr = L"0";
      } else if (fallbackFontsVector.size() == 2) // fontName : codepoint
      {
        _fontFaceWstr = fallbackFontsVector[0];
        firstWstr = fallbackFontsVector[1];
        if (firstWstr.empty())
          firstWstr = L"0";
        lastWstr = L"10ffff";
      } else if (fallbackFontsVector.size() ==
                 1) // if only font defined, use all range
      {
        _fontFaceWstr = fallbackFontsVector[0];
        firstWstr = L"0";
        lastWstr = L"10ffff";
      }
      const auto func = [](wstring wstr, UINT fallback) -> UINT {
        try {
          return stoul(wstr.c_str(), 0, 16);
        } catch (...) {
          return fallback;
        }
      };
      UINT first = func(firstWstr, 0), last = func(lastWstr, 0x10ffff);
      DWRITE_UNICODE_RANGE range = {first, last};
      const WCHAR *familys = {_fontFaceWstr.c_str()};
      HR(pFontFallbackBuilder->AddMapping(&range, 1, &familys, 1));
      decltype(fallbackFontsVector)().swap(fallbackFontsVector);
    }
    // add system defalt font fallback
    HR(pFontFallbackBuilder->AddMappings(pSysFallback.Get()));
    HR(pFontFallbackBuilder->CreateFontFallback(
        pFontFallback.ReleaseAndGetAddressOf()));
    HR(textFormat->SetFontFallback(pFontFallback.Get()));
    decltype(fallbackFontsVector)().swap(fallbackFontsVector);
    return S_OK;
  }

  float GetDpi(HWND m_hWnd) {
    HMONITOR const mon = MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTONEAREST);
    UINT x = 0, y = 0;
    HR(GetDpiForMonitor(mon, MDT_EFFECTIVE_DPI, &x, &y));
    auto m_dpiX = static_cast<float>(x);
    auto m_dpiY = static_cast<float>(y);
    if (m_dpiY == 0)
      m_dpiX = m_dpiY = 96.0;
    auto dpiScaleFontPoint = m_dpiY / 72.0f;
    return dpiScaleFontPoint;
  }

  void InitializeDirect2D() {
    // 创建 Direct2D 工厂
    HR(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
                         __uuidof(ID2D1Factory), nullptr, &m_pD2DFactory));

    // 创建渲染目标
    GetClientRect(m_hWnd, &m_rect);
    HR(m_pD2DFactory->CreateHwndRenderTarget(
        D2D1::RenderTargetProperties(),
        D2D1::HwndRenderTargetProperties(
            m_hWnd, D2D1::SizeU(m_rect.right - m_rect.left,
                                m_rect.bottom - m_rect.top)),
        &m_pRenderTarget));

    // 创建画刷
    HR(m_pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::Black),
                                              &m_pBrush));

    // 初始化 DirectWrite 工厂
    HR(DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown **>(m_pDWriteFactory.GetAddressOf())));
    HR(InitFontFormat(m_fontFace, m_fontPoint));
  }

  HWND m_hParent;
  const wstring m_className = L"FontPreviewControlClass";

  // Direct2D资源
  ComPtr<ID2D1Factory> m_pD2DFactory;
  ComPtr<ID2D1HwndRenderTarget> m_pRenderTarget;
  ComPtr<ID2D1SolidColorBrush> m_pBrush;
  ComPtr<IDWriteFactory2> m_pDWriteFactory;
  ComPtr<IDWriteTextFormat1> m_pTextFormat;

  // 字体数据
  wstring &m_fontFace;
  int &m_fontPoint;
  wstring &m_text;
  float m_dpiScaleFontPoint = 96.0f / 72.0f;
};

class MainWindow : public BaseWin<MainWindow> {
public:
  MainWindow(HINSTANCE hInstance) {
    m_hInstance = hInstance;
    Initialize();
  }

  void Run() {
    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
  }

  virtual LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam,
                                LPARAM lParam) {
    switch (msg) {
    case WM_COMMAND: {
      auto ID = LOWORD(wParam);
      if (ID == IDC_CHECK_BOX) {
        OnRangeEnableChkBox(wParam);
      } else if (ID == IDC_EDIT_RANGE_START || ID == IDC_EDIT_RANGE_END) {
        OnRangeChanged(wParam);
      } else if (ID == IDC_ADD_FONT) {
        OnAddFontBtn();
      } else if (ID == IDC_EDIT_FONT_FACE && (HIWORD(wParam) == EN_CHANGE)) {
        OnFontFaceEditChanged();
      } else if (ID == IDC_EDIT_PREVIEW_TEXT && (HIWORD(wParam) == EN_CHANGE)) {
        m_text = GetTextOfEdit(m_hEditPreviewText);
        UpdateView();
      } else if (((ID == IDC_COMBO_BOX_FONT_STYLE) ||
                  (ID == IDC_COMBO_BOX_FONT_WEIGHT)) &&
                 HIWORD(wParam) == CBN_SELCHANGE) {
        OnStyleOrWeightChanged(wParam);
      } else if (ID == IDC_COMBO_BOX_FONT_POINT &&
                 HIWORD(wParam) == CBN_SELCHANGE) {
        OnFontPointChanged(wParam);
      } else if (ID == IDC_COMBO_BOX_FONT_SCALE &&
                 HIWORD(wParam) == CBN_SELCHANGE) {
        OnFontScaleChanged(wParam);
      }
    } break;
    case WM_PAINT: {
      PAINTSTRUCT ps;
      HDC hdc = BeginPaint(hwnd, &ps);
      GetClientRect(hwnd, &m_rect);
      FillRect(hdc, &m_rect, (HBRUSH)(COLOR_BTNFACE + 1));
      EndPaint(hwnd, &ps);
      return 0;
    }
    case WM_ERASEBKGND:
      return 1;
    case WM_SIZE:
      OnResize();
      break;
    case WM_DESTROY:
      m_exitFlag = true;
      {
        char path[MAX_PATH];
        GetModuleFileNameA(NULL, path, MAX_PATH);
        filesystem::path dataPath = fs::path(path).parent_path() / "data.json";
        // update json file data.json
        auto fontPointStr = GetComboBoxSelectStr(m_hComboBoxFontPoint);
        m_fontPoint = std::stoul(fontPointStr);
        json j;
        j["font_face"] = wtou8(m_fontFace);
        j["test_text"] = wtou8(m_text);
        j["font_point"] = m_fontPoint;
        try {
          ofstream json_file(dataPath.string());
          json_file << std::setw(2) << j << std::endl;
        } catch (const std::exception &e) {
          auto errorMessage = ((string("save data failed, ") + e.what() +
                                ", please check data.json"));
          MessageBoxA(NULL, errorMessage.c_str(), "错误", MB_ICONERROR | MB_OK);
        }
      }
      PostQuitMessage(0);
      return 0;
    default:
      return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
  }

private:
  void Initialize() {
    RegisterWindowClass();
    CreateWin();
    CreateControls();
    ShowWindow(m_hWnd, true);
    UpdateWindow(m_hWnd);

    UpdateDataFromJson();
    initialized = true;
    StartFileMonitor();
    m_monitorThread.detach();
  }

  void UpdateView() {
    if (initialized) {
      m_previewControl->Refresh();
    }
  }

  void RegisterWindowClass() {
    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = m_hInstance;
    wc.lpszClassName = L"MainWindowClass";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hIcon = LoadIcon(m_hInstance, MAKEINTRESOURCE(IDI_WEASELFONT));
    wc.style = CS_HREDRAW | CS_VREDRAW;
    if (!RegisterClass(&wc)) {
      DWORD err = GetLastError();
      if (err != ERROR_CLASS_ALREADY_EXISTS) {
        throw std::runtime_error("Failed to register window class");
      }
    }
  }

  void CreateWin() {
    m_hWnd = CreateWindowExW(0, L"MainWindowClass", L"Weasel Font Previewer",
                             WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                             CW_USEDEFAULT, CW_USEDEFAULT, 800, 400, nullptr,
                             nullptr, m_hInstance, this);
    m_rect = {0, 0, 800, 400};
    if (!m_hWnd)
      throw std::runtime_error("Failed to create window");
  }

  void InitFontComboBoxItems() {
    wchar_t locale[LOCALE_NAME_MAX_LENGTH] = {0};
    if (!GetUserDefaultLocaleName(locale, LOCALE_NAME_MAX_LENGTH)) {
      DWORD errorCode = GetLastError();
      string errorMsgA = wtoacp(StrzHr(errorCode));
      throw std::runtime_error("Error Code: " + std::to_string(errorCode) +
                               ", Error Message: " +
                               std::string(errorMsgA.begin(), errorMsgA.end()));
    }
    ComPtr<IDWriteFactory> pDWriteFactory;
    HR(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                           reinterpret_cast<IUnknown **>(
                               pDWriteFactory.ReleaseAndGetAddressOf())));
    ComPtr<IDWriteFontCollection> pFontCollection;
    // Get the system font collection.
    HR(pDWriteFactory->GetSystemFontCollection(
        pFontCollection.ReleaseAndGetAddressOf()));
    UINT32 familyCount = 0;
    // Get the number of font families in the collection.
    familyCount = pFontCollection->GetFontFamilyCount();
    ComPtr<IDWriteFontFamily> pFontFamily;
    ComPtr<IDWriteLocalizedStrings> pFamilyNames;
    const auto func = [&](const wchar_t *localeName) -> wstring {
      UINT32 index = 0;
      BOOL exists = false;
      HR(pFamilyNames->FindLocaleName(localeName, &index, &exists));
      // If the specified locale doesn't exist, select the first on the list.
      index = !exists ? 0 : index;
      UINT32 length = 0;
      HR(pFamilyNames->GetStringLength(index, &length));
      std::unique_ptr<wchar_t[]> name(new wchar_t[length + 1]);
      HR(!name ? E_OUTOFMEMORY : S_OK);
      HR(pFamilyNames->GetString(index, name.get(), length + 1));
      return wstring(name.get());
    };
    vector<wstring> fonts;
    for (UINT32 i = 0; i < familyCount; ++i) {
      HR(pFontCollection->GetFontFamily(i,
                                        pFontFamily.ReleaseAndGetAddressOf()));
      HR(pFontFamily->GetFamilyNames(pFamilyNames.ReleaseAndGetAddressOf()));
      auto font_family_name = func(locale);
      fonts.push_back(font_family_name);
    }
    std::sort(fonts.begin(), fonts.end());
    for (const auto &font : fonts) {
      SendMessage(m_hComboBox, CB_ADDSTRING, 0, (LPARAM)font.c_str());
    }
  }

  void CreateControls() {
    HDC hdc = GetDC(NULL);
    int dpi = GetDeviceCaps(hdc, LOGPIXELSX);
    ReleaseDC(NULL, hdc);
    float scale = dpi / 72.0f;

    // add a static text to show the font name
    HWND hStaticFontNameCombo =
        CreateWindowW(L"STATIC", L"font name:", WS_VISIBLE | WS_CHILD | SS_LEFT,
                      10, 15, 80, 30, m_hWnd, nullptr, m_hInstance, nullptr);
    // 创建 ComboBox 并设置高度(展开的)
    m_hComboBox = CreateWindowW(
        L"COMBOBOX", L"Font",
        WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST | CBS_AUTOHSCROLL | WS_VSCROLL,
        100, 13, 150, 300, m_hWnd, (HMENU)IDC_COMBO_BOX, m_hInstance, nullptr);
    // 初始化字体选择框选项
    InitFontComboBoxItems();
    // 范围使能勾选框
    m_hCheckBox = CreateWindowW(
        L"BUTTON", L"Enable Range", WS_VISIBLE | WS_CHILD | BS_CHECKBOX, 280,
        15, 120, 20, m_hWnd, (HMENU)IDC_CHECK_BOX, m_hInstance, nullptr);
    // 范围起始输入框
    m_hEditRangeStart = CreateWindowW(
        L"EDIT", L"0", WS_VISIBLE | WS_CHILD | ES_LEFT | WS_BORDER,
        m_rect.right - 330, 15, 80, 25, m_hWnd, (HMENU)IDC_EDIT_RANGE_START,
        m_hInstance, nullptr);
    // 范围中间的 ~ 符号
    m_hStaticEnd = CreateWindowW(
        L"STATIC", L" ~ ", WS_VISIBLE | WS_CHILD | SS_LEFT, m_rect.right - 248,
        15, 50, 30, m_hWnd, nullptr, m_hInstance, nullptr);
    // 范围结束输入框
    m_hEditRangeEnd = CreateWindowW(
        L"EDIT", L"10ffff", WS_VISIBLE | WS_CHILD | ES_LEFT | WS_BORDER,
        m_rect.right - 230, 15, 80, 25, m_hWnd, (HMENU)IDC_EDIT_RANGE_END,
        m_hInstance, nullptr);
    // 添加文字按键
    m_hButtonAddFont = CreateWindowW(L"BUTTON", L"Add Font",
                                     WS_VISIBLE | WS_CHILD | BS_PUSHBUTTON,
                                     m_rect.right - 130, 15, 120, 30, m_hWnd,
                                     (HMENU)IDC_ADD_FONT, m_hInstance, nullptr);
    // 预览文字提示符
    HWND hStaticPreview =
        CreateWindowW(L"STATIC", L"content:", WS_VISIBLE | WS_CHILD | SS_LEFT,
                      10, 55, 70, 30, m_hWnd, nullptr, m_hInstance, nullptr);
    // 预览文字输入框
    m_hEditPreviewText = CreateWindowW(
        L"EDIT", L"Arial",
        WS_VISIBLE | WS_CHILD | ES_LEFT | WS_BORDER | ES_AUTOHSCROLL, 100, 50,
        m_rect.right - m_rect.left - 350, 30, m_hWnd,
        (HMENU)IDC_EDIT_PREVIEW_TEXT, m_hInstance, nullptr);
    // 字号选择框
    m_hComboBoxFontPoint = CreateWindowW(
        L"COMBOBOX", L"Font",
        WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST | CBS_AUTOHSCROLL | WS_VSCROLL,
        m_rect.right - 130, 50, 120, 300, m_hWnd,
        (HMENU)IDC_COMBO_BOX_FONT_POINT, m_hInstance, nullptr);
    // 样式选择框
    m_hComboBoxFontStyle = CreateWindowW(
        L"COMBOBOX", L"FontStyle",
        WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST | CBS_AUTOHSCROLL | WS_VSCROLL,
        m_rect.right - 230, 50, 80, 300, m_hWnd,
        (HMENU)IDC_COMBO_BOX_FONT_STYLE, m_hInstance, nullptr);
    for (auto &it : _mapStyle) {
      SendMessage(m_hComboBoxFontStyle, CB_ADDSTRING, 0,
                  (LPARAM)it.first.c_str());
    }
    // 字重选择框
    m_hComboBoxFontWeight = CreateWindowW(
        L"COMBOBOX", L"FontWeight",
        WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST | CBS_AUTOHSCROLL | WS_VSCROLL,
        m_rect.right - 230, 90, 80, 300, m_hWnd,
        (HMENU)IDC_COMBO_BOX_FONT_WEIGHT, m_hInstance, nullptr);
    for (auto &it : _mapWeight) {
      SendMessage(m_hComboBoxFontWeight, CB_ADDSTRING, 0,
                  (LPARAM)it.first.c_str());
    }
    // 初始化字号选择框
    for (auto i = 0; i < 95; i++) {
      auto font_point = std::to_wstring(i + 6);
      SendMessage(m_hComboBoxFontPoint, CB_ADDSTRING, 0,
                  (LPARAM)font_point.c_str());
    }
    // add a static text to show the font name
    HWND hStaticFontName =
        CreateWindowW(L"STATIC", L"font_face:", WS_VISIBLE | WS_CHILD | SS_LEFT,
                      10, 95, 70, 30, m_hWnd, nullptr, m_hInstance, nullptr);
    // 字体名称输入框
    m_hEditFontFace = CreateWindowW(
        L"EDIT", L"Arial",
        WS_VISIBLE | WS_CHILD | ES_LEFT | WS_BORDER | ES_AUTOHSCROLL, 100, 100,
        m_rect.right - m_rect.left - 350, 30, m_hWnd, (HMENU)IDC_EDIT_FONT_FACE,
        m_hInstance, nullptr);
    // 缩放选择框
    m_hComboBoxScale = CreateWindowW(
        L"COMBOBOX", L"FontWeight",
        WS_VISIBLE | WS_CHILD | CBS_DROPDOWNLIST | CBS_AUTOHSCROLL | WS_VSCROLL,
        m_rect.right - 130, 90, 120, 300, m_hWnd,
        (HMENU)IDC_COMBO_BOX_FONT_SCALE, m_hInstance, nullptr);
    for (auto &it : _mapScale) {
      SendMessage(m_hComboBoxScale, CB_ADDSTRING, 0, (LPARAM)it.first.c_str());
    }
    // 控件字体初始化
    HFONT hFont =
        CreateFontW(15 * scale, 0, 0, 0, FW_NORMAL, 0, 0, 0, DEFAULT_CHARSET,
                    OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY,
                    DEFAULT_PITCH, L"Segoe UI");

    // 设置控件字体及初始化选择
#define SETFONT(hwnd, font) SendMessage(hwnd, WM_SETFONT, (WPARAM)font, true)
#define SETFONT_MULTI(font, ...)                                               \
  do {                                                                         \
    HWND hwnds[] = {__VA_ARGS__};                                              \
    for (int i = 0; i < sizeof(hwnds) / sizeof(hwnds[0]); ++i) {               \
      SETFONT(hwnds[i], font);                                                 \
    }                                                                          \
  } while (0)
    SETFONT_MULTI(hFont, m_hComboBoxScale, m_hButtonAddFont, m_hComboBox,
                  m_hComboBoxFontPoint, m_hComboBoxFontWeight,
                  m_hComboBoxFontStyle, m_hCheckBox, m_hEditRangeStart,
                  m_hEditRangeEnd, m_hStaticEnd, m_hEditFontFace,
                  m_hEditPreviewText, hStaticFontName, hStaticFontNameCombo,
                  hStaticPreview);

    SendMessage(m_hComboBox, CB_SETCURSEL, (WPARAM)0, 0);
    SendMessage(m_hComboBoxFontPoint, CB_SETCURSEL, (WPARAM)14, 0);
    SendMessage(m_hComboBoxFontStyle, CB_SETCURSEL, (WPARAM)1, 0);
    SendMessage(m_hComboBoxFontWeight, CB_SETCURSEL, (WPARAM)9, 0);
    SendMessage(m_hComboBoxScale, CB_SETCURSEL, (WPARAM)0, 0);
    EnableWindow(m_hEditRangeStart, false);
    EnableWindow(m_hEditRangeEnd, false);
    // 创建预览控件
    RECT rc = {0, previewTop, m_rect.right, m_rect.bottom};
    m_previewControl.reset(new FontPreviewControl(
        m_hWnd, m_hInstance, rc, m_fontFace, m_fontPoint, m_text));
  }

  void StartFileMonitor() {
    m_monitorThread = thread([this]() { MonitorFileChanges(); });
  }

  void MonitorFileChanges() {
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    fs::path fullFilePath = fs::path(path).parent_path() / "data.json";
    fs::path dirPath = fullFilePath.parent_path();

    HANDLE hDir = CreateFile(
        dirPath.c_str(), FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
        OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, NULL);

    if (hDir == INVALID_HANDLE_VALUE) {
      OutputDebugString(L"无法打开目录");
      return;
    }

    while (!m_exitFlag) {
      DWORD dwBytesReturned;
      char buffer[1024];

      if (ReadDirectoryChangesW(hDir, &buffer, sizeof(buffer), FALSE,
                                FILE_NOTIFY_CHANGE_LAST_WRITE, &dwBytesReturned,
                                NULL, NULL)) {
        FILE_NOTIFY_INFORMATION *pNotify =
            reinterpret_cast<FILE_NOTIFY_INFORMATION *>(buffer);
        wstring file_name(pNotify->FileName,
                          pNotify->FileNameLength / sizeof(WCHAR));
        if (pNotify->Action == FILE_ACTION_MODIFIED &&
            file_name == L"data.json") {
          if (!initialized)
            UpdateDataFromJson();
        }
      }
      this_thread::sleep_for(chrono::seconds(1));
    }
    CloseHandle(hDir);
  }

  void UpdateDataFromJson() {
    char path[MAX_PATH] = {0};
    GetModuleFileNameA(NULL, path, MAX_PATH);
    filesystem::path dataPath = fs::path(path).parent_path() / "data.json";
    if (fs::exists(dataPath)) {
      try {
        ifstream json_file(dataPath.string());
        json j;
        json_file >> j;
        m_fontFace = u8tow(j["font_face"]);
        m_text = u8tow(j["test_text"]);
        m_fontPoint = j["font_point"];
        auto rmspace = RemoveSpaceAround;
        rmspace(m_fontFace);
      } catch (const exception &e) {
        auto errorMessage = ((string("update data failed, ") + e.what() +
                              ", please check data.json"));
        MessageBoxA(NULL, errorMessage.c_str(), "错误", MB_ICONERROR | MB_OK);
      }
    }
    // set m_hEditFontFace text by m_text
    SendMessage(m_hEditFontFace, EM_SETLIMITTEXT, (WPARAM)65535, 0);
    SendMessage(m_hEditFontFace, WM_SETTEXT, 0, (LPARAM)m_fontFace.c_str());
    SendMessage(m_hEditPreviewText, WM_SETTEXT, 0, (LPARAM)m_text.c_str());
    SendMessage(m_hComboBoxFontPoint, CB_SETCURSEL, (WPARAM)(m_fontPoint - 6),
                0);
    OnFontScaleChanged(0);
    m_previewControl->Refresh();
  }

  wstring GetCurrentFontToAdd() {
    auto ret = GetComboBoxSelectStr(m_hComboBox);
    int checkState = SendMessage(m_hCheckBox, BM_GETCHECK, 0, 0);
    if (checkState == BST_CHECKED) {
      auto start_str = GetTextOfEdit(m_hEditRangeStart);
      auto end_str = GetTextOfEdit(m_hEditRangeEnd);
      start_str = regex_replace(start_str, wregex(L"^0+$"), L"0");
      end_str = regex_replace(end_str, wregex(L"^0+$"), L"0");
      if (start_str == L"0") {
        if (!end_str.empty())
          ret += L"::" + end_str;
      } else {
        ret += L":" + start_str;
        if (!end_str.empty())
          ret += L":" + end_str;
      }
    }
    return ret;
  }

  wstring GetTextOfEdit(HWND &hwndEdit) {
    wchar_t buffer[4096] = {0};
    LRESULT textLength = SendMessage(hwndEdit, WM_GETTEXTLENGTH, 0, 0);
    if (textLength > 0) {
      SendMessage(hwndEdit, WM_GETTEXT, sizeof(buffer) / sizeof(wchar_t),
                  (LPARAM)buffer);
      return wstring(buffer);
    }
    return wstring(L"");
  };

  void OnResize() {
    GetClientRect(m_hWnd, &m_rect);
    SetWindowPos(m_hButtonAddFont, nullptr, m_rect.right - 130, 10, 120, 30,
                 SWP_NOZORDER);
    SetWindowPos(m_hEditPreviewText, nullptr, 100, 50,
                 m_rect.right - m_rect.left - 350, 30, SWP_NOZORDER);

    SetWindowPos(m_hEditFontFace, nullptr, 100, 90,
                 m_rect.right - m_rect.left - 350, 30, SWP_NOZORDER);
    SetWindowPos(m_hComboBoxScale, nullptr, m_rect.right - 130, 90, 120, 300,
                 SWP_NOZORDER);
    SetWindowPos(m_hComboBoxFontPoint, nullptr, m_rect.right - 130, 50, 120,
                 300, SWP_NOZORDER);
    SetWindowPos(m_hStaticEnd, nullptr, m_rect.right - 248, 15, 50, 30,
                 SWP_NOZORDER);
    SetWindowPos(m_hEditRangeStart, nullptr, m_rect.right - 330, 15, 80, 25,
                 SWP_NOZORDER);
    SetWindowPos(m_hEditRangeEnd, nullptr, m_rect.right - 230, 15, 80, 25,
                 SWP_NOZORDER);
    SetWindowPos(m_hComboBoxFontStyle, nullptr, m_rect.right - 230, 50, 80, 300,
                 SWP_NOZORDER);
    SetWindowPos(m_hComboBoxFontWeight, nullptr, m_rect.right - 230, 90, 80,
                 300, SWP_NOZORDER);
    SetWindowPos(m_previewControl->m_hWnd, nullptr, 0, previewTop, m_rect.right,
                 max((long int)1, m_rect.bottom - previewTop), SWP_NOZORDER);
    RedrawWindow(m_hWnd, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN);
  }

  wstring GetComboBoxSelectStr(HWND target) {
    int selectedIndex = SendMessage(target, CB_GETCURSEL, 0, 0);
    if (selectedIndex != CB_ERR) {
      int textLength = SendMessage(target, CB_GETLBTEXTLEN, selectedIndex, 0);
      if (textLength != CB_ERR) {
        wchar_t buffer[4096] = {0};
        SendMessage(target, CB_GETLBTEXT, selectedIndex, (LPARAM)buffer);
        return wstring(buffer);
      }
    }
    return L"";
  }

  void OnFontPointChanged(WPARAM wParam) {
    auto fontPointStr = GetComboBoxSelectStr(m_hComboBoxFontPoint);
    m_fontPoint = std::stoul(fontPointStr) * m_scale;
    UpdateView();
  }

  void OnFontScaleChanged(WPARAM wParam) {
    m_scale = _mapScale.at(GetComboBoxSelectStr(m_hComboBoxScale));
    auto fontPointStr = GetComboBoxSelectStr(m_hComboBoxFontPoint);
    m_fontPoint = std::stoul(fontPointStr) * m_scale;
    UpdateView();
  }

  void OnFontFaceEditChanged() {
    m_fontFace = GetTextOfEdit(m_hEditFontFace);
    RemoveSpaceAround(m_fontFace);
    UpdateView();
  }

  void OnRangeChanged(WPARAM wParam) {
    bool isStart = (LOWORD(wParam) == IDC_EDIT_RANGE_START);
    HWND target = isStart ? m_hEditRangeStart : m_hEditRangeEnd;
    auto fallback = isStart ? L"0" : L"10ffff";
    if (HIWORD(wParam) == EN_CHANGE) {
      const wregex regex(L"^[0-9a-fA-F]+$");
      auto str = GetTextOfEdit(target);
      if (!regex_match(str, regex))
        SendMessage(target, WM_SETTEXT, 0, (LPARAM)fallback); // 设置为默认值
    }
  }

  void OnStyleOrWeightChanged(WPARAM wParam) {
    if (HIWORD(wParam) == CBN_SELCHANGE) {
      bool isWeight = (LOWORD(wParam) == IDC_COMBO_BOX_FONT_WEIGHT);
      HWND target = isWeight ? m_hComboBoxFontWeight : m_hComboBoxFontStyle;
      const wregex pat = isWeight ? wregex(patWeight) : wregex(patStyle);
      auto defaultValue = isWeight ? L"regular" : L"normal";
      int selectedIndex = SendMessage(target, CB_GETCURSEL, 0, 0);
      auto str = GetComboBoxSelectStr(target);
      if (str.empty())
        return;
      m_fontFace = GetTextOfEdit(m_hEditFontFace);
      // 清除原来的字体样式或字重
      m_fontFace = regex_replace(m_fontFace, pat, L"");
      // 如果不是normal/regular, 使用正则表达式将 buffer 插入到第一个 ":" 之前
      if (str != defaultValue) {
        auto pat = wregex(L"(^\\s*(\\w| )+)(\\s*:\\s*)?");
        const auto replaceStr = L"$1 : " + str + L"$3";
        m_fontFace = regex_replace(m_fontFace, pat, replaceStr);
      }
      // 更新编辑框内容
      SendMessage(m_hEditFontFace, WM_SETTEXT, 0, (LPARAM)m_fontFace.c_str());
    }
  }

  void OnRangeEnableChkBox(WPARAM wParam) {
    if (HIWORD(wParam) == BN_CLICKED) {
      int checkState = SendMessage(m_hCheckBox, BM_GETCHECK, 0, 0);
      auto status = (checkState == BST_CHECKED) ? BST_UNCHECKED : BST_CHECKED;
      SendMessage(m_hCheckBox, BM_SETCHECK, status, 0);
      EnableWindow(m_hEditRangeStart, !(checkState == BST_CHECKED));
      EnableWindow(m_hEditRangeEnd, !(checkState == BST_CHECKED));
    }
  }

  void OnAddFontBtn() {
    auto font = GetCurrentFontToAdd();
    auto font_face = GetTextOfEdit(m_hEditFontFace);
    if (!font_face.empty())
      font_face += L", ";
    font_face += font;
    SendMessage(m_hEditFontFace, WM_SETTEXT, 0, (LPARAM)font_face.c_str());
  }

  enum {
    IDC_PREVIEW = 200,
    IDC_COMBO_BOX,
    IDC_COMBO_BOX_FONT_POINT,
    IDC_COMBO_BOX_FONT_STYLE,
    IDC_COMBO_BOX_FONT_WEIGHT,
    IDC_CHECK_BOX,
    IDC_EDIT_RANGE_START,
    IDC_EDIT_RANGE_END,
    IDC_EDIT_FONT_FACE,
    IDC_EDIT_PREVIEW_TEXT,
    IDC_COMBO_BOX_FONT_SCALE,
    IDC_ADD_FONT
  };

  HWND m_hComboBox = nullptr;
  HWND m_hCheckBox = nullptr;
  HWND m_hEditRangeStart = nullptr;
  HWND m_hEditRangeEnd = nullptr;
  HWND m_hButtonAddFont = nullptr;
  HWND m_hEditFontFace = nullptr;
  HWND m_hEditPreviewText = nullptr;
  HWND m_hComboBoxFontPoint = nullptr;
  HWND m_hComboBoxFontStyle = nullptr;
  HWND m_hComboBoxFontWeight = nullptr;
  HWND m_hStaticEnd = nullptr;
  HWND m_hComboBoxScale = nullptr;

  std::unique_ptr<FontPreviewControl> m_previewControl;

  wstring m_fontFace = L"Arial";
  int m_fontPoint = 16;
  std::wstring m_text = L"Sample Text";
  float m_scale = 1.0;

  bool initialized = false;
  const int previewTop = 120;
  thread m_monitorThread;
  atomic<bool> m_exitFlag;
};

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow) {
  // SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
  try {
    MainWindow(hInstance).Run();
  } catch (const std::exception &e) {
    MessageBoxA(nullptr, e.what(), "Error", MB_ICONERROR);
    return 1;
  }
  return 0;
}
