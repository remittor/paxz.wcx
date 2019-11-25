#pragma once

#include <windows.h>

#define LL_FATAL_ERROR 1
#define LL_CRIT_ERROR  2
#define LL_ERROR       3
#define LL_WARNING     4
#define LL_NOTICE      5
#define LL_INFO        6
#define LL_DEBUG       7
#define LL_TRACE       8 

#define WCX_LL_STRING   "#FCEwnidt"

void WcxSetLogLevel(int level);
void WcxPrintMsgA(int level, const char * fmt, ...);
void WcxPrintMsgW(int level, const wchar_t * fmt, ...);


#define LOGX_IF(_level_, _cond_, ...)   if ((_cond_)) WcxPrintMsgA(_level_, __VA_ARGS__)
#define LOGX(_level_, ...)              WcxPrintMsgA(_level_, __VA_ARGS__)

#define WLOGX_IF(_level_, _cond_, ...)  if ((_cond_)) WcxPrintMsgW(_level_, __VA_ARGS__)
#define WLOGX(_level_, ...)             WcxPrintMsgW(_level_, __VA_ARGS__)


#if defined(MAX_LOG_LEVEL) && MAX_LOG_LEVEL >= LL_TRACE
#define LOGt(...)                  LOGX(LL_TRACE, __VA_ARGS__)
#define LOGt_IF(_cond_, ...)    LOGX_IF(LL_TRACE, (_cond_), __VA_ARGS__)
#define WLOGt(...)                WLOGX(LL_TRACE, __VA_ARGS__)
#define WLOGt_IF(_cond_, ...)  WLOGX_IF(LL_TRACE, (_cond_), __VA_ARGS__)
#else
#define LOGt(...)  
#define LOGt_IF(_cond_, ...) 
#define WLOGt(...) 
#define WLOGt_IF(_cond_, ...)
#endif

#if defined(MAX_LOG_LEVEL) && MAX_LOG_LEVEL >= LL_DEBUG
#define LOGd(...)                  LOGX(LL_DEBUG, __VA_ARGS__)
#define LOGd_IF(_cond_, ...)    LOGX_IF(LL_DEBUG, (_cond_), __VA_ARGS__)
#define WLOGd(...)                WLOGX(LL_DEBUG, __VA_ARGS__)
#define WLOGd_IF(_cond_, ...)  WLOGX_IF(LL_DEBUG, (_cond_), __VA_ARGS__)
#else
#define LOGd(...)  
#define LOGd_IF(_cond_, ...) 
#define WLOGd(...) 
#define WLOGd_IF(_cond_, ...)
#endif

#if defined(MAX_LOG_LEVEL) && MAX_LOG_LEVEL >= LL_INFO
#define LOGi(...)                  LOGX(LL_INFO, __VA_ARGS__)
#define LOGi_IF(_cond_, ...)    LOGX_IF(LL_INFO, (_cond_), __VA_ARGS__)
#define WLOGi(...)                WLOGX(LL_INFO, __VA_ARGS__)
#define WLOGi_IF(_cond_, ...)  WLOGX_IF(LL_INFO, (_cond_), __VA_ARGS__)
#else
#define LOGi(...)  
#define LOGi_IF(_cond_, ...) 
#define WLOGi(...) 
#define WLOGi_IF(_cond_, ...)
#endif

#if defined(MAX_LOG_LEVEL) && MAX_LOG_LEVEL >= LL_NOTICE
#define LOGn(...)                  LOGX(LL_NOTICE, __VA_ARGS__)
#define LOGn_IF(_cond_, ...)    LOGX_IF(LL_NOTICE, (_cond_), __VA_ARGS__)
#define WLOGn(...)                WLOGX(LL_NOTICE, __VA_ARGS__)
#define WLOGn_IF(_cond_, ...)  WLOGX_IF(LL_NOTICE, (_cond_), __VA_ARGS__)
#else
#define LOGn(...)  
#define LOGn_IF(_cond_, ...) 
#define WLOGn(...) 
#define WLOGn_IF(_cond_, ...)
#endif

#if defined(MAX_LOG_LEVEL) && MAX_LOG_LEVEL >= LL_WARNING
#define LOGw(...)                  LOGX(LL_WARNING, __VA_ARGS__)
#define LOGw_IF(_cond_, ...)    LOGX_IF(LL_WARNING, (_cond_), __VA_ARGS__)
#define WLOGw(...)                WLOGX(LL_WARNING, __VA_ARGS__)
#define WLOGw_IF(_cond_, ...)  WLOGX_IF(LL_WARNING, (_cond_), __VA_ARGS__)
#else
#define LOGw(...)  
#define LOGw_IF(_cond_, ...) 
#define WLOGw(...) 
#define WLOGw_IF(_cond_, ...)
#endif

#if defined(MAX_LOG_LEVEL) && MAX_LOG_LEVEL >= LL_ERROR
#define LOGe(...)                  LOGX(LL_ERROR, __VA_ARGS__)
#define LOGe_IF(_cond_, ...)    LOGX_IF(LL_ERROR, (_cond_), __VA_ARGS__)
#define WLOGe(...)                WLOGX(LL_ERROR, __VA_ARGS__)
#define WLOGe_IF(_cond_, ...)  WLOGX_IF(LL_ERROR, (_cond_), __VA_ARGS__)
#else
#define LOGe(...)  
#define LOGe_IF(_cond_, ...) 
#define WLOGe(...) 
#define WLOGe_IF(_cond_, ...)
#endif

#if defined(MAX_LOG_LEVEL) && MAX_LOG_LEVEL >= LL_CRIT_ERROR
#define LOGc(...)                  LOGX(LL_CRIT_ERROR, __VA_ARGS__)
#define LOGc_IF(_cond_, ...)    LOGX_IF(LL_CRIT_ERROR, (_cond_), __VA_ARGS__)
#define WLOGc(...)                WLOGX(LL_CRIT_ERROR, __VA_ARGS__)
#define WLOGc_IF(_cond_, ...)  WLOGX_IF(LL_CRIT_ERROR, (_cond_), __VA_ARGS__)
#else
#define LOGc(...)  
#define LOGc_IF(_cond_, ...) 
#define WLOGc(...) 
#define WLOGc_IF(_cond_, ...)
#endif

#if defined(MAX_LOG_LEVEL) && MAX_LOG_LEVEL >= LL_FATAL_ERROR
#define LOGf(...)                  LOGX(LL_FATAL_ERROR, __VA_ARGS__)
#define LOGf_IF(_cond_, ...)    LOGX_IF(LL_FATAL_ERROR, (_cond_), __VA_ARGS__)
#define WLOGf(...)                WLOGX(LL_FATAL_ERROR, __VA_ARGS__)
#define WLOGf_IF(_cond_, ...)  WLOGX_IF(LL_FATAL_ERROR, (_cond_), __VA_ARGS__)
#else
#define LOGf(...)  
#define LOGf_IF(_cond_, ...) 
#define WLOGf(...) 
#define WLOGf_IF(_cond_, ...)
#endif
