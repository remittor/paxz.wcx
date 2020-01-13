#include "stdafx.h"
#include "wcx_archive.h"
#include "lz4lib.h"


namespace wcx {

archive::archive()
{
  m_thread_id = 0;
  m_open_mode = 0;
  m_file = NULL;
  m_cache = NULL;
  m_cur_dir = NULL;
  m_cur_file = NULL;
}

archive::~archive()
{
  WLOGi(L"%S: [0x%X] <<<DESTROY>>> \"%s\" {%p}", __func__, m_thread_id, get_name(), this);
  if (m_file)
    CloseHandle(m_file);
  if (m_cache)
    m_cache->release();
}

int archive::init(wcx::cfg & cfg, LPWSTR arcName, int openMode, DWORD thread_id, wcx::cache * cache)
{
  int hr = E_BAD_ARCHIVE;

  m_cache = cache;  // method add_ref must be already called!!!
  m_cfg = cfg;

  bool xb = m_arcfile.assign(m_cache->get_arcfile());
  FIN_IF(!xb, 0x7000100 | E_NO_MEMORY);

  m_open_mode = openMode;
  m_thread_id = thread_id;
  WLOGd(L"%S: [0x%X] <<<INIT>>> \"%s\" {%p}", __func__, thread_id, arcName, this);
  
  m_file = CreateFileW(get_fullname(), GENERIC_READ, FILE_SHARE_VALID_FLAGS, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  m_file = (m_file == INVALID_HANDLE_VALUE) ? NULL : m_file;
  FIN_IF(!m_file, 0x7055000 | E_EOPEN);

  hr = 0;

fin:
  //LOGe_IF(hr, "%s: ERR = 0x%X \n", __func__, hr);
  return hr;
}

BST_INLINE
DWORD get_tc_fileattr(DWORD attr)
{
  DWORD res = 0;
  if (attr & FILE_ATTRIBUTE_READONLY)  res |= 0x01;
  if (attr & FILE_ATTRIBUTE_HIDDEN)    res |= 0x02;
  if (attr & FILE_ATTRIBUTE_SYSTEM)    res |= 0x04;
  if (attr & FILE_ATTRIBUTE_DEVICE)    res |= 0x08;
  if (attr & FILE_ATTRIBUTE_DIRECTORY) res |= 0x10;
  if (attr & FILE_ATTRIBUTE_ARCHIVE)   res |= 0x20;
  return res;
}

BST_INLINE
int get_tc_filetime(const FILETIME * file_time)
{
  FILETIME ft;
  FileTimeToLocalFileTime(file_time, &ft);
  SYSTEMTIME st;
  FileTimeToSystemTime(&ft, &st);
  return ((DWORD)st.wYear - 1980) << 25 
        | (DWORD)st.wMonth << 21
        | (DWORD)st.wDay << 16
        | (DWORD)st.wHour << 11
        | (DWORD)st.wMinute << 5
        | (DWORD)(st.wSecond / 2);
}

int get_tc_filetime(INT64 file_time)
{
  return get_tc_filetime((const FILETIME *)&file_time);
}

int archive::list(tHeaderDataExW * hdr)
{
  int hr = E_BAD_ARCHIVE;
  cache_item * ci;

  //WLOGd(L"%S: [0x%X] <<<LIST>>> \"%s\" {%p}", __func__, GetCurrentThreadId(), hdr->ArcName, this);
  m_cur_file = NULL;

  WLOGe_IF(!m_cache, L"%S: ERROR: cache not init!!! \"%s\" ", __func__, get_name());
  if (!m_cache)
    return E_END_ARCHIVE;

  //bst::scoped_read_lock lock(m_cache->get_mutex());
  if (m_cache->get_status() != wcx::cache::stReady) {
    WLOGe(L"%S: ERROR: cache not ready!!! \"%s\" %d ", __func__, get_name(), m_cache->get_status());
    FIN(E_END_ARCHIVE);
  }

  while (ci = m_cache->get_next_file(m_cenum)) {
    if (ci->deleted)
      continue;
    if (ci->name_len < _countof(hdr->FileName))
      break;    
    WLOGw(L"%S: <<<SKIP LONG FILE>>> data_size = %I64d \"%s\"  ", __func__, ci->info.data_size, ci->name);
  }  
  if (!ci) {
    LOGi("%s: <<<END OF FILE LIST>>> (file count = %I64d) ", __func__, get_file_count());
    m_cenum.reset();
    m_dst_dir.clear();
    FIN(E_END_ARCHIVE);
  }

  WLOGd(L"%S: \"%s\" data_size = %I64d ", __func__, ci->name, ci->info.data_size);
  FIN_IF(ci->name_len >= _countof(hdr->FileName), E_SMALL_BUF);

  memset(hdr, 0, sizeof(*hdr));
  //wcsncpy(hdr->ArcName, a->ArcName, _countof(hdr->ArcName));
  //hdr->ArcName[_countof(hdr->ArcName) - 1] = 0;
  wcsncpy(hdr->FileName, ci->name, _countof(hdr->FileName));
  hdr->FileName[_countof(hdr->FileName) - 1] = 0;
  hdr->FileAttr = get_tc_fileattr(ci->info.attr);
  hdr->FileTime = get_tc_filetime(ci->info.mtime);
  hdr->PackSize = ci->info.pack_size;
  hdr->UnpSize  = ci->info.data_size;
  if (ci->info.pax.realsize > ci->info.data_size)
    hdr->UnpSize = ci->info.pax.realsize;
  if (m_open_mode == PK_OM_EXTRACT) {
    if (ci->info.attr & FILE_ATTRIBUTE_DIRECTORY) {
      m_cur_dir = ci;
    } else {
      m_cur_file = ci;
    }
  }
  hr = 0;

fin:
  return hr;
}

int archive::extract(int Operation, LPCWSTR DestPath, LPCWSTR DestName)
{
  int hr = E_BAD_ARCHIVE;
  HANDLE hDstFile = NULL;
  FILE_BASIC_INFORMATION fbi;

  m_operation = Operation;
  m_delete_out_file = false;
  
  if (Operation == PK_SKIP) {
    if (m_cur_dir && m_dst_dir.length()) {
      extract_dir();  /* !!! mega hack !!! */
    } else {
      //WLOGd(L"%S(%d): '%s' <<<SKIP FILE>>> Dest = \"%s\" \"%s\" ", __func__, Operation, get_name(), DestPath, DestName);
    }
    FIN(0);
  }
  WLOGd(L"%S(%d): '%s' Dest = \"%s\" \"%s\" ", __func__, Operation, get_name(), DestPath, DestName);
  
  if (m_dst_dir.empty() && DestName && m_cur_file) {
    size_t nlen = wcslen(DestName);
    if (m_cur_file->name_len < nlen) {
      size_t dlen = nlen - m_cur_file->name_len;
      LPCWSTR name = DestName + dlen;
      if (_wcsicmp(name, m_cur_file->name) == 0) {
        m_dst_dir.assign(DestName, dlen);
        FIN_IF(m_dst_dir.length() != dlen, 0x189000 | E_NO_MEMORY);
        WLOGd(L"%S: DestDir = \"%s\" ", __func__, m_dst_dir.c_str());
      }
    }
  }
  if (Operation == PK_TEST) {
    // TODO: test unpack file to memory
    FIN(E_NOT_SUPPORTED);
  }
  if (DestPath) {
    WLOGe(L"%S: ERROR: arg DestPath not supported!!! \"%s\" ", __func__, DestPath);
    FIN(E_NOT_SUPPORTED);
  }
  FIN_IF(!DestName, 0x100000 | E_EOPEN);
  FIN_IF(!m_cur_file, 0x101000 | E_NO_FILES);
  FIN_IF(m_cur_file->deleted, 0x102000 | E_NO_FILES);
  size_t len = wcslen(DestName);
  FIN_IF(len <= 3, 0x103000 | E_ECREATE);

  m_filename.clear();
  hr = get_full_filename(DestName, m_filename);
  FIN_IF(hr, 0x104000 | E_ECREATE);

  const size_t buf_size = 4*1024*1024;
  if (m_buf.capacity() < buf_size) {
    FIN_IF(!m_buf.reserve(buf_size + 256), 0x117000 | E_NO_MEMORY);
    m_buf.resize(buf_size);
  }
  if (m_dst.capacity() < buf_size) {
    FIN_IF(!m_dst.reserve(buf_size + 256), 0x118000 | E_NO_MEMORY);
    m_dst.resize(buf_size);
  }
  
  {
    //bst::scoped_read_lock lock(m_cache->get_mutex());
    if (m_cache->get_status() != wcx::cache::stReady) {
      WLOGe(L"%S: ERROR: cache not ready!!! \"%s\" %d ", __func__, get_name(), m_cache->get_status());
      FIN(0x121000 | E_EOPEN);
    }
    FIN_IF(m_cur_file->deleted, 0x122000 | E_NO_FILES);
    
    m_cb.set_file_name(DestName);
    //ret = m_cb.tell_top_progressbar(1);
    //FIN_IF(ret == psCancel, 0);   // user press Cancel
    
    UINT64 file_size = m_cache->get_file_size();
    if (m_cache->get_type() == wcx::atPax) {
      hr = extract_pax(m_filename.c_str(), file_size, hDstFile);
      FIN_IF(hr, hr | E_EREAD);      
    }
    if (m_cache->get_type() == wcx::atLz4 || m_cache->get_type() == wcx::atZstd) {
      m_cb.set_total_read(file_size);   /* set reading mode */
      hr = extract_native(m_filename.c_str(), file_size, hDstFile);
      FIN_IF(hr, hr | E_EREAD);
    }
    if (m_cache->get_type() == wcx::atPaxLz4) {
      hr = extract_paxlz4(m_filename.c_str(), file_size, hDstFile);
      FIN_IF(hr, hr | E_EREAD);
    }
    if (m_cache->get_type() == wcx::atPaxZstd) {
      hr = extract_paxzst(m_filename.c_str(), file_size, hDstFile);
      FIN_IF(hr, hr | E_EREAD);
    }
    memset(&fbi, 0, sizeof(fbi));    
    fbi.FileAttributes = m_cur_file->info.attr;
    if (m_cur_file->info.ctime > 0) {
      fbi.CreationTime.QuadPart = m_cur_file->info.ctime;
    }
    if (m_cur_file->info.mtime > 0) {
      fbi.LastWriteTime.QuadPart = m_cur_file->info.mtime;
      fbi.LastAccessTime.QuadPart = m_cur_file->info.mtime;
      if (fbi.CreationTime.QuadPart == 0)
        fbi.CreationTime.QuadPart = m_cur_file->info.mtime;
    }
  }

  hr = 0;
  if (hDstFile) {
    nt::SetFileAttrByHandle(hDstFile, &fbi);
  }
  
fin:
  if (hr) {
    WLOGe(L"%S: ERROR = 0x%X \"%s\" ", __func__, hr, get_name());
    m_delete_out_file = true;
  }
  if (hDstFile) {
    if (m_delete_out_file)
      nt::DeleteFileByHandle(hDstFile);
    CloseHandle(hDstFile);
  }
  m_cur_file = NULL;
  m_operation = -1;
  return hr & 0xFF;
}

HANDLE archive::create_new_file(LPCWSTR fn)
{
  DWORD dwAccess = GENERIC_READ | GENERIC_WRITE | DELETE | FILE_WRITE_ATTRIBUTES;   
  HANDLE hDstFile = CreateFileW(fn, dwAccess, FILE_SHARE_VALID_FLAGS, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
  hDstFile = (hDstFile == INVALID_HANDLE_VALUE) ? NULL : hDstFile;
  return hDstFile;
}

int archive::extract_pax(LPCWSTR fn, UINT64 file_size, HANDLE & hDstFile)
{
  int hr = 0;
  UINT64 pos;
  LARGE_INTEGER fpos;
  DWORD dw;
  BOOL x;
  UINT64 data_size = 0;
  UINT64 written = 0;

  pos = m_cur_file->info.pax.pos + m_cur_file->info.pax.hdr_size;    
  FIN_IF(pos >= file_size, 0x131000 | E_EOPEN);
  fpos.QuadPart = pos;
  BOOL xp = SetFilePointerEx(m_file, fpos, NULL, FILE_BEGIN);
  FIN_IF(xp == FALSE, 0x132000 | E_EOPEN);
  data_size = m_cur_file->info.data_size;
  if (pos + data_size > file_size) {
    data_size = file_size - pos;   // may be ERROR ???
  }
  
  hDstFile = create_new_file(fn);
  FIN_IF(!hDstFile, 0x133000 | E_ECREATE);
  m_delete_out_file = true;

  int ret = m_cb.tell_process_data(0);
  FIN_IF(ret == psCancel, 0);   // user press Cancel
  
  if (data_size) {
    UINT64 end = pos + data_size;
    while (pos < end) {
      size_t sz = m_buf.size();
      if (pos + sz > end)
        sz = (size_t)(end - pos);
      x = ReadFile(m_file, m_buf.data(), (DWORD)sz, &dw, NULL);
      FIN_IF(!x || sz != dw, 0x134000 | E_EREAD);
      x = WriteFile(hDstFile, m_buf.c_data(), (DWORD)sz, &dw, NULL);
      FIN_IF(!x || sz != dw, 0x135000 | E_EWRITE);
      pos += sz;
      written += sz;
      ret = m_cb.tell_process_data(written);
      FIN_IF(ret == psCancel, 0);   // user press Cancel
    }
  }
  hr = 0;
  m_delete_out_file = false;
  ret = m_cb.tell_process_data(data_size);
  FIN_IF(ret == psCancel, 0);   // user press Cancel
  
fin:  
  return hr;
}

int archive::extract_native(LPCWSTR fn, UINT64 file_size, HANDLE & hDstFile)
{
  int hr = 0;
  DWORD magicNumber;
  size_t nbReadBytes = 0;
  union {
    UINT64 pos;
    LARGE_INTEGER fpos;
  };
  DWORD dw;
  UINT64 data_size = 0;

  pos = 0;
  BOOL xp = SetFilePointerEx(m_file, fpos, NULL, FILE_BEGIN);
  FIN_IF(xp == FALSE, 0x142000 | E_EOPEN);

  hDstFile = create_new_file(fn);
  FIN_IF(!hDstFile, 0x143000 | E_ECREATE);
  m_delete_out_file = false;

  int ret = m_cb.tell_process_data(0);
  FIN_IF(ret == psCancel, 0);   // user press Cancel

  while (1) {
    BOOL x = ReadFile(m_file, &magicNumber, sizeof(magicNumber), (PDWORD)&nbReadBytes, NULL);
    FIN_IF(!x, 0x145000 | E_EREAD);
    FIN_IF(nbReadBytes != sizeof(magicNumber), 0);  /* EOF */
    pos += sizeof(magicNumber);

    if ((magicNumber & lz4::LZ4IO_SKIPPABLEMASK) == lz4::LZ4IO_SKIPPABLE0)
      magicNumber = lz4::LZ4IO_SKIPPABLE0;

    switch (magicNumber) {
    case zst::MAGICNUMBER: {
      hr = m_zst.ctx.init();
      FIN_IF(hr, 0x144700 | E_EREAD);
      memcpy(m_buf.data(), &magicNumber, sizeof(magicNumber));
      size_t srcBufferLoaded = sizeof(magicNumber);
      size_t toRead = ZSTD_FRAMEHEADERSIZE_MAX - srcBufferLoaded;
      BOOL x = ReadFile(m_file, m_buf.data() + srcBufferLoaded, (DWORD)toRead, (PDWORD)&nbReadBytes, NULL);
      FIN_IF(!x, 0x7146000 | E_EREAD);
      FIN_IF(nbReadBytes == 0, 0);  /* EOF */
      pos += nbReadBytes;
      srcBufferLoaded += nbReadBytes;
      FIN_IF(srcBufferLoaded < zst::FRAMEHEADER_SIZE_MIN, 0x7146100 | E_EREAD);

      while (1) {
        ZSTD_inBuffer  inBuff  = { m_buf.data(), srcBufferLoaded, 0 };
        ZSTD_outBuffer outBuff = { m_dst.data(), m_dst.size(), 0 };
        size_t readSizeHint = ZSTD_decompressStream(m_zst.ctx.get_ctx(), &outBuff, &inBuff);
        FIN_IF(ZSTD_isError(readSizeHint), 0x7146300 | E_EREAD);
        if (outBuff.pos) {
          BOOL x = WriteFile(hDstFile, m_dst.c_data(), (DWORD)outBuff.pos, &dw, NULL);
          FIN_IF(!x || outBuff.pos != dw, 0x7146600 | E_EWRITE);
        }
        if (readSizeHint == 0)  /* end of frame */
          break;

        if (inBuff.pos > 0) {
          memmove(m_buf.data(), m_buf.data() + inBuff.pos, inBuff.size - inBuff.pos);
          srcBufferLoaded -= inBuff.pos;
        }
        size_t toDecode = BST_MIN(readSizeHint, m_buf.size());  /* support large skippable frames */
        if (srcBufferLoaded < toDecode) {
          size_t toRead = toDecode - srcBufferLoaded;
          BOOL x = ReadFile(m_file, m_buf.data() + srcBufferLoaded, (DWORD)toRead, (PDWORD)&nbReadBytes, NULL);
          FIN_IF(!x, 0x7147100 | E_EREAD);
          FIN_IF(nbReadBytes == 0, 0x7147200 | E_EREAD);
          pos += nbReadBytes;
          srcBufferLoaded += nbReadBytes;
        }
        int ret = m_cb.tell_process_data(pos);
        FIN_IF(ret == psCancel, 0);   // user press Cancel
      }
      break;
    }

    case lz4::LZ4IO_MAGICNUMBER: {
      hr = m_lz4.ctx.init();
      FIN_IF(hr, 0x144400 | E_EREAD);
      LZ4F_errorCode_t nextToLoad;
      size_t inSize = lz4::MAGICNUMBER_SIZE;
      size_t outSize= 0;
      memcpy(m_buf.data(), &magicNumber, sizeof(magicNumber));
      nextToLoad = LZ4F_decompress(m_lz4.ctx.get_ctx(), m_dst.data(), &outSize, m_buf.c_data(), &inSize, NULL);
      FIN_IF(LZ4F_isError(nextToLoad), 0x147000 | E_EREAD);

      UINT64 end = file_size;
      while (nextToLoad) {
        size_t readSize = 0;
        size_t offset = 0;
        size_t decodedBytes = m_dst.size();

        if (nextToLoad > m_buf.size())
          nextToLoad = m_buf.size();
        FIN_IF(pos + nextToLoad > end, 0x148000 | E_EREAD);
        BOOL x = ReadFile(m_file, m_buf.data(), (DWORD)nextToLoad, (PDWORD)&readSize, NULL);
        FIN_IF(!x || nextToLoad != readSize, 0x149000 | E_EREAD);
        pos += readSize;

        while ((offset < readSize) || (decodedBytes == m_dst.size())) {  /* still to read, or still to flush */
          size_t remaining = readSize - offset;
          decodedBytes = m_dst.size();
          LPBYTE srcBuf = (LPBYTE)m_buf.data() + offset;
          nextToLoad = LZ4F_decompress(m_lz4.ctx.get_ctx(), m_dst.data(), &decodedBytes, srcBuf, &remaining, NULL);
          LZ4F_frameInfo_t * fi = m_lz4.ctx.get_frame_info();
          FIN_IF(LZ4F_isError(nextToLoad), 0x151000 | E_EREAD);
          offset += remaining;
          if (decodedBytes) {
            BOOL x = WriteFile(hDstFile, m_dst.c_data(), (DWORD)decodedBytes, &dw, NULL);
            FIN_IF(!x || decodedBytes != dw, 0x152000 | E_EWRITE);
          }
          if (!nextToLoad)
            break;
        } 
        int ret = m_cb.tell_process_data(pos);
        FIN_IF(ret == psCancel, 0);   // user press Cancel
      }
      break;
    }

    case lz4::LZ4IO_SKIPPABLE0: {
      DWORD size = 0;
      BOOL x = ReadFile(m_file, &size, sizeof(size), (PDWORD)&nbReadBytes, NULL);
      FIN_IF(!x || nbReadBytes != sizeof(size), 0x155000 | E_EREAD);
      pos += sizeof(size) + size;
      BOOL xp = SetFilePointerEx(m_file, fpos, NULL, FILE_BEGIN);
      FIN_IF(xp == FALSE, 0x156000 | E_EREAD);
      int ret = m_cb.tell_process_data(pos);
      FIN_IF(ret == psCancel, 0);   // user press Cancel
      break;
    }

    case lz4::LEGACY_MAGICNUMBER: {
      size_t insz = LZ4_compressBound(lz4::LEGACY_BLOCKSIZE);
      size_t sz = m_in_buf.resize(insz);
      FIN_IF(sz == bst::npos, 0x161000 | E_NO_MEMORY);
      sz = m_out_buf.resize(lz4::LEGACY_BLOCKSIZE);
      FIN_IF(sz == bst::npos, 0x162000 | E_NO_MEMORY);
      while (1) {
        DWORD blockSize;
        BOOL x = ReadFile(m_file, &blockSize, sizeof(blockSize), (PDWORD)&nbReadBytes, NULL);
        FIN_IF(!x || nbReadBytes != sizeof(blockSize), 0x163000 | E_EREAD);
        if (blockSize > LZ4_COMPRESSBOUND(lz4::LEGACY_BLOCKSIZE)) {
          /* Cannot read next block : maybe new stream ? */
          BOOL xp = SetFilePointerEx(m_file, fpos, NULL, FILE_BEGIN);
          FIN_IF(xp == FALSE, 0x164000 | E_EREAD);
          break;
        }
        pos += sizeof(blockSize);
        BOOL v = ReadFile(m_file, m_in_buf.data(), blockSize, (PDWORD)&nbReadBytes, NULL);
        FIN_IF(!v || nbReadBytes != blockSize, 0x165000 | E_EREAD);
        pos += blockSize;

        int decodeSize = LZ4_decompress_safe((LPCSTR)m_in_buf.c_data(), (LPSTR)m_out_buf.data(), (int)blockSize, lz4::LEGACY_BLOCKSIZE);
        FIN_IF(decodeSize < 0, 0x166000 | E_EREAD);
        if (decodeSize > 0) {
          BOOL x = WriteFile(hDstFile, m_out_buf.c_data(), (DWORD)decodeSize, &dw, NULL);
          FIN_IF(!x || decodeSize != dw, 0x167000 | E_EWRITE);
        }

        int ret = m_cb.tell_process_data(pos);
        FIN_IF(ret == psCancel, 0);   // user press Cancel
      }
      break;
    }

    default:
      FIN(0);     // ignore trash on end file

    } /* switch */
  }

  hr = 0;
  m_delete_out_file = false;

fin:
  if (hr >= 0x145000) {
    hr = 0;   // ignore ALL errors on unpacking LZ4/Zstd stream
    m_delete_out_file = false;
  }  
  return hr;
}

int archive::extract_paxlz4(LPCWSTR fn, UINT64 file_size, HANDLE & hDstFile)
{
  int hr = 0;
  DWORD magicNumber;
  size_t nbReadBytes = 0;
  union {
    UINT64 pos;
    LARGE_INTEGER fpos;
  };
  DWORD dw;
  UINT64 written = 0;

  pos = m_cur_file->info.pax.pos + m_cur_file->info.pax.hdr_p_size;
  BOOL xp = SetFilePointerEx(m_file, fpos, NULL, FILE_BEGIN);
  FIN_IF(xp == FALSE, 0x171000 | E_EOPEN);

  hDstFile = create_new_file(fn);
  FIN_IF(!hDstFile, 0x172000 | E_ECREATE);
  if (m_cur_file->info.data_size == 0 || m_cur_file->info.pack_size == m_cur_file->info.pax.hdr_p_size) {
    m_delete_out_file = false;   /* file have zero len */
    FIN(0);
  }
  m_delete_out_file = true;

  int ret = m_cb.tell_process_data(0);
  FIN_IF(ret == psCancel, 0);   // user press Cancel

  hr = m_lz4.ctx.init();
  FIN_IF(hr, 0x173000 | E_EREAD);

  UINT64 end = file_size;

  BOOL x = ReadFile(m_file, &magicNumber, sizeof(magicNumber), (PDWORD)&nbReadBytes, NULL);
  FIN_IF(!x || nbReadBytes != sizeof(magicNumber), 0x174000 | E_EREAD);
  pos += sizeof(magicNumber);

  FIN_IF(magicNumber != lz4::LZ4IO_MAGICNUMBER, 0x175000 | E_EREAD);
  LZ4F_errorCode_t nextToLoad;
  size_t inSize = lz4::MAGICNUMBER_SIZE;
  size_t outSize= 0;
  memcpy(m_buf.data(), &magicNumber, sizeof(magicNumber));
  nextToLoad = LZ4F_decompress(m_lz4.ctx.get_ctx(), m_dst.data(), &outSize, m_buf.c_data(), &inSize, NULL);
  FIN_IF(LZ4F_isError(nextToLoad), 0x177000 | E_EREAD);

  while (nextToLoad) {
    size_t readSize = 0;
    size_t offset = 0;
    size_t decodedBytes = m_dst.size();

    if (nextToLoad > m_buf.size())
      nextToLoad = m_buf.size();
    FIN_IF(pos + nextToLoad > end, 0x178000 | E_EREAD);
    BOOL x = ReadFile(m_file, m_buf.data(), (DWORD)nextToLoad, (PDWORD)&readSize, NULL);
    FIN_IF(!x || nextToLoad != readSize, 0x179000 | E_EREAD);
    pos += readSize;

    while ((offset < readSize) || (decodedBytes == m_dst.size())) {  /* still to read, or still to flush */
      size_t remaining = readSize - offset;
      decodedBytes = m_dst.size();
      LPBYTE srcBuf = (LPBYTE)m_buf.data() + offset;
      nextToLoad = LZ4F_decompress(m_lz4.ctx.get_ctx(), m_dst.data(), &decodedBytes, srcBuf, &remaining, NULL);
      FIN_IF(LZ4F_isError(nextToLoad), 0x181000 | E_EREAD);
      offset += remaining;
      size_t sz = decodedBytes;
      if (written + sz >= m_cur_file->info.data_size) {
        sz = (size_t)(m_cur_file->info.data_size - written);
        nextToLoad = 0;  /* stop decoding */
      }
      if (sz) {
        BOOL x = WriteFile(hDstFile, m_dst.data(), (DWORD)sz, &dw, NULL);
        FIN_IF(!x || dw != (DWORD)sz, 0x182000 | E_EWRITE);
        written += sz;
        int ret = m_cb.tell_process_data(written);
        FIN_IF(ret == psCancel, 0);   // user press Cancel
      }
      if (!nextToLoad)
        break;
    } 
  }

  hr = 0;
  m_delete_out_file = false;
  ret = m_cb.tell_process_data(written);
  FIN_IF(ret == psCancel, 0);   // user press Cancel

fin:
  return hr;
}

int archive::extract_paxzst(LPCWSTR fn, UINT64 file_size, HANDLE & hDstFile)
{
  int hr = 0;
  DWORD magicNumber;
  size_t nbReadBytes = 0;
  union {
    UINT64 pos;
    LARGE_INTEGER fpos;
  };
  DWORD dw;
  UINT64 data_size = 0;
  UINT64 written = 0;

  pos = m_cur_file->info.pax.pos + m_cur_file->info.pax.hdr_p_size;
  BOOL const xp = SetFilePointerEx(m_file, fpos, NULL, FILE_BEGIN);
  FIN_IF(xp == FALSE, 0x7171000 | E_EOPEN);

  hDstFile = create_new_file(fn);
  FIN_IF(!hDstFile, 0x7172000 | E_ECREATE);
  if (m_cur_file->info.data_size == 0 || m_cur_file->info.pack_size == m_cur_file->info.pax.hdr_p_size) {
    m_delete_out_file = false;   /* file have zero len */
    FIN(0);
  }
  m_delete_out_file = true;

  int ret = m_cb.tell_process_data(0);
  FIN_IF(ret == psCancel, 0);   // user press Cancel

  hr = m_zst.ctx.init();
  FIN_IF(hr, 0x7173000 | E_EREAD);

  UINT64 end = file_size;

  BOOL x = ReadFile(m_file, &magicNumber, sizeof(magicNumber), (PDWORD)&nbReadBytes, NULL);
  FIN_IF(!x, 0x7174000 | E_EREAD);
  FIN_IF(nbReadBytes != sizeof(magicNumber), 0x7174100 | E_EREAD);
  pos += sizeof(magicNumber);

  FIN_IF(magicNumber != zst::MAGICNUMBER, 0x7175000 | E_EREAD);

  memcpy(m_buf.data(), &magicNumber, sizeof(magicNumber));
  size_t srcBufferLoaded = sizeof(magicNumber);
  size_t toRead = ZSTD_FRAMEHEADERSIZE_MAX - srcBufferLoaded;
  x = ReadFile(m_file, m_buf.data() + srcBufferLoaded, (DWORD)toRead, (PDWORD)&nbReadBytes, NULL);
  FIN_IF(!x, 0x7176000 | E_EREAD);
  FIN_IF(nbReadBytes == 0, 0x7176100 | E_EREAD);
  pos += nbReadBytes;
  srcBufferLoaded += nbReadBytes;
  FIN_IF(srcBufferLoaded < zst::FRAMEHEADER_SIZE_MIN, 0x7176200 | E_EREAD);

  while (1) {
    ZSTD_inBuffer  inBuff  = { m_buf.data(), srcBufferLoaded, 0 };
    ZSTD_outBuffer outBuff = { m_dst.data(), m_dst.size(), 0 };
    size_t readSizeHint = ZSTD_decompressStream(m_zst.ctx.get_ctx(), &outBuff, &inBuff);
    FIN_IF(ZSTD_isError(readSizeHint), 0x7176300 | E_EREAD);
    size_t sz = outBuff.pos;
    if (written + sz >= m_cur_file->info.data_size) {
      sz = (size_t)(m_cur_file->info.data_size - written);
      readSizeHint = 0;  /* stop decoding */
    }
    if (sz) {
      BOOL x = WriteFile(hDstFile, m_dst.c_data(), (DWORD)sz, &dw, NULL);
      FIN_IF(!x || sz != dw, 0x7176600 | E_EWRITE);
      written += sz;
      int ret = m_cb.tell_process_data(written);
      FIN_IF(ret == psCancel, 0);   // user press Cancel
    }
    if (readSizeHint == 0)  /* end of frame */
      break;   

    if (inBuff.pos > 0) {
      memmove(m_buf.data(), m_buf.data() + inBuff.pos, inBuff.size - inBuff.pos);
      srcBufferLoaded -= inBuff.pos;
    }
    size_t toDecode = BST_MIN(readSizeHint, m_buf.size());  /* support large skippable frames */
    if (srcBufferLoaded < toDecode) {
      size_t toRead = toDecode - srcBufferLoaded;
      BOOL x = ReadFile(m_file, m_buf.data() + srcBufferLoaded, (DWORD)toRead, (PDWORD)&nbReadBytes, NULL);
      FIN_IF(!x, 0x7177100 | E_EREAD);
      FIN_IF(nbReadBytes == 0, 0x7177200 | E_EREAD);
      pos += nbReadBytes;
      srcBufferLoaded += nbReadBytes;
    }
  }

  hr = 0;
  m_delete_out_file = false;
  ret = m_cb.tell_process_data(written);
  FIN_IF(ret == psCancel, 0);   // user press Cancel

fin:
  return hr;
}

int archive::extract_dir()
{
  int hr = 0;
  HANDLE hDstDir = NULL;
  cache_item * dir = m_cur_dir;
  FILE_BASIC_INFO fbi;
  
  m_cur_dir = NULL;
  
  m_filename.clear();
  hr = get_full_filename(m_dst_dir.c_str(), dir->name, m_filename);
  FIN_IF(hr < 0, 0x191100 | E_NO_MEMORY);
  
  DWORD dwAccess = GENERIC_READ | FILE_READ_ATTRIBUTES | FILE_WRITE_ATTRIBUTES;
  hDstDir = CreateFileW(m_filename.c_str(), dwAccess, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
  hDstDir = (hDstDir == INVALID_HANDLE_VALUE) ? NULL : hDstDir;
  FIN_IF(!hDstDir, 0x192000 | E_EOPEN);

  BOOL x = nt::GetFileInformationByHandleEx(hDstDir, FileBasicInfo, &fbi, sizeof(fbi));
  FIN_IF(!x, 0x193000 | E_EOPEN);

  INT64 now;
  GetSystemTimeAsFileTime((LPFILETIME)&now);

  const INT WINDOWS_TICK = 10000000L;
  INT64 time_diff = now - fbi.CreationTime.QuadPart;
  FIN_IF(time_diff < 0, 0);                  // UB
  FIN_IF(time_diff > 30 * WINDOWS_TICK, 0);  // 30 seconds

  FIN_IF(dir->info.ctime <= 0 && dir->info.mtime <= 0, 0);
  
  fbi.FileAttributes = dir->info.attr;
  fbi.CreationTime.QuadPart = 0;
  if (dir->info.ctime > 0) {
    fbi.CreationTime.QuadPart = dir->info.ctime;
  }
  if (dir->info.mtime > 0) {
    fbi.LastWriteTime.QuadPart = dir->info.mtime;
    fbi.LastAccessTime.QuadPart = dir->info.mtime;
    if (fbi.CreationTime.QuadPart == 0)
      fbi.CreationTime.QuadPart = dir->info.mtime;
  }
  
  WLOGd(L"%S(%d): <<<UPDATE DIR>>> \"%s\" ", __func__, m_operation, m_filename.c_str());
  nt::SetFileAttrByHandle(hDstDir, &fbi);
  hr = 0;

fin:
  WLOGe_IF(hr, L"%S(%d): ERROR: 0x%X <<<UPDATE DIR>>> \"%s\" ", __func__, m_operation, hr, m_filename.c_str());
  if (hDstDir)
    CloseHandle(hDstDir);
  return hr;
}


} /* namespace */
