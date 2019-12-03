#include "wcx_plugin.h"

HANDLE g_hInstance = NULL;
wcx::plugin g_wcx;


//#define WcxDllExport extern "C" __declspec( dllexport )
#define WcxDllExport

WcxDllExport
void WINAPI PackSetDefaultParams(tPackDefaultParam * dps)
{
  #pragma comment(linker, "/EXPORT:" __FUNCTION__ "=" __FUNCDNAME__)
  LOGt("%s: ver = %d.%d, ini = \"%s\" ", __func__, dps->PluginInterfaceVersionHi, dps->PluginInterfaceVersionLow, dps->DefaultIniName);
}

WcxDllExport
void WINAPI SetChangeVolProc(void * hArcData, tChangeVolProcW pChangeVolProc)
{
  #pragma comment(linker, "/EXPORT:" __FUNCTION__ "=" __FUNCDNAME__)
  wcx::archive * arc = (wcx::archive *)hArcData;
  if ((SSIZE_T)arc == -1 || !arc) {
    WLOGt(L"%S: %p [hArcData = %Id]", __func__, pChangeVolProc, (SSIZE_T)arc);
  } else {
    WLOGt(L"%S: %p \"%s\" ", __func__, pChangeVolProc, arc->get_name());
  }
}

WcxDllExport
void WINAPI SetChangeVolProcW(void * hArcData, tChangeVolProcW pChangeVolProc)
{
  #pragma comment(linker, "/EXPORT:" __FUNCTION__ "=" __FUNCDNAME__)
  wcx::archive * arc = (wcx::archive *)hArcData;
  if ((SSIZE_T)arc == -1 || !arc) {
    WLOGt(L"%S: %p [hArcData = %Id]", __func__, pChangeVolProc, (SSIZE_T)arc);
  } else {
    WLOGt(L"%S: %p \"%s\" ", __func__, pChangeVolProc, arc->get_name());
    arc->m_cb.set_ChangeVolProc(pChangeVolProc);
  }
}

WcxDllExport
void WINAPI SetProcessDataProc(void * hArcData, tProcessDataProcW pProcessDataProc)
{
  #pragma comment(linker, "/EXPORT:" __FUNCTION__ "=" __FUNCDNAME__)
  wcx::archive * arc = (wcx::archive *)hArcData;
  if ((SSIZE_T)arc == -1 || !arc) {
    WLOGt(L"%S: %p [hArcData = %Id]", __func__, pProcessDataProc, (SSIZE_T)arc);
  } else {
    WLOGt(L"%S: %p \"%s\" ", __func__, pProcessDataProc, arc->get_name());
  }
}

WcxDllExport
void WINAPI SetProcessDataProcW(void * hArcData, tProcessDataProcW pProcessDataProc)
{
  #pragma comment(linker, "/EXPORT:" __FUNCTION__ "=" __FUNCDNAME__)
  wcx::archive * arc = (wcx::archive *)hArcData;
  if ((SSIZE_T)arc == -1 || !arc) {
    WLOGt(L"%S: %p [hArcData = %Id]", __func__, pProcessDataProc, (SSIZE_T)arc);
    g_wcx.set_ProcessDataProcW(pProcessDataProc);
  } else {
    WLOGt(L"%S: %p \"%s\" ", __func__, pProcessDataProc, arc->get_name());
    arc->m_cb.set_ProcessDataProc(pProcessDataProc);
  }
}

WcxDllExport
BOOL WINAPI CanYouHandleThisFile(char * filename)
{
  #pragma comment(linker, "/EXPORT:" __FUNCTION__ "=" __FUNCDNAME__)
  LOGw("%s: <<<NOT_SUPPORTED>>> filename = \"%s\" ", __func__, filename);
  return E_NOT_SUPPORTED;
}

WcxDllExport
BOOL WINAPI CanYouHandleThisFileW(LPCWSTR filename)
{
  #pragma comment(linker, "/EXPORT:" __FUNCTION__ "=" __FUNCDNAME__)
  int hr = g_wcx.check_file(filename);
  WLOGi(L"%S: filename = \"%s\" ==> result = 0x%X ", __func__, filename, hr);
  return hr ? FALSE : TRUE;
}

WcxDllExport
void * WINAPI OpenArchive(tOpenArchiveData * ArchiveData)
{
  #pragma comment(linker, "/EXPORT:" __FUNCTION__ "=" __FUNCDNAME__)
  LOGw("%s: <<<NOT_SUPPORTED>>> name = \"%s\" ", __func__, ArchiveData->ArcName);
  ArchiveData->OpenResult = E_NOT_SUPPORTED;
  return NULL;
}

WcxDllExport
void * WINAPI OpenArchiveW(tOpenArchiveDataW * ArchiveData)
{
  #pragma comment(linker, "/EXPORT:" __FUNCTION__ "=" __FUNCDNAME__)
  wcx::archive * arc = NULL;
  ArchiveData->OpenResult = g_wcx.open_file(ArchiveData->ArcName, ArchiveData->OpenMode, &arc);
  return ArchiveData->OpenResult ? NULL : arc;
}

WcxDllExport
int WINAPI CloseArchive(void * hArcData)
{
  #pragma comment(linker, "/EXPORT:" __FUNCTION__ "=" __FUNCDNAME__)
  if (!hArcData)
    return E_ECLOSE;
  return g_wcx.close_file((wcx::archive *)hArcData);
}

WcxDllExport
int WINAPI ReadHeader(HANDLE hArcData, tHeaderData * headerData)
{
  #pragma comment(linker, "/EXPORT:" __FUNCTION__ "=" __FUNCDNAME__)
  LOGw("%s: <<<NOT_SUPPORTED>>>", __func__);
  return E_NOT_SUPPORTED;
}

WcxDllExport
int WINAPI ReadHeaderEx(void * hArcData, tHeaderDataEx * headerData)
{
  #pragma comment(linker, "/EXPORT:" __FUNCTION__ "=" __FUNCDNAME__)
  LOGw("%s: <<<NOT_SUPPORTED>>>", __func__);
  return E_NOT_SUPPORTED;
}

WcxDllExport
int WINAPI ReadHeaderExW(void * hArcData, tHeaderDataExW * headerData)
{
  #pragma comment(linker, "/EXPORT:" __FUNCTION__ "=" __FUNCDNAME__)
  wcx::archive * arc = (wcx::archive *)hArcData;
  return arc->list(headerData);
}

WcxDllExport
int WINAPI ProcessFile(void * hArcData, int Operation, char * DestPath, char * DestName)
{
  #pragma comment(linker, "/EXPORT:" __FUNCTION__ "=" __FUNCDNAME__)
  LOGw("%s: <<<NOT_SUPPORTED>>>", __func__);
  return E_NOT_SUPPORTED;
}

WcxDllExport
int WINAPI ProcessFileW(void * hArcData, int Operation, LPCWSTR DestPath, LPCWSTR DestName)
{
  #pragma comment(linker, "/EXPORT:" __FUNCTION__ "=" __FUNCDNAME__)
  wcx::archive * arc = (wcx::archive *)hArcData;
  return arc->extract(Operation, DestPath, DestName);
}

WcxDllExport
int WINAPI PackFiles(char * PackedFile, char * SubPath, char * SrcPath, char * AddList, int Flags)
{
  #pragma comment(linker, "/EXPORT:" __FUNCTION__ "=" __FUNCDNAME__)
  LOGw("%s: <<<NOT_SUPPORTED>>>", __func__);
  return E_NOT_SUPPORTED;
}

WcxDllExport
int WINAPI PackFilesW(LPCWSTR PackedFile, LPCWSTR SubPath, LPCWSTR SrcPath, LPCWSTR AddList, int Flags)
{
  #pragma comment(linker, "/EXPORT:" __FUNCTION__ "=" __FUNCDNAME__)
  return g_wcx.pack_files(PackedFile, SubPath, SrcPath, AddList, Flags);
}

WcxDllExport
int WINAPI DeleteFiles(char * PackedFile, char ** DeleteList)
{
  #pragma comment(linker, "/EXPORT:" __FUNCTION__ "=" __FUNCDNAME__)
  LOGw("%s: <<<NOT_SUPPORTED>>>", __func__);
  return E_NOT_SUPPORTED;
}

WcxDllExport
int WINAPI DeleteFilesW(LPCWSTR PackedFile, LPCWSTR * DeleteList)
{
  #pragma comment(linker, "/EXPORT:" __FUNCTION__ "=" __FUNCDNAME__)
  LOGw("%s: <<<NOT_SUPPORTED>>>", __func__);
  return E_NOT_SUPPORTED;
}

WcxDllExport
void WINAPI ConfigurePacker(HWND parentWnd, HINSTANCE dllInstance)
{
  #pragma comment(linker, "/EXPORT:" __FUNCTION__ "=" __FUNCDNAME__)
  g_wcx.show_cfg_dialog(parentWnd, dllInstance);
}

WcxDllExport
int WINAPI GetBackgroundFlags(void)
{
  #pragma comment(linker, "/EXPORT:" __FUNCTION__ "=" __FUNCDNAME__)
  LOGt("%s", __func__);
  return BACKGROUND_UNPACK | BACKGROUND_PACK;
}

WcxDllExport
int WINAPI GetPackerCaps()
{
  #pragma comment(linker, "/EXPORT:" __FUNCTION__ "=" __FUNCDNAME__)
  LOGt("%s", __func__);
  return PK_CAPS_BY_CONTENT | PK_CAPS_NEW | PK_CAPS_MULTIPLE | PK_CAPS_SEARCHTEXT;
}

WcxDllExport
int WINAPI SetTestMode(int reserved)
{
  #pragma comment(linker, "/EXPORT:" __FUNCTION__ "=" __FUNCDNAME__)
  LOGd("%s", __func__);
  g_wcx.m_testmode = true;  // for python test utilities
  return 0;
}

// ==================================================================================================

extern "C" BOOL WINAPI _CRT_INIT(HINSTANCE hInstDLL, DWORD fdwReason, LPVOID lpReserved);

BOOL WINAPI DllMain(HINSTANCE hInstDLL, DWORD fdwReason, LPVOID lpReserved)
{
  if (fdwReason == DLL_PROCESS_ATTACH) {
    DWORD tid = GetCurrentThreadId();
    g_hInstance = hInstDLL;
    _CRT_INIT(hInstDLL, fdwReason, lpReserved);
    LOGn("WCX Plugin Loaded ===================== Thread ID = 0x%X ========", tid);
    g_wcx.init(hInstDLL, tid);
  }  
  if (fdwReason == DLL_PROCESS_DETACH) {
    DWORD tid = g_wcx.get_main_thread_id();
    LOGn("WCX Plugin unload! -------------------------------- 0x%X --------", tid);
    _CRT_INIT(hInstDLL, fdwReason, lpReserved);
  }
  return TRUE;
}

