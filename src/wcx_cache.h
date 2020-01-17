#pragma once

#include <windows.h>
#include "wcx_arcfile.h"
#include "bst/list.hpp"
#include "tar.h"
#include "wcx_filetree.h"

namespace wcx {

const size_t cache_max_items = 1024;

typedef wcx::TTreeElem  cache_item;
typedef wcx::TElemInfo  cache_info;
typedef wcx::TDirEnum   cache_dir;
typedef wcx::TTreeEnum  cache_enum;

class cache_list;  /* forward declaration */

class cache
{
public:
  friend cache_list;

  enum status : char {
    stAlloc,
    stInit,
    stProcessed,
    stReady,      /* ready for public use */
    stZombie,
  };

  cache() BST_DELETED;
  cache(cache_list & list, size_t index);
  ~cache();
  
  void clear();
  bool set_arcfile(const arcfile & af);
  bool is_same_file(const arcfile & af);
  LPCWSTR get_name() { return m_arcfile.get_name(); }

  status get_status() { return m_status; }
  void   set_status(status st) { m_status = st; }

  cache_item * add_file(LPCWSTR fullname, UINT64 size, BYTE type = 0);
  cache_item * add_file_internal(LPCWSTR fullname, UINT64 size, DWORD attr);

  bool find_directory(cache_dir & cd, LPCWSTR dir, WCHAR delimiter = L'/');
  cache_item * get_next(cache_dir & cd);

  bool find_directory(cache_enum & ce, LPCWSTR dir, WCHAR delimiter = L'/', size_t max_depth = 0);
  cache_item * get_next_file(cache_enum & ce);

  bst::srw_lock & get_mutex() { return m_mutex; }
  void read_lock()   { m_mutex.read_lock(); } 
  void read_unlock() { m_mutex.read_unlock(); } 

  UINT64      get_capacity()   { return m_ftree.get_capacity(); }
  UINT64      get_num_elem()   { return m_ftree.get_num_elem(); }
  UINT64      get_file_count() { return m_ftree.get_num_elem(); }

  arcfile &   get_arcfile()    { return m_arcfile; }
  ArcType     get_type()       { return m_arcfile.get_type(); }
  lz4::Format get_lz4_format() { return m_arcfile.get_lz4_format(); }
  tar::Format get_tar_format() { return m_arcfile.get_tar_format(); }
  UINT64      get_file_size()  { return m_arcfile.get_size(); }
  
  int get_lz4_content_size(HANDLE hFile, UINT64 & content_size);

  int update_release_time();
  int update_time(UINT64 ctime, UINT64 mtime);
  int update_time(HANDLE hFile);
  
  int init(wcx::arcfile * af = NULL);
  int add_ref();
  int release();

protected:  
  int add_pax_info(tar::pax_decode & pax, UINT64 file_pos, int hdr_pack_size, UINT64 total_size);
  int scan_pax_file(HANDLE hFile);
  int scan_paxlz4_file(HANDLE hFile);
  int scan_paxzst_file(HANDLE hFile);


  size_t          m_index;      // object index in cache list
  cache_list    & m_list;
  
  bst::srw_lock   m_mutex;
  volatile status m_status;
  
  wcx::arcfile    m_arcfile;
  INT64           m_last_used_time;
  
  wcx::FileTree   m_ftree;
  
  bst::filepath   m_add_name;      // tmp buffer
  UINT64          m_end;           // end of file data (zero padding etc.)
};


class cache_list
{
public:
  cache_list();
  ~cache_list();

  int init(DWORD main_thread_id, wcx::inicfg & inicfg);
  int clear();
  int add_ref(size_t index);
  int add_ref(wcx::cache * obj);
  int release(size_t index);
  int release(wcx::cache * obj);

  int get_cache(const wcx::arcfile & arcfile, bool addref, wcx::cache ** p_cache);
  UINT64 get_cache_size();
  int delete_zombies();

protected:
  size_t get_free_item();
  int create(const wcx::arcfile & af);

  wcx::inicfg   * m_inicfg;
  int             m_refcount[cache_max_items];
  cache         * m_object[cache_max_items];
  bst::srw_lock   m_mutex;
  int             m_count;
  DWORD           m_main_thread_id;
};


} /* namespace */

