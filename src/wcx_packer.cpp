#include "stdafx.h"
#include "wcx_packer.h"


namespace wcx {

packer::packer(wcx::cfg & cfg, int Flags, DWORD thread_id, tProcessDataProcW callback)
{
  m_cfg = cfg;
  m_thread_id = thread_id;
  m_AddListSize = 0;
  m_outFile = NULL;
  reset();
  m_Flags = Flags;  
  m_cb.set_ProcessDataProc(callback);
}

packer::~packer()
{
  WLOGi(L"%S: <<<DESTROY>>> tid = 0x%X ", __func__, m_thread_id);
  reset();
}

void packer::reset()
{
  m_cb.reset();
  if (m_outFile) {
    if (m_delete_out_file)
      nt::DeleteFileByHandle(m_outFile);
    CloseHandle(m_outFile);
    m_outFile = NULL;
  }
  m_delete_out_file = false;
}

int packer::pack_files(LPCWSTR SubPath, LPCWSTR SrcPath, LPCWSTR AddList)
{
  int hr = E_BAD_ARCHIVE;
  HANDLE hFile = NULL;
  BOOL x;
  int ret;
  size_t sz;
  size_t fn_num = 0;
  tar::pax_encode pax;
  DWORD dw;

  FIN_IF(!m_AddListSize, 0x201000 | E_NO_FILES);
  
  DWORD dwAccess = GENERIC_READ | GENERIC_WRITE | DELETE;
  m_outFile = CreateFileW(m_arcfile.get_fullname(), dwAccess, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  m_outFile = (m_outFile == INVALID_HANDLE_VALUE) ? NULL : m_outFile;
  FIN_IF(!m_outFile, 0x206000 | E_ECREATE);
  m_delete_out_file = true;

  if (m_buf.empty()) {
    size_t bufsize = 4*1024*1024;
    sz = m_buf.reserve(bufsize + tar::BLOCKSIZE * 8);
    FIN_IF(sz == bst::npos, 0x207000 | E_NO_MEMORY);
    sz = m_buf.resize(bufsize);
    FIN_IF(m_buf.size() & tar::BLOCKSIZE_MASK, 0x208000 | E_NO_MEMORY);
  }
  m_lz4.reset();
  m_lz4.set_block_size((int)m_buf.size());
  m_lz4.set_compression_level(m_cfg.get_compression_level());
  m_lz4.frame_set_file(m_outFile);
  
  hr = m_lz4.frame_create(0);    /* create LZ4 zero frame */
  FIN_IF(hr, 0x209100 | E_ECREATE);
  hr = m_lz4.frame_close();
  FIN_IF(hr, 0x209200 | E_ECREATE);
  
  paxz::frame_root fr;
  int frlen = fr.init_pax(0, 0, false);
  x = WriteFile(m_outFile, &fr, frlen, &dw, NULL);
  FIN_IF(!x || dw != frlen, 0x209300 | E_ECREATE);

  if (m_ext_buf.empty()) {
    size_t bufsize = 2*1024*1024;
    sz = m_ext_buf.reserve(bufsize + 64);
    FIN_IF(sz == bst::npos, 0x209900 | E_NO_MEMORY);
    m_ext_buf.resize(bufsize);
  }

  pax.m_attr_time = (tar::pax_encode::AttrTime)m_cfg.get_attr_time();
  pax.m_attr_file = (tar::pax_encode::AttrFile)m_cfg.get_attr_file();

  m_srcFullName.clear();
  size_t srcPathLen = wcslen(SrcPath);

  fn_num = 0;
  for (LPCWSTR fn = AddList; fn[0]; fn += wcslen(fn) + 1) {
    if (hFile) {
      CloseHandle(hFile);
      hFile = NULL;
    }
    fn_num++;
    bool latest_file = (fn_num == m_AddListSize);
    bool is_dir = false;
    //WLOGd(L"    \"%s\" ", fn);
    hr = get_full_filename(SrcPath, fn, m_srcFullName);
    FIN_IF(hr, 0x221000 | E_EOPEN);

    DWORD dwSharedMode = FILE_SHARE_READ;
    hFile = CreateFileW(m_srcFullName.c_str(), GENERIC_READ, dwSharedMode, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    hFile = (hFile == INVALID_HANDLE_VALUE) ? NULL : hFile;
    if (!hFile) {
      DWORD attr = GetFileAttributesW(m_srcFullName.c_str());
      if (attr == INVALID_FILE_ATTRIBUTES) {
        WLOGw(L"%S: <<<SKIP FILE>>> \"%s\" REASON: can't found ", __func__, m_srcFullName.c_str());
        continue;
      }
      if (attr & FILE_ATTRIBUTE_DIRECTORY) {
        hFile = CreateFileW(m_srcFullName.c_str(), GENERIC_READ, dwSharedMode, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
        hFile = (hFile == INVALID_HANDLE_VALUE) ? NULL : hFile;
        if (!hFile) {
          WLOGw_IF(is_dir, L"%S: <<<SKIP DIR>>> \"%s\" REASON: can't open", __func__, m_srcFullName.c_str());
          continue;
        }
        is_dir = true;
      } else {
        WLOGw(L"%S: <<<SKIP FILE>>> \"%s\" REASON: can't read ", __func__, m_srcFullName.c_str());
        UINT64 fsize = 0;
        WIN32_FILE_ATTRIBUTE_DATA fattr = {0};
        x = GetFileAttributesExW(m_srcFullName.c_str(), GetFileExInfoStandard, &fattr);
        fsize = x ? (UINT64)fattr.nFileSizeHigh << 32 | fattr.nFileSizeLow : 0;
        m_cb.set_file_name(fn);
        ret = m_cb.tell_skip_file(fsize, latest_file ? true : false);
        FIN_IF(ret == psCancel, 0);   // user press Cancel
        continue;   // skip unreaded files (TotalCmd collect info for all files)
      }
    }
    
    if (is_dir) {
      WLOGi(L"%S: #### pack DIR \"%s\" ", __func__, m_srcFullName.c_str());
    } else {
      WLOGi(L"%S: #### pack file \"%s\" ", __func__, m_srcFullName.c_str());
    }

    tar::fileinfo fi;
    fi.realsize = 0;
    size_t hsz = m_ext_buf.size();
    hr = pax.create_header(hFile, &fi, NULL, fn, m_ext_buf.data(), hsz);
    FIN_IF(hr, hr | E_ECREATE);
    FIN_IF(hsz > (size_t)m_lz4.get_block_size(), 0x231000 | E_ECREATE);

    UINT64 fsize = fi.size;
    UINT64 content_size = tar::blocksize_round64(hsz + fsize);
    
    hr = m_lz4.frame_create(content_size);
    FIN_IF(hr, 0x232000 | E_ECREATE);    
    hr = m_lz4.frame_add_data(m_ext_buf.data(), hsz);
    FIN_IF(hr, 0x233000 | E_ECREATE);

    if (is_dir || fsize == 0) {
      hr = m_lz4.frame_close();
      FIN_IF(hr, 0x234000 | E_ECREATE);
      continue;
    }
    
    m_cb.set_file_name(fn);
    ret = m_cb.tell_process_data(0);
    FIN_IF(ret == psCancel, 0);   // user press Cancel

    UINT64 rsize = 0;
    while (rsize < fsize) {
      DWORD dw;
      size_t sz = m_buf.size();
      if (rsize + m_buf.size() > fsize) {
        sz = (size_t)(fsize - rsize);
      }
      x = ReadFile(hFile, m_buf.ptr(), (DWORD)sz, &dw, NULL);
      FIN_IF(!x, 0x267000 | E_EREAD);
      FIN_IF(dw != (DWORD)sz, 0x268000 | E_EREAD);
      rsize += sz;
      if (rsize == fsize) {
        size_t dk = (size_t)rsize & tar::BLOCKSIZE_MASK;
        if (dk) {
          memset(m_buf.data() + sz, 0, tar::BLOCKSIZE - dk);
          sz += tar::BLOCKSIZE - dk;
        }
      }      
      hr = m_lz4.frame_add_data(m_buf.data(), sz);
      FIN_IF(hr, 0x269000 | E_ECREATE);
      ret = m_cb.tell_process_data(rsize);
      FIN_IF(ret == psCancel, 0);   // user press Cancel
    }

    hr = m_lz4.frame_close();
    FIN_IF(hr, 0x279000 | E_ECREATE);
    ret = m_cb.tell_process_data(fsize, latest_file ? true: false);
    FIN_IF(ret == psCancel, 0);   // user press Cancel
  }

  frlen = fr.init_end(0);
  x = WriteFile(m_outFile, &fr, frlen, &dw, NULL);
  FIN_IF(!x || dw != frlen, 0x279500 | E_ECREATE);

  m_delete_out_file = false;
  hr = 0;

fin:
  LOGd_IF(hr == 0, "%s: OK!  output_size = %I64d ", __func__, m_buf.size() ? m_lz4.get_output_size() : -1LL);
  if (hr && m_outFile) {
    m_delete_out_file = true;
  }
  if (hFile)
    CloseHandle(hFile);
  return hr;
}


} /* namespace */
