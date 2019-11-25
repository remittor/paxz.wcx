#include "stdafx.h"
#include "lz4pack.h"


namespace lz4 {

void packer::reset()
{
  m_readed_size = 0;
  m_output_size = 0;
}

int packer::set_block_size(int blk_size)
{
  if (blk_size > 128)
    blk_size = (blk_size + 1) / (1024*1024);   // convert to MiB
  if (blk_size <= 0) {
    m_block_size = 512*1024;
  } else {
    m_block_size = blk_size * (1024*1024);
  }
  return (int)m_block_size;
}

int packer::frame_set_file(HANDLE file)
{
  m_file = file;
  return 0;
}

int packer::frame_create(UINT64 data_size)
{
  int hr = 0;

  FIN_IF(!m_file, -1);
  FIN_IF(m_cmp_level < 0, 0);

  memset(&m_prefs, 0, sizeof(m_prefs));
  m_prefs.compressionLevel = (m_cmp_level == 0) ? 1 : m_cmp_level;
  m_prefs.frameInfo.blockMode = LZ4F_blockIndependent;
  m_prefs.frameInfo.blockSizeID = LZ4F_max4MB;
  m_prefs.frameInfo.blockChecksumFlag = LZ4F_blockChecksumEnabled;
  m_prefs.frameInfo.contentChecksumFlag = LZ4F_contentChecksumEnabled;
  m_prefs.favorDecSpeed = 0;   /* 1: parser favors decompression speed vs compression ratio. Only works for high compression modes (>= LZ4HC_CLEVEL_OPT_MIN) */
  m_prefs.frameInfo.contentSize = data_size;
  m_prefs.autoFlush = 1;

  hr = m_ctx.init(m_prefs);
  FIN_IF(hr, -10);

  size_t bsz = LZ4F_compressFrameBound(m_block_size, &m_prefs);
  if (bsz > m_buf.size()) {
    size_t msz = bsz + 2048;
    size_t sz = m_buf.reserve(msz + 2048);
    FIN_IF(sz == bst::npos, -11);
    m_buf.resize(msz);    
  }

  size_t sz = LZ4F_compressBegin(m_ctx.get_ctx(), m_buf.data(), m_buf.size(), &m_prefs);
  FIN_IF(LZ4F_isError(sz), -12);
  
  DWORD dw;
  BOOL x = WriteFile(m_file, m_buf.c_data(), (DWORD)sz, &dw, NULL);
  FIN_IF(!x, -15);
  FIN_IF((DWORD)sz != dw, -16);
  
  m_output_size += sz;
  hr = 0;

fin:
  return hr;
}

int packer::frame_add_data(PBYTE buf, size_t bufsize)
{
  int hr = 0;
  size_t sz;
  //bool first_block;

  if (m_cmp_level >= 0) {
    //first_block = (m_ctx.get_totalInSize() == 0);
    FIN_IF(m_prefs.autoFlush == 0, -21);
    FIN_IF(m_ctx.get_ctx() == NULL, -22);
    FIN_IF(bufsize > m_block_size, -23);
    sz = LZ4F_compressUpdate(m_ctx.get_ctx(), m_buf.data(), m_buf.size(), buf, bufsize, NULL);
    FIN_IF(LZ4F_isError(sz), -24);
    buf = m_buf.data();
  } else {
    //first_block = false;
    sz = bufsize;
  }
  //LOGw("%s: m_output_size = 0x%X  sz = %Id [0x%X]  data = 0x%08X", __func__, (DWORD)m_output_size, sz, (DWORD)sz, *(PDWORD)(m_buf.data() + sz - 4));
  DWORD dw;
  BOOL x = WriteFile(m_file, (LPCVOID)buf, (DWORD)sz, &dw, NULL);
  FIN_IF(!x, -25);
  FIN_IF((DWORD)sz != dw, -26);
  
  m_readed_size += bufsize;
  m_output_size += sz;
  hr = 0;

fin:
  return hr;
}

int packer::frame_close()
{
  int hr = 0;

  FIN_IF(m_cmp_level < 0, 0);
  
  size_t sz = LZ4F_compressEnd(m_ctx.get_ctx(), m_buf.data(), m_buf.size(), NULL);
  //LOGe("%s: ERROR: %Id ", __func__, sz);
  FIN_IF(LZ4F_isError(sz), -31);
  
  //LOGw("%s: m_output_size = 0x%X  sz = %Id  [data = 0x%08X 0x%08X]", __func__, (DWORD)m_output_size, sz, *(PDWORD)m_buf.data(), *(PDWORD)(m_buf.data() + 4));
  DWORD dw;
  BOOL x = WriteFile(m_file, m_buf.c_data(), (DWORD)sz, &dw, NULL);
  FIN_IF(!x, -35);
  FIN_IF((DWORD)sz != dw, -36);

  m_output_size += sz;
  hr = 0;

fin:
  return hr;
}


} /* namespace */
