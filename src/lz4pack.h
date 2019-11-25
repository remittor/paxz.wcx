#pragma once

#include "lz4lib.h"
#include "tar.h"

namespace lz4 {

class packer
{
public:
  static const int def_block_size = 4;  // 0 = 512 KiB, 1 = 1MiB, 2 = 2 MiB, 4 = 4 MiB ...
  static const int def_compress_level = 4;
  
  packer();
  ~packer();

  void reset();
  int get_block_size() { return (int)m_block_size; }
  int set_block_size(int blk_size);
  int set_compression_level(int cmp_level);

  int frame_set_file(HANDLE file);
  int frame_create(UINT64 data_size);
  int frame_add_data(PBYTE buf, size_t bufsize);
  int frame_close();
  
  UINT64 get_readed_size() { return m_readed_size; }
  UINT64 get_output_size() { return m_output_size; }

private:
  HANDLE      m_file;
  LZ4F_preferences_t m_prefs;
  encode_context m_ctx;
  size_t      m_block_size;
  int         m_cmp_level;
  bst::buf    m_buf;
  UINT64      m_readed_size;
  UINT64      m_output_size;
  
};

inline
packer::packer()
{
  m_file = NULL;
  m_prefs.autoFlush = 0;
  set_block_size(def_block_size);
  m_cmp_level = def_compress_level;
  reset();
}

inline
packer::~packer()
{
  reset();
}

inline
int packer::set_compression_level(int cmp_level)
{
  if (cmp_level <= 0) cmp_level = -1;
  if (cmp_level > LZ4HC_CLEVEL_DEFAULT) cmp_level = LZ4HC_CLEVEL_DEFAULT;
  m_cmp_level = cmp_level;
  return 0;
}

} /* namespace */

