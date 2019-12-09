#pragma once

#define WCX_VER_MAJOR     0
#define WCX_VER_MINOR     3

#define WCX_VER_GET_STR(num)  WCX_VER_GET_STR2(num)
#define WCX_VER_GET_STR2(num) #num

#define WCX_INTERNAL_NAME "paxz"
#define WCX_DESCRIPTION   "PAXZ  wcx-plugin"
#define WCX_VERSION       WCX_VER_GET_STR(WCX_VER_MAJOR) "." WCX_VER_GET_STR(WCX_VER_MINOR)
#define WCX_RC_VERSION    WCX_VER_MAJOR, WCX_VER_MINOR, 0, 0

#define WCX_COPYRIGHT     "\xA9 2019-2020 remittor"      // A9 for (c)
#define WCX_COMPANY_NAME  "https://github.com/remittor"
#define WCX_SOURCES       "https://github.com/remittor/paxz.wcx"
#define WCX_LICENSE       "https://github.com/remittor/paxz.wcx/blob/master/License.txt"
//#define WCX_LICENSE     "https://raw.githubusercontent.com/remittor/paxz.wcx/master/License.txt"

#ifdef WCX_DEBUG
#define WCX_FILE_DESC     WCX_DESCRIPTION " (DEBUG)"
#else
#define WCX_FILE_DESC     WCX_DESCRIPTION
#endif

#ifdef _WIN64
#define WCX_ORIG_FILENAME WCX_INTERNAL_NAME ".wcx64"
#else
#define WCX_ORIG_FILENAME WCX_INTERNAL_NAME ".wcx"
#endif

