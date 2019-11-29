#include "stdafx.h"
#include "wcx_arcfile.h"
#include "lz4lib.h"


namespace wcx {

arcfile::arcfile()
{
  clear();
}

arcfile::~arcfile()
{
  //
}

void arcfile::clear()
{
  m_init_mode = imName;
  m_name = NULL;
  m_ArcName.clear();
  m_fullname.clear();
  m_size = 0;
  m_ctime = 0;
  m_mtime = 0;
  m_volumeid = 0;
  m_fileid = 0;
  m_type = atUnknown;
  m_lz4format = lz4::ffUnknown;
  m_tarformat = tar::UNKNOWN_FORMAT;
  memset(&m_paxz, 0, sizeof(m_paxz));
  m_data_begin = 0;
}

arcfile::arcfile(const arcfile & af)
{
  if (!assign(af))
    clear();
}

bool arcfile::assign(const arcfile & af)
{
  m_ArcName = af.m_ArcName;
  m_fullname = af.m_fullname;
  m_name = wcsrchr(m_ArcName.c_str(), L'\\');
  if (!m_name)
    return false; // m_name = m_ArcName.c_str();
  else
    m_name++;
  m_size = af.m_size;
  m_ctime = af.m_ctime;
  m_mtime = af.m_mtime;
  m_volumeid = af.m_volumeid;
  m_fileid = af.m_fileid;
  m_type = af.m_type;
  m_lz4format = af.m_lz4format;
  m_tarformat = af.m_tarformat;
  m_paxz = af.m_paxz;
  m_data_begin = af.m_data_begin;
  return true;
} 

int arcfile::init(LPCWSTR arcName, InitMode mode)
{
  int hr = 0;
  HANDLE hFile = NULL;

  m_init_mode = mode;
  size_t len = wcslen(arcName);
  FIN_IF(len <= 2, 0x6001000 | E_BAD_ARCHIVE);

  m_ArcName.assign(arcName, len);
  FIN_IF(m_ArcName.has_error(), 0x6011000 | E_NO_MEMORY);

  m_name = wcsrchr(m_ArcName.c_str(), L'\\');
  FIN_IF(!m_name, 0x6021000 | E_BAD_ARCHIVE);
  m_name++;
  FIN_IF(wcslen(m_name) == 0, 0x6031000 | E_BAD_ARCHIVE);

  m_fullname.clear();
  hr = get_full_filename(arcName, m_fullname);
  FIN_IF(hr, 0x6051000 | E_NO_FILES);
  
  if (mode != imName) {
    hFile = CreateFileW(m_fullname.c_str(), GENERIC_READ, FILE_SHARE_VALID_FLAGS, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    hFile = (hFile == INVALID_HANDLE_VALUE) ? NULL : hFile;
    FIN_IF(!hFile, 0x6055000 | E_EOPEN);

    hr = update_attr(hFile);
    FIN_IF(hr, hr);
    
    if (mode & imType) {
      hr = update_type(hFile);
      FIN_IF(hr, hr);
    }
  }
  
fin:
  if (hFile)
    CloseHandle(hFile);
  if (hr)
    clear();
  return hr;
}

int arcfile::update_attr(HANDLE hFile)
{
  int hr = 0;
  BOOL x;
  FILE_BASIC_INFO fbi;

  m_size = 0;
  m_ctime = 0;
  m_mtime = 0;
  m_fileid = 0;
  m_volumeid = 0;

  x = GetFileSizeEx(hFile, (PLARGE_INTEGER)&m_size);
  FIN_IF(!x, 0x6066000 | E_EOPEN);
  
  x = nt::GetFileInformationByHandleEx(hFile, FileBasicInfo, &fbi, sizeof(fbi));
  FIN_IF(!x, 0x6065000 | E_EOPEN);
  m_ctime = fbi.CreationTime.QuadPart;
  m_mtime = fbi.LastWriteTime.QuadPart;

  nt::GetFileIdByHandle(hFile, &m_fileid);
  nt::GetVolumeIdByHandle(hFile, &m_volumeid);
  hr = 0;

fin:
  return hr;
}

bool arcfile::is_same_file(const arcfile & af)
{
  if (m_volumeid && m_fileid) {
    if (m_fileid == af.m_fileid && m_volumeid == af.m_volumeid)
      return true;
  }
  if (m_ArcName.length() == af.m_ArcName.length()) {
    if (wcscmp(m_ArcName.c_str(), af.m_ArcName.c_str()) == 0)
      return true;
  }
  return false;
}

bool arcfile::is_same_attr(const arcfile & af)
{
  if (m_size != af.m_size)
    return false;
  if (m_ctime != af.m_ctime)
    return false;
  if (m_mtime != af.m_mtime)
    return false;
  return true;
}

int arcfile::update_type(HANDLE hFile)
{
  const size_t reserve_size = 256;
  int hr = 0;
  HANDLE hArcFile = NULL;
  BYTE buf[tar::BLOCKSIZE * 2];
  BYTE dst[tar::BLOCKSIZE * 2 + reserve_size];
  LARGE_INTEGER pos;
  DWORD dw;
  BOOL x;
  
  m_type = atUnknown;
  m_lz4format = lz4::ffUnknown;
  m_tarformat = tar::UNKNOWN_FORMAT;
    
  if (!hFile) {
    hArcFile = CreateFileW(m_fullname.c_str(), GENERIC_READ, FILE_SHARE_VALID_FLAGS, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    hArcFile = (hArcFile == INVALID_HANDLE_VALUE) ? NULL : hArcFile;
    FIN_IF(!hArcFile, 0x6075000 | E_EOPEN);
    hFile = hArcFile;
  }
  if (m_size == 0) {
    BOOL x = GetFileSizeEx(hFile, (PLARGE_INTEGER)&m_size);
    FIN_IF(!x, 0x6076000 | E_EOPEN);
  }
  FIN_IF(m_size < LZ4F_HEADER_SIZE_MIN, 0x6076000 | E_EOPEN);
  
  pos.QuadPart = 0;
  dw = SetFilePointerEx(hFile, pos, NULL, FILE_BEGIN);
  FIN_IF(dw == INVALID_SET_FILE_POINTER, 0x6077000 | E_EOPEN);

  size_t rsz = (m_size > sizeof(buf)) ? sizeof(buf) : (size_t)m_size;
  x = ReadFile(hFile, buf, (DWORD)rsz, &dw, NULL);
  FIN_IF(!x, 0x6078000 | E_EOPEN);
  FIN_IF(dw != rsz, 0x6079000 | E_EOPEN);

  if (rsz >= tar::BLOCKSIZE) {
    hr = tar::check_tar_header(buf, rsz, true); 
    if (hr > 0) {
      m_type = atPax;
      m_tarformat = (tar::Format)hr;
      FIN(0);
    }
  }

  DWORD magic = *(PDWORD)buf;
  if (lz4::is_legacy_frame(magic)) {
    m_type = atLz4;
    m_lz4format = lz4::ffLegacy;
    FIN(0);
  }

  // TODO: may be also check LZ4 skippable frames ?
  FIN_IF(!lz4::check_frame_magic(magic), 0x6081100 | E_EOPEN);

  lz4::frame_info finfo;
  int data_offset = lz4::get_frame_info(buf, rsz, &finfo);
  FIN_IF(data_offset <= 0, 0x6081200 | E_EOPEN);
  FIN_IF(finfo.frameType != LZ4F_frame, 0);

  m_type = atLz4;
  m_lz4format = lz4::ffActual;
  
  if (finfo.data_size) {
    int sz = 0;
    int srcSize = (int)rsz - data_offset;
    if (finfo.data_size < (size_t)srcSize)
      srcSize = (int)finfo.data_size;
    if (finfo.is_compressed) { 
      sz = lz4::decode_data_partial(buf + data_offset, srcSize, dst, sizeof(dst) - reserve_size, sizeof(dst));
      FIN_IF(sz <= 0, 0);
    } else {
      sz = (int)tar::BLOCKSIZE;
      FIN_IF(sz > srcSize, 0);
      memcpy(dst, buf + data_offset, sz);
    }
    FIN_IF(sz < tar::BLOCKSIZE, 0);
    
    hr = tar::check_tar_header(dst, sz, true);
    FIN_IF(hr <= 0, 0);
    
    m_tarformat = (tar::Format)hr;
  } else {
    FIN_IF(finfo.blockChecksumFlag == 0, 0);
    FIN_IF(finfo.contentChecksumFlag == 0, 0);
    FIN_IF(data_offset + sizeof(DWORD) > rsz, 0);
    DWORD frame_hash = *(PDWORD)(buf + data_offset);
    FIN_IF(frame_hash != 0x02CC5D05, 0);    // hash for zero content
    data_offset += sizeof(DWORD);

    FIN_IF(data_offset + sizeof(paxz::frame_root) > rsz, 0);
    memcpy(&m_paxz, buf + data_offset, sizeof(paxz::frame_root));

    FIN_IF(m_paxz.is_valid(0) == false, 0);
    FIN_IF(m_paxz.version != 0, 0);
    FIN_IF(m_paxz.cipher_algo, 0);   // TODO: support cipher ChaCha20
    FIN_IF(m_paxz.pbkdf2_iter, 0);   // TODO: support PBKDF2 algo
    FIN_IF(m_paxz.flags, 0);
    
    m_type = atPaxLz4;
    m_tarformat = (tar::Format)tar::POSIX_FORMAT;  /* PAX */
    m_data_begin = data_offset + sizeof(paxz::frame_root);
  }
  hr = 0;

fin:
  if (hArcFile)
    CloseHandle(hArcFile);

  return hr;
}


} /* namespace */
