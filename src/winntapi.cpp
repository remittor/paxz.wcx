#include "stdafx.h"
#include "winntapi.h"

#define WIN32_NO_STATUS
#include <ntstatus.h>

namespace nt {

BOOL GetFileInformationByHandleEx(HANDLE handle, FILE_INFO_BY_HANDLE_CLASS FileInfoClass, LPVOID info, size_t size)
{
  NTSTATUS status;
  IO_STATUS_BLOCK io;

  switch (FileInfoClass) {
    case FileBasicInfo:
      status = NtQueryInformationFile(handle, &io, info, (ULONG)size, FileBasicInformation);
      break;

    case FileStandardInfo:
      status = NtQueryInformationFile(handle, &io, info, (ULONG)size, FileStandardInformation);
      break;

    case FileNameInfo:
      status = NtQueryInformationFile(handle, &io, info, (ULONG)size, FileNameInformation);
      break;

    case FileIdBothDirectoryRestartInfo:
    case FileIdBothDirectoryInfo:
      status = NtQueryDirectoryFile(handle, NULL, NULL, NULL, &io, info, (ULONG)size,
        FileIdBothDirectoryInformation, FALSE, NULL,
        (FileInfoClass == FileIdBothDirectoryRestartInfo) );
      break;

    default:
      SetLastError(ERROR_INVALID_PARAMETER);
      return FALSE;
  }

  if (status != NT_STATUS_SUCCESS) {
    SetLastError(RtlNtStatusToDosError(status));
    return FALSE;
  }
  return TRUE;
}

BOOL GetFileIdByHandle(HANDLE handle, UINT64 * fid)
{
  NTSTATUS status;
  IO_STATUS_BLOCK io;
  __declspec(align(8)) FILE_INTERNAL_INFORMATION fii;

  status = NtQueryInformationFile(handle, &io, &fii, sizeof(fii), FileInternalInformation);
  if (status != NT_STATUS_SUCCESS) {
    SetLastError(RtlNtStatusToDosError(status));
    return FALSE;
  }
  *fid = (UINT64)fii.IndexNumber.QuadPart;
  return TRUE;
}

BOOL GetVolumeIdByHandle(HANDLE handle, UINT64 * vid)
{
  NTSTATUS status;
  IO_STATUS_BLOCK io;
  __declspec(align(8)) BYTE buf[sizeof(FILE_FS_VOLUME_INFORMATION) + 400];
  FILE_FS_VOLUME_INFORMATION * fvi = (FILE_FS_VOLUME_INFORMATION *)buf;

  status = NtQueryVolumeInformationFile(handle, &io, fvi, sizeof(buf), FileFsVolumeInformation);
  if (status != NT_STATUS_SUCCESS) {
    SetLastError(RtlNtStatusToDosError(status));
    return FALSE;
  }
  *vid = fvi->VolumeSerialNumber;
  return TRUE;
}

DWORD BaseSetLastNTError(NTSTATUS Status)
{
  DWORD dwErrCode;
  dwErrCode = RtlNtStatusToDosError(Status);
  SetLastError(dwErrCode);
  return dwErrCode;
}

/* Quick and dirty table for conversion */
static FILE_INFORMATION_CLASS ConvertToFileInfo[MaximumFileInfoByHandleClass] =
{
  FileBasicInformation, FileStandardInformation, FileNameInformation, FileRenameInformation,
  FileDispositionInformation, FileAllocationInformation, FileEndOfFileInformation, FileStreamInformation,
  FileCompressionInformation, FileAttributeTagInformation, FileIdBothDirectoryInformation, (FILE_INFORMATION_CLASS)-1,
  FileIoPriorityHintInformation
};

BOOL SetFileInformationByHandle(HANDLE handle, FILE_INFO_BY_HANDLE_CLASS FileInfoClass, LPCVOID info, size_t size)
{
  NTSTATUS Status;
  IO_STATUS_BLOCK IoStatusBlock;
  FILE_INFORMATION_CLASS ntFileInfoClass;

  ntFileInfoClass = (FILE_INFORMATION_CLASS)-1;
  if (FileInfoClass < MaximumFileInfoByHandleClass) {
    ntFileInfoClass = ConvertToFileInfo[FileInfoClass];
  }
  if (ntFileInfoClass == -1) {
    SetLastError(ERROR_INVALID_PARAMETER);
    return FALSE;
  }
  Status = NtSetInformationFile(handle, &IoStatusBlock, (PVOID)info, (ULONG)size, ntFileInfoClass);
  if (!NT_SUCCESS(Status)) {
    nt::BaseSetLastNTError(Status);
    return FALSE;
  }
  return TRUE;
}

BOOL SetFileAttrByHandle(HANDLE handle, const FILE_BASIC_INFORMATION * fbi)
{
  return nt::SetFileInformationByHandle(handle, FileBasicInfo, fbi, sizeof(*fbi));
} 

BOOL SetFileAttrByHandle(HANDLE handle, const FILE_BASIC_INFO * fbi)
{
  return nt::SetFileInformationByHandle(handle, FileBasicInfo, fbi, sizeof(*fbi));
} 

BOOL DeleteFileByHandle(HANDLE handle)
{
  FILE_DISPOSITION_INFORMATION fdi;
  fdi.DeleteFile = TRUE;
  return nt::SetFileInformationByHandle(handle, FileDispositionInfo, &fdi, sizeof(fdi));
} 

} /* namespace */
