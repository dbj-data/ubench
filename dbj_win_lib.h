#ifndef DBJ_WIN_LIB_INC_
#define DBJ_WIN_LIB_INC_
// (c) 2020 by dbj@dbj.org -- LICENSE_DBJ -- https://dbj.org/license_dbj

#ifndef _WIN32
#error This is WIN32 header
#endif // ! _WIN32

#ifdef __STDC_ALLOC_LIB__
#define __STDC_WANT_LIB_EXT2__ 1
#else
#define _POSIX_C_SOURCE 200809L
#endif

#include <errno.h>
#undef DBJ_PERROR
#define DBJ_PERROR (perror(__FILE__ " # " _CRT_STRINGIZE(__LINE__)))

/// there is no point in trying to avoid windows.h when on windows
// so lets include it properly
#define NOMINMAX
#define STRICT 1
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winnt.h>

#undef min
#define min(x, y) ((x) < (y) ? (x) : (y))

#undef max
#define max(x, y) ((x) > (y) ? (x) : (y))

// a minor ask: please compile on Windows 10
#if !(_WIN32_WINNT >= _WIN32_WINNT_WIN10)
#error Windows build using UBENCH, requires /DWINVER=0x0A00 /D_WIN32_WINNT=0x0A00
#endif

/// --------------------------------------------------------------------------------
#ifdef __cplusplus
extern "C" {
#endif
/// --------------------------------------------------------------------------------
//
// we emulate and return this, to the callers
// reason is mainly, we do not like the *agrarian* notation
//
// typedef struct _OSVERSIONINFOA {
//    DWORD size_info_;
//    DWORD major;
//    DWORD minor;
//    DWORD build_num;
//    DWORD platform_id;
//    CHAR   csd_version[128];     // Maintenance string for PSS usage
//} OSVERSIONINFOA, * POSVERSIONINFOA, * LPOSVERSIONINFOA;
//
typedef struct osinfo_struct {
  long size_info_;
  long major;
  long minor;
  long build_num;
  long platform_id;
  char csd_version[128]; // Maintenance string for PSS usage
} osinfo, *osinfo_ptr;

typedef LONG NTSTATUS; // , * PNTSTATUS{};
#define DBJ_STATUS_SUCCESS (0x00000000)

typedef NTSTATUS(WINAPI *get_version_fp)(osinfo_ptr);

// actually the sure-fire way to obtain windows version
inline osinfo get_win_version(void) {
  HMODULE hMod = ::GetModuleHandleA("ntdll.dll");
  if (hMod) {
    get_version_fp fxPtr =
        (get_version_fp)::GetProcAddress(hMod, "RtlGetVersion");
    if (fxPtr != nullptr) {
      osinfo osinfo_var_ = {0};
      osinfo_var_.size_info_ = sizeof(osinfo_var_);
      if (DBJ_STATUS_SUCCESS == fxPtr(&osinfo_var_)) {
        return osinfo_var_;
      }
    }
  }
  static osinfo osinfo_empty_ = {0, 0, 0, 0, 0, {0}};
  return osinfo_empty_;
}

#undef DBJ_STATUS_SUCCESS

#define IS_OS_INFO_EMPTY(info_)                                                \
  (0 ==  info_.major)

/*
return true if Windows version is greater than or equal to the specified number

If the Windows version is equal to or greater than 10.0.14393
then ENABLE_VIRTUAL_TERMINAL_PROCESSING is supported.

*/
inline bool is_win_ver_or_greater(WORD major_ver, WORD minor_ver,
                                  WORD build_ver) {
  osinfo ver_info_ = get_win_version();

  if (IS_OS_INFO_EMPTY(ver_info_))
    return false;

  if (ver_info_.major > major_ver)
    return true;
  else if (ver_info_.major < major_ver)
    return false;

  if (ver_info_.minor > minor_ver)
    return true;
  else if (ver_info_.minor < minor_ver)
    return false;

  //  we are here because both major and minor are equal
  if (ver_info_.build_num >= build_ver)
    return true;
  return false;
}

#undef IS_OS_INFO_EMPTY

/*
current machine may or may not  be on WIN10 where VT100 ESC codes are
*supposed to be* on by default, Sigh ...

Required WIN10 build number is 10586 or greater
to dance with exact win version please proceed here:
https://docs.microsoft.com/en-us/windows/win32/sysinfo/verifying-the-system-version
but please be aware that is semi-officialy abandoned method. it may or may not
work.

instead you can use the method bellow. if it returns false no VT100 colours.
WARNING: this will not exit the app *only* if app is started in WIN32 CONSOLE
Example: if running from git bash on win this will exit the app
if app output is redirected to file, this will also fail.
at runtime.
*/

#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#error ENABLE_VIRTUAL_TERMINAL_PROCESSING not found? Try Visual Studio solution re-targeting to the latest SDK.
#endif

/*
If your win10 terminal is not showing colors use this
it will not exit the app *only* if app is started in WIN32 CONSOLE
Example: if running from git bash on win this will exit the app
if app output is redirected to file, this will also fail.
*/
inline bool win_enable_vt_100_and_unicode() {
  static bool visited = false;
  if (visited)
    return true;

  int rez = SetConsoleOutputCP(CP_UTF8 /*65001*/);
  {
    if (rez == 0) {
      DBJ_PERROR;
      return false;
    }
  }
  // Set output mode to handle virtual terminal sequences
  HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
  if (hOut == INVALID_HANDLE_VALUE) {
    DBJ_PERROR;
    return false;
  }

  DWORD dwMode;
  if (!GetConsoleMode(hOut, &dwMode)) {
    fprintf(stderr, "\nFile: %s\nLine: %ul\nWhy: %s\n", __FILE__, __LINE__,
            ", GetConsoleMode() failed");
    fprintf(stderr, "\nPlease re-run in either WIN console %s",
            " or powershell console\n");
    DBJ_PERROR;
    return false;
  }

  dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
  if (!SetConsoleMode(hOut, dwMode)) {
    DBJ_PERROR;
    return false;
  }
  visited = true;
  return true;
}

/*
Where is the font_name coming from?

vs win32 immensely complex method for enumerating fonts, font names are easy to
find, just open your cmd.exe and go to properties; or if you like to be exact
find them here:
https://docs.microsoft.com/en-us/typography/fonts/windows_10_font_list

font_height_ if 0 will not be changed, other than that it can be any number
between 7 and 145. That is pixels.
*/
inline bool win_set_console_font(wchar_t *font_name,
                                 SHORT font_height_ /*= SHORT(0)*/) {
  CONSOLE_FONT_INFOEX font_info;
  font_info.cbSize = sizeof(CONSOLE_FONT_INFOEX);

  HANDLE con_out_handle = GetStdHandle(STD_OUTPUT_HANDLE);
  if (con_out_handle == INVALID_HANDLE_VALUE)
    return false;

  BOOL rez_ = GetCurrentConsoleFontEx(con_out_handle, TRUE, &font_info);

  if (rez_ == 0) {
    DBJ_PERROR;
    exit(-1);
  }

  // set the new font name
  (void)memset(font_info.FaceName, 0, LF_FACESIZE);
  (void)memcpy(font_info.FaceName, font_name, lstrlenW(font_name));

  // if requested set the new font size
  if (font_height_ > 0) {
    // quietly discard the silly sizes
    if ((font_height_ > 7) && (font_height_ < 145)) {
      font_info.dwFontSize.Y = font_height_;
    }
  }

  rez_ = SetCurrentConsoleFontEx(con_out_handle,
                                 TRUE, /* for the max window size */
                                 &font_info);

  if (rez_ == 0) {
    DBJ_PERROR;
    exit(-1);
  }
  return true;
}

#ifdef __cplusplus
} // extern "C" {
#endif

#endif // DBJ_WIN_LIB_INC_