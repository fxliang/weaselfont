#pragma once
#include <sstream>
#include <tchar.h>
#include <windows.h>
#include <memory>

using namespace std;
using wstring = std::wstring;
using string = std::string;

// convert string to wstring, in code_page
inline std::wstring string_to_wstring(const std::string &str,
                                      int code_page = CP_ACP) {
  // support CP_ACP and CP_UTF8 only
  if (code_page != CP_ACP && code_page != CP_UTF8)
    return L"";
  int len = MultiByteToWideChar(code_page, 0, str.c_str(), (int)str.size(),
                                nullptr, 0);
  if (len <= 0)
    return L"";
  // Use unique_ptr to manage the buffer memory automatically
  std::unique_ptr<WCHAR[]> buffer = std::make_unique<WCHAR[]>(len + 1);
  MultiByteToWideChar(code_page, 0, str.c_str(), (int)str.size(), buffer.get(),
                      len);
  buffer[len] = L'\0'; // Null-terminate the wide string
  return std::wstring(buffer.get());
}
// convert wstring to string, in code_page
inline std::string wstring_to_string(const std::wstring &wstr,
                                     int code_page = CP_ACP) {
  // support CP_ACP and CP_UTF8 only
  if (code_page != CP_ACP && code_page != CP_UTF8)
    return "";
  int len = WideCharToMultiByte(code_page, 0, wstr.c_str(), (int)wstr.size(),
                                nullptr, 0, nullptr, nullptr);
  if (len <= 0)
    return "";
  // Use unique_ptr to manage the buffer memory automatically
  std::unique_ptr<char[]> buffer = std::make_unique<char[]>(len + 1);
  WideCharToMultiByte(code_page, 0, wstr.c_str(), (int)wstr.size(),
                      buffer.get(), len, nullptr, nullptr);
  buffer[len] = '\0'; // Null-terminate the string
  return std::string(buffer.get());
}

inline int utf8towcslen(const char *utf8_str, int utf8_len) {
  return MultiByteToWideChar(CP_UTF8, 0, utf8_str, utf8_len, NULL, 0);
}

#define wtou8(x) wstring_to_string(x, CP_UTF8)
#define wtoacp(x) wstring_to_string(x)
#define u8tow(x) string_to_wstring(x, CP_UTF8)
#define acptow(x) string_to_wstring(x, CP_ACP)
#define MAX(x, y) ((x > y) ? x : y)
#define MIN(x, y) ((x < y) ? x : y)

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
  ~DebugStream() { OutputDebugString(ss.str().c_str()); }
  template <typename T> DebugStream &operator<<(const T &value) {
    ss << value;
    return *this;
  }
  DebugStream &operator<<(const char *value) {
    if (value) {
      std::wstring wvalue(u8tow(value)); // utf-8
      ss << wvalue;
    }
    return *this;
  }
  DebugStream &operator<<(const std::string value) {
    std::wstring wvalue(u8tow(value)); // utf-8
    ss << wvalue;
    return *this;
  }
  DebugStream &operator<<(const RECT &rc) {
    ss << L"rc = {" << rc.left << L", " << rc.top << L", " << rc.right << L", "
       << rc.bottom << L"}";
    return *this;
  }
  DebugStream &operator<<(const POINT &pt) {
    ss << L"pt = (" << pt.x << L", " << pt.y << L")";
    return *this;
  }

private:
  std::wstringstream ss;
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


