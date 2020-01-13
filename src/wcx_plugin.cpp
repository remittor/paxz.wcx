#include "stdafx.h"
#include "wcx_plugin.h"
#include "winuser.h"
#include "res\resource.h"


namespace wcx {

plugin::plugin()
{
  m_module = NULL;
  m_main_thread_id = 0;
  m_ProcessDataProcW = NULL;
  m_packer_count = 0;
  m_testmode = false;
  m_inited = false;
}

plugin::~plugin()
{
  destroy();
}

int plugin::init(HMODULE lib_addr, DWORD thread_id)
{
  m_module = lib_addr;
  m_main_thread_id = thread_id;
  m_inited = true;
  m_inicfg.init(lib_addr);
  m_clist.init(m_main_thread_id, m_inicfg);
  return 0;
}

int plugin::destroy()
{
  m_arclist.clear();
  m_plist.clear();
  m_clist.clear();
  return 0;
}

// =========================================================================================

int plugin::show_cfg_dialog(HWND parentWnd, HINSTANCE dllInstance)
{
  m_inicfg.set_cache_size(m_clist.get_cache_size());
  m_inicfg.show_cfg_dialog(dllInstance, parentWnd);
  return 0;
}

wcx::cfg plugin::get_cfg()
{
  wcx::cfg cfg;
  m_inicfg.copy(cfg);
  return cfg;
}

// =========================================================================================

int plugin::check_file(LPCWSTR filename)
{
  wcx::arcfile arcfile;
  int hr = arcfile.init(filename, imType);
  if (hr == 0) {
    return (arcfile.get_type() == atUnknown) ? E_BAD_ARCHIVE : 0;
  }
  return hr;
}

int plugin::open_file(LPWSTR arcName, int openMode, wcx::archive ** pArc)
{
  int hr = E_BAD_ARCHIVE;
  wcx::arcfile arcfile;
  wcx::cache * cache = NULL;
  wcx::archive * arc = NULL;
  wcx::cfg cfg;

  m_clist.delete_zombies();
  DWORD tid = GetCurrentThreadId();
  WLOGi(L"%S(%d): [0x%X] \"%s\" ", __func__, openMode, tid, arcName);

  arc = new wcx::archive();
  FIN_IF(!arc, 0x30001000 | E_NO_MEMORY);
  
  hr = arcfile.init(arcName, imName | imAttr | imType);
  FIN_IF(hr, hr);

  hr = m_clist.get_cache(arcfile, true, &cache);
  FIN_IF(hr, hr);

  m_inicfg.copy(cfg);
  hr = arc->init(cfg, arcName, openMode, tid, cache);
  FIN_IF(hr, 0x30012000 | E_BAD_ARCHIVE);
  
  bool xb = m_arclist.add(arc);
  FIN_IF(!xb, 0x30014000 | E_NO_MEMORY);

  cache->read_lock();
  
  *pArc = arc;
  arc = NULL;
  hr = 0;

fin:
  LOGe_IF(hr, "%s: ERROR = 0x%X ", __func__, hr);
  if (arc)
    delete arc;
  return hr & 0xFF;
}

int plugin::close_file(wcx::archive * arc)
{
  wcx::cache * cache = arc->get_cache(); 
  if (cache) {
    cache->read_unlock();
  }
  if (!m_arclist.del(arc)) {
    delete arc;
  }
  return 0;
}

// =========================================================================================

int plugin::pack_files(LPCWSTR PackedFile, LPCWSTR SubPath, LPCWSTR SrcPath, LPCWSTR AddList, int Flags)
{
  int hr = E_BAD_ARCHIVE;
  size_t AddListSize = 0;
  wcx::arcfile arcfile;
  wcx::packer * packer = NULL;
  wcx::cfg cfg;
  
  InterlockedIncrement(&m_packer_count);

  DWORD tid = GetCurrentThreadId();
  if (m_testmode) {
    for (LPWSTR s = (LPWSTR)AddList; *s; s++) {
      if (*s == L'\1')
        *s = 0;
    }
  }
  for (LPCWSTR fn = AddList; fn[0]; fn += wcslen(fn) + 1) {
    AddListSize++;
    FIN_IF(AddListSize >= UINT_MAX, 0x300100 | E_TOO_MANY_FILES);
  }
  WLOGd(L"%S(%d): [0x%X] PackedFile = \"%s\" SrcPath = \"%s\" <%Id> ", __func__, Flags, tid, PackedFile, SrcPath, AddListSize);
  //WLOGd(L"  SubPath= \"%s\" AddList = \"%s\" SrcPath = \"%s\" ", SubPath, AddList, SrcPath);
  FIN_IF(AddList[0] == 0, 0x300200 | E_NO_FILES);
  FIN_IF(!m_ProcessDataProcW, 0x300400 |E_NO_FILES);
  
  if (tid == m_main_thread_id) {
    UINT uType = MB_OK | MB_ICONERROR | MB_SYSTEMMODAL;
    int btn = MessageBoxW(NULL, L"Please, update Total Commander!!!", L"CRITICAL ERROR", uType);
    FIN(0x302200 | E_NO_FILES);
  }

  hr = arcfile.init(PackedFile, imName);
  FIN_IF(hr, 0x302300 | E_EOPEN);
  
  DWORD attr = GetFileAttributesW(arcfile.get_fullname());
  if (attr != INVALID_FILE_ATTRIBUTES) {
    if (attr & FILE_ATTRIBUTE_DIRECTORY) {
      WLOGe(L"%S: ERROR: can't replace directory!!! ", __func__);
      FIN(0x302400 | E_ECREATE);
    }
    WLOGw(L"%S: WARN: archive editing not supported!!! ", __func__);
    FIN(0x302500 | E_ECREATE);

    arcfile.clear();
    hr = arcfile.init(PackedFile, imName | imAttr | imType);
    FIN_IF(hr, 0x302600 | E_EOPEN);

    // TODO: parse archive
  }
    
  m_inicfg.copy(cfg);

  wcx::ArcType ctype = wcx::atPaxZstd;   // create PAXZ on default
  
  LPCWSTR p = wcsrchr(PackedFile, L'.');
  if (p) {
    if (_wcsicmp(p, L".pax") == 0) {
      ctype = wcx::atUnknown;
      cfg.set_uncompress_mode();   // without packing
    }
    if (_wcsicmp(p, L".lz4") == 0) {
      ctype = wcx::atPaxLz4;
    }
  }
  packer = new wcx::packer(ctype, cfg, Flags, tid, m_ProcessDataProcW);
  FIN_IF(!packer, 0x303200 | E_NO_MEMORY);
  packer->set_AddListSize(AddListSize);
  packer->set_arcfile(arcfile);
  bool xb = m_plist.add(packer);
  FIN_IF(!xb, 0x303300 | E_NO_MEMORY);
  
  hr = packer->pack_files(SubPath, SrcPath, AddList);
  
fin:
  if (packer) {
    if (!m_plist.del(packer)) {
      delete packer;
    }
  }
  LOGe_IF(hr, "%s: ERROR = 0x%X ", __func__, hr);
  InterlockedDecrement(&m_packer_count);
  return hr & 0xFF;
}

void plugin::destroy_all_packers()
{
  //if (!m_plist.is_empty()) {
  //   TerminateThread for all packers!  (exclude m_main_thread_id)
  //}
}

} /* namespace */
