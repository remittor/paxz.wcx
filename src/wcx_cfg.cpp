#include "stdafx.h"
#include "wcx_cfg.h"
#include "res\resource.h"
#include <CommCtrl.h>


namespace wcx {

cfg::cfg()
{
  set_defaults();
}

cfg::~cfg()
{
  //
}

bool cfg::assign(const cfg & config)
{
  m_debug_level = config.m_debug_level;
  m_cmp_level = config.m_cmp_level;
  m_attr_time = config.m_attr_time;
  m_attr_file = config.m_attr_file;
  m_cache_lifetime = config.m_cache_lifetime;
  return true;
}

bool cfg::set_debug_level(int dbg_level)
{
  m_debug_level = dbg_level;
  if (dbg_level < LL_ERROR)
    m_debug_level = LL_ERROR;
  if (dbg_level > LL_TRACE)
    m_debug_level = LL_TRACE;
  return true;
}

bool cfg::set_compression_level(int cmp_level)
{
  if (cmp_level < 1) {
    m_cmp_level = 1;
  } else
  if (cmp_level > 9) { 
    m_cmp_level = 9;
  } else {
    m_cmp_level = cmp_level;
  }
  return true;
}

bool cfg::set_uncompress_mode()
{
  m_cmp_level = -1;
  return true;
}

bool cfg::set_cache_lifetime(int minutes)
{
  if (minutes < 1) {
    m_cache_lifetime = 1;
  } else
  if (minutes > 2000) { 
    m_cache_lifetime = 2000;
  } else {
    m_cache_lifetime = minutes;
  }
  return true;
}

bool cfg::set_file_time(int flags)
{
  m_Attr_Time = 0;
  if (flags & save_ctime)       m_Attr_Time |= save_ctime;
  if (flags & save_atime)       m_Attr_Time |= save_atime;
  return true;
}

bool cfg::set_file_attr(int flags)
{
  m_Attr_File = 0;
  if (flags & save_readonly)    m_Attr_File |= save_readonly;
  if (flags & save_hidden)      m_Attr_File |= save_hidden;
  if (flags & save_system)      m_Attr_File |= save_system;
  if (flags & save_archive)     m_Attr_File |= save_archive;
  return true;
}

// =====================================================================================

bool inicfg::copy(wcx::cfg & config)
{
  bst::scoped_read_lock lock(m_mutex);
  config = m_cfg;
  return true;
}

wcx::cfg inicfg::get_cfg()
{
  wcx::cfg res;
  bst::scoped_read_lock lock(m_mutex);
  res.assign(m_cfg);
  return res;
}

int inicfg::init(HMODULE mod_addr)
{
  int hr = 0;
  size_t pos, sz;

  m_wcx_path.clear();
  DWORD nlen = GetModuleFileNameW(mod_addr, m_wcx_path.data(), MAX_PATH);
  FIN_IF(nlen < 4 || nlen >= MAX_PATH, -2);
  FIN_IF(GetLastError() != ERROR_SUCCESS, -3);  
  m_wcx_path.fix_length();
  //WLOGd(L"wcx path = '%s'", m_wcx_path.c_str());
  pos = m_wcx_path.rfind(L'\\');
  m_wcx_path.resize(pos + 1);
  
  m_ini_file.assign(m_wcx_path);
  m_ini_file.append_fmt(L"%S", ini::filename);
  WLOGd(L"INI = \"%s\" ", m_ini_file.c_str());
  
  update_lang();
  
  hr = load_from_ini(true);

fin:
  LOGe_IF(hr, "%s: ERROR = %d ", __func__, hr);
  return hr;  
}

int inicfg::load_from_ini(bool forced)
{
  int hr = 0;
  int val;
  wcx::cfg config;
  
  if (!forced) {
    DWORD now = GetTickCount();
    if (GetTickBetween(m_last_load_time, now) < 60000)
      return 0;
  }
  m_last_load_time = GetTickCount();  

  FIN_IF(m_ini_file.empty(), -11);
  DWORD dw = GetFileAttributesW(m_ini_file.c_str());
  FIN_IF(dw == INVALID_FILE_ATTRIBUTES, -12);

#ifdef WCX_DEBUG
  val = (int)GetPrivateProfileIntW(ini::settings, L"DebugLevel", -1, m_ini_file.c_str());
  if (val >= 0) {
    config.set_debug_level(val);
    LOGi("%s: debug level = %d ", __func__, val);
    WcxSetLogLevel(val);
  }
#endif
  val = (int)GetPrivateProfileIntW(ini::settings, L"CompressionLevel", -1, m_ini_file.c_str());
  if (val >= 0) {
    config.set_compression_level(val);
    LOGd("%s: compression level = %d ", __func__, val);
  }
  val = (int)GetPrivateProfileIntW(ini::settings, L"CacheLifetime", -1, m_ini_file.c_str());
  if (val >= 0) {
    config.set_cache_lifetime(val);
    LOGd("%s: cache lifetime = %d ", __func__, val);
  }
  val = (int)GetPrivateProfileIntW(ini::settings, L"FileTimeFlags", -1, m_ini_file.c_str());
  if (val >= 0) {
    config.set_file_time(val);
    LOGd("%s: file time = 0x%04X ", __func__, val);
  }
  val = (int)GetPrivateProfileIntW(ini::settings, L"FileAttrFlags", -1, m_ini_file.c_str());
  if (val >= 0) {
    config.set_file_attr(val);
    LOGd("%s: file attr = 0x%04X ", __func__, val);
  }
  {
    bst::scoped_write_lock lock(m_mutex);
    m_cfg.assign(config);
  }
  hr = 0;

fin:
  return hr;
}

int inicfg::save_to_ini(wcx::cfg & cfg)
{
  int hr = 0;
  bst::filename str;

  FIN_IF(m_ini_file.empty(), -21);
  DWORD dw = GetFileAttributesW(m_ini_file.c_str());
  FIN_IF(dw == INVALID_FILE_ATTRIBUTES, -22);

#ifdef WCX_DEBUG
  str.assign_fmt(L"%d", cfg.get_debug_level());
  WritePrivateProfileStringW(ini::settings, L"DebugLevel", str.c_str(), m_ini_file.c_str());
#endif
  str.assign_fmt(L"%d", cfg.get_compression_level());
  WritePrivateProfileStringW(ini::settings, L"CompressionLevel", str.c_str(), m_ini_file.c_str());
  
  str.assign_fmt(L"%d", cfg.get_cache_lifetime());
  WritePrivateProfileStringW(ini::settings, L"CacheLifetime", str.c_str(), m_ini_file.c_str());

  str.assign_fmt(L"%d", (int)cfg.get_attr_time());
  WritePrivateProfileStringW(ini::settings, L"FileTimeFlags", str.c_str(), m_ini_file.c_str());

  str.assign_fmt(L"%d", (int)cfg.get_attr_file());
  WritePrivateProfileStringW(ini::settings, L"FileAttrFlags", str.c_str(), m_ini_file.c_str());

  LOGd("%s: INI saved! ", __func__);
  hr = 0;

fin:  
  return hr;
}

int inicfg::save_to_ini()
{
  wcx::cfg cfg;
  {
    bst::scoped_read_lock lock(m_mutex);
    cfg = m_cfg;
  }
  return save_to_ini(cfg);
}

int inicfg::check_lang_file_by_name(LPCWSTR lang)
{
  int hr = -1;
  DWORD dw;
  
  m_lang.assign(lang);
  FIN_IF(m_lang.length() < 2, -1);
  m_lang_file.clear();
  m_lang_file.append(m_lng_path).append(m_lang.c_str()).append(lng::ext);
  FIN_IF(m_lang_file.has_error(), -2);
  dw = GetFileAttributesW(m_lang_file.c_str());
  FIN_IF(dw != INVALID_FILE_ATTRIBUTES, 0);
    
  size_t pos = m_lang.find(L'-');
  FIN_IF(pos == bst::npos, -3);
  m_lang.resize(pos);
  FIN_IF(m_lang.length() < 2, -4);
  m_lang_file.clear();
  m_lang_file.append(m_lng_path).append(m_lang.c_str()).append(lng::ext);
  FIN_IF(m_lang_file.has_error(), -5);
  dw = GetFileAttributesW(m_lang_file.c_str());
  FIN_IF(dw != INVALID_FILE_ATTRIBUTES, 0);
  hr = -1;
  
fin:
  if (hr)
    m_lang.clear();
  return hr;
}

int inicfg::update_lang()
{
  int hr = -1;
  DWORD plen;
  WCHAR lang[64];  
  
  lang[0] = 0;
  m_lang.clear();

  if (m_lng_path.empty()) {
    m_lng_path.append(m_wcx_path).append(lng::subdir).append(L"\\");
    FIN_IF(m_lng_path.has_error(), -4);
  }
  plen = GetPrivateProfileStringW(ini::settings, L"Lang", NULL, lang, 62, m_ini_file.c_str());
  if (plen >= 2 && plen < 60) {
    WLOGd(L"%S: read Lang = '%s' ", __func__, lang);
    hr = check_lang_file_by_name(lang);
    WLOGd_IF(hr == 0, L"%S: set Lang = '%s' ", __func__, m_lang.c_str());
  }
  if (m_lang.empty()) {
    int len;
    LCID lcid = GetUserDefaultLCID();
    LANGID langid = LOWORD(lcid);
    len = GetLocaleInfoW(MAKELCID(langid, SORT_DEFAULT), LOCALE_SISO639LANGNAME, lang, 20);
    FIN_IF(len < 2, -5);
    wcscat(lang, L"-");
    LPWSTR country = lang + wcslen(lang);
    len = GetLocaleInfoW(MAKELCID(langid, SORT_DEFAULT), LOCALE_SISO3166CTRYNAME, country, 20);
    FIN_IF(len < 2, -6);
    hr = check_lang_file_by_name(lang);
    WLOGd_IF(hr == 0, L"%S: SET Lang = '%s' ", __func__, m_lang.c_str());
  }
  
fin:
  if (hr) {
    if (m_lng_path.length()) {
      m_lang.assign(L"en");
      m_lang_file.clear();
      m_lang_file.append(m_lng_path).append(m_lang).append(lng::ext);
    }
  }
  WLOGd(L"%S: LangFile = \"%s\" ", __func__, m_lang_file.c_str());
  return hr;
}

void inicfg::set_compression_level(int cmp_level)
{
  {
    bst::scoped_write_lock lock(m_mutex);
    m_cfg.set_compression_level(cmp_level);
  }
  bst::prealloc_string<WCHAR, 16> str(bst::fmtstr(L"%d"), m_cfg.get_compression_level());
  WritePrivateProfileStringW(ini::settings, L"CompressionLevel", str.c_str(), m_ini_file.c_str());
}

void inicfg::set_lang(LPCWSTR lang)
{
  WritePrivateProfileStringW(ini::settings, L"Lang", lang, m_ini_file.c_str());
  m_lang.assign(lang);
}

int inicfg::show_cfg_dialog(HINSTANCE dllInstance, HWND parentWnd)
{
  load_from_ini(true);
  update_lang();
  wcx::dialog dlg(*this, dllInstance, parentWnd);
  int ret = dlg.show();
  if (ret == IDOK) {
    bst::scoped_write_lock lock(m_mutex);
    m_cfg.assign(dlg.get_cfg());
    save_to_ini(m_cfg);
  }
  return 0;
}

// ===========================================================================================

dialog::dialog(wcx::inicfg & ini, HINSTANCE dllInstance, HWND parentWnd) : m_ini(ini)
{
  m_dll = dllInstance;
  m_parent_wnd = parentWnd;
  m_wnd = NULL;
  m_idc = IDD_CONFIG;
  m_hBoldFont = NULL;
  m_result = 0;
}

dialog::~dialog()
{
  if (m_hBoldFont)
    DeleteObject(m_hBoldFont);
}

bool dialog::set_button_check(int idc, int checked)
{
  BOOL x = CheckDlgButton(m_wnd, idc, checked ? BST_CHECKED : BST_UNCHECKED);
  return x ? true : false;
}

void dialog::set_file_time_checkbox(cfg::AttrTime flags)
{
  set_button_check(IDC_LBL_SAVE_CREATE_TIME, flags & cfg::save_ctime);
  set_button_check(IDC_LBL_SAVE_CREATE_TIME, flags & cfg::save_ctime);
  set_button_check(IDC_LBL_SAVE_ACCESS_TIME, flags & cfg::save_atime);
}

void dialog::set_file_attr_checkbox(cfg::AttrFile flags)
{
  set_button_check(IDC_LBL_SAVE_READONLY, flags & cfg::save_readonly);
  set_button_check(IDC_LBL_SAVE_HIDDEN,   flags & cfg::save_hidden);
  set_button_check(IDC_LBL_SAVE_SYSTEM,   flags & cfg::save_system);
  set_button_check(IDC_LBL_SAVE_ARCHIVE,  flags & cfg::save_archive);
}

static INT_PTR CALLBACK WcxConfigDialog(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
  dialog * dlg;
  if (msg == WM_INITDIALOG) {
    dlg = (dialog *)lParam;
  } else {
    dlg = (dialog *)GetWindowLongPtrW(hwndDlg, GWLP_USERDATA);
  }
  switch (msg) {
    case WM_INITDIALOG:
      return dlg->wm_init(hwndDlg, wParam);

    case WM_CTLCOLORSTATIC:
      return dlg->wm_control_color_static((HWND)lParam, (HDC)wParam);

    case WM_COMMAND:
      return dlg->wm_command(LOWORD(wParam), HIWORD(wParam));

    case WM_NOTIFY:
      break;

    case WM_DESTROY:
      return dlg->wm_destroy();
  }
  return FALSE;
}

int dialog::show()
{
  m_ini.copy(m_cfg);
  DialogBoxParamW(m_dll, MAKEINTRESOURCEW(m_idc), m_parent_wnd, WcxConfigDialog, (LPARAM)this);
  return m_result;
}

int dialog::wm_destroy()
{
  if (m_hBoldFont)
    DeleteObject(m_hBoldFont);
  m_hBoldFont = NULL;
  return 0;
}

LPCWSTR dialog::get_control_name(int idc, LPCWSTR default_name)
{
  static WCHAR locname[255];
  bst::prealloc_string<WCHAR, 32> key(bst::fmtstr(L"%d"), idc);
  DWORD plen = GetPrivateProfileStringW(lng::section, key.c_str(), NULL, locname, 250, m_ini.m_lang_file.c_str());
  return (plen > 0 && plen < 250) ? locname : default_name;
}

static BOOL CALLBACK TranslateDialogEnumProc(HWND hwnd, LPARAM lParam)
{
  dialog * dlg = (dialog *)lParam;
  LPCWSTR name = dlg->get_control_name(dlg->m_idc + GetDlgCtrlID(hwnd));
  if (name)
    SetWindowTextW(hwnd, name);
  return TRUE;
}

int dialog::translate()
{
  LPCWSTR name = get_control_name(m_idc);
  if (name)
    SetWindowTextW(m_wnd, name);
  EnumChildWindows(m_wnd, TranslateDialogEnumProc, (LPARAM)this);
  return TRUE;
}

int dialog::combobox_add(int idc, LPCWSTR txt, int data)
{
  int index = (int)SendDlgItemMessage(m_wnd, idc, CB_ADDSTRING, 0, (LPARAM)txt);
  if (index >= 0)
    SendDlgItemMessage(m_wnd, idc, CB_SETITEMDATA, (WPARAM)index, (LPARAM)data);
  return index;
}

int dialog::combobox_add(int idc, int data)
{
  bst::prealloc_string<WCHAR, 64> wstr(bst::fmtstr(L" %d"), data);
  return combobox_add(idc, wstr.c_str(), data);
}

int dialog::get_combobox_seleted_data(int idc)
{
  int index = (int)SendDlgItemMessageW(m_wnd, idc, CB_GETCURSEL, (WPARAM)0, (LPARAM)0);
  if (index < 0)
    return -1;
  return (int)SendDlgItemMessageW(m_wnd, idc, CB_GETITEMDATA, (WPARAM)index, (LPARAM)0);
}

int dialog::get_compression_level()
{
  return get_combobox_seleted_data(IDC_COMP_LEVEL);
}

int dialog::show_control(int idc)
{
  HWND wnd = GetDlgItem(m_wnd, idc);
  if (!wnd)
    return -1;
  ShowWindow(wnd, SW_SHOW);
  return 0;
}

int dialog::enable_controls()
{
  //int index = SendDlgItemMessage(m_wnd, IDC_COMP_LEVEL, CB_GETCURSEL, (WPARAM)0, (LPARAM)0);
  //EnableWindow(GetDlgItem(m_wnd, IDC_COMP_METHOD), bEnable);
  //EnableWindow(GetDlgItem(m_wnd, IDC_DICT_SIZE), bEnable);
  return 0;
}

int dialog::update_combos()
{
  //int comp_level = get_compression_level();
  return 0;
}

void dialog::set_combobox_height(int idc, int nItems)
{
  HWND wnd = GetDlgItem(m_wnd, idc);
  RECT rect;      
  int h = (int)SendMessageW(wnd, CB_GETITEMHEIGHT, 0, 0);
  GetWindowRect(wnd, &rect);
  SetWindowPos(wnd, 0, 0, 0, rect.right - rect.left, h * (nItems+2), SWP_NOMOVE | SWP_NOZORDER);
}

int dialog::wm_init(HWND hwndDlg, WPARAM wParam)
{
  bst::filename_a str;
  bst::filename   wstr;
  
  m_wnd = hwndDlg;
  SetWindowLongPtrW(m_wnd, GWLP_USERDATA, (LONG_PTR)this);
  translate();
  
  /* init copyright block */
  str.clear();
  str.append("      ").append(DLG::copyright).append("            ").append(DLG::sources).append("\r\n");
  str.append(DLG::license);
  SetWindowTextA(GetDlgItem(m_wnd, IDC_AUTHORS), str.c_str());

  /* init title */
  str.assign(WCX_DESCRIPTION "  " WCX_VERSION "   ");
#ifdef WCX_DEBUG
  str.append("(DEBUG)");
#endif
  SetWindowTextA(GetDlgItem(m_wnd, IDC_TITLE), str.c_str());
  LOGFONTW lf;
  HFONT hNormalFont = (HFONT)SendDlgItemMessageW(m_wnd, IDC_TITLE, WM_GETFONT, 0, 0);
  GetObjectW(hNormalFont, sizeof(lf), &lf);
  lf.lfWeight = FW_BOLD;
  m_hBoldFont = CreateFontIndirectW(&lf);
  if (m_hBoldFont)
    SendDlgItemMessageW(m_wnd, IDC_TITLE, WM_SETFONT, (WPARAM)m_hBoldFont, 0);

  /* init compression combobox */
  for (int i = 1; i <= 9; i++) {
    int index = combobox_add(IDC_COMP_LEVEL, i);
    if (i == m_cfg.get_compression_level())
      SendDlgItemMessage(m_wnd, IDC_COMP_LEVEL, CB_SETCURSEL, (WPARAM)index, 0);
  }

  /* init cache lifetime combobox */
  int clt = m_cfg.get_cache_lifetime();
  for (int i = 0; i < _countof(DLG::cache_lifetimes); i++) {
    int val = DLG::cache_lifetimes[i];
    if (val > clt) {
      int index = combobox_add(IDC_CACHE_TIME, clt);
      SendDlgItemMessage(m_wnd, IDC_CACHE_TIME, CB_SETCURSEL, (WPARAM)index, 0);
      clt = INT_MAX;
    }
    if (val >= INT_MAX)
      break;
    int index = combobox_add(IDC_CACHE_TIME, val);
    if (val == clt) {
      SendDlgItemMessage(m_wnd, IDC_CACHE_TIME, CB_SETCURSEL, (WPARAM)index, 0);
      clt = INT_MAX;
    }
  }

  /* init WCX internal cache size */
  {
    HWND wnd = GetDlgItem(m_wnd, IDC_LBL_CACHE_SIZE);
    GetWindowTextW(wnd, wstr.data(), (int)wstr.capacity() - 8);
    wstr.fix_length();
    wstr.append_fmt(L" %Id MB", m_ini.m_cache_size / (1024*1024) + 1);
    SetWindowTextW(wnd, wstr.c_str());
  }

#ifdef WCX_DEBUG  
  /* init debug level */
  int cdl = m_cfg.get_debug_level();
  for (int i = 0; i < _countof(DLG::debug_level); i++) {
    int val = DLG::debug_level[i];
    wstr.assign_fmt(L" %d - %S", val, DLG::debug_level_name[i]);
    int index = combobox_add(IDC_DEBUG_LEVEL, wstr.c_str(), val);
    if (val == cdl) {
      SendDlgItemMessage(m_wnd, IDC_DEBUG_LEVEL, CB_SETCURSEL, (WPARAM)index, 0);
    }
  }
  show_control(IDC_DEBUG_LEVEL);
  show_control(IDC_LBL_DEBUG_LEVEL);
#endif

  /* init file attr flags */
  set_file_time_checkbox(m_cfg.get_attr_time());
  set_file_attr_checkbox(m_cfg.get_attr_file());

  enable_controls(); 
  return TRUE;
}

int dialog::wm_control_color_static(HWND wnd, HDC hdc)
{
  if ( GetDlgItem(m_wnd, IDC_TITLE) == wnd
    || GetDlgItem(m_wnd, IDC_SUBTITLE) == wnd
    || GetDlgItem(m_wnd, IDI_ICON) == wnd)
  {
    SetBkMode(hdc, TRANSPARENT);
    return GetStockObject(NULL_BRUSH) ? TRUE : FALSE;
  }
  return FALSE;
}

int dialog::wm_command(UINT16 ctrl, UINT16 val)
{
  int hr;
  wcx::cfg defcfg;
  
  switch (ctrl) {
    case IDC_COMP_METHOD:
    case IDC_COMP_LEVEL:
      if (val == CBN_SELCHANGE) {
        update_combos();
        enable_controls();
      }
      break;

    case IDC_DICT_SIZE:
    case IDC_WORD_SIZE: 
      if (val == CBN_SELCHANGE) {
        //
      }
      break;

    case IDC_SHOW_PASSWORD:
      //update_password();
      break;

    case IDOK:
      hr = wm_command_ok();
      m_result = (hr) ? IDCANCEL : IDOK;
      EndDialog(m_wnd, IDOK);
      break;

    case IDCANCEL:
      m_result = IDCANCEL;
      EndDialog(m_wnd, IDCANCEL);
      break;

    case IDC_RESET_FILE_TIMES:
      set_file_time_checkbox(defcfg.get_attr_time());
      break;

    case IDC_RESET_FILE_ATTRIBUTES:
      set_file_attr_checkbox(defcfg.get_attr_file());
      break;
  }
  return FALSE;
}

bool dialog::is_checked(int idc)
{
  return (IsDlgButtonChecked(m_wnd, idc) == BST_CHECKED) ? true : false;
}

int dialog::wm_command_ok()
{
  int hr = 0;

#ifdef WCX_DEBUG
  m_cfg.set_debug_level(get_combobox_seleted_data(IDC_DEBUG_LEVEL));
  WcxSetLogLevel(m_cfg.get_debug_level());
#endif

  m_cfg.set_compression_level(get_compression_level());
  
  m_cfg.set_cache_lifetime(get_combobox_seleted_data(IDC_CACHE_TIME));
  {
    int flags = 0;
    flags |= is_checked(IDC_LBL_SAVE_CREATE_TIME) ? cfg::save_ctime : 0;
    flags |= is_checked(IDC_LBL_SAVE_ACCESS_TIME) ? cfg::save_atime : 0;
    m_cfg.set_file_time(flags);
  }
  {
    int flags = 0;
    flags |= is_checked(IDC_LBL_SAVE_READONLY)    ? cfg::save_readonly : 0;
    flags |= is_checked(IDC_LBL_SAVE_HIDDEN)      ? cfg::save_hidden   : 0;
    flags |= is_checked(IDC_LBL_SAVE_SYSTEM)      ? cfg::save_system   : 0;
    flags |= is_checked(IDC_LBL_SAVE_ARCHIVE)     ? cfg::save_archive  : 0;
    m_cfg.set_file_attr(flags);
  }
  return hr;
}

} /* namespace */

