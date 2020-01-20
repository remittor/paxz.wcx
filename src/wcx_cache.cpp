#include "stdafx.h"
#include "wcx_cache.h"
#include "bst\srw_lock.hpp"


namespace wcx {

cache::cache(cache_list & list, size_t index) : m_list(list)
{
  m_index = index;
  m_status = stAlloc;
  m_last_used_time = 0;
}

cache::~cache()
{
  WLOGd(L"%S: <<<DESTROY CACHE ITEM>>> \"%s\" ", __func__, m_arcfile.get_name());
  clear();
}

void cache::clear()
{
  m_status = stZombie;
  m_ftree.clear();
  m_end = 0;
}

bool cache::set_arcfile(const arcfile & af)
{
  return m_arcfile.assign(af);
}

bool cache::is_same_file(const arcfile & af)
{
  return (m_status == stZombie) ? false : m_arcfile.is_same_file(af);
}

cache_item * cache::add_file(LPCWSTR fullname, UINT64 size, BYTE type)
{
  bst::scoped_write_lock lock(m_mutex);
  return add_file_internal(fullname, size, type);
}  

cache_item * cache::add_file_internal(LPCWSTR fullname, UINT64 size, DWORD attr)
{
  if (!fullname)
    return NULL;

  size_t name_len = wcslen(fullname);
  if (!name_len)
    return NULL;
  
  cache_item * item = NULL;
  int hr = m_ftree.add_file(fullname, attr, NULL, &item);
  if (hr == 0 && item) {
    item->info.data_size = size;
    item->info.attr = attr;
    return item;
  }
  return NULL;
}

bool cache::find_directory(cache_dir & cd, LPCWSTR dir, WCHAR delimiter)
{
  return m_ftree.find_directory(cd, dir, delimiter);
}

cache_item * cache::get_next(cache_dir & cd)
{
  return cd.get_next();
}

bool cache::find_directory(cache_enum & ce, LPCWSTR dir, WCHAR delimiter, size_t max_depth)
{
  return m_ftree.find_directory(ce, dir, delimiter, max_depth);
}

cache_item * cache::get_next_file(cache_enum & ce)
{
  return ce.get_next();
}

int cache::update_release_time()
{
  GetSystemTimeAsFileTime((LPFILETIME)&m_last_used_time);
  return 0;
}

int cache::update_time(UINT64 ctime, UINT64 mtime)
{
  m_arcfile.update_time(ctime, mtime);
  return 0;
}

int cache::update_time(HANDLE hFile)
{
  UINT64 ctime, mtime;
  BOOL xb = GetFileTime(hFile, (LPFILETIME)&ctime, NULL, (LPFILETIME)&mtime);
  if (!xb)
    return 0x51300 | E_EREAD;
  return update_time(ctime, mtime);
}

int cache::init(wcx::arcfile * af)
{
  int hr = 0;
  HANDLE hFile = NULL;
  bst::filename name;

  bst::scoped_write_lock lock(m_mutex);   /* enter to exclusive lock */
  
  FIN_IF(m_status != stAlloc, 0x44001000 | E_EREAD);
  clear();
  if (af) {
    bool xb = m_arcfile.assign(*af);
    FIN_IF(!xb, 0x44001100 | E_EREAD);
  }
  FIN_IF(m_arcfile.get_size() == 0, 0x44001200 | E_EREAD);

  hFile = CreateFileW(m_arcfile.get_fullname(), GENERIC_READ, FILE_SHARE_VALID_FLAGS, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  hFile = (hFile == INVALID_HANDLE_VALUE) ? NULL : hFile;
  FIN_IF(!hFile, 0x44001500 | E_EREAD);

  if (m_arcfile.get_type() == atUnknown) {
    hr = m_arcfile.update_type(hFile);
    FIN_IF(hr, hr);
  }
  
  m_status = stInit;
  FIN_IF(m_arcfile.get_type() == atUnknown, 0x44002000 | E_EREAD);

  m_add_name.clear();

  if (m_arcfile.get_type() == atPax) {
    hr = scan_pax_file(hFile);
    FIN(hr);
  }

  if (m_arcfile.get_type() == atPaxZstd) {
    hr = scan_paxzst_file(hFile);
    FIN_IF(!hr, 0);
    WLOGw(L"%S: Incorrect PAXZ archive \"%s\" (err = 0x%X) ", __func__, get_name(), hr);
    clear();
    m_arcfile.set_type(atZstd);
    hr = 0;
  }

  if (m_arcfile.get_type() == atPaxLz4) {
    hr = scan_paxlz4_file(hFile);
    FIN_IF(!hr, 0);
    WLOGw(L"%S: Incorrect PaxLz4 archive \"%s\" (err = 0x%X) ", __func__, get_name(), hr);
    clear();
    m_arcfile.set_type(atLz4);
    hr = 0;
  }

  name.assign(m_arcfile.get_name());
  FIN_IF(name.length() == 0, 0x44003000 | E_EREAD);
  size_t pos = name.rfind(L'.');
  if (pos != bst::npos && pos > 0) {
    name.resize(pos);
    if (name.length() + 4 <= 255) {
      if (m_arcfile.get_tar_format() != tar::UNKNOWN_FORMAT) {
        if (m_arcfile.get_tar_format() == tar::POSIX_FORMAT) {
          name.append(L".pax");
        } else {
          name.append(L".tar");
        }
      }
    }
  }

  UINT64 content_size = 0;
  DWORD file_attr = FILE_ATTRIBUTE_NORMAL | FILE_ATTRIBUTE_ARCHIVE;

  if (m_arcfile.get_type() == atZstd) {
    content_size = 0;
  }
  if (m_arcfile.get_type() == atLz4) {
    hr = get_lz4_content_size(hFile, content_size);
    FIN_IF(hr, hr);
  }

  cache_item * item = add_file_internal(name.c_str(), content_size, file_attr);
  FIN_IF(!item, 0x44003300 | E_EREAD);
  item->info.ctime = m_arcfile.get_ctime();
  item->info.mtime = m_arcfile.get_mtime();

  hr = 0;

fin:
  if (hr) {
    m_status = stZombie;
    LOGe("%s: ERROR = 0x%X ", __func__, hr);
  } else {
    m_status = stReady;
    WLOGd(L"%S: Created fresh cache for \"%s\" <%I64d> [%d/%d/%d] ", __func__, get_name(), get_file_count(), get_type(), get_lz4_format(), get_tar_format());
  }
  if (hFile)
    CloseHandle(hFile);
  return hr;
}

int cache::get_lz4_content_size(HANDLE hFile, UINT64 & content_size)
{
  int hr = 0;
  BYTE buf[LZ4F_HEADER_SIZE_MAX + 64];
  LARGE_INTEGER pos;
  DWORD dw;

  pos.QuadPart = 0;
  BOOL xp = SetFilePointerEx(hFile, pos, NULL, FILE_BEGIN);
  FIN_IF(xp == FALSE, 0x44003400 | E_EREAD);

  size_t rsz = LZ4F_HEADER_SIZE_MAX;
  BOOL x = ReadFile(hFile, buf, (DWORD)rsz, &dw, NULL);
  FIN_IF(!x || dw != rsz, 0x44003500 | E_EREAD);

  lz4::frame_info finfo;
  int data_offset = finfo.init(buf, rsz);
  FIN_IF(data_offset <= 0, 0x44003600 | E_EREAD);

  content_size = finfo.is_unknown_content_size() ? 0 : finfo.contentSize;
  hr = 0;
  
fin:
  return hr;
}

int cache::add_pax_info(tar::pax_decode & pax, UINT64 file_pos, int hdr_pack_size, UINT64 total_size)
{
  int hr = 0;
  
  //LOGi("%s: \"%s\" %d (pax = %Id) %Id", __func__, pax.get_header(), pax.m_format, pax.m_pax_size, pax.m_header_size);
  // TODO: support GNU tar!!!
  FIN_IF(pax.m_format != tar::POSIX_FORMAT, 0x45013100 | E_EOPEN);

  m_add_name.clear();

  bool is_file_dir = (pax.m_type == tar::NORMAL) || (pax.m_type == tar::DIRECTORY);
  if (is_file_dir || pax.is_huge_data_size()) {
    hr = pax.parse_header();
    FIN_IF(hr, hr | E_EOPEN); // 0x45012900 | E_EOPEN);
    if (is_file_dir) {
      size_t nlen = pax.get_name_len();
      LPCSTR name = pax.get_name();
      if (nlen >= 2 && name[0] == '.' && name[1] == '/') {
        nlen -= 2;
        name += 2;
      }
      if (nlen && *name == '/') {
        nlen--;
        name++;
      }
      if (nlen && pax.m_type == tar::DIRECTORY && name[nlen-1] == '/') {
        nlen--;
      }
      if (nlen) {
        int x = MultiByteToWideChar(CP_UTF8, 0, name, (int)nlen, m_add_name.data(), (int)m_add_name.capacity() - 32);
        FIN_IF(x <= 0, 0x45013300 | E_EOPEN);
        FIN_IF((size_t)x >= m_add_name.capacity() - 32, 0x45013400 | E_EOPEN);
        m_add_name.resize(x);
        cache_item * item = add_file_internal(m_add_name.c_str(), pax.m_info.size, pax.m_info.attr.FileAttributes);
        FIN_IF(!item, 0x45013500 | E_EOPEN);
        item->info.ctime = pax.m_info.attr.CreationTime.QuadPart;
        item->info.mtime = pax.m_info.attr.LastWriteTime.QuadPart;
        item->info.pack_size = total_size;
        item->info.pax.pos = file_pos;
        item->info.pax.realsize = pax.m_info.realsize;
        item->info.pax.hdr_p_size = hdr_pack_size;
        item->info.pax.hdr_size = (UINT32)pax.get_header_size();
      }
    }
  }

fin:
  return hr;
}

int cache::scan_pax_file(HANDLE hFile)
{
  const size_t buf_size = 2*1024*1024;
  int hr = 0;
  LARGE_INTEGER pos;
  DWORD dw;
  bst::buf buf;
  tar::pax_decode pax;

  // TODO: support GNU tar!!!
  FIN_IF(m_arcfile.get_tar_format() != tar::POSIX_FORMAT, 0x45010100 | E_EOPEN);

  FIN_IF(!buf.reserve(buf_size + tar::BLOCKSIZE * 2), 0x45010100 | E_EOPEN);
  buf.resize(buf_size);

  pos.QuadPart = 0;
  BOOL xp = SetFilePointerEx(hFile, pos, NULL, FILE_BEGIN);
  FIN_IF(xp == FALSE, 0x45010300 | E_EOPEN);

  UINT64 file_size = m_arcfile.get_size();
  UINT64 read_size = 0;
  
  while (1) {
    UINT64 pad = file_size - read_size;
    size_t sz = tar::BLOCKSIZE;
    if (pad < sz)
      break;

    pax.clear();
    UINT64 hdr_pos = read_size;
    BOOL x = ReadFile(hFile, buf.data(), (DWORD)sz, &dw, NULL);
    FIN_IF(!x || dw != sz, 0x45011100 | E_EOPEN);
    read_size += sz;
    
    hr = tar::check_tar_is_zero(buf.data(), sz);
    if (hr == 1)
      break;

    hr = pax.add_header(buf.data(), sz);
    FIN_IF(hr, 0x45011300 | E_EOPEN);
    if (pax.is_big_header()) {
      FIN_IF(pax.m_header_size >= buf_size, 0x45012100 | E_EOPEN);
      LPBYTE dst = buf.data() + sz;
      sz = pax.m_header_size - sz;
      FIN_IF(pad < pax.m_header_size, 0x45012200 | E_EOPEN);
      BOOL x = ReadFile(hFile, dst, (DWORD)sz, &dw, NULL);
      FIN_IF(!x || dw != sz, 0x45012300 | E_EOPEN);
      read_size += sz;

      hr = pax.add_header(buf.data(), pax.m_header_size);
      FIN_IF(hr, 0x45012500 | E_EOPEN);
    }

    UINT64 frame_size = pax.m_header_size + tar::blocksize_round64(pax.m_info.size);
    hr = add_pax_info(pax, hdr_pos, 0, frame_size);
    FIN_IF(hr, hr | E_EOPEN);

    if (pax.m_info.size) {
      read_size += tar::blocksize_round64(pax.m_info.size);
      if (read_size >= file_size) {
        read_size = file_size;
        break;
      }
      pos.QuadPart = read_size;
      BOOL xp = SetFilePointerEx(hFile, pos, NULL, FILE_BEGIN);
      FIN_IF(xp == FALSE, 0x45015500 | E_EOPEN);
    }  
  }
  m_end = read_size;
  hr = 0;
  
fin:  
  return hr;
}  

int cache::scan_paxlz4_file(HANDLE hFile)
{
  const size_t buf_size = 2*1024*1024;
  int hr = 0;
  LARGE_INTEGER pos;
  DWORD dw;
  bst::buf buf;
  bst::buf dst;
  tar::pax_decode pax;

  FIN_IF(!buf.reserve(buf_size), 0x44010100 | E_EOPEN);
  buf.resize(buf_size);

  FIN_IF(!dst.reserve(buf_size), 0x44010200 | E_EOPEN);
  dst.resize(buf_size);
  
  pos.QuadPart = m_arcfile.get_data_begin();
  BOOL xp = SetFilePointerEx(hFile, pos, NULL, FILE_BEGIN);
  FIN_IF(xp == FALSE, 0x44010300 | E_EOPEN);

  UINT64 file_size = m_arcfile.get_size();
  lz4::frame_info frame;

  const size_t frame_header_size = LZ4F_HEADER_SIZE_MIN + 8 + 4;  /* + ContentSize + BlockHeader */
  while(1) {
    UINT64 hdr_pos = pos.QuadPart;
    UINT64 total_size = 0;
    size_t sz = frame_header_size;

    if ((UINT64)pos.QuadPart + sz > file_size)
      break;
    
    BOOL x = ReadFile(hFile, buf.data(), (DWORD)sz, &dw, NULL);
    FIN_IF(!x || dw != sz, 0x44011100 | E_EOPEN);
    pos.QuadPart += sz;
    total_size += sz;
    
    DWORD magic = *(PDWORD)buf.data();
    if (magic != lz4::LZ4IO_MAGICNUMBER) {
      FIN(0);  // EOF
    }
    int data_offset = frame.init(buf.c_data(), sz);
    FIN_IF(data_offset <= 0, 0x44011200 | E_EOPEN); 
    
    FIN_IF(frame.contentSize < tar::BLOCKSIZE, 0x44011300 | E_EOPEN);
    FIN_IF(frame.contentSize & tar::BLOCKSIZE_MASK, 0x44011400 | E_EOPEN);
    FIN_IF(frame.data_size == 0, 0x44011500 | E_EOPEN);
    FIN_IF(frame.blockChecksumFlag == 0, 0x44011600 | E_EOPEN);
    FIN_IF(frame.contentChecksumFlag == 0, 0x44011700 | E_EOPEN);
    FIN_IF(frame.blockMode != LZ4F_blockIndependent, 0x44011800 | E_EOPEN);
    FIN_IF(frame.blockSizeID != LZ4F_max4MB, 0x44011900 | E_EOPEN);
    FIN_IF(frame.frameType != LZ4F_frame, 0x44012100 | E_EOPEN);
    
    FIN_IF(data_offset != frame_header_size, 0x44012200 | E_EOPEN);
    FIN_IF(frame.data_size > 1*1024*1024, 0x44012500 | E_EOPEN);

    PBYTE pd = buf.data() + frame_header_size;
    sz = frame.data_size + 4 + 4 + 4;  /* + BlockCkSum + EndMark + ContentCkSum */
    FIN_IF((UINT64)pos.QuadPart + sz > file_size, 0x44012600 | E_EOPEN);
    x = ReadFile(hFile, pd, (DWORD)sz, &dw, NULL);
    FIN_IF(!x || dw != (DWORD)sz, 0x44012700 | E_EOPEN);
    pos.QuadPart += sz;
    total_size += sz;

    tar::posix_header * ph = (tar::posix_header *)pd;
    size_t plen = frame.data_size;
    DWORD calc_hash = XXH32(pd, plen, 0);
    DWORD read_hash = *(PDWORD)(pd + plen);
    FIN_IF(read_hash != calc_hash, 0x44013700 | E_EOPEN);

    DWORD end_mark = *(PDWORD)(pd + plen + 4);
    FIN_IF(end_mark != 0, 0x44013800 | E_EOPEN);   /* header frame must contain only one block! */ 

    if (frame.is_compressed) {
      int dsz = lz4::decode_data_partial(pd, plen, dst.data(), dst.size() - tar::BLOCKSIZE, dst.size());
      FIN_IF(dsz <= 0, 0x44014100 | E_EOPEN);
      ph = (tar::posix_header *)dst.data();
      plen = dsz;
    }
    FIN_IF(plen & tar::BLOCKSIZE_MASK, 0x44016100 | E_EOPEN);
    
    int hdr_pack_size = (int)total_size;

    //hr = tar::check_tar_is_zero(ph, plen);
    //if (hr == 1)
    //  break;

    pax.clear();
    hr = pax.add_header(ph, plen);
    FIN_IF(hr, 0x44017100 | E_EOPEN);
    if (pax.is_big_header()) {
      FIN_IF(pax.m_header_size != plen, 0x44017200 | E_EOPEN);
      hr = pax.add_header(ph, plen);
      FIN_IF(hr, 0x44017300 | E_EOPEN);
    }

    if (pax.m_info.size) {
      BYTE fbuf[frame_header_size + 16];
      size_t sz = frame_header_size;
      FIN_IF((UINT64)pos.QuadPart + sz > file_size, 0x44121000 | E_EOPEN);
      BOOL x = ReadFile(hFile, fbuf, (DWORD)sz, &dw, NULL);
      FIN_IF(!x || dw != (DWORD)sz, 0x44122000 | E_EOPEN);
      pos.QuadPart += sz;
      total_size += sz;

      DWORD magic = *(PDWORD)fbuf;
      FIN_IF(magic != lz4::LZ4IO_MAGICNUMBER, 0x44123000 | E_EOPEN);

      int data_offset = frame.init(fbuf, sz);
      FIN_IF(data_offset <= 0, 0x44124000 | E_EOPEN); 

      FIN_IF(frame.contentSize < tar::BLOCKSIZE, 0x44125000 | E_EOPEN);
      FIN_IF(frame.contentSize & tar::BLOCKSIZE_MASK, 0x44126000 | E_EOPEN);
      FIN_IF(frame.data_size == 0, 0x44127000 | E_EOPEN);
      FIN_IF(frame.blockChecksumFlag == 0, 0x44128000 | E_EOPEN);
      FIN_IF(frame.contentChecksumFlag == 0, 0x44129000 | E_EOPEN);
      FIN_IF(frame.blockMode != LZ4F_blockIndependent, 0x44130000 | E_EOPEN);
      FIN_IF(frame.blockSizeID != LZ4F_max4MB, 0x44131000 | E_EOPEN);
      FIN_IF(frame.frameType != LZ4F_frame, 0x44132000 | E_EOPEN);
      FIN_IF(data_offset != frame_header_size, 0x44133000 | E_EOPEN);

      DWORD blksize = *(PDWORD)(fbuf + frame_header_size - 4);
      FIN_IF(blksize == 0, 0x44134000 | E_EOPEN);

      while(1) {
        if (blksize == 0) {
          DWORD contentCkSum;
          BOOL x = ReadFile(hFile, &contentCkSum, sizeof(contentCkSum), &dw, NULL);
          FIN_IF(!x || dw != sizeof(blksize), 0x44142000 | E_EOPEN);
          pos.QuadPart += sizeof(contentCkSum);
          total_size += sizeof(contentCkSum);
          break;
        }
        size_t sz = (blksize & (~lz4::BLOCKUNCOMPRES_FLAG)) + 4;  /* + BlockCkSum */
        pos.QuadPart += sz;
        BOOL xp = SetFilePointerEx(hFile, pos, NULL, FILE_BEGIN);
        FIN_IF(xp == FALSE, 0x44143000 | E_EOPEN);
        total_size += sz;

        BOOL x = ReadFile(hFile, &blksize, sizeof(blksize), &dw, NULL);
        FIN_IF(!x || dw != sizeof(blksize), 0x44144000 | E_EOPEN);
        pos.QuadPart += sizeof(blksize);
        total_size += sizeof(blksize);
      }
    }

    hr = add_pax_info(pax, hdr_pos, hdr_pack_size, total_size);
    FIN_IF(hr, hr | E_EOPEN);
  }

  hr = 0;
  
fin:
  return hr;
}

int cache::scan_paxzst_file(HANDLE hFile)
{
  int hr = 0;
  union {
    LARGE_INTEGER fpos;
    UINT64 pos;
  };
  size_t nbReaded = 0;;
  bst::buf buf;
  bst::buf dst;
  tar::pax_decode pax;
  zst::decode_context dctx;

  const size_t buf_size = 2*1024*1024;

  FIN_IF(!buf.reserve(buf_size), 0x74010100 | E_EOPEN);
  buf.resize(buf_size);

  FIN_IF(!dst.reserve(buf_size), 0x74102000 | E_EOPEN);
  dst.resize(buf_size);

  pos = m_arcfile.get_data_begin();
  BOOL xp = SetFilePointerEx(hFile, fpos, NULL, FILE_BEGIN);
  FIN_IF(xp == FALSE, 0x74103000 | E_EOPEN);

  UINT64 file_size = m_arcfile.get_size();
  zst::frame_info frame;

  const size_t frame_header_size = zst::FRAMEHEADER_SIZE_MIN + zst::BLOCKHEADERSIZE;  /* + BlockHeader */
  while(1) {
    UINT64 hdr_pos = pos;
    BOOL x = ReadFile(hFile, buf.data(), 4, (PDWORD)&nbReaded, NULL);
    FIN_IF(!x || nbReaded != 4, 0x74104000 | E_EOPEN);
    pos += 4;
    DWORD magic = *(PDWORD)buf.data();
    FIN_IF(magic != zst::MAGICNUMBER && magic != paxz::FRAME_MAGIC, 0x74104300 | E_EOPEN);
    if (magic == paxz::FRAME_MAGIC) {
      paxz::frame_end endframe;
      endframe.magic = magic;
      size_t sz = sizeof(paxz::frame_end) - sizeof(endframe.magic);
      BOOL x = ReadFile(hFile, &endframe.size, (DWORD)sz, (PDWORD)&nbReaded, NULL);
      FIN_IF(!x || nbReaded != sz, 0x74104700 | E_EOPEN);
      pos += sz;
      FIN_IF(endframe.is_valid() == false, 0x74104900 | E_EOPEN);
      FIN(0);   /* end of PAXZ */
    }
    int fsz = frame.read_frame(hFile, buf.data(), buf.size(), true);
    FIN_IF(fsz <= 0, 0x74107500 | E_EOPEN);
    pos += fsz - 4;   /* magic size already added */
    int hdr_pack_size = fsz;
    FIN_IF(frame.get_type() != ZSTD_frame, 0x74112100 | E_EOPEN);
    FIN_IF(frame.is_unknown_content_size(), 0x74112200 | E_EOPEN);
    FIN_IF(frame.get_content_size() < tar::BLOCKSIZE, 0x74113000 | E_EOPEN);
    FIN_IF(frame.get_content_size() & tar::BLOCKSIZE_MASK, 0x74114000 | E_EOPEN);
    FIN_IF(frame.get_content_size() >= buf_size, 0x74114500 | E_EOPEN);
    FIN_IF(frame.m_block.size == 0, 0x74115000 | E_EOPEN);
    FIN_IF(frame.m_header.checksumFlag == 0, 0x74116000 | E_EOPEN);

    int dsz = dctx.frame_decode(buf.data(), fsz, dst.data(), dst.size());
    FIN_IF(dsz <= 0, 0x74117100 | E_EOPEN);
    size_t sz = (size_t)dsz;
    FIN_IF(sz != (size_t)frame.get_content_size(), 0x74117200 | E_EOPEN);

    pax.clear();
    hr = pax.add_header(dst.data(), sz);
    FIN_IF(hr, 0x74121000 | E_EOPEN);
    if (pax.is_big_header()) {
      hr = pax.add_header(dst.data(), sz);
      FIN_IF(hr, 0x74133000 | E_EOPEN);
    }
    UINT64 frame_size = 0;
    if (pax.m_info.size) {
      hr = frame.read_frame(hFile, &frame_size, false);
      FIN_IF(hr, 0x74143000 | E_EOPEN);
      pos += frame_size;
    }
    hr = add_pax_info(pax, hdr_pos, hdr_pack_size, frame_size + hdr_pack_size);
    FIN_IF(hr, hr | E_EOPEN);
  }

  hr = 0;

fin:
  return hr;
}

// =============================================================================================

int cache::add_ref()
{
  return m_list.add_ref(m_index);
}

int cache::release()
{
  update_release_time();
  return m_list.release(m_index);
}

// =============================================================================================

cache_list::cache_list()
{
  m_main_thread_id = 0;
  m_inicfg = NULL;
  memset(m_refcount, 0, sizeof(m_refcount));
  memset(m_object, 0, sizeof(m_object));
  m_count = 0;
}

cache_list::~cache_list()
{
  clear();
}

int cache_list::init(DWORD main_thread_id, wcx::inicfg & inicfg)
{
  m_main_thread_id = main_thread_id;
  m_inicfg = &inicfg;
  return 0;
} 

int cache_list::clear()
{
  bst::scoped_write_lock lock(m_mutex);
  for (size_t i = 1; i < cache_max_items; i++) {
    wcx::cache * obj = m_object[i];
    if (m_refcount[i] <= 0) {
      if (obj) {
        delete(obj);
        m_object[i] = NULL;
      }
      m_refcount[i] = 0;
    } else {
      WLOGe(L"%S: ERROR: can't delete cache object!!! <%d> \"%s\" ", __func__, m_refcount[i], obj ? obj->get_name() : NULL);
    }
  }
  return 0;
}

int cache_list::release(size_t index)
{
  if (!index || index >= cache_max_items)
    return -1001;
  
  bst::scoped_write_lock lock(m_mutex);
  int refcnt = m_refcount[index];
  if (refcnt > 0) {
    --m_refcount[index];
    --refcnt;
  }
  return refcnt;
}

int cache_list::release(wcx::cache * obj)
{
  return obj ? release(obj->m_index) : -1003;
}

int cache_list::add_ref(size_t index)
{
  if (!index || index >= cache_max_items)
    return -1011;

  bst::scoped_write_lock lock(m_mutex);
  if (m_object[index] == NULL)
    return -1012;
  return ++m_refcount[index];
}

int cache_list::add_ref(wcx::cache * obj)
{
  return obj ? release(obj->m_index) : -1013;
}

size_t cache_list::get_free_item()
{
  for (size_t i = 1; i < cache_max_items; i++) {
    if (m_object[i] == NULL && m_refcount[i] == 0)
      return i;
  }
  return 0;  
}

int cache_list::create(const wcx::arcfile & af)
{
  int hr = 0;
  size_t index = 0;
  wcx::cache * cache = NULL;
  bool cache_inited = false;

  FIN_IF(af.get_size() == 0, -20011);
  FIN_IF(af.get_type() == atUnknown, -20012);
  index = get_free_item();
  FIN_IF(index == 0, 0);
  cache = new(bst::nothrow) wcx::cache(*this, index);
  FIN_IF(!cache, -20014);
  cache->set_case_sensitive(true);
  cache_inited = cache->set_arcfile(af);
  FIN_IF(!cache_inited, -20015);

  m_refcount[index] = 0;
  m_object[index] = cache;  
  m_count++;
  return (int)index;

fin:
  LOGe_IF(hr, "%s: ERROR = %d ", __func__, hr);
  if (cache)
    delete cache;
  return hr;
}

int cache_list::get_cache(const wcx::arcfile & arcfile, bool addref, wcx::cache ** p_cache)
{
  int hr = 0;
  wcx::cache * cache = NULL;
  wcx::cache * new_cache = NULL;
  bool release_on_err = false;
  
  { /* enter exclusive lock */
    bst::scoped_write_lock lock(m_mutex);
    for (size_t i = 1; i < cache_max_items; i++) {
      wcx::cache * obj = m_object[i];
      if (obj && obj->is_same_file(arcfile)) {
        cache = obj;
        break;  // archive file already exist in cache list
      }  
    }
    if (cache) {
      while (cache->m_status < wcx::cache::stReady) {   // wait building cache into other WCX thread
        Sleep(10);
      }
      if (cache->m_status == wcx::cache::stReady) {
        wcx::arcfile & af = cache->get_arcfile();
        if (af.is_same_attr(arcfile)) {
          if (addref) {
            m_refcount[cache->m_index]++;
            release_on_err = true;
          }
        } else {
          cache->set_status(wcx::cache::stZombie);    /* signal to delete not actual cache object */
          cache = NULL;
        }
      } else {
        cache = NULL;
      }
    }
    if (cache == NULL) {
      hr = create(arcfile);
      FIN_IF(hr <= 0, 0x30006000 | E_NO_MEMORY);
      new_cache = m_object[hr];
      if (addref) {
        m_refcount[hr]++;
        release_on_err = true;
      }
      cache = new_cache;
    }
  } /* exit from lock */

  hr = 0;
  
  if (new_cache) {
    hr = new_cache->init();
    FIN_IF(hr, hr);
  }
  
  *p_cache = cache;
  hr = 0;
  
fin:
  if (hr) {
    LOGe("%s: ERROR = 0x%X ", __func__, hr);
    if (new_cache)
      new_cache->set_status(wcx::cache::stZombie);
    if (release_on_err)
      cache->release();
  }
  return hr;
}

UINT64 cache_list::get_cache_size()
{
  UINT64 total_size = 0;
  bst::scoped_read_lock lock(m_mutex);
  for (size_t i = 1; i < cache_max_items; i++) {
    wcx::cache * obj = m_object[i];
    if (obj) {
      total_size += obj->get_capacity() + sizeof(wcx::cache);
    }
  }
  return total_size;
}

BST_INLINE
static SSIZE_T get_time_diff_min(INT64 prev, INT64 now)
{
  return (SSIZE_T)((now - prev) / (10000000L * 60L));
}

int cache_list::delete_zombies()
{
  INT64 cur_time;

  if (GetCurrentThreadId() != m_main_thread_id)
    return 0;
  
  SSIZE_T cache_lifetime = 20; // minutes
  if (m_inicfg) {
    wcx::cfg cfg = m_inicfg->get_cfg();
    cache_lifetime = cfg.get_cache_lifetime();
  }

  bst::scoped_write_lock lock(m_mutex);
  GetSystemTimeAsFileTime((LPFILETIME)&cur_time);  
  int count = 0;
  for (size_t i = 1; i < cache_max_items; i++) {
    wcx::cache * obj = m_object[i];
    if (m_refcount[i] == 0 && obj) {
      if (obj->m_status == wcx::cache::stZombie || get_time_diff_min(obj->m_last_used_time, cur_time) >= cache_lifetime) {
        WLOGi(L"%S: delete zombie cache item \"%s\" ", __func__, obj->get_name());
        delete(obj);
        m_object[i] = NULL;
        continue;
      }      
    }
    if (obj)
      count++;
  }
  m_count = count;
  return 0;
}  


} /* namespace */





