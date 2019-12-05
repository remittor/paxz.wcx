#pragma once

#include <windows.h>


namespace tar { 

static const size_t BLOCKSIZE = 512;
static const size_t BLOCKSIZE_BITS = 9;
static const size_t BLOCKSIZE_MASK = 0x01FF;

static const char TMAGIC[]   = "ustar";  /* ustar and a null */
static const int  TMAGLEN    = 6;
static const char TVERSION[] = "00";   /* 00 and no null */
static const int  TVERSLEN   = 2;

static const char OLDGNU_MAGIC[] = "ustar  ";  /* 7 chars and a null */ 

static const UINT64 OLD_SIZE_LIMIT = 077777777777ULL;

enum file_type : char {
  REGULAR         =  0,
  NORMAL          = '0',
  HARDLINK        = '1',   /* ignore size field */
  SYMLINK         = '2',   /* ignore size field */
  CHAR            = '3',
  BLOCK           = '4',
  DIRECTORY       = '5',   /* ignore size field */
  FIFO            = '6',   /* ignore size field */
  CONTIGUOUS      = '7',
  // PAX Extended types:
  XHDTYPE         = 'x',   /* Extended header referring to the next file in the archive */
  XGLTYPE         = 'g',   /* Global extended header */
  // Other Extended types:
  DUMPDIR         = 'D',   /* This is a dir entry that contains the names of files that were in the dir at the time the dump was made. */
  LONGLINK        = 'K',   /* Identifies the *next* file on the tape as having a long linkname.  */
  LONGNAME        = 'L',   /* Identifies the *next* file on the tape as having a long name.  */
  MULTIVOL        = 'M',   /* This is the continuation of a file that began on another volume.  */
  SPARSE          = 'S',   /* This is for sparse files.  */
  VOLHDR          = 'V',   /* This file is a tape/volume header.  Ignore it on extraction.  */
  SOLARIS_XHDTYPE = 'X',   /* Solaris extended header (POSIX 1003.1-2001 eXtended) */
  SOLARIS_ACL     = 'A',   /* Solaris Access Control List */
  SOLARIS_EAF     = 'E',   /* Solaris Extended Attribute File */
  FT_INODE        = 'I',   /* Inode only, as in 'star' */
  FT_OLD_LONGNAME = 'N',   /* Obsolete GNU tar, for file names that do not fit into the main header. */
};

/* Bits used in the mode field, values in octal.  */
enum mode {
  TSUID   = 04000,         /* set UID on execution */
  TSGID   = 02000,         /* set GID on execution */
  TSVTX   = 01000,         /* reserved */
  /* file permissions */
  TUREAD  = 00400,         /* read by owner */
  TUWRITE = 00200,         /* write by owner */
  TUEXEC  = 00100,         /* execute/search by owner */
  TGREAD  = 00040,         /* read by group */
  TGWRITE = 00020,         /* write by group */
  TGEXEC  = 00010,         /* execute/search by group */
  TOREAD  = 00004,         /* read by other */
  TOWRITE = 00002,         /* write by other */
  TOEXEC  = 00001,         /* execute/search by other */
};

/* for files 644 */
static const USHORT DEF_FILE_MODE = TUREAD | TUWRITE | TGREAD | TOREAD;
/* for directories 755 */
static const USHORT DEF_DIR_MODE  = DEF_FILE_MODE | TUEXEC | TGEXEC | TOEXEC;


#pragma pack(push, 1) 

/* POSIX header.  */
struct posix_header
{
  char name[100];     /*   0 */
  char mode[8];       /* 100 */
  char uid[8];        /* 108 */
  char gid[8];        /* 116 */
  char size[12];      /* 124 */
  char mtime[12];     /* 136 */
  char chksum[8];     /* 148 */
  char typeflag;      /* 156 */
  char linkname[100]; /* 157 */
  char magic[6];      /* 257 */
  char version[2];    /* 263 */
  char uname[32];     /* 265 */
  char gname[32];     /* 297 */
  char devmajor[8];   /* 329 */
  char devminor[8];   /* 337 */
  char prefix[155];   /* 345 */
};                    /* 500 */


/* Descriptor for a single file hole.  */
struct sparse
{
  char offset[12];    /*   0 */
  char numbytes[12];  /*  12 */
};                    /*  24 */

static const int SPARSES_IN_EXTRA_HEADER  = 16;
static const int SPARSES_IN_OLDGNU_HEADER = 4;
static const int SPARSES_IN_SPARSE_HEADER = 21; 
 
struct sparse_header
{
  struct sparse sp[SPARSES_IN_SPARSE_HEADER];  /*   0 */
  char isextended;    /* 504 */
};                    /* 505 */  
 
struct oldgnu_header
{
  char unused_pad1[345];  /*   0 */
  char atime[12];     /* 345 Incr. archive: atime of the file */
  char ctime[12];     /* 357 Incr. archive: ctime of the file */
  char offset[12];    /* 369 Multivolume archive: the offset of the start of this volume */
  char longnames[4];  /* 381 Not used */
  char unused_pad2;   /* 385 */
  struct sparse sp[SPARSES_IN_OLDGNU_HEADER]; /* 386 */
  char isextended;    /* 482 Sparse file: Extension sparse header follows */
  char realsize[12];  /* 483 Sparse file: Real size*/
};                    /* 495 */

/* Schilling star header */
struct star_header
{
  char name[100];     /*   0 */
  char mode[8];       /* 100 */
  char uid[8];        /* 108 */
  char gid[8];        /* 116 */
  char size[12];      /* 124 */
  char mtime[12];     /* 136 */
  char chksum[8];     /* 148 */
  char typeflag;      /* 156 */
  char linkname[100]; /* 157 */
  char magic[6];      /* 257 */
  char version[2];    /* 263 */
  char uname[32];     /* 265 */
  char gname[32];     /* 297 */
  char devmajor[8];   /* 329 */
  char devminor[8];   /* 337 */
  char prefix[131];   /* 345 */
  char atime[12];     /* 476 */
  char ctime[12];     /* 488 */
};                    /* 500 */

static const int SPARSES_IN_STAR_HEADER     = 4;
static const int SPARSES_IN_STAR_EXT_HEADER = 21;

struct star_in_header
{
  char fill[345];       /*   0  Everything that is before t_prefix */
  char prefix[1];       /* 345  t_name prefix */
  char fill2;           /* 346  */
  char fill3[8];        /* 347  */
  char isextended;      /* 355  */
  struct sparse sp[SPARSES_IN_STAR_HEADER]; /* 356  */
  char realsize[12];    /* 452  Actual size of the file */
  char offset[12];      /* 464  Offset of multivolume contents */
  char atime[12];       /* 476  */
  char ctime[12];       /* 488  */
  char mfill[8];        /* 500  */
  char xmagic[4];       /* 508  "tar" */
}; 
 
struct star_ext_header
{
  struct sparse sp[SPARSES_IN_STAR_EXT_HEADER];
  char isextended;
};

enum Format : char
{
  UNKNOWN_FORMAT,
  V7_FORMAT,        /* old V7 tar format */
  OLDGNU_FORMAT,    /* GNU format as per before tar 1.12 */
  USTAR_FORMAT,     /* POSIX.1-1988 (ustar) format */
  POSIX_FORMAT,     /* POSIX.1-2001 format */
  STAR_FORMAT,      /* Star format defined in 1994 */
  GNU_FORMAT,
};

union block
{
  char raw[BLOCKSIZE];
  struct posix_header    header;
  struct star_header     star_header;
  struct oldgnu_header   oldgnu_header;
  struct sparse_header   sparse_header;
  struct star_in_header  star_in_header;
  struct star_ext_header star_ext_header;
};   

#pragma pack(pop) 

// ==========================================================================

inline static size_t blocksize_round(size_t size)
{
  return (size + tar::BLOCKSIZE - 1) & ~BLOCKSIZE_MASK;
}

inline static UINT64 blocksize_round64(UINT64 size)
{
  return (size + tar::BLOCKSIZE - 1) & ~(UINT64)BLOCKSIZE_MASK;
}

typedef struct _dynbuf {
  union {
    LPVOID ptr;
    PBYTE  bytes;
    LPWSTR wstr;
    LPSTR  str;
  };
  size_t size;
} dynbuf;

typedef struct _fileinfo {
  UINT64              size;
  UINT64              realsize;
  FILE_STANDARD_INFO  std;
  FILE_BASIC_INFO     attr;
} fileinfo;

class pax_encode
{
public:
  pax_encode();
  ~pax_encode();

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

  int set_tar_name(struct posix_header * ph, char type, bool is_dir, LPCSTR fullname, size_t fullname_len, LPCSTR fname);
  int set_tar(struct posix_header * ph, char type, UINT64 size, fileinfo * fi);
  int set_tar_chksum(struct posix_header * ph, char type);
  int create_header(HANDLE hFile, fileinfo * fi, LPCWSTR fullFilename, LPCWSTR fn, void * buf, size_t & buf_size);
 
  void set_ext_buf(LPVOID buf, size_t size);
 
  dynbuf   m_ext_buf;
  dynbuf   m_fullname;
  dynbuf   m_utf8name;
  dynbuf   m_utf8tmp;
  dynbuf   m_out_buf;
  
  AttrTime m_attr_time;
  AttrFile m_attr_file;
};

int check_tar_is_zero(LPCVOID buf, size_t size);
int check_tar_cksum(LPCVOID buf, size_t size);
int check_tar_header(LPCVOID buf, size_t size, bool cksum = false);

class pax_decode
{
public:
  pax_decode();
  ~pax_decode();
  
  void clear();
  int add_header(LPCVOID buf, size_t size);
  int parse_header();
  int parse_pax_header();
  int parse_pax_param(LPCSTR keyword, size_t klen, LPCSTR str, size_t size);

  LPBYTE get_header()      { return m_buf; }
  LPCSTR get_name()        { return m_name ? (LPCSTR)m_buf + m_name : NULL; }
  size_t get_name_len()    { return m_name ? (int)m_name_len : 0; }
  size_t get_header_size() { return m_header_size; }

  bool   is_big_header()     { return m_header_size > BLOCKSIZE; }
  bool   is_huge_data_size() { return m_info.size == OLD_SIZE_LIMIT; }

  LPBYTE     m_buf;
  Format     m_format;

  file_type  m_pax_type;     // PAX header type
  size_t     m_pax_size;     // PAX data extensions size
  
  file_type  m_type;
  size_t     m_header_size;  // total header size (PAX + USTAR)

  size_t     m_name;         // UFT8 name offset (from header begin)
  size_t     m_name_len;
  fileinfo   m_info;
};


} /* namespace */
