#pragma once

#include <windows.h>
#include "..\fake_ntdll\fake_ntdll.h"

namespace nt {

DWORD BaseSetLastNTError(NTSTATUS Status);

BOOL GetFileInformationByHandleEx(HANDLE handle, FILE_INFO_BY_HANDLE_CLASS FileInfoClass, LPVOID info, size_t size);
BOOL GetFileIdByHandle(HANDLE handle, UINT64 * fid);
BOOL GetVolumeIdByHandle(HANDLE handle, UINT64 * vid);

BOOL SetFileInformationByHandle(HANDLE handle, FILE_INFO_BY_HANDLE_CLASS FileInfoClass, LPCVOID info, size_t size);
BOOL SetFileAttrByHandle(HANDLE handle, const FILE_BASIC_INFORMATION * fbi);
BOOL SetFileAttrByHandle(HANDLE handle, const FILE_BASIC_INFO * fbi);
BOOL DeleteFileByHandle(HANDLE handle);


} /* namespace */
