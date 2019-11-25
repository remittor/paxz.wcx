#ifndef __NTDEF_H_
#define __NTDEF_H_

#include <windows.h>
#include "ntstatus.h"

#ifndef _NTDEF_
#define _NTDEF_ 

typedef __success(return >= 0) LONG NTSTATUS;

typedef NTSTATUS *PNTSTATUS;

#if _WIN32_WINNT >= 0x0600
typedef CONST NTSTATUS *PCNTSTATUS;
#endif // _WIN32_WINNT >= 0x0600 


#define NT_SUCCESS(Status) (((NTSTATUS)(Status)) >= 0)

// Generic test for information on any status value.
#define NT_INFORMATION(Status) ((((ULONG)(Status)) >> 30) == 1)

// Generic test for warning on any status value.
#define NT_WARNING(Status) ((((ULONG)(Status)) >> 30) == 2)

// Generic test for error on any status value.
#define NT_ERROR(Status) ((((ULONG)(Status)) >> 30) == 3)

#ifndef ERROR_SEVERITY_ERROR

#define APPLICATION_ERROR_MASK       0x20000000
#define ERROR_SEVERITY_SUCCESS       0x00000000
#define ERROR_SEVERITY_INFORMATIONAL 0x40000000
#define ERROR_SEVERITY_WARNING       0x80000000
#define ERROR_SEVERITY_ERROR         0xC0000000

#endif

#ifndef __SECSTATUS_DEFINED__
typedef long SECURITY_STATUS;
#define __SECSTATUS_DEFINED__
#endif

#ifndef NtCurrentProcess
#define NtCurrentProcess() ( (HANDLE)(LONG_PTR) -1 )  
#define ZwCurrentProcess() NtCurrentProcess()         
#endif /* NtCurrentProcess */

#ifndef NtCurrentThread
#define NtCurrentThread() ( (HANDLE)(LONG_PTR) -2 )   
#define ZwCurrentThread() NtCurrentThread()
#endif /* NtCurrentThread */


//
// Pointer to an Asciiz string
//

typedef CHAR *PSZ;
typedef CONST char *PCSZ;

//
// Counted String
//

typedef USHORT RTL_STRING_LENGTH_TYPE;

typedef struct _STRING {
  __maybevalid USHORT Length;
  __maybevalid USHORT MaximumLength;
  __field_bcount_part_opt(MaximumLength, Length) PCHAR Buffer;
} STRING;

typedef STRING *PSTRING;
typedef STRING ANSI_STRING;
typedef PSTRING PANSI_STRING;

typedef STRING OEM_STRING;
typedef PSTRING POEM_STRING;
typedef CONST STRING* PCOEM_STRING;

//
// CONSTCounted String
//

typedef struct _CSTRING {
  USHORT Length;
  USHORT MaximumLength;
  CONST char *Buffer;
} CSTRING;

typedef CSTRING *PCSTRING;
#define ANSI_NULL ((CHAR)0)     // winnt

typedef STRING CANSI_STRING;
typedef PSTRING PCANSI_STRING;

//
// Unicode strings are counted 16-bit character strings. If they are
// NULL terminated, Length does not include trailing NULL.
//

typedef struct _UNICODE_STRING {
  USHORT Length;
  USHORT MaximumLength;
  __field_bcount_part(MaximumLength, Length) PWCH   Buffer;
} UNICODE_STRING;

typedef UNICODE_STRING *PUNICODE_STRING;
typedef const UNICODE_STRING *PCUNICODE_STRING;
#define UNICODE_NULL ((WCHAR)0) // winnt

#if _WIN32_WINNT >= 0x0500

//#define UNICODE_STRING_MAX_BYTES ((USHORT) 65534) // winnt
//#define UNICODE_STRING_MAX_CHARS (32767) // winnt

#define DECLARE_CONST_UNICODE_STRING(_var, _string) \
  const WCHAR _var ## _buffer[] = _string; \
  __pragma(warning(push)) \
  __pragma(warning(disable:4221)) __pragma(warning(disable:4204)) \
  const UNICODE_STRING _var = { sizeof(_string) - sizeof(WCHAR), sizeof(_string), (PWCH) _var ## _buffer } \
  __pragma(warning(pop))

#define DECLARE_GLOBAL_CONST_UNICODE_STRING(_var, _str) \
  extern const __declspec(selectany) UNICODE_STRING _var = RTL_CONSTANT_STRING(_str)

#define DECLARE_UNICODE_STRING_SIZE(_var, _size) \
  WCHAR _var ## _buffer[_size]; \
  __pragma(warning(push)) \
  __pragma(warning(disable:4221)) __pragma(warning(disable:4204)) \
  UNICODE_STRING _var = { 0, _size * sizeof(WCHAR) , _var ## _buffer } \
  __pragma(warning(pop))

#endif // _WIN32_WINNT >= 0x0500


//
// Valid values for the Attributes field
//

#define OBJ_INHERIT             0x00000002L
#define OBJ_PERMANENT           0x00000010L
#define OBJ_EXCLUSIVE           0x00000020L
#define OBJ_CASE_INSENSITIVE    0x00000040L
#define OBJ_OPENIF              0x00000080L
#define OBJ_OPENLINK            0x00000100L
#define OBJ_KERNEL_HANDLE       0x00000200L
#define OBJ_FORCE_ACCESS_CHECK  0x00000400L
#define OBJ_VALID_ATTRIBUTES    0x000007F2L

//
// Object Attributes structure
//

typedef struct _OBJECT_ATTRIBUTES64 {
  ULONG Length;
  ULONG64 RootDirectory;
  ULONG64 ObjectName;
  ULONG Attributes;
  ULONG64 SecurityDescriptor;
  ULONG64 SecurityQualityOfService;
} OBJECT_ATTRIBUTES64;

typedef OBJECT_ATTRIBUTES64 *POBJECT_ATTRIBUTES64;
typedef CONST OBJECT_ATTRIBUTES64 *PCOBJECT_ATTRIBUTES64;

typedef struct _OBJECT_ATTRIBUTES32 {
  ULONG Length;
  ULONG RootDirectory;
  ULONG ObjectName;
  ULONG Attributes;
  ULONG SecurityDescriptor;
  ULONG SecurityQualityOfService;
} OBJECT_ATTRIBUTES32;

typedef OBJECT_ATTRIBUTES32 *POBJECT_ATTRIBUTES32;
typedef CONST OBJECT_ATTRIBUTES32 *PCOBJECT_ATTRIBUTES32;

typedef struct _OBJECT_ATTRIBUTES {
  ULONG Length;
  HANDLE RootDirectory;
  PUNICODE_STRING ObjectName;
  ULONG Attributes;
  PVOID SecurityDescriptor;        // Points to type SECURITY_DESCRIPTOR
  PVOID SecurityQualityOfService;  // Points to type SECURITY_QUALITY_OF_SERVICE
} OBJECT_ATTRIBUTES;

typedef OBJECT_ATTRIBUTES *POBJECT_ATTRIBUTES;
typedef CONST OBJECT_ATTRIBUTES *PCOBJECT_ATTRIBUTES;

#define InitializeObjectAttributes( p, n, a, r, s ) { \
  (p)->Length = sizeof( OBJECT_ATTRIBUTES );          \
  (p)->RootDirectory = r;                             \
  (p)->Attributes = a;                                \
  (p)->ObjectName = n;                                \
  (p)->SecurityDescriptor = s;                        \
  (p)->SecurityQualityOfService = NULL;               \
}

#define RTL_CONSTANT_OBJECT_ATTRIBUTES(n, a) \
{ sizeof(OBJECT_ATTRIBUTES), NULL, RTL_CONST_CAST(PUNICODE_STRING)(n), a, NULL, NULL }

#define RTL_INIT_OBJECT_ATTRIBUTES(n, a) RTL_CONSTANT_OBJECT_ATTRIBUTES(n, a)


#endif  /* _NTDEF_ */

#endif  /* __NTDEF_H_ */
