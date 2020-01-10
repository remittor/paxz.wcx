#define _CRT_SECURE_NO_WARNINGS

#include "log.h"
#include <stdio.h>
#include <vadefs.h>

static int  g_log_level = LL_TRACE;
static const char g_log_level_str[] = WCX_LL_STRING "--------------------------";

void WcxSetLogLevel(int level)
{
  if (level < 0) {
    g_log_level = -1;
  } else if (level > LL_TRACE) {
    g_log_level = LL_TRACE;
  } else {
    g_log_level = level;
  }
}

void WcxPrintMsgA(int level, const char * fmt, ...)
{
  char buf[4096];
  if (level <= g_log_level && level > 0) {
    va_list argptr;
    va_start(argptr, fmt);
    const size_t prefix_len = 9;
    memcpy(buf, "[PAXZ-X]", prefix_len - 1);
    buf[prefix_len - 3] = g_log_level_str[level];
    buf[prefix_len - 1] = 0x20;
    int len = _vsnprintf(buf + prefix_len, _countof(buf)-2, fmt, argptr);
    va_end(argptr);
    if (len < 0) {
      strcat(buf, "<INCORRECT-INPUT-DATA> ");
      strcat(buf, fmt);
    } else {
      len += prefix_len;
      buf[len] = 0;
    }
    OutputDebugStringA(buf);
  }
}

void WcxPrintMsgW(int level, const wchar_t * fmt, ...)
{
  union {
    struct {
      wchar_t reserved[4];
      wchar_t wbuf[4096];
    };
    char buf[4096 * 2];
  };
  if (level <= g_log_level && level > 0) {
    va_list argptr;
    va_start(argptr, fmt);
    const size_t prefix_len = 9;
    memcpy(wbuf, L"[PAXZ-X]", (prefix_len - 1) * sizeof(wchar_t));
    wbuf[prefix_len - 3] = (wchar_t)g_log_level_str[level];
    wbuf[prefix_len - 1] = 0x20;
    int len = _vsnwprintf(wbuf + prefix_len, _countof(wbuf)-2, fmt, argptr);
    va_end(argptr);
    if (len < 0) {
      wcscat(wbuf, L"<INCORRECT-INPUT-DATA> ");
      wcscat(wbuf, fmt);
      len = (int)wcslen(wbuf);
    } else {
      len += prefix_len;
    }
    len = WideCharToMultiByte(CP_ACP, 0, wbuf, len, (LPSTR)buf, sizeof(buf)-2, NULL, NULL);
    buf[(len > 0) ? len : 0] = 0;
    OutputDebugStringA(buf);
  }
}
