#include "ntapi.h"

#pragma warning(disable:4273)
#pragma warning(disable:4985)

#ifdef NTSYSAPI
#undef NTSYSAPI
#define NTSYSAPI __declspec(dllexport)
#endif

#ifdef __cplusplus
extern "C" {
#endif

// =============================================================================

NTSYSAPI UINT NTAPI RtlGetLastWin32Error()
{
  return 0;
}

NTSYSAPI ULONG NTAPI RtlNtStatusToDosError(IN NTSTATUS Status)
{
  return 0;
}

NTSYSAPI VOID NTAPI RtlSetLastWin32ErrorAndNtStatusFromNtStatus(IN NTSTATUS Status)
{
  //
}

NTSYSAPI NTSTATUS NTAPI NtQueryInformationFile(
  HANDLE FileHandle,
  PIO_STATUS_BLOCK IoStatusBlock,
  PVOID FileInformation,
  ULONG Length,
  ULONG FileInformationClass
)
{
  return 0;
}

NTSYSAPI NTSTATUS NTAPI NtQueryDirectoryFile(
  HANDLE FileHandle,
  HANDLE Event,
  PIO_APC_ROUTINE ApcRoutine,
  PVOID ApcContext,
  PIO_STATUS_BLOCK IoStatusBlock,
  PVOID FileInformation,
  ULONG Length,
  FILE_INFORMATION_CLASS FileInformationClass,
  BOOLEAN ReturnSingleEntry,
  PUNICODE_STRING FileName,
  BOOLEAN RestartScan
)
{
  return 0;
}

NTSYSAPI NTSTATUS NTAPI NtQueryVolumeInformationFile(
  HANDLE               FileHandle,
  PIO_STATUS_BLOCK     IoStatusBlock,
  PVOID                FsInformation,
  ULONG                Length,
  FS_INFORMATION_CLASS FsInformationClass
) 
{
  return 0;
}

NTSYSAPI NTSTATUS NtSetInformationFile(
  HANDLE                 FileHandle,
  PIO_STATUS_BLOCK       IoStatusBlock,
  PVOID                  FileInformation,
  ULONG                  Length,
  FILE_INFORMATION_CLASS FileInformationClass
)
{
  return 0;
} 


NTSYSAPI VOID NTAPI RtlInitializeResource(PRTL_RESOURCE Resource)
{
  //
}

NTSYSAPI VOID NTAPI RtlDeleteResource(PRTL_RESOURCE Resource)
{
  //
}

NTSYSAPI VOID NTAPI RtlReleaseResource(PRTL_RESOURCE Resource)
{
  //
}
 
NTSYSAPI BOOLEAN NTAPI RtlAcquireResourceExclusive(PRTL_RESOURCE Resource, BOOLEAN Wait)
{
  return 0;
}

NTSYSAPI BOOLEAN NTAPI RtlAcquireResourceShared(PRTL_RESOURCE Resource, BOOLEAN Wait)
{
  return 0;
}

NTSYSAPI VOID NTAPI RtlConvertExclusiveToShared(PRTL_RESOURCE Resource)
{
  //
}

NTSYSAPI VOID NTAPI RtlConvertSharedToExclusive(PRTL_RESOURCE Resource)
{
  //
}

NTSYSAPI VOID NTAPI RtlDumpResource(PRTL_RESOURCE Resource)
{
  //
}

// ==============================================================================

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, PVOID lpReserved)
{
  return TRUE;
}

#ifdef __cplusplus
};
#endif
