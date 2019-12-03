// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#define WIN32_LEAN_AND_MEAN
#define _CRT_SECURE_NO_WARNINGS
#define _CRT_NON_CONFORMING_SWPRINTFS

#include "targetver.h"
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <shlwapi.h>

#ifndef RESOURCE_ENUM_VALIDATE
#error "Please, update Microsoft SDKs to 6.1 or later"
#endif

#ifndef _STDINT
#include "win32common\stdint.h"
#define _STDINT
#endif

#ifdef WCX_DEBUG
#define MAX_LOG_LEVEL   LL_TRACE
#else
#define MAX_LOG_LEVEL   LL_WARNING
#endif

/* block double using xxhash.h */
#define XXH_STATIC_H_3543687687345

#include "wcxhead.h"
#include "bst\string.hpp"
#include "bst\list.hpp"
#include "log.h"
#include "utils.h"
#include "winntapi.h"

#include "paxz.h"

#pragma comment (lib, "fake_ntdll.lib")
#pragma comment (lib, "shlwapi.lib")


#define FIN_IF(_cond_,_code_) do { if ((_cond_)) { hr = _code_; goto fin; } } while(0)
#define FIN(_code_)           do { hr = _code_; goto fin; } while(0)

#ifndef FILE_SHARE_VALID_FLAGS
#define FILE_SHARE_VALID_FLAGS  (FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE)
#endif


