#pragma once
#include <sstream>
#include <tchar.h>
#include <windows.h>
#include <memory>
#include <vector>
#include <regex>

inline std::basic_string<TCHAR> StrzHr(HRESULT hr) {
  TCHAR buffer[1024] = {0};
  if (FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                    NULL, hr, 0, buffer, sizeof(buffer) / sizeof(TCHAR),
                    NULL)) {
    return std::basic_string<TCHAR>(buffer);
  } else {
    std::basic_ostringstream<TCHAR> oss;
    oss << _T("Unknown error: 0x") << std::hex << hr;
    return oss.str();
  }
}
class DebugStream {
public:
  DebugStream() = default;
  ~DebugStream() {
    if (!wss.str().empty())
      OutputDebugString(wss.str().c_str());
    if (!ss.str().empty())
      OutputDebugStringA(ss.str().c_str());
  }
  template <typename T> DebugStream &operator<<(const T &value) {
    ss << value;
    return *this;
  }
  DebugStream &operator<<(const char *value) {
    if (value) {
      std::string svalue((value));
      ss << svalue;
    }
    return *this;
  }
  DebugStream &operator<<(const std::string value) {
    std::string svalue(value);
    ss << svalue;
    return *this;
  }
  DebugStream &operator<<(const std::wstring value) {
    std::wstring wvalue(value);
    wss << wvalue;
    return *this;
  }
private:
  std::wstringstream wss;
  std::stringstream ss;
};
struct ComException {
  HRESULT result;
  ComException(HRESULT const value) : result(value) {}
};
inline void HR_Impl(HRESULT const result, const char *file, int line) {
  if (S_OK != result) {
    DebugStream() << "[" << file << ":" << line << "] " << StrzHr(result);
    throw ComException(result);
  }
}
#define HR(result) HR_Impl(result, __FILE__, __LINE__)

#define TEST(hr)                                                               \
  if (FAILED(hr))                                                              \
  return hr

using namespace std;

// convert string to wstring, in code_page
inline wstring string_to_wstring(const string &str, int code_page = CP_ACP) {
  // support CP_ACP and CP_UTF8 only
  if (code_page != CP_ACP && code_page != CP_UTF8)
    return L"";
  int len = MultiByteToWideChar(code_page, 0, str.c_str(), (int)str.size(),
                                nullptr, 0);
  if (len <= 0)
    return L"";
  // Use unique_ptr to manage the buffer memory automatically
  unique_ptr<WCHAR[]> buffer = make_unique<WCHAR[]>(len + 1);
  MultiByteToWideChar(code_page, 0, str.c_str(), (int)str.size(), buffer.get(),
                      len);
  buffer[len] = L'\0'; // Null-terminate the wide string
  return wstring(buffer.get());
}
// convert wstring to string, in code_page
inline string wstring_to_string(const wstring &wstr, int code_page = CP_ACP) {
  // support CP_ACP and CP_UTF8 only
  if (code_page != CP_ACP && code_page != CP_UTF8)
    return "";
  int len = WideCharToMultiByte(code_page, 0, wstr.c_str(), (int)wstr.size(),
                                nullptr, 0, nullptr, nullptr);
  if (len <= 0)
    return "";
  // Use unique_ptr to manage the buffer memory automatically
  unique_ptr<char[]> buffer = make_unique<char[]>(len + 1);
  WideCharToMultiByte(code_page, 0, wstr.c_str(), (int)wstr.size(),
                      buffer.get(), len, nullptr, nullptr);
  buffer[len] = '\0'; // Null-terminate the string
  return string(buffer.get());
}
inline int utf8towcslen(const char *utf8_str, int utf8_len) {
  return MultiByteToWideChar(CP_UTF8, 0, utf8_str, utf8_len, NULL, 0);
}

#define wtou8(x) wstring_to_string(x, CP_UTF8)
#define wtoacp(x) wstring_to_string(x)
#define u8tow(x) string_to_wstring(x, CP_UTF8)
#define acptow(x) string_to_wstring(x, CP_ACP)

