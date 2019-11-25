#pragma once

#include <windows.h>
#include "wcx_arcfile.h"
#include "bst/list.hpp"
#include "wcx_cache.h"
#include "wcx_callback.h"


namespace wcx {


class archive
{
public:
  archive();
  ~archive();
  
  int init(wcx::cfg & cfg, LPWSTR arcName, int openMode, DWORD thread_id, wcx::cache * cache);
  const arcfile & get_arcfile() { return m_arcfile; }
  int get_open_mode() { return m_open_mode; }
  
  int list(tHeaderDataExW * headerData);
  int extract(int Operation, LPCWSTR DestPath, LPCWSTR DestName);
  
  LPCWSTR get_ArcName()    { return m_arcfile.get_ArcName(); }
  LPCWSTR get_name()       { return m_arcfile.get_name(); }
  LPCWSTR get_fullname()   { return m_arcfile.get_fullname(); }
  UINT64  get_file_count() { return m_cache ? m_cache->get_file_count() : 0; }
  wcx::cache * get_cache() { return m_cache; }

  wcx::callback  m_cb;
  
public:
  HANDLE create_new_file(LPCWSTR fn);
  int extract_pax(LPCWSTR fn, UINT64 file_size, HANDLE & hDstFile);
  int extract_lz4(LPCWSTR fn, UINT64 file_size, HANDLE & hDstFile);
  int extract_paxlz4(LPCWSTR fn, UINT64 file_size, HANDLE & hDstFile);
  int extract_dir();  /* !!! mega hack !!! */

  wcx::cfg       m_cfg;
  DWORD          m_thread_id;
  wcx::arcfile   m_arcfile;
  int            m_open_mode;
  int            m_operation;
  bst::filepath  m_filename;
  HANDLE         m_file;
  bool           m_delete_out_file;
  wcx::cache   * m_cache;
  cache_enum     m_cenum;
  cache_item   * m_cur_dir;
  cache_item   * m_cur_file;
  bst::filepath  m_dst_dir;
  bst::buf       m_buf;
  bst::buf       m_dst;
  bst::buf       m_in_buf;
  bst::buf       m_out_buf;
  lz4::decode_context m_ctx;
};

typedef bst::list<archive> archive_list;


} /* namespace */

