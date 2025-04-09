#include "resource.h"
#include "utils.h"
#include <ShellScalingApi.h>
#include <algorithm>
#include <d2d1_2.h>
#include <dwrite_2.h>
#include <map>
#include <regex>
#include <string>
#include <vector>
#include <windows.h>
#include <wrl.h>

using namespace Microsoft::WRL;
using namespace std;

#define STYLEORWEIGHT (L":[^:]*[^a-f0-9:]+[^:]*")
#define DEBUG DebugStream() << __FUNCTION__ << L" @ line " << __LINE__ << L" : "

typedef ComPtr<IDWriteTextFormat1> PtTextFormat;

const wstring patStyle(L"(\\s*:\\s*italic|\\s*:\\s*oblique|\\s*:\\s*normal)");
// ----------------------------------------------------------------------------
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

struct D2D {

  D2D(HWND &hwnd, const RECT &rect, const wstring &text,
      COLORREF color = GetSysColor(COLOR_BTNFACE))
      : m_hWnd(hwnd), m_rect(rect), m_text(text) {
    m_color = D2D1::ColorF(GetRValue(color) / 255.0f, GetGValue(color) / 255.0f,
                           GetBValue(color) / 255.0f);
    m_rect.right -= m_rect.left;
    m_rect.bottom -= m_rect.top;
    m_rect.top = 0;
    m_rect.left = 0;
    InitializeDirect2D();
  }

  HRESULT InitFontFormat(const wstring &font_face, const int font_point) {
    DWRITE_WORD_WRAPPING wrapping = DWRITE_WORD_WRAPPING_WHOLE_WORD;
    DWRITE_FLOW_DIRECTION flow = DWRITE_FLOW_DIRECTION_LEFT_TO_RIGHT;
    GetDpi();

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

  bool GetDpi() {
    HMONITOR const mon = MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTONEAREST);
    UINT x = 0, y = 0;
    HR(GetDpiForMonitor(mon, MDT_EFFECTIVE_DPI, &x, &y));
    auto m_dpiX = static_cast<float>(x);
    auto m_dpiY = static_cast<float>(y);
    if (m_dpiY == 0)
      m_dpiX = m_dpiY = 96.0;
    auto dpiScaleFontPoint = m_dpiY / 72.0f;
    bool ret = (dpiScaleFontPoint == m_dpiScaleFontPoint);
    m_dpiScaleFontPoint = dpiScaleFontPoint;
    return ret;
  }

  void InitializeDirect2D() {
    // 创建 Direct2D 工厂
    HR(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
                         __uuidof(ID2D1Factory), nullptr, &m_pD2DFactory));
    // 创建渲染目标
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
  }

  void Draw() {
    if (!m_pRenderTarget)
      InitializeDirect2D();

    m_pRenderTarget->BeginDraw();
    m_pRenderTarget->Clear(m_color);
    if (m_pTextFormat && !m_text.empty()) {
      m_pRenderTarget->DrawText(
          m_text.c_str(), static_cast<UINT32>(m_text.length()),
          m_pTextFormat.Get(),
          D2D1::RectF(m_rect.left, m_rect.top, m_rect.right, m_rect.bottom),
          m_pBrush.Get(), D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
    }
    m_pRenderTarget->EndDraw();
  }

  ComPtr<ID2D1Factory> m_pD2DFactory;
  ComPtr<ID2D1HwndRenderTarget> m_pRenderTarget;
  ComPtr<ID2D1SolidColorBrush> m_pBrush;
  ComPtr<IDWriteFactory2> m_pDWriteFactory;
  ComPtr<IDWriteTextFormat1> m_pTextFormat;

  D2D1::ColorF m_color = D2D1::ColorF::White;
  const wstring &m_text;
  float m_dpiScaleFontPoint = 1.0f;
  HWND m_hWnd;
  RECT m_rect;
};

class FontSettingDialog {
public:
  FontSettingDialog(HINSTANCE hInstance) : hInstance_(hInstance), hDlg_(NULL) {}

  INT_PTR ShowDialog() {
    return DialogBoxParam(hInstance_, MAKEINTRESOURCE(IDD_DIALOG_FONT), NULL,
                          DialogProc, reinterpret_cast<LPARAM>(this));
  }

  wstring m_font_face = L"微软雅黑";
  wstring m_label_font_face = L"微软雅黑";
  wstring m_comment_font_face = L"微软雅黑";

  int m_font_point;
  int m_label_font_point;
  int m_comment_font_point;

private:
  static INT_PTR CALLBACK DialogProc(HWND hDlg, UINT message, WPARAM wParam,
                                     LPARAM lParam) {
    FontSettingDialog *pThis = nullptr;
    if (message == WM_INITDIALOG) {
      pThis = reinterpret_cast<FontSettingDialog *>(lParam);
      SetWindowLongPtr(hDlg, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pThis));
      pThis->hDlg_ = hDlg;
      return pThis->OnInitDialog();
    } else {
      pThis = reinterpret_cast<FontSettingDialog *>(
          GetWindowLongPtr(hDlg, GWLP_USERDATA));
      if (pThis) {
        return pThis->HandleMsg(hDlg, message, wParam, lParam);
      }
    }
    return (INT_PTR)FALSE;
  }

  INT_PTR HandleMsg(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
    case WM_COMMAND:
      return OnCommand(wParam);
    case WM_DPICHANGED:
    case WM_MOVE:
      if (m_pD2D && m_pD2D->GetDpi())
        return 0;
      RECT rect;
      GetWindowRect(m_hPreview, &rect);
      m_pD2D = make_unique<D2D>(m_hPreview, rect, m_text);
      UpdatePreview();
      return 0;
    case WM_PAINT: {
      PAINTSTRUCT ps;
      HDC hdc = BeginPaint(hDlg_, &ps);
      RECT rect;
      GetClientRect(hDlg_, &rect);
      FillRect(hdc, &rect, (HBRUSH)(COLOR_BTNFACE + 1));
      if (m_pD2D)
        m_pD2D->Draw();
      EndPaint(hDlg_, &ps);
      return 0;
    }
    }
    return (INT_PTR)FALSE;
  }

  INT_PTR OnInitDialog() {
    // 在这里初始化窗口控件
    HICON hIconSmall =
        (HICON)LoadIcon(hInstance_, MAKEINTRESOURCE(IDI_WEASELFONT));
    SendMessage(hDlg_, WM_SETICON, ICON_SMALL, (LPARAM)hIconSmall);

    m_hFontPickerCbb = GetDlgItem(hDlg_, IDC_CBB_FONTPICKER);
    InitFontPickerItems();

    m_hFontStyleCbb = GetDlgItem(hDlg_, IDC_CBB_FONTSTYLE);
    for (auto &it : _mapStyle) {
      SendMessage(m_hFontStyleCbb, CB_ADDSTRING, 0, (LPARAM)it.first.c_str());
    }
    SendMessage(m_hFontStyleCbb, CB_SETCURSEL, 1, 0);
    m_hFontWeightCbb = GetDlgItem(hDlg_, IDC_CBB_FONTWEIGHT);
    for (auto &it : _mapWeight) {
      SendMessage(m_hFontWeightCbb, CB_ADDSTRING, 0, (LPARAM)it.first.c_str());
    }
    SendMessage(m_hFontWeightCbb, CB_SETCURSEL, 9, 0);

    m_hFontFaceNameCbb = GetDlgItem(hDlg_, IDC_CBB_FONTFACE_NAME);
    const wstring fontFaceName[] = {L"label_font_face", L"font_face",
                                    L"comment_font_face"};
    for (const auto &name : fontFaceName)
      SendMessage(m_hFontFaceNameCbb, CB_ADDSTRING, 0, (LPARAM)name.c_str());
    SendMessage(m_hFontFaceNameCbb, CB_SETCURSEL, 1, 0);

    m_hComboBoxFontPoint = GetDlgItem(hDlg_, IDC_CBB_FONTPOINT);
    for (auto i = 0; i < 95; i++) {
      auto font_point = std::to_wstring(i + 6);
      SendMessage(m_hComboBoxFontPoint, CB_ADDSTRING, 0,
                  (LPARAM)font_point.c_str());
    }
    SendMessage(m_hComboBoxFontPoint, CB_SETCURSEL, 10, 0);

    m_hCheckBoxRangeEn = GetDlgItem(hDlg_, IDC_CKB_RANGEEN);
    m_hEditRangeEnd = GetDlgItem(hDlg_, IDC_EDIT_RANGEEND);
    m_hEditRangeStart = GetDlgItem(hDlg_, IDC_EDIT_RANGESTART);
    // set start L"0" and end L"10ffff"
    SetDlgItemText(hDlg_, IDC_EDIT_RANGESTART, L"0");
    SetDlgItemText(hDlg_, IDC_EDIT_RANGEEND, L"10ffff");

    m_font_face_ptr = &m_font_face;
    m_font_point_ptr = &m_font_point;

    m_hEditFontFace = GetDlgItem(hDlg_, IDC_EDIT_FONTFACE);
    m_hEditPreviewText = GetDlgItem(hDlg_, IDC_EDIT_PREVIEW_TEXT);
    SetDlgItemText(hDlg_, IDC_EDIT_PREVIEW_TEXT, m_text.c_str());
    SetDlgItemText(hDlg_, IDC_EDIT_FONTFACE, m_font_face_ptr->c_str());

    m_hPreview = GetDlgItem(hDlg_, IDC_STATIC_PREVIEW);
    RECT rect;
    GetWindowRect(m_hPreview, &rect);
    m_pD2D = make_unique<D2D>(m_hPreview, rect, m_text);
    UpdatePreview();
    auto str = GetComboBoxSelectStr(m_hComboBoxFontPoint);
    m_font_point = m_label_font_point = m_comment_font_point = stoi(str);

    return (INT_PTR)TRUE;
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

  wstring GetTextOfEdit(HWND &hwndEdit) {
    wchar_t buffer[4096] = {0};
    LRESULT textLength = SendMessage(hwndEdit, WM_GETTEXTLENGTH, 0, 0);
    if (textLength > 0 && textLength <= (_countof(buffer) - 1)) {
      SendMessage(hwndEdit, WM_GETTEXT, sizeof(buffer) / sizeof(wchar_t),
                  (LPARAM)buffer);
      return wstring(buffer);
    }
    return wstring(L"");
  };

  void UpdatePreview() {
    if (m_pD2D) {
      *m_font_face_ptr = GetTextOfEdit(m_hEditFontFace);
      auto font_point_str = GetComboBoxSelectStr(m_hComboBoxFontPoint);
      *m_font_point_ptr = std::stoi(font_point_str);
      m_text = GetTextOfEdit(m_hEditPreviewText);
      RemoveSpaceAround(*m_font_face_ptr);
      HR(m_pD2D->InitFontFormat(*m_font_face_ptr, *m_font_point_ptr));
      InvalidateRect(hDlg_, nullptr, true);
    }
  }

  void OnAddFont() {
    auto ret = GetComboBoxSelectStr(m_hFontPickerCbb);
    auto rangeEnabled = SendMessage(m_hCheckBoxRangeEn, BM_GETCHECK, 0, 0);
    if (rangeEnabled == BST_CHECKED) {
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
    auto font_face = GetTextOfEdit(m_hEditFontFace);
    if (!font_face.empty())
      font_face += L", ";
    font_face += ret;
    SendMessage(m_hEditFontFace, WM_SETTEXT, 0, (LPARAM)font_face.c_str());
  }

  void OnChangeFontFace() {
    auto curFontFaceName = GetComboBoxSelectStr(m_hFontFaceNameCbb);
    if (curFontFaceName == L"label_font_face") {
      m_font_face_ptr = &m_label_font_face;
      SendMessage(m_hComboBoxFontPoint, CB_SETCURSEL, m_label_font_point - 6,
                  0);
      m_font_point_ptr = &m_label_font_point;
    } else if (curFontFaceName == L"font_face") {
      m_font_face_ptr = &m_font_face;
      SendMessage(m_hComboBoxFontPoint, CB_SETCURSEL, m_font_point - 6, 0);
      m_font_point_ptr = &m_font_point;
    } else if (curFontFaceName == L"comment_font_face") {
      m_font_face_ptr = &m_comment_font_face;
      SendMessage(m_hComboBoxFontPoint, CB_SETCURSEL, m_comment_font_point - 6,
                  0);
      m_font_point_ptr = &m_comment_font_point;
    }
    DWRITE_FONT_STYLE style = DWRITE_FONT_STYLE_NORMAL;
    DWRITE_FONT_WEIGHT weight = DWRITE_FONT_WEIGHT_NORMAL;
    DWRITE_FONT_STRETCH stretch = DWRITE_FONT_STRETCH_NORMAL;
    ParseFontFace(*m_font_face_ptr, weight, style, stretch);
    wstring style_str(L"normal");
    for (const auto &it : _mapStyle) {
      if (it.second == style) {
        style_str = it.first;
        break;
      }
    }
    SendMessage(m_hFontStyleCbb, CB_SELECTSTRING, 0, (LPARAM)style_str.c_str());
    wstring weight_str(L"regular");
    for (const auto &it : _mapWeight) {
      if (it.second == weight) {
        weight_str = it.first;
        break;
      }
    }
    SendMessage(m_hFontWeightCbb, CB_SELECTSTRING, 0,
                (LPARAM)weight_str.c_str());
    SendMessage(m_hEditFontFace, WM_SETTEXT, 0,
                (LPARAM)m_font_face_ptr->c_str());
  }

  void OnRangeToggle() {
    int checkState = SendMessage(m_hCheckBoxRangeEn, BM_GETCHECK, 0, 0);
    auto status = (checkState == BST_CHECKED) ? BST_CHECKED : BST_UNCHECKED;
    SendMessage(m_hCheckBoxRangeEn, BM_SETCHECK, status, 0);
    EnableWindow(m_hEditRangeStart, (checkState == BST_CHECKED));
    EnableWindow(m_hEditRangeEnd, (checkState == BST_CHECKED));
  }

  INT_PTR OnChangeFontPoint() {
    auto font_point_str = GetComboBoxSelectStr(m_hComboBoxFontPoint);
    if (font_point_str.empty())
      return (INT_PTR)FALSE;
    auto font_point = std::stoi(font_point_str);
    *m_font_point_ptr = font_point;
    UpdatePreview();
    return TRUE;
  }

  void OnChangeFontFaceEdit() {
    *m_font_face_ptr = GetTextOfEdit(m_hEditFontFace);
    wstring curfont = L"";
    if (m_font_face_ptr == &m_label_font_face) {
      curfont = L"label_font_face";
    } else if (m_font_face_ptr == &m_comment_font_face) {
      curfont = L"comment_font_face";
    } else if (m_font_face_ptr == &m_font_face) {
      curfont = L"font_face";
    }
    UpdatePreview();
  }

  void HandleRangeInput(int controlId, const wchar_t *defaultValue) {
    wchar_t range[10] = {0};
    GetDlgItemText(hDlg_, controlId, range, 10);
    std::wstring rangeStr(range);
    std::wregex hexRegex(L"^[0-9a-fA-F]+$");

    if (rangeStr.empty() || !std::regex_match(rangeStr, hexRegex)) {
      SetDlgItemText(hDlg_, controlId, defaultValue);
    }
  }

  void OnStyleOrWeightChanged(WPARAM wParam) {
    if (HIWORD(wParam) == CBN_SELCHANGE) {
      bool isWeight = (LOWORD(wParam) == IDC_CBB_FONTWEIGHT);
      HWND target = isWeight ? m_hFontWeightCbb : m_hFontStyleCbb;
      const wregex pat = isWeight ? wregex(patWeight) : wregex(patStyle);
      auto defaultValue = isWeight ? L"regular" : L"normal";
      int selectedIndex = SendMessage(target, CB_GETCURSEL, 0, 0);
      auto str = GetComboBoxSelectStr(target);
      if (str.empty())
        return;
      *m_font_face_ptr = GetTextOfEdit(m_hEditFontFace);
      // 清除原来的字体样式或字重
      *m_font_face_ptr = regex_replace(*m_font_face_ptr, pat, L"");
      // 如果不是normal/regular, 使用正则表达式将 buffer 插入到第一个 ":" 之前
      if (str != defaultValue) {
        auto pat = wregex(L"(^\\s*(\\w| )+)(\\s*:\\s*)?");
        const auto replaceStr = L"$1:" + str + L"$3";
        *m_font_face_ptr = regex_replace(*m_font_face_ptr, pat, replaceStr);
      }
      // 更新编辑框内容
      SendMessage(m_hEditFontFace, WM_SETTEXT, 0,
                  (LPARAM)m_font_face_ptr->c_str());
    }
  }

  INT_PTR OnCommand(WPARAM wParam) {
    auto commandId = LOWORD(wParam);
    if (commandId == IDOK || commandId == IDCANCEL) {
      EndDialog(hDlg_, commandId);
      return (INT_PTR)TRUE;
    } else if (commandId == IDC_BTN_ADDFONT) {
      OnAddFont();
    } else if (commandId == IDC_CKB_RANGEEN && (HIWORD(wParam) == BN_CLICKED)) {
      OnRangeToggle();
    } else if (commandId == IDC_CBB_FONTFACE_NAME &&
               (HIWORD(wParam) == CBN_SELCHANGE)) {
      OnChangeFontFace();
    } else if (commandId == IDC_CBB_FONTPOINT &&
               HIWORD(wParam) == CBN_SELCHANGE) {
      return OnChangeFontPoint();
    } else if (commandId == IDC_EDIT_RANGESTART) {
      HandleRangeInput(IDC_EDIT_RANGESTART, L"0");
    } else if (commandId == IDC_EDIT_RANGEEND) {
      HandleRangeInput(IDC_EDIT_RANGEEND, L"10ffff");
    } else if (commandId == IDC_EDIT_FONTFACE && HIWORD(wParam) == EN_CHANGE) {
      OnChangeFontFaceEdit();
    } else if (commandId == IDC_EDIT_PREVIEW_TEXT &&
               HIWORD(wParam) == EN_CHANGE) {
      UpdatePreview();
    } else if ((commandId == IDC_CBB_FONTSTYLE) ||
               (commandId == IDC_CBB_FONTWEIGHT)) {
      OnStyleOrWeightChanged(wParam);
    }
    return (INT_PTR)FALSE;
  }

  void InitFontPickerItems() {
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
    // std::sort(fonts.begin(), fonts.end());
    //  sort fonts reverse order
    std::sort(fonts.begin(), fonts.end(), std::greater<wstring>());
    for (const auto &font : fonts) {
      SendMessage(m_hFontPickerCbb, CB_ADDSTRING, 0, (LPARAM)font.c_str());
    }
    SendMessage(m_hFontPickerCbb, CB_SETCURSEL, 0, 0);
  }

  HWND m_hFontPickerCbb = nullptr;
  HWND m_hFontStyleCbb = nullptr;
  HWND m_hFontWeightCbb = nullptr;
  HWND m_hFontFaceNameCbb = nullptr;
  HWND m_hComboBoxFontPoint = nullptr;
  HWND m_hCheckBoxRangeEn = nullptr;
  HWND m_hEditRangeStart = nullptr;
  HWND m_hEditRangeEnd = nullptr;
  HWND m_hEditFontFace = nullptr;
  HWND m_hEditPreviewText = nullptr;
  HWND m_hPreview = nullptr;

  wstring *m_font_face_ptr = nullptr;
  int *m_font_point_ptr = nullptr;

  // wstring m_text = L"Innovation in China，智造中国，慧及全球👉😁👈";
  wstring m_text = L"Weasel Powered by Rime 聰明的輸入法懂我心意🎉🎉🎉";
  std::unique_ptr<D2D> m_pD2D;

  HINSTANCE hInstance_;
  HWND hDlg_;
};

int APIENTRY _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                       LPTSTR lpCmdLine, int nCmdShow) {
  UNREFERENCED_PARAMETER(hPrevInstance);
  UNREFERENCED_PARAMETER(nCmdShow);
  // LANGID id = MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US);
  // LANGID id = MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_TRADITIONAL);
  // SetThreadUILanguage(id);
  // SetThreadLocale(id);
  SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
  FontSettingDialog dialog(hInstance);
  auto ret = dialog.ShowDialog();
  DEBUG << "dialog.ShowDialog(): " << ((ret == IDOK) ? L"OK" : L"Cancel");
  DEBUG << "font_face: " << dialog.m_font_face
        << ", font_point: " << dialog.m_font_point;
  DEBUG << "label_font_face: " << dialog.m_label_font_face
        << ", label_font_point: " << dialog.m_label_font_point;
  DEBUG << "comment_font_face: " << dialog.m_comment_font_face
        << ", comment_font_point: " << dialog.m_comment_font_point;

  return 0;
}
