#include "resource.h"
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
#include <FontSettingDialog.h>

using namespace Microsoft::WRL;
using namespace std;

int APIENTRY _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                       LPTSTR lpCmdLine, int nCmdShow) {
  UNREFERENCED_PARAMETER(hPrevInstance);
  UNREFERENCED_PARAMETER(nCmdShow);
  // LANGID id = MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US);
  // LANGID id = MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_TRADITIONAL);
  // SetThreadUILanguage(id);
  // SetThreadLocale(id);
  SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
  FontSettingDialog dialog(L"微软雅黑", 16,L"微软雅黑", 16,L"微软雅黑", 16, nullptr);
  auto ret = dialog.DoModal();
  DEBUG << "dialog.ShowDialog(): " << ((ret == IDOK) ? L"OK" : L"Cancel");
  DEBUG << "font_face: " << dialog.m_font_face
        << ", font_point: " << dialog.m_font_point;
  DEBUG << "label_font_face: " << dialog.m_label_font_face
        << ", label_font_point: " << dialog.m_label_font_point;
  DEBUG << "comment_font_face: " << dialog.m_comment_font_face
        << ", comment_font_point: " << dialog.m_comment_font_point;

  return 0;
}
