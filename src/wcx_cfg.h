#pragma once

#include <windows.h>
#include "bst\string.hpp"
#include "wcx_version.h"


namespace wcx {

namespace DLG {
  static const char license[]   = "This software is published under MIT license (Expat)";
  static const char copyright[] = WCX_COPYRIGHT;
  static const char sources[]   = WCX_SOURCES;
  static const int cache_lifetimes[] = {2, 5, 12, 20, 60, 180, INT_MAX};
  static const int  debug_level[]         = { 3,       4,         5,        6,      7,       8};
  static const char debug_level_name[][9] = {"ERROR", "WARNING", "NOTICE", "INFO", "DEBUG", "TRACE"};
};

namespace ini {
  static const char  filename[] = WCX_INTERNAL_NAME ".ini";
  static const WCHAR settings[] = L"settings";
};

namespace lng {
  static const WCHAR ext[]      = L".lng";
  static const WCHAR subdir[]   = L"lang";
  static const WCHAR section[]  = L"lang";
};

class cfg
{
public:
  enum AttrTime : int {
    save_ctime      = 0x01,
    save_atime      = 0x02,
  };
  enum AttrFile : int {
    save_readonly   = 0x01,
    save_hidden     = 0x02,
    save_system     = 0x04,
    save_archive    = 0x08,
  };
  
  void set_defaults()
  {
    m_debug_level = LL_DEBUG;
    m_cmp_level   = 4;
    m_Attr_Time   = save_ctime | save_atime;
    m_Attr_File   = save_readonly | save_hidden | save_system | save_archive;
    m_cache_lifetime = 20;   // minutes
  }
  
  cfg();
  ~cfg();
  cfg(const cfg & config) { assign(config); }
  bool assign(const cfg & config);

  int        get_debug_level()        { return m_debug_level; }
  int        get_compression_level()  { return m_cmp_level; }
  int        get_cache_lifetime()     { return m_cache_lifetime; }
  AttrTime   get_attr_time() { return m_attr_time; }
  AttrFile   get_attr_file() { return m_attr_file; }
  
  bool set_debug_level(int dbg_level);
  bool set_compression_level(int cmp_level);
  bool set_uncompress_mode();
  bool set_cache_lifetime(int minutes);
  bool set_file_time(int flags);
  bool set_file_attr(int flags);

protected:  
  int        m_debug_level;
  int        m_cmp_level;
  union {
    AttrTime m_attr_time;
    int      m_Attr_Time;
  };  
  union {
    AttrFile m_attr_file;
    int      m_Attr_File;
  };
  int        m_cache_lifetime; 
  
};

class dialog;   /* forward declaration */

class inicfg
{
public:
  friend class dialog;
  friend class cfg;
  
  inicfg()
  {
    m_cache_size = 0;
  }
  
  ~inicfg()
  {
    //
  }
  
  bool copy(wcx::cfg & config);
  wcx::cfg get_cfg();

  int init(HMODULE mod_addr);
  int update() { return load_from_ini(false); }
  
  int show_cfg_dialog(HINSTANCE dllInstance, HWND parentWnd);
  void set_cache_size(UINT64 size) { m_cache_size = size; } 

  void set_compression_level(int cmp_level);
  
  int update_lang();
  void set_lang(LPCWSTR lang);

protected:
  int load_from_ini(bool forced);
  int check_lang_file_by_name(LPCWSTR lang);
  int save_to_ini();
  int save_to_ini(wcx::cfg & cfg);

  wcx::cfg       m_cfg;
  bst::srw_mutex m_mutex;    // SRW lock for m_cfg;

  DWORD          m_last_load_time;
  bst::filepath  m_wcx_path;
  bst::filepath  m_ini_file;
  bst::filepath  m_lng_path;
  bst::filename  m_lang;         // "en", "en-EN", "en-AU", "ru", "ru-RU"
  bst::filepath  m_lang_file;
  
  UINT64         m_cache_size;
};


class dialog
{
public:
  friend class cfg;
  
  dialog() BST_DELETED;
  dialog(wcx::inicfg & ini, HINSTANCE dllInstance, HWND parentWnd);
  ~dialog();

  LPCWSTR get_control_name(int idc, LPCWSTR default_name = NULL);

  void set_file_time_checkbox(cfg::AttrTime flags);
  void set_file_attr_checkbox(cfg::AttrFile flags);

  int show();

  int wm_destroy();
  int translate();
  int wm_init(HWND hwndDlg, WPARAM wParam);
  int wm_control_color_static(HWND wnd, HDC hdc);
  int wm_command(UINT16 ctrl, UINT16 val);
  int wm_command_ok();
  
  wcx::cfg & get_cfg() { return m_cfg; }
  int get_result() { return m_result; }
  
public:
  bool set_button_check(int idc, int checked);
  int combobox_add(int idc, LPCWSTR txt, int data);
  int combobox_add(int idc, int data);
  int get_combobox_seleted_data(int idc);
  bool is_checked(int idc);
  int get_compression_level();
  int show_control(int idc);
  int enable_controls();
  int update_combos();
  void set_combobox_height(int idc, int nItems);

  wcx::inicfg & m_ini;
  wcx::cfg   m_cfg;
  HMODULE    m_dll;
  HWND       m_parent_wnd;
  HWND       m_wnd;
  int        m_idc;          // dialog ID
  HFONT      m_hBoldFont;
  int        m_result;       // IDOK or IDCANCEL

};

} /* namespace */

