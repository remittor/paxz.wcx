#pragma once

#include "ntapi.h"

#ifdef __cplusplus
extern "C" {
#endif

NTSYSAPI UINT NTAPI RtlGetLastWin32Error();
NTSYSAPI ULONG NTAPI RtlNtStatusToDosError(IN NTSTATUS Status);
NTSYSAPI VOID NTAPI RtlSetLastWin32ErrorAndNtStatusFromNtStatus(IN NTSTATUS Status);

NTSYSAPI NTSTATUS NTAPI NtQueryInformationFile(
  HANDLE FileHandle,
  PIO_STATUS_BLOCK IoStatusBlock,
  PVOID FileInformation,
  ULONG Length,
  ULONG FileInformationClass
);

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
);

NTSYSAPI NTSTATUS NTAPI NtQueryVolumeInformationFile(
  HANDLE               FileHandle,
  PIO_STATUS_BLOCK     IoStatusBlock,
  PVOID                FsInformation,
  ULONG                Length,
  FS_INFORMATION_CLASS FsInformationClass
);

NTSYSAPI NTSTATUS NtSetInformationFile(
  HANDLE                 FileHandle,
  PIO_STATUS_BLOCK       IoStatusBlock,
  PVOID                  FileInformation,
  ULONG                  Length,
  FILE_INFORMATION_CLASS FileInformationClass
); 

NTSYSAPI VOID NTAPI RtlInitializeResource(PRTL_RESOURCE Resource);
NTSYSAPI VOID NTAPI RtlDeleteResource(PRTL_RESOURCE Resource);
NTSYSAPI VOID NTAPI RtlReleaseResource(PRTL_RESOURCE Resource); 
NTSYSAPI BOOLEAN NTAPI RtlAcquireResourceExclusive(PRTL_RESOURCE Resource, BOOLEAN Wait);
NTSYSAPI BOOLEAN NTAPI RtlAcquireResourceShared(PRTL_RESOURCE Resource, BOOLEAN Wait);
NTSYSAPI VOID NTAPI RtlConvertExclusiveToShared(PRTL_RESOURCE Resource);
NTSYSAPI VOID NTAPI RtlConvertSharedToExclusive(PRTL_RESOURCE Resource);
NTSYSAPI VOID NTAPI RtlDumpResource(PRTL_RESOURCE Resource);

#ifdef __cplusplus
};
#endif

