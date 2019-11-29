#include "stdafx.h"
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
    memcpy(buf, "[PAXZ-X]", 8);
    buf[6] = g_log_level_str[level];
    buf[8] = 0x20;
    int len = _vsnprintf(buf + 9, _countof(buf)-2, fmt, argptr);
    if (len < 0)
      len = 0;
    buf[9 + len] = 0;
    OutputDebugStringA(buf);
    va_end(argptr);
  }
}

void WcxPrintMsgW(int level, const wchar_t * fmt, ...)
{
  wchar_t buf[4096];
  if (level <= g_log_level && level > 0) {
    va_list argptr;
    va_start(argptr, fmt);
    memcpy(buf, L"[PAXZ-X]", 8 * sizeof(wchar_t));
    buf[6] = (wchar_t)g_log_level_str[level];
    buf[8] = 0x20;
    int len = _vsnwprintf(buf + 9, _countof(buf)-2, fmt, argptr);
    if (len < 0)
      len = 0;
    buf[9 + len] = 0;
    OutputDebugStringW(buf);
    va_end(argptr);
  }
}
