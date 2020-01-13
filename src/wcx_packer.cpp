#include "stdafx.h"
#include "wcx_packer.h"


namespace wcx {

packer::packer(ArcType ctype, wcx::cfg & cfg, int Flags, DWORD thread_id, tProcessDataProcW callback)
{
  m_ctype = ctype;
  m_cfg = cfg;
  m_thread_id = thread_id;
  m_AddListSize = 0;
  m_outFile = NULL;
  reset();
  m_Flags = Flags;  
  m_cb.set_ProcessDataProc(callback);
  m_readed_size = 0;
  m_output_size = 0;
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

void packer::reset_ctx(bool full_reset)
{
  m_readed_size = 0;
  m_output_size = 0;
  if (m_ctype == atPaxLz4 || m_ctype == atLz4) {
    m_lz4.ctx.reset();
  }
  if (m_ctype == atPaxZstd || m_ctype == atZstd) {
    m_zst.ctx.reset(full_reset);
  }
}

int packer::set_block_size(SSIZE_T blk_size)
{
  if (blk_size > 128)
    blk_size = (blk_size + 1) / (1024*1024);   // convert to MiB

  if (blk_size <= 0) {
    m_block_size = 512*1024;
  } else {
    m_block_size = (size_t)blk_size * (1024*1024);
  }
  return (int)m_block_size;
}

int packer::set_compression_level(int cpr_level)
{
  if (cpr_level <= 0)
    cpr_level = -1;

  if (cpr_level > 9)
    cpr_level = 9;     /* LZ4HC_CLEVEL_DEFAULT = 9; ZSTD_MAX_CLEVEL = 22; */ 

  m_cpr_level = cpr_level;
  if ((m_ctype == atPaxZstd || m_ctype == atZstd) && m_cpr_level >= 1) {
    m_cpr_level += 1;  /* 9 -> 10 */
  }
  return 0;
}

static ZSTD_inBuffer emptyInput = { NULL, 0, 0 };

int packer::frame_create(UINT64 total_size)
{
  int hr = 0;
  size_t sz;

  FIN_IF(!m_outFile, -1);
  FIN_IF(m_cpr_level < 0, 0);

  if (m_ctype == atPaxLz4) {
    LZ4F_preferences_t * prefs = &m_lz4.prefs;
    memset(prefs, 0, sizeof(LZ4F_preferences_t));
    prefs->compressionLevel = (m_cpr_level == 0) ? 1 : m_cpr_level;
    prefs->frameInfo.blockMode = LZ4F_blockIndependent;
    prefs->frameInfo.blockSizeID = LZ4F_max4MB;
    prefs->frameInfo.blockChecksumFlag = LZ4F_blockChecksumEnabled;
    prefs->frameInfo.contentChecksumFlag = LZ4F_contentChecksumEnabled;
    prefs->favorDecSpeed = 0;   /* 1: parser favors decompression speed vs compression ratio. Only works for high compression modes (>= LZ4HC_CLEVEL_OPT_MIN) */
    prefs->frameInfo.contentSize = total_size;
    prefs->autoFlush = 1;

    hr = m_lz4.ctx.init(m_lz4.prefs);
    FIN_IF(hr, -10);

    size_t bsz = LZ4F_compressFrameBound(m_block_size, &m_lz4.prefs);
    if (bsz > m_cpr_buf.size()) {
      size_t msz = bsz + 2048;
      FIN_IF(!m_cpr_buf.reserve(msz + 2048), -11);
      m_cpr_buf.resize(msz);
    }

    sz = LZ4F_compressBegin(get_lz4_ctx(), m_cpr_buf.data(), m_cpr_buf.size(), &m_lz4.prefs);
    FIN_IF(LZ4F_isError(sz), -12);
  }

  if (m_ctype == atPaxZstd) { 
    ZSTD_parameters * params = &m_zst.params;
    memset(params, 0, sizeof(ZSTD_parameters));

    hr = m_zst.ctx.init(m_cpr_level, NULL);
    FIN_IF(hr, -10);

    ZSTD_CCtx_setPledgedSrcSize(get_zst_ctx(), total_size);

    size_t bsz = ZSTD_compressBound(m_block_size) + 1024;
    if (bsz > m_cpr_buf.size()) {
      FIN_IF(!m_cpr_buf.reserve(bsz + 2048), -11);
      m_cpr_buf.resize(bsz);
    }

    ZSTD_outBuffer output = { m_cpr_buf.data(), m_cpr_buf.size(), 0 };
    sz = ZSTD_compressStream2(get_zst_ctx(), &output, &emptyInput, ZSTD_e_flush);
    FIN_IF(ZSTD_isError(sz), -12);
    sz = output.pos;
  }

  DWORD dw;
  BOOL x = WriteFile(m_outFile, m_cpr_buf.c_data(), (DWORD)sz, &dw, NULL);
  FIN_IF(!x, -15);
  FIN_IF((DWORD)sz != dw, -16);

  m_output_size += sz;
  hr = 0;

fin:
  return hr;
}

int packer::frame_add_data(PBYTE buf, size_t bufsize)
{
  int hr = 0;
  size_t sz;

  if (m_cpr_level >= 0) {
    if (m_ctype == atPaxLz4) {
      FIN_IF(m_lz4.prefs.autoFlush == 0, -21);
      FIN_IF(get_lz4_ctx() == NULL, -22);
      FIN_IF(bufsize > m_block_size, -23);
      sz = LZ4F_compressUpdate(get_lz4_ctx(), m_cpr_buf.data(), m_cpr_buf.size(), buf, bufsize, NULL);
      FIN_IF(LZ4F_isError(sz), -24);
    }
    if (m_ctype == atPaxZstd) {
      FIN_IF(get_zst_ctx() == NULL, -22);
      FIN_IF(bufsize > m_block_size, -23);
      ZSTD_inBuffer  input  = { buf, bufsize, 0 };
      ZSTD_outBuffer output = { m_cpr_buf.data(), m_cpr_buf.size(), 0 };
      sz = ZSTD_compressStream2(get_zst_ctx(), &output, &input, ZSTD_e_flush);
      FIN_IF(ZSTD_isError(sz), -24);
      sz = output.pos;
    }
    buf = m_cpr_buf.data();
  } else {
    sz = bufsize;
  }
  DWORD dw;
  BOOL x = WriteFile(m_outFile, (LPCVOID)buf, (DWORD)sz, &dw, NULL);
  FIN_IF(!x, -25);
  FIN_IF((DWORD)sz != dw, -26);

  m_readed_size += bufsize;
  m_output_size += sz;
  hr = 0;

fin:
  return hr;
}

int packer::frame_close()
{
  int hr = 0;
  size_t sz;

  FIN_IF(m_cpr_level < 0, 0);

  if (m_ctype == atPaxLz4) {
    sz = LZ4F_compressEnd(get_lz4_ctx(), m_cpr_buf.data(), m_cpr_buf.size(), NULL);
    FIN_IF(LZ4F_isError(sz), -31);
  }
  if (m_ctype == atPaxZstd) {
    ZSTD_outBuffer output = { m_cpr_buf.data(), m_cpr_buf.size(), 0 };
    sz = ZSTD_compressStream2(get_zst_ctx(), &output, &emptyInput, ZSTD_e_end);
    FIN_IF(ZSTD_isError(sz), -31);
    sz = output.pos;
  }
  DWORD dw;
  BOOL x = WriteFile(m_outFile, m_cpr_buf.c_data(), (DWORD)sz, &dw, NULL);
  FIN_IF(!x, -35);
  FIN_IF((DWORD)sz != dw, -36);

  m_output_size += sz;
  hr = 0;

fin:
  return hr;
}

int packer::pack_files(LPCWSTR SubPath, LPCWSTR SrcPath, LPCWSTR AddList)
{
  int hr = E_BAD_ARCHIVE;
  HANDLE hFile = NULL;
  BOOL x;
  int ret;
  size_t fn_num = 0;
  tar::pax_encode pax;
  paxz::frame_pax paxframe;
  paxz::frame_end endframe;
  DWORD dw;
  const char paxz_ver = 0;   /* actual version paxz format */
  int paxz_flags = 0;

  FIN_IF(!m_AddListSize, 0x201000 | E_NO_FILES);
  
  DWORD dwAccess = GENERIC_READ | GENERIC_WRITE | DELETE;
  m_outFile = CreateFileW(m_arcfile.get_fullname(), dwAccess, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  m_outFile = (m_outFile == INVALID_HANDLE_VALUE) ? NULL : m_outFile;
  FIN_IF(!m_outFile, 0x206000 | E_ECREATE);
  m_delete_out_file = true;

  if (m_buf.empty()) {
    const size_t bufsize = 4*1024*1024;
    FIN_IF(!m_buf.reserve(bufsize + tar::BLOCKSIZE * 8), 0x207000 | E_NO_MEMORY);
    m_buf.resize(bufsize);
    FIN_IF(m_buf.size() & tar::BLOCKSIZE_MASK, 0x208000 | E_NO_MEMORY);
  }

  set_block_size(m_buf.size());
  LOGi("%s: compression level = %d", __func__, m_cfg.get_compression_level());
  set_compression_level(m_cfg.get_compression_level());
  m_start_time = GetTickCount();
  reset_ctx(true);

  if (m_cpr_level >= 0) {
    FIN_IF(frame_create(0), 0x209100 | E_ECREATE);    /* create LZ4/Zstd zero frame */
    FIN_IF(frame_close(), 0x209200 | E_ECREATE);    
    int frlen = paxframe.init(paxz_ver, paxz_flags, false);
    BOOL x = WriteFile(m_outFile, &paxframe, frlen, &dw, NULL);
    FIN_IF(!x || dw != frlen, 0x209300 | E_ECREATE);
    if (paxz_flags & paxz::FLAG_DICT_FOR_HEADER) {
      // TODO: add dictionary support!
    }
  }
  if (m_ext_buf.empty()) {
    const size_t bufsize = 2*1024*1024;
    FIN_IF(!m_ext_buf.reserve(bufsize + 64), 0x209900 | E_NO_MEMORY);
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
    size_t hdr_size = m_ext_buf.size();
    hr = pax.create_header(hFile, &fi, NULL, fn, m_ext_buf.data(), hdr_size);
    FIN_IF(hr, hr | E_ECREATE);
    FIN_IF(hdr_size > m_block_size, 0x231000 | E_ECREATE);

    FIN_IF(frame_create(hdr_size), 0x232100 | E_ECREATE);
    FIN_IF(frame_add_data(m_ext_buf.data(), hdr_size), 0x232200 | E_ECREATE);
    FIN_IF(frame_close(), 0x232300 | E_ECREATE);

    UINT64 fsize = fi.size;
    if (is_dir || fsize == 0)
      continue;

    UINT64 content_size = tar::blocksize_round64(fsize);
    FIN_IF(frame_create(content_size), 0x233100 | E_ECREATE);

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
      hr = frame_add_data(m_buf.data(), sz);
      FIN_IF(hr, 0x269000 | E_ECREATE);
      ret = m_cb.tell_process_data(rsize);
      FIN_IF(ret == psCancel, 0);   // user press Cancel
    }

    hr = frame_close();
    FIN_IF(hr, 0x279000 | E_ECREATE);
    ret = m_cb.tell_process_data(fsize, latest_file ? true: false);
    FIN_IF(ret == psCancel, 0);   // user press Cancel
  }

  if (m_cpr_level >= 0) {
    int frlen = endframe.init(paxz_ver);
    BOOL x = WriteFile(m_outFile, &endframe, frlen, &dw, NULL);
    FIN_IF(!x || dw != frlen, 0x279500 | E_ECREATE);
  }
  m_delete_out_file = false;
  hr = 0;

fin:
  DWORD dt = GetTickBetween(m_start_time, GetTickCount());
  if (m_buf.size() && dt > 0) {
    UINT64 speed = m_readed_size / dt;  // (m_readed_size * 1000) / (dt * 1024);
    LOGi("%s: readed_size = %I64d MB  speed = %I64d kB/s  [%d ms]", __func__, m_readed_size / (1024*1024), speed, dt);
  }
  LOGd_IF(hr == 0, "%s: OK!  output_size = %I64d ", __func__, m_buf.size() ? m_output_size : -1LL);
  if (hr && m_outFile) {
    m_delete_out_file = true;
  }
  if (hFile)
    CloseHandle(hFile);
  return hr;
}


} /* namespace */
