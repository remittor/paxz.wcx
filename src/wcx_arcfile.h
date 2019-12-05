#pragma once

#include <windows.h>
#include "bst\string.hpp"
#include "wcx_cfg.h"
#include "tar.h"
#include "lz4lib.h"


namespace wcx {

enum ArcType : char {
  atUnknown = 0,
  atPax     = 1,
  atLz4     = 2,
  atPaxLz4  = 3,
};

enum InitMode : char {
  imName    = 0x00,
  imAttr    = 0x01,
  imType    = 0x02,
};

inline InitMode operator + (const InitMode & a1, const InitMode & a2)
{
  return (InitMode)((char)a1 | (char)a2);
}

inline InitMode operator | (const InitMode & a1, const InitMode & a2)
{
  return (InitMode)((char)a1 | (char)a2);
}


class arcfile
{
public:
  arcfile();
  ~arcfile();
  
  arcfile(const arcfile & af);  // copy ctor
  bool assign(const arcfile & af);
  void clear();
  
  int  init(LPCWSTR arcName, InitMode mode);
  bool is_same_file(const arcfile & af);
  bool is_same_attr(const arcfile & af);
  
  LPCWSTR get_ArcName()  const { return m_ArcName.c_str(); }
  LPCWSTR get_fullname() const { return m_fullname.c_str(); }
  LPCWSTR get_name()     const { return m_name; }
  UINT64  get_size()     const { return m_size; }
  
  ArcType     get_type()       const { return m_type; }
  lz4::Format get_lz4_format() const { return m_lz4format; }
  tar::Format get_tar_format() const { return m_tarformat; }
  UINT64      get_ctime()      const { return m_ctime; }   
  UINT64      get_mtime()      const { return m_mtime; }   
  size_t      get_data_begin() const { return m_data_begin; }

  void set_type(ArcType type)  { m_type = type; }

  int  update_attr(HANDLE hFile);
  int  update_type(HANDLE hFile);
  void update_time(UINT64 ctime, UINT64 mtime) { m_ctime = ctime; m_mtime = mtime; }

private:
  InitMode       m_init_mode;
  bst::filepath  m_ArcName;        // string from WCX.OpenArchive
  bst::filepath  m_fullname;       // ArcName with prefix
  LPCWSTR        m_name;           // only name of archive (link to m_ArcName)
  UINT64         m_size;           // file size
  UINT64         m_ctime;
  UINT64         m_mtime;
  UINT64         m_volumeid;
  UINT64         m_fileid;
  ArcType        m_type;
  lz4::Format    m_lz4format;
  tar::Format    m_tarformat;
  paxz::frame_pax m_paxz;
  size_t         m_data_begin;
};


} /* namespace */

