#pragma once

#include <windows.h>
#include "bst\string.hpp"
#include "wcx_arcfile.h"
#include "wcx_callback.h"
#include "lz4lib.h"

namespace wcx {


class packer
{
public:  
  enum type {
    ctUnknown = 0,
    ctLz4     = 1,
  };

  packer() BST_DELETED;  // disable default constructor!
  packer(type ctype, wcx::cfg & cfg, int Flags, DWORD thread_id, tProcessDataProcW callback);
  ~packer();

  void reset();
  UINT get_thread_id() { return m_thread_id; }
  void set_AddListSize(size_t AddListSize) { m_AddListSize = AddListSize; }
  void set_arcfile(const wcx::arcfile & af) { m_arcfile = af; }

  int  pack_files(LPCWSTR SubPath, LPCWSTR SrcPath, LPCWSTR AddList);

private:
  type      m_ctype;        // compressor type
  wcx::cfg  m_cfg;
  DWORD     m_thread_id;
  int       m_Flags;
  size_t    m_AddListSize;
  bool      m_delete_out_file;
  arcfile   m_arcfile;
  HANDLE    m_outFile;           // out file
  bst::filepath m_srcFullName;   // current src file

  bst::buf  m_buf;

  wcx::callback m_cb;
  DWORD     m_start_time;

  bst::buf  m_ext_buf;

private:
  void reset_ctx(bool full_reset = false);
  int set_block_size(SSIZE_T blk_size);
  int set_compression_level(int cpr_level);

  int frame_create(UINT64 total_size);
  int frame_add_data(PBYTE buf, size_t bufsize);
  int frame_close();

  LZ4F_cctx * get_lz4_ctx()   { return m_lz4.ctx.get_ctx(); }

  struct {
    LZ4F_preferences_t  prefs;
    lz4::encode_context ctx;
  } m_lz4;

  size_t      m_block_size;      // read chunk size
  int         m_cpr_level;       // compression level
  bst::buf    m_cpr_buf;
  UINT64      m_readed_size;
  UINT64      m_output_size;
};

typedef bst::list<packer> packer_list;


} /* namespace */

