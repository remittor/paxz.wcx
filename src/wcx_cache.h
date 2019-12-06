#pragma once

#include <windows.h>
#include "wcx_arcfile.h"
#include "bst/list.hpp"
#include "tar.h"

namespace wcx {

#define FILE_INFO_FLAG_DIR       0x01
#define FILE_INFO_FLAG_EMPTY     0x02   // for skipping record 

const size_t cache_max_items = 1024;

class cache_enum
{
public:
  cache_enum()
  {
    reset();
  }
  
  ~cache_enum()
  {
    // nothing
  }
  
  void reset()
  {
    m_block = NULL;
    m_record = NULL;
  }
  
  PBYTE m_block;
  PBYTE m_record;  
};

#pragma pack(push, 1)

typedef struct _pax_item {
  UINT64     pos;
  UINT32     hdr_p_size; // size of packed_header
  UINT32     hdr_size;   // size of orig PAX header
  UINT64     realsize;   // SCHILY.realsize
} pax_item;

struct cache_item {
  UINT32     item_size;
  BOOLEAN    deleted;   // deleted item
  BYTE       dummy;
  BYTE       flags;
  BYTE       type;
  pax_item   pax;
  UINT64     data_size; // orig content size
  UINT64     pack_size; // total_size = packed_header + packed_content
  DWORD      attr;      // windows file attributes
  INT64      ctime;     // creation time
  INT64      mtime;     // modification time
  UINT16     name_len;
  WCHAR      name[1];
};

#pragma pack(pop)


class cache_list;

class cache
{
public:
  friend cache_list;

  static const int alloc_block_size = 1*1024*1024;

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

  cache_item * add_file(LPCWSTR name, UINT64 size, BYTE type = 0);
  cache_item * add_file_internal(LPCWSTR name, UINT64 size, DWORD attr);
  cache_item * get_next_file(cache_enum & ce);
  
  bst::srw_lock & get_mutex() { return m_mutex; }
  void read_lock()   { m_mutex.read_lock(); } 
  void read_unlock() { m_mutex.read_unlock(); } 

  UINT64      get_capacity()   { return (UINT64)m_block_count * alloc_block_size; }
  UINT64      get_file_count() { return m_file_count; }

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
  
  int dump_to_file(LPCWSTR file_name);

protected:  
  static const size_t block_header_size = sizeof(LPVOID);
  PBYTE  get_next_block(PBYTE cur_block);
  void   set_next_block(PBYTE cur_block, PBYTE next_block);

  int add_pax_info(tar::pax_decode & pax, UINT64 file_pos, int hdr_pack_size, UINT64 total_size);
  int scan_pax_file(HANDLE hFile);
  int scan_paxlz4_file(HANDLE hFile);
  

  size_t          m_index;      // object index in cache list
  cache_list    & m_list;
  
  bst::srw_lock   m_mutex;
  volatile status m_status;
  
  wcx::arcfile    m_arcfile;
  INT64           m_last_used_time;
  
  PBYTE           m_root_blk;
  size_t          m_block_count;
  PBYTE           m_block;         // current block
  size_t          m_block_pos;     // position for write
  UINT64          m_item_count;
  UINT64          m_file_count;
  
  bst::filepath   m_add_name;      // tmp buffer
  UINT64          m_end;           // end of file data (zero padding etc.)
};


inline
PBYTE cache::get_next_block(PBYTE cur_block)
{
  return cur_block ? *(PBYTE *)cur_block : m_root_blk;
}

inline
void cache::set_next_block(PBYTE cur_block, PBYTE next_block)
{
  LPVOID * lnk = (LPVOID *)cur_block;
  *lnk = next_block;
}

// ================================================================================

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

