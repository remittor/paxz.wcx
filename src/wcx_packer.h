#pragma once

#include <windows.h>
#include "bst\string.hpp"
#include "wcx_arcfile.h"
#include "wcx_callback.h"
#include "lz4pack.h"

namespace wcx {


class packer
{
public:  
  packer() BST_DELETED;  // disable default constructor!
  packer(wcx::cfg & cfg, int Flags, DWORD thread_id, tProcessDataProcW callback);
  ~packer();

  void reset();
  UINT get_thread_id() { return m_thread_id; }
  void set_AddListSize(size_t AddListSize) { m_AddListSize = AddListSize; }
  void set_arcfile(const wcx::arcfile & af) { m_arcfile = af; }

  int  pack_files(LPCWSTR SubPath, LPCWSTR SrcPath, LPCWSTR AddList);

private:  
  wcx::cfg  m_cfg;
  DWORD     m_thread_id;
  int       m_Flags;
  size_t    m_AddListSize;
  bool      m_delete_out_file;
  arcfile   m_arcfile;
  HANDLE    m_outFile;           // out file
  bst::filepath m_srcFullName;   // current src file

  bst::buf  m_buf;
  lz4::packer m_lz4;

  wcx::callback m_cb;

  bst::buf  m_ext_buf;
};

typedef bst::list<packer> packer_list;
//typedef bst::list_node<packer> packer_x;



} /* namespace */

