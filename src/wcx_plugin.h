#pragma once

#include "stdafx.h"
#include "lz4lib.h"
#include "lz4pack.h"
#include "wcx_cache.h"
#include "wcx_cfg.h"
#include "wcx_packer.h"
#include "wcx_archive.h"


namespace wcx {

class plugin
{
public:
  plugin();
  ~plugin();

  int  init(HMODULE lib_addr, DWORD thread_id);
  int  destroy();
  
  UINT get_main_thread_id() { return m_main_thread_id; }
  void set_ProcessDataProcW(tProcessDataProcW ptr) { m_ProcessDataProcW = ptr; }

  int  show_cfg_dialog(HWND parentWnd, HINSTANCE dllInstance);

  int  check_file(LPCWSTR filename);
  
  int  open_file(LPWSTR arcName, int openMode, wcx::archive ** pArc);
  int  close_file(wcx::archive * arc);

  int  pack_files(LPCWSTR PackedFile, LPCWSTR SubPath, LPCWSTR SrcPath, LPCWSTR AddList, int Flags);
  void destroy_all_packers();
  
  wcx::cfg get_cfg();

  bool m_testmode;
  
private:
  DWORD                m_main_thread_id;
  HMODULE              m_module;
  bool                 m_inited;
  tProcessDataProcW    m_ProcessDataProcW;
  wcx::inicfg          m_inicfg;
  wcx::cache_list      m_clist;
  wcx::archive_list    m_arclist;
  wcx::packer_list     m_plist;
  volatile LONG        m_packer_count;
};


extern plugin g_wcx;

INT_PTR WINAPI ConfigDialog(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam);


} /* namespace */
