#include "stdafx.h"
#include "tar.h"
#include <ctime>


#ifndef TAR_INLINE
#define TAR_INLINE __forceinline
#endif

#ifndef TAR_NOINLINE
#define TAR_NOINLINE __declspec(noinline)
#endif

namespace tar {

/* Log base 2 of common values.  */
const unsigned LG_8 = 3;
const unsigned LG_64 = 6;
const unsigned LG_256 = 8;

#ifdef ISOCTAL
#undef ISOCTAL
#endif
#define ISOCTAL(c) ((c)>='0' && (c)<='7')

#ifdef ISDIGIT
#undef ISDIGIT
#endif
#define ISDIGIT(c) ((c)>='0' && (c)<='9')

TAR_INLINE
static void to_octal(UINT64 v, char * buf, size_t size)
{
  size_t i = size;
  do {
    buf[--i] = '0' + (v & ((1 << LG_8) - 1));
    v >>= LG_8;
  } while (i);
}

static const size_t INT32_STRSIZE = 16;
static const size_t INT_STRSIZE = INT32_STRSIZE;

TAR_INLINE
static LPSTR to_decimal(UINT32 v, char buf[INT_STRSIZE], size_t * size = NULL)
{
  size_t s = 0;
  LPSTR p = buf + INT_STRSIZE - 1;
  *p = 0;  
  do {
    s++;
    *--p = '0' + v % 10;
  } while ((v /= 10) != 0);
  if (size)
    *size = s;
  return p;
}

static const size_t INT64_STRSIZE = 32;

TAR_INLINE
static LPSTR to_decimal64(UINT64 v, char buf[INT64_STRSIZE], size_t * size = NULL)
{
  size_t s = 0;
  LPSTR p = buf + INT64_STRSIZE - 1;
  *p = 0;  
  do {
    s++;
    *--p = '0' + v % 10;
  } while ((v /= 10) != 0);
  if (size)
    *size = s;
  return p;
}

static const int UINTMAX_STRSIZE_BOUND = 32;
static const int BILLION = 1000000000;
static const int LOG10_BILLION = 9;
static const int TIMESPEC_STRSIZE_BOUND = UINTMAX_STRSIZE_BOUND + LOG10_BILLION + sizeof("-.") - 1;

TAR_INLINE
void code_ns_fraction(UINT ns, size_t precision, char * p)
{
  if (ns == 0) {
    *p = 0;
    return;
  }  
  size_t i = precision;
  *p++ = '.';
  while (ns % 10 == 0) {
    ns /= 10;
    i--;
  }
  p[i] = 0;
  for (;;) {
    p[--i] = '0' + ns % 10;
    if (i == 0)
      break;
    ns /= 10;
  }
}

TAR_INLINE
char * code_time(INT64 sec, UINT nsec, size_t precision, char sbuf[TIMESPEC_STRSIZE_BOUND])
{
  char * np;
  char * frac;
  size_t sz;
  bool negative = sec < 0;

  if (nsec >= BILLION)
    nsec = 0;

  if (negative && nsec != 0) {
    sec++;
    nsec = BILLION - nsec;
  }
  np = to_decimal64(negative ? (UINT64)(-sec) : (UINT64)sec, sbuf, &sz);
  frac = np + sz;
  if (negative)
    *--np = '-';
  code_ns_fraction(nsec, precision, frac);
  return np;
} 

static const INT64 DELTA_EPOCH_IN_SECS = 11644473600LL;
static const INT   SECS_TO_100NANOSECS = 10000000L;
static const INT   WINDOWS_TICK = SECS_TO_100NANOSECS;

static LPSTR code_filetime(INT64 win_time, char sbuf[TIMESPEC_STRSIZE_BOUND])
{
  win_time -= DELTA_EPOCH_IN_SECS * WINDOWS_TICK;
  INT64 sec = win_time / WINDOWS_TICK;
  UINT nsec = (UINT)(win_time % WINDOWS_TICK);
  return code_time(sec, nsec, 7, sbuf);
}

TAR_INLINE
static LPSTR code_filetime(FILETIME * ft, char sbuf[TIMESPEC_STRSIZE_BOUND])
{
  INT64 t = *(INT64 *)ft;
  return code_filetime(t, sbuf);
}

TAR_INLINE
static UINT64 filetime_to_epoch(INT64 win_time)
{
  if (win_time <= DELTA_EPOCH_IN_SECS * WINDOWS_TICK)
    return 0;
  return ((UINT64)win_time - (UINT64)DELTA_EPOCH_IN_SECS * WINDOWS_TICK) / (UINT)WINDOWS_TICK;
}

TAR_INLINE
static bool ignore_field_size(file_type type)
{
  if (type == HARDLINK || type == SYMLINK || type == DIRECTORY || type == FIFO)
    return true;
  return false;
}



// ====================================================================================================

class xheader
{
public:
  int init(LPVOID buf, size_t bufsize);
  int add_param_str(LPCSTR keyword, LPCSTR value, size_t value_len = 0);
  int add_path(LPCSTR path, size_t path_len = 0);
  int add_param_time(LPCSTR keyword, FILETIME * t);
  int add_param_time(LPCSTR keyword, LARGE_INTEGER * t);
  int add_size(UINT64 size);
  int add_realsize(UINT64 size);
  int add_attr(DWORD attr);
  size_t get_data_size() { return (size_t)m_pos - (size_t)m_buf; }
  size_t get_used_size(bool with_header = true) { return with_header ? BLOCKSIZE + m_used : m_used; }

public:
  struct posix_header * m_header;
  LPSTR   m_buf;
  size_t  m_buf_cap;
  LPSTR   m_pos;
  size_t  m_used;
};

int xheader::init(LPVOID buf, size_t bufsize)
{
  if (bufsize < 3 * BLOCKSIZE)
    return -1;
  m_header = (struct posix_header *)buf;
  m_buf = (LPSTR)m_header + BLOCKSIZE;
  m_buf_cap = (bufsize & ~BLOCKSIZE_MASK) - (BLOCKSIZE * 2);
  m_pos = m_buf;
  m_used = BLOCKSIZE;
  memset(m_header, 0, BLOCKSIZE * 2);
  return 0;  
}

// "%d <keyword>=<value>/n"
int xheader::add_param_str(LPCSTR keyword, LPCSTR value, size_t value_len)
{
  char nbuf[INT_STRSIZE];
  size_t klen = strlen(keyword);
  value_len = value_len ? value_len : strlen(value);
  size_t len = klen + value_len + 3; /* ' ' + '=' + '\n' */
  char * np;
  size_t sz;
  size_t szn = 1;
  while (1) {
    np = to_decimal((UINT32)(len + szn), nbuf, &sz);
    if (sz == szn)
      break;
    szn = (szn == 1) ? sz : szn + 1;
  }
  len += sz;
  size_t offset = (size_t)m_pos - (size_t)m_buf;
  if (offset + len > m_used) {
    size_t usize = tar::blocksize_round(offset + len);
    if (usize >= m_buf_cap)
      return -1;   // critical error
    memset(m_buf + m_used, 0, usize - m_used);
    m_used = usize;
  }  
  memcpy(m_pos, np, sz);
  m_pos += sz;
  *m_pos++ = ' ';
  memcpy(m_pos, keyword, klen);
  m_pos += klen;
  *m_pos++ = '=';
  if (value_len) {
    memcpy(m_pos, value, value_len);
    m_pos += value_len;
  }
  *m_pos++ = '\n';
  return 0;
}

int xheader::add_path(LPCSTR path, size_t path_len)
{
  return add_param_str("path", path, path_len);
}

int xheader::add_param_time(LPCSTR keyword, LARGE_INTEGER * t)
{
  char nbuf[TIMESPEC_STRSIZE_BOUND];
  LPSTR np = code_filetime(t->QuadPart, nbuf);
  return add_param_str(keyword, np);
}

int xheader::add_param_time(LPCSTR keyword, FILETIME * t)
{
  return add_param_time(keyword, (LARGE_INTEGER *)t);
}

int xheader::add_size(UINT64 size)
{
  char nbuf[INT64_STRSIZE];
  size_t nlen;
  LPSTR np = to_decimal64(size, nbuf, &nlen);
  return add_param_str("size", np, nlen);
}

int xheader::add_realsize(UINT64 size)
{
  char nbuf[INT64_STRSIZE];
  size_t nlen;
  LPSTR np = to_decimal64(size, nbuf, &nlen);
  return add_param_str("SCHILY.realsize", np, nlen);
}

int xheader::add_attr(DWORD attr)
{
  char buf[300];
  LPSTR p = buf;
  if (attr & FILE_ATTRIBUTE_READONLY) {
    memcpy(p, "readonly,", 9);
    p += 9;
  }
  if (attr & FILE_ATTRIBUTE_HIDDEN) {
    memcpy(p, "hidden,", 7);
    p += 7;
  }
  if (attr & FILE_ATTRIBUTE_SYSTEM) {
    memcpy(p, "system,", 7);
    p += 7;
  }
  if (attr & FILE_ATTRIBUTE_ARCHIVE) {
    memcpy(p, "archived,", 9);
    p += 9;
  }
  if (attr & FILE_ATTRIBUTE_DEVICE) {
    memcpy(p, "topdir,", 7);
    p += 7;
  }
  if (attr & FILE_ATTRIBUTE_SPARSE_FILE) {
    memcpy(p, "sparse,", 7);
    p += 7;
  }
  if (attr & FILE_ATTRIBUTE_REPARSE_POINT) {
    memcpy(p, "reparse,", 8);
    p += 8;
  }
  if (attr & FILE_ATTRIBUTE_COMPRESSED) {
    memcpy(p, "compressed,", 11);
    p += 11;
  }
  if (p == buf)
    return 0;
  *--p = 0;
  size_t len = (size_t)p - (size_t)buf;  
  return add_param_str("SCHILY.fflags", buf, len);
}

// ====================================================================================================

pax_encode::pax_encode()
{
  m_attr_time = (AttrTime)0x7FFFFFFF;
  m_attr_file = (AttrFile)0x7FFFFFFF;
}

pax_encode::~pax_encode()
{
  //
}

void pax_encode::set_ext_buf(LPVOID buf, size_t size)
{
  m_ext_buf.ptr = buf;
  m_ext_buf.size = size;
}

// fullname must be "./<dir1>/<dir2>/<fname>"
int pax_encode::set_tar_name(struct posix_header * ph, char type, bool is_dir, LPCSTR fullname, size_t fullname_len, LPCSTR fname)
{
  size_t v1, vh, v2;
  size_t nlen = (fullname_len) ? fullname_len : strlen(fullname);

  if (type == XHDTYPE) {
    if (!fname) {
      for (size_t i = nlen-1; i != 0; i--) {
        if (fullname[i-1] == '/') {
          fname = &fullname[i];
          break;
        }
      }
    }
    LPCSTR p = fname - 1;
    LPSTR name = ph->name;
    v1 = (size_t)(p - fullname);
    if (v1 >= sizeof(ph->name)) {
      v1 = sizeof(ph->name);
      memcpy(name, fullname, v1);
      name += v1;
    } else {
      memcpy(name, fullname, v1);
      name += v1;
      vh = 11; // strlen("/PaxHeaders");
      memcpy(name, "/PaxHeaders", vh);
      name += vh;
      v2 = nlen - v1;
      if (v2 >= sizeof(ph->name) - vh)
        v2 = sizeof(ph->name) - vh;
      memcpy(name, p, v2);
      name += v2;
    }    
    name[0] = 0;
    if (name[-1] == '/')
      name[-1] = 0;
  } else {
    if (nlen >= sizeof(ph->name)) {
      memcpy(ph->name, fullname, sizeof(ph->name));
    } else {
      memcpy(ph->name, fullname, nlen+1);
    }  
  }
  ph->name[sizeof(ph->name)-1] = 0;
  return 0;
}

int pax_encode::set_tar(struct posix_header * ph, char type, UINT64 size, fileinfo * fi)
{
  UINT mode = 0;
  if (type == XHDTYPE) {
    mode = DEF_FILE_MODE;     // directories "/PaxHeaders/" is always content files
  } else {
    mode = (type == DIRECTORY) ? DEF_DIR_MODE : DEF_FILE_MODE;
    if (fi->attr.FileAttributes & FILE_ATTRIBUTE_READONLY) {
      mode &= ~(TUWRITE | TGWRITE | TOWRITE);
    }
  }  
  to_octal(mode, ph->mode, sizeof(ph->mode) - 1);
  ph->mode[sizeof(ph->mode) - 1] = 0;

  memset(ph->uid, '0', sizeof(ph->uid) - 1);
  ph->uid[sizeof(ph->uid) - 1] = 0;

  memset(ph->gid, '0', sizeof(ph->gid) - 1);
  ph->gid[sizeof(ph->gid) - 1] = 0;

  if (size > OLD_SIZE_LIMIT) {
    to_octal(OLD_SIZE_LIMIT, ph->size, sizeof(ph->size) - 1);
  } else {
    to_octal(size, ph->size, sizeof(ph->size) - 1);
  }
  ph->size[sizeof(ph->size) - 1] = 0;
  
  if (fi->attr.LastWriteTime.QuadPart == _I64_MAX) {
    to_octal(0, ph->mtime, sizeof(ph->mtime) - 1);
  } else {
    UINT64 mtime = filetime_to_epoch(fi->attr.LastWriteTime.QuadPart);
    to_octal(mtime, ph->mtime, sizeof(ph->mtime) - 1);
  }
  ph->mtime[sizeof(ph->mtime) - 1] = 0;

  ph->typeflag = type;

  memcpy(ph->magic, TMAGIC, TMAGLEN);
  memcpy(ph->version, TVERSION, TVERSLEN);

  //to_octal(0, ph->devmajor, sizeof(ph->devmajor) - 1);
  memset(ph->devmajor, '0', sizeof(ph->devmajor) - 1);
  ph->devmajor[sizeof(ph->devmajor) - 1] = 0;
  
  //to_octal(0, ph->devminor, sizeof(ph->devminor) - 1);
  memset(ph->devminor, '0', sizeof(ph->devminor) - 1);
  ph->devminor[sizeof(ph->devminor) - 1] = 0;

  return 0;
}

int pax_encode::set_tar_chksum(struct posix_header * ph, char type)
{
  memset(ph->chksum, 0x20, sizeof(ph->chksum));
  UINT sum = 0;
  PBYTE p = (PBYTE)ph;
  for (size_t i = BLOCKSIZE; i-- != 0; ) {
    sum += *p++;
  }
  to_octal(sum, ph->chksum, sizeof(ph->chksum) - 1);
  ph->chksum[sizeof(ph->chksum) - 1] = 0;
  return 0;
}

int pax_encode::create_header(HANDLE hFile, fileinfo * fi, LPCWSTR fullFilename, LPCWSTR fn, void * buf, size_t & buf_size)
{
  int hr = -1;
  xheader xh;
  BOOL x;
  size_t fn_len;

  m_ext_buf.ptr = buf;
  m_ext_buf.size = buf_size;
  FIN_IF(buf_size < BLOCKSIZE * 2, 0x1501000);
  //FIN_IF(!m_ext_buf, 0x1501100);
  FIN_IF(m_ext_buf.size < UNICODE_STRING_MAX_CHARS * 4, 0x1501200);

  if (hFile) {
    x = nt::GetFileInformationByHandleEx(hFile, FileStandardInfo, &fi->std, sizeof(fi->std));
    FIN_IF(!x, 0x1501300);
    x = nt::GetFileInformationByHandleEx(hFile, FileBasicInfo, &fi->attr, sizeof(fi->attr));
    FIN_IF(!x, 0x1501400);
    if (fi->std.Directory) {
      fi->size = 0;
    } else {
      x = GetFileSizeEx(hFile, (PLARGE_INTEGER)&fi->size);
      FIN_IF(!x, 0x1501500);
    }
  }  
    
  fn_len = fn ? wcslen(fn) : 0;
  FIN_IF(fn_len > UNICODE_STRING_MAX_CHARS, 0x1501600);
  FIN_IF(fn_len == 1 && fn[0] == L'\\', 0x1501600);

  m_fullname.size = (UNICODE_STRING_MAX_CHARS + 33) * sizeof(WCHAR);
  m_fullname.bytes = (PBYTE)m_ext_buf.bytes + m_ext_buf.size - m_fullname.size;
  
  m_utf8name.size = (UNICODE_STRING_MAX_CHARS + 1) * 4;
  m_utf8name.bytes = (PBYTE)m_fullname.bytes - m_utf8name.size;

  m_utf8tmp.size = (UNICODE_STRING_MAX_CHARS + 1) * 4;
  m_utf8tmp.bytes = (PBYTE)m_utf8name.bytes - m_utf8tmp.size;

  FIN_IF(m_utf8tmp.ptr <= m_ext_buf.ptr, 0x1501700);
  m_out_buf.bytes = m_ext_buf.bytes;
  m_out_buf.size = (size_t)m_utf8tmp.ptr - (size_t)m_ext_buf.ptr;
  FIN_IF(m_out_buf.size < BLOCKSIZE * 4, 0x1501800);

  size_t ffn_len = 0;
  if (fullFilename) {
    ffn_len = wcslen(fullFilename);
    FIN_IF(ffn_len <= 1, 0x1501900);
    FIN_IF(ffn_len > UNICODE_STRING_MAX_CHARS, 0x1502000);
    memcpy(m_fullname.ptr, fullFilename, (ffn_len + 1) * sizeof(WCHAR));
  } else {
    FIN_IF(!hFile, 0x1502100);
    PFILE_NAME_INFO pfni = (PFILE_NAME_INFO)m_fullname.ptr;
    x = nt::GetFileInformationByHandleEx(hFile, FileNameInfo, pfni, (DWORD)m_fullname.size);
    FIN_IF(!x, 0x1502200);
    ffn_len = pfni->FileNameLength / sizeof(WCHAR);
    FIN_IF(ffn_len > UNICODE_STRING_MAX_CHARS, 0x1502300);
    FIN_IF(ffn_len < fn_len, 0x1502400);
    m_fullname.wstr = pfni->FileName;
    m_fullname.size -= FIELD_OFFSET(FILE_NAME_INFO, FileName);
    m_fullname.wstr[ffn_len] = 0;
  }
  if (m_fullname.wstr[ffn_len-1] == L'\\') {
    m_fullname.wstr[--ffn_len] = 0;
  }
  LPSTR utf8name = m_utf8name.str + 2; 
  int aLen = WideCharToMultiByte(CP_UTF8, 0, m_fullname.wstr, (int)ffn_len, utf8name, (int)m_utf8name.size - 8, NULL, NULL);
  FIN_IF(aLen <= 0, 0x1502700);
  FIN_IF((size_t)aLen >= m_utf8name.size - 8, 0x1502800);
  size_t ufn_len = aLen;
  utf8name[ufn_len] = 0;

  size_t k = INT_MAX;
  if (fn_len > 0) {
    if (fn[fn_len-1] == L'\\')
      fn_len--;
    k = 0;
    for (size_t i = fn_len; i != 0; i--) {
      if (fn[i-1] == L'\\')
        k++;
    }
  }
  LPSTR utf8n = utf8name;
  size_t e = 0;
  for (size_t i = ufn_len; i != 0; i--) {
    if (utf8name[i-1] == L'\\') {
      if (e == 0)
        utf8n = &utf8name[i];
      e++;
      if (e == k + 1) {
        utf8name = &utf8name[i];
        break;
      }
    }
  }
  utf8name -= 2;
  utf8name[0] = '.';
  utf8name[1] = '\\';
  
  bool name_is_7bit = true;
  size_t nlen = 0;
  for (LPSTR s = utf8name; *s; s++) {
    nlen++;
    if ((BYTE)(*s) >= 0x80)
      name_is_7bit = false;
    if (*s == '\\')
      *s = '/';
  }
  if (fi->std.Directory) {
    utf8name[nlen++] = '/';
    utf8name[nlen] = 0;
  }
  
  xh.init(m_out_buf.bytes, m_out_buf.size);
  
  if (fi->size >= OLD_SIZE_LIMIT) {
    hr = xh.add_size(fi->size);
    FIN_IF(hr, 0x1511000);
  }
  if (fi->realsize) {
    hr = xh.add_realsize(fi->realsize);
    FIN_IF(hr, 0x1511000);
  }
  {
    DWORD attr = fi->attr.FileAttributes;
    if (!(m_attr_file & save_readonly))  attr &= ~FILE_ATTRIBUTE_READONLY;
    if (!(m_attr_file & save_hidden))    attr &= ~FILE_ATTRIBUTE_HIDDEN;
    if (!(m_attr_file & save_system))    attr &= ~FILE_ATTRIBUTE_SYSTEM;
    if (!(m_attr_file & save_archive))   attr &= ~FILE_ATTRIBUTE_ARCHIVE;
    if (attr) {
      hr = xh.add_attr(attr);
      FIN_IF(hr, 0x1512000);
    }
  }
  if (fi->attr.LastWriteTime.QuadPart != _I64_MAX) {
    hr = xh.add_param_time("mtime", &fi->attr.LastWriteTime);
    FIN_IF(hr, 0x1513000);
  }
  if (m_attr_time & save_atime) {
    if (fi->attr.LastAccessTime.QuadPart != _I64_MAX) {
      hr = xh.add_param_time("atime", &fi->attr.LastAccessTime);
      FIN_IF(hr, 0x1514000);
    }
  }
  if (m_attr_time & save_ctime) {
    if (fi->attr.CreationTime.QuadPart != _I64_MAX) {  
      hr = xh.add_param_time("ctime", &fi->attr.CreationTime);
      FIN_IF(hr, 0x1515000);
    }
  }
  if (!name_is_7bit || nlen >= sizeof(xh.m_header->name) || xh.get_data_size() == 0) {
    hr = xh.add_path(utf8name, nlen);
    FIN_IF(hr, 0x1516000);
  }
  FIN_IF(xh.get_data_size() == 0, 0x1519000);
  FIN_IF(xh.get_used_size() >= 1*1024*1024, 0x1520000);
  
  hr = set_tar_name(xh.m_header, XHDTYPE, fi->std.Directory ? true : false, utf8name, nlen, utf8n);  
  FIN_IF(hr, 0x1521000);
  
  hr = set_tar(xh.m_header, XHDTYPE, xh.get_data_size(), fi);
  FIN_IF(hr, 0x1522000);

  hr = set_tar_chksum(xh.m_header, XHDTYPE);
  FIN_IF(hr, 0x1523000);
  
  FIN_IF(xh.get_used_size() + BLOCKSIZE >= xh.m_buf_cap, 0x1530000);
  struct posix_header * ph = (struct posix_header *) ((PBYTE)xh.m_header + xh.get_used_size());
  memset(ph, 0, BLOCKSIZE);
  
  char type = (fi->std.Directory) ? DIRECTORY : NORMAL;
  
  hr = set_tar_name(ph, type, fi->std.Directory ? true : false, utf8name, nlen, utf8n);  
  FIN_IF(hr, 0x1531000);

  hr = set_tar(ph, type, fi->size, fi);
  FIN_IF(hr, 0x1532000);

  hr = set_tar_chksum(ph, type);
  FIN_IF(hr, 0x1533000);
  
  buf_size = xh.get_used_size() + BLOCKSIZE;
  hr = 0;
  
fin:
  return hr;
}

// ========================================================================================================

TAR_INLINE
static int decode_decimal(LPCSTR str, size_t len, size_t & value)
{
  value = 0;
  for (size_t i = len; i-- != 0; ) {
    register char x = *str++;
    if (!ISDIGIT(x))
      return -1;    
    value *= 10;
    value += (BYTE)x - '0';
  }
  return 0; 
}

TAR_INLINE
static int decode_decimal64(LPCSTR str, size_t len, UINT64 & value)
{
  value = 0;
  for (size_t i = len; i-- != 0; ) {
    register char x = *str++;
    if (!ISDIGIT(x))
      return -1;
    value *= 10;
    value += (BYTE)x - '0';
  }
  return 0; 
}

TAR_INLINE
static UINT32 decode_octal32(LPCSTR str, size_t max_len)
{
  UINT value = 0;
  for (size_t i = max_len; i-- != 0; ) {
    register char x = *str++;
    if (!ISOCTAL(x))
      break;    
    value <<= LG_8;
    value += (BYTE)x - '0';
  }
  return value; 
}

TAR_INLINE
static UINT64 decode_octal(LPCSTR str, size_t max_len)
{
  UINT64 value = 0;
  for (size_t i = max_len; i-- != 0; ) {
    register char x = *str++;
    if (!ISOCTAL(x))
      break;    
    value <<= LG_8;
    value += (BYTE)x - '0';
  }
  return value; 
}

TAR_INLINE
static int decode_time(LPCSTR str, size_t size, INT64 & sec, size_t & nsec)
{
  int hr = -1;
  bool negative = false;
  FIN_IF(size == 0, -1);
  if (*str == '-') {
    negative = true;
    str++;
    size--;
    FIN_IF(size == 0, -2);
  }
  LPCSTR end = str + size;
  LPCSTR pnt = NULL;
  size_t k = 0;
  size_t slen = 0;
  size_t nslen = 0;
  for (LPCSTR p = str; p < end; p++) {
    if (*p == '.') {
      FIN_IF(pnt, -3);  /* second point */
      pnt = p;
      slen = k;
      k = 0;
      continue;
    }
    FIN_IF(!ISDIGIT(*p), -4);
    k++;
  }
  if (pnt) {
    nslen = (k < 7) ? k : 7;
    FIN_IF(nslen == 0, -5);
    int x = decode_decimal(pnt+1, nslen, nsec);
    FIN_IF(x, -6);
    for (size_t i = nslen ; i < 7; i++) {
      nsec *= 10;
    }
  } else {
    slen = k;
  }
  FIN_IF(slen == 0, -7);
  UINT64 vu64;
  int x = decode_decimal64(str, slen, vu64);
  FIN_IF(x, -8);
  FIN_IF(vu64 >= _I64_MAX, -9);
  sec = negative ? -(INT64)vu64 : (INT64)vu64;
  return 0;

fin:
  return hr;
}

static int decode_epoch_time(LPCSTR str, size_t size, INT64 & win_time)
{
  int hr = 0;
  INT64 sec;
  size_t nsec;
  hr = decode_time(str, size, sec, nsec);
  FIN_IF(hr, hr);
  win_time = sec * WINDOWS_TICK + nsec;
  win_time += DELTA_EPOCH_IN_SECS * WINDOWS_TICK;
fin:  
  return hr;
}

TAR_INLINE
static UINT64 epoch_to_win_time(UINT64 sec)
{
  return sec * WINDOWS_TICK + DELTA_EPOCH_IN_SECS * WINDOWS_TICK;
}

int check_tar_is_zero(LPCVOID buf, size_t size)
{
  if (size < BLOCKSIZE)
    return -1;
  PDWORD p = (PDWORD)buf;
  PDWORD end = (PDWORD)((size_t)buf + BLOCKSIZE);
  do {
    if (*p++ != 0)
      return 0;
  } while (p < end);
  return 1;
}

int check_tar_cksum(LPCVOID buf, size_t size)
{
  int hr = 0;
  struct posix_header * ph = (struct posix_header *)buf;
  UINT32 usum = 0;
  INT32 ssum = 0;
  char * p = (char *)buf;
  
  FIN_IF(size < BLOCKSIZE, -3);
  FIN_IF(!ISOCTAL(ph->chksum[0]), -4);
  for (size_t i = BLOCKSIZE; i-- != 0; ) {
    register char x = *p++;
    usum += (unsigned char)x;
    ssum += (signed char)x;
  }
  FIN_IF(usum == 0, 0);  // HEADER_ZERO_BLOCK
  p = ph->chksum;
  for (size_t i = sizeof(ph->chksum); i-- != 0; ) {
    register char x = *p++;
    usum -= (unsigned char)x;
    ssum -= (signed char)x;
  } 
  usum += ' ' * sizeof(ph->chksum);
  ssum += ' ' * sizeof(ph->chksum);
  UINT32 cksum = decode_octal32(ph->chksum, sizeof(ph->chksum));
  return (cksum == usum || cksum == (UINT32)ssum) ? 1 : -1;
  
fin:
  return hr;
}

int check_tar_header(LPCVOID buf, size_t size, bool cksum)
{
  const DWORD c_magic1  = 'atsu';      // "usta"
  const DWORD c_magic2  = 0x30300072;  // "ustar#00"   # = Null
  const DWORD c_magic2g = 0x00303072;  // "ustar  #"   # = Null

  int hr = 0;
  int fmt = UNKNOWN_FORMAT;
  struct posix_header * ph = (struct posix_header *)buf;
  union block * hdr = (union block *)buf;
  
  FIN_IF(size < BLOCKSIZE, -3);
  DWORD magic1 = *(PDWORD)&ph->magic[0];
  FIN_IF(magic1 != c_magic1, -4);   // "usta"
  DWORD magic2 = *(PDWORD)&ph->magic[4];
  FIN_IF(magic2 != c_magic2 && magic2 != c_magic2g, -5);
  FIN_IF(!ISOCTAL(ph->chksum[0]), -6);  /* ignore older tars, which output leading spaces.  */ 
  if (cksum) {
    hr = check_tar_cksum(buf, size);
    FIN_IF(hr != 1, -7);
  }
  if (magic2 == c_magic2) {
    fmt = USTAR_FORMAT;
    struct star_header * sh = (struct star_header *)buf;
    if (ph->typeflag == XHDTYPE || ph->typeflag == XGLTYPE) {
      fmt = POSIX_FORMAT;
    } else
    if (sh->prefix[130] == 0 && ISOCTAL(sh->atime[0]) && sh->atime[11] == ' ' && ISOCTAL(sh->ctime[0]) && sh->ctime[11] == ' ') {
      fmt = STAR_FORMAT;
    }
  } else {
    fmt = OLDGNU_FORMAT;
    if (ph->mode[0] == '0' && ph->mode[1] == '0' && ph->mode[2] == '0')
      fmt = GNU_FORMAT;
  }
  return (int)fmt;

fin:
  return hr;
}

pax_decode::pax_decode()
{
  clear();
}

pax_decode::~pax_decode()
{
  // 
}

void pax_decode::clear()
{
  m_buf = NULL;
  m_format = UNKNOWN_FORMAT;
  m_pax_type = REGULAR;
  m_pax_size = 0;
  m_type = REGULAR;
  m_header_size = 0;
  m_name = 0;
  m_name_len = 0;
  memset(&m_info, 0, sizeof(m_info));
}

int pax_decode::parse_pax_param(LPCSTR keyword, size_t klen, LPCSTR str, size_t size)
{
  int hr = 0x1556800;
  UINT64 vu64;
  INT64 vs64;

  if (klen == 0)
    klen = strlen(keyword);
  FIN_IF(klen == 0, 0x1556000);

  if (klen == 4 && memcmp(keyword, "path", 4) == 0) {
    m_name = (size_t)str - (size_t)m_buf;
    m_name_len = size; 
    return 0;
  }
  if (klen == 4 && memcmp(keyword, "size", 4) == 0) {
    if (ignore_field_size(m_type))
      return 0;
    int x = decode_decimal64(str, size, vu64);
    FIN_IF(x, 0x1556100);
    FIN_IF(vu64 >= _I64_MAX, 0x1556200);
    m_info.size = vu64;
    return 0;
  }
  if (klen == 7+8 && strcmp(keyword, "SCHILY.realsize") == 0) {
    int x = decode_decimal64(str, size, vu64);
    FIN_IF(x, 0x1556100);
    FIN_IF(vu64 >= _I64_MAX, 0x1556200);
    m_info.realsize = vu64;
    return 0;
  }
  if (klen == 5) {
    if (memcmp(keyword, "ctime", 5) == 0) {
      int x = decode_epoch_time(str, size, vs64);
      FIN_IF(x, 0x1556300);
      m_info.attr.CreationTime.QuadPart = vs64;
      return 0;
    }
    if (memcmp(keyword, "mtime", 5) == 0) {
      int x = decode_epoch_time(str, size, vs64);
      FIN_IF(x, 0x1556300);
      m_info.attr.LastWriteTime.QuadPart = vs64;
      return 0;
    }
    if (memcmp(keyword, "atime", 5) == 0) {
      int x = decode_epoch_time(str, size, vs64);
      FIN_IF(x, 0x1556300);
      m_info.attr.LastAccessTime.QuadPart = vs64;
      return 0;
    }
  }
  if (klen == 7+6 && strcmp(keyword, "SCHILY.fflags") == 0) {
    LPCSTR end = str + size;
    size_t k = 0;
    LPCSTR n = str;
    for (LPCSTR p = str; p <= end; p++) {
      if (p == end || *p == ',') {
        if (k > 0) {
          if (k == 8 && memcmp(n, "readonly", 8) == 0)
            m_info.attr.FileAttributes |= FILE_ATTRIBUTE_READONLY;
          if (k == 6 && memcmp(n, "hidden", 6) == 0)
            m_info.attr.FileAttributes |= FILE_ATTRIBUTE_HIDDEN;
          if (k == 6 && memcmp(n, "system", 6) == 0)
            m_info.attr.FileAttributes |= FILE_ATTRIBUTE_SYSTEM;
          if (k == 8 && memcmp(n, "archived", 8) == 0)
            m_info.attr.FileAttributes |= FILE_ATTRIBUTE_ARCHIVE;
        }
        k = 0;
        n = p + 1;
        continue;
      }
      k++;
    }
    return 0;
  } 

fin:
  return hr;
}

int pax_decode::parse_pax_header()
{
  int hr = 0;
  char keyword[64];

  LPCSTR str = (LPCSTR)m_buf + tar::BLOCKSIZE;
  LPCSTR end = str + m_pax_size;
  while (str < end) {
    size_t sz = 0;
    size_t kp = 0;
    size_t kw = 0;
    FIN_IF(!ISDIGIT(*str), 0x1555100);
    for (LPCSTR p = str; p < end; p++) {
      if (sz == 0 && !ISDIGIT(*p)) {
        sz = (size_t)p - (size_t)str;
        continue;
      }
      if (*p == '=') {
        kw = kp;
        keyword[kp] = 0;
        break;
      }
      if (sz) {
        FIN_IF(kp + 2 >= sizeof(keyword), 0x1555200);
        FIN_IF(*p == 0, 0x1555300);
        keyword[kp++] = *p;
      }
    }
    FIN_IF(sz == 0, 0x1555400);
    FIN_IF(sz >= 6, 0x1555500);
    FIN_IF(kw == 0, 0x1555600);
    size_t fsz = 0;
    int x = decode_decimal(str, sz, fsz);
    FIN_IF(x, 0x1555700);
    FIN_IF(fsz < sz + kw + 3, 0x1555800);
    FIN_IF(str + fsz > end, 0x1555900);
    size_t plen = fsz - (sz + kw + 3);    /* ' ' + '=' + '\n' */
    if (plen > 0) {
      hr = parse_pax_param(keyword, kw, str + sz + 1 + kw + 1, plen);
      FIN_IF(hr, hr);
    }
    str += fsz;
  }
  hr = 0;

fin:
  return hr;
}

int pax_decode::parse_header()
{
  int hr = 0;

  //FIN_IF(m_type != NORMAL && m_type != DIRECTORY, 0);   /* skip unsupported headers */

  struct posix_header * ph = (struct posix_header *)(m_buf + m_header_size - tar::BLOCKSIZE);
  m_name = (size_t)ph->name - (size_t)m_buf;
  LPCSTR name = (LPCSTR)m_buf + m_name;
  m_name_len = sizeof(ph->name);
  if (name[sizeof(ph->name)-1] == 0)
    m_name_len = strlen(name);

  UINT mode = decode_octal32(ph->mode, sizeof(ph->mode));
  if ((mode & (TUWRITE | TGWRITE | TOWRITE )) == 0)
    m_info.attr.FileAttributes |= FILE_ATTRIBUTE_READONLY;
  if (m_type == DIRECTORY)
    m_info.attr.FileAttributes |= FILE_ATTRIBUTE_DIRECTORY;
  if (m_type == NORMAL)
    m_info.attr.FileAttributes |= FILE_ATTRIBUTE_NORMAL;

  m_info.std.Directory = (m_type == DIRECTORY) ? TRUE : FALSE;

  if (ignore_field_size(m_type) == false) {
    UINT64 size = decode_octal(ph->size, sizeof(ph->size));
    m_info.size = size;
  }
  UINT64 mtime = decode_octal(ph->mtime, sizeof(ph->mtime));
  mtime = epoch_to_win_time(mtime);
  m_info.attr.LastWriteTime.QuadPart = mtime;
  m_info.attr.CreationTime.QuadPart = mtime;

  if (m_pax_type == XHDTYPE) {
    // TODO: support GNU tar Ext headers
    FIN_IF(m_pax_size == 0, 0x1552200);
    hr = parse_pax_header();
    FIN_IF(hr, hr);
  }

fin:
  return hr;
}

bool pax_decode::is_huge_data_size()
{
  return m_info.size == OLD_SIZE_LIMIT;
}

/* return: negative = error, 0 = header is readed, 1 = required addon header */
int pax_decode::add_header(LPCVOID buf, size_t size)
{
  int hr = 0;

  if (!m_buf) {
    FIN_IF(size < tar::BLOCKSIZE, 0x1550100);
    m_buf = (LPBYTE)buf;
    hr = check_tar_header(buf, tar::BLOCKSIZE, true);
    FIN_IF(hr <= 0, 0x1550200);
    struct posix_header * ph = (struct posix_header *)buf;
    m_format = (tar::Format)hr;
    m_type = (ph->typeflag == 0) ? NORMAL : (file_type)ph->typeflag;
    m_header_size = tar::BLOCKSIZE;
    UINT64 size = decode_octal(ph->size, sizeof(ph->size));
    if (m_type == XHDTYPE || m_type == LONGLINK || m_type == LONGNAME) {
      FIN_IF(size == 0, 0x1550300);
      FIN_IF(size >= 1*1024*1024, 0x1550400);
      m_pax_size = (size_t)size;
      m_header_size += tar::blocksize_round(m_pax_size);
      m_pax_type = m_type;;
      m_type = REGULAR;
      m_header_size += tar::BLOCKSIZE;
      return 1;
    }
    return 0;
  }

  FIN_IF(m_buf != buf, 0x1550500);
  FIN_IF(size < m_header_size, 0x1550600);
  struct posix_header * ph = (struct posix_header *)(m_buf + m_header_size - tar::BLOCKSIZE);
  hr = check_tar_header(ph, tar::BLOCKSIZE, true);
  FIN_IF(hr <= 0, 0x1550700);
  m_type = (ph->typeflag == 0) ? NORMAL : (file_type)ph->typeflag;
  UINT64 data_size = decode_octal(ph->size, sizeof(ph->size));
  if (ignore_field_size(m_type) == false) {
    m_info.size = data_size;
  }
  return 0;

fin:
  return hr;
}


} /* namespace */
