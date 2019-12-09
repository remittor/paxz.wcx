#include "stdafx.h"
#include "zstd_lib.h"
#include "zstd\lib\compress\zstd_compress_internal.h"
#include "zstd\lib\decompress\zstd_decompress_internal.h"
#include "zstd_internal.h"


namespace zst {

Format check_frame_magic(DWORD magic)
{
  if (is_legacy_frame(magic))
    return ffLegacy;

  return (magic == zst::MAGICNUMBER) ? ffActual : ffUnknown;
}

int check_file_header(HANDLE hFile, UINT64 file_size)
{
  int hr = -1;
  LARGE_INTEGER pos;
  union {
    BYTE  buf[FRAMEHEADER_SIZE_MIN];
    DWORD magic;
  } header;
  DWORD dwRead = 0;

  if (file_size == 0) {
    BOOL x = GetFileSizeEx(hFile, (PLARGE_INTEGER)&file_size);
    FIN_IF(!x, -2);
  }
  FIN_IF(file_size < FRAMEHEADER_SIZE_MIN, -3);
  pos.QuadPart = 0;
  BOOL xp = SetFilePointerEx(hFile, pos, NULL, FILE_BEGIN);
  FIN_IF(xp == FALSE, -4);

  BOOL x = ReadFile(hFile, &header.magic, sizeof(header.magic), &dwRead, NULL);
  FIN_IF(!x, -5);
  FIN_IF(dwRead != sizeof(header.magic), -6);

  Format fmt = zst::check_frame_magic(header.magic);
  FIN_IF(fmt == ffUnknown, -7);

  hr = (int)fmt;

fin:
  return hr;  
}

int check_file_header(LPCWSTR filename)
{
  int hr = -1;

  HANDLE hFile = CreateFileW(filename, GENERIC_READ, FILE_SHARE_VALID_FLAGS, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  hFile = (hFile == INVALID_HANDLE_VALUE) ? NULL : hFile;
  FIN_IF(!hFile, -2);

  hr = zst::check_file_header(hFile, 0);

fin:
  if (hFile)
    CloseHandle(hFile);
  return hr;
}

// =========================================================================================

/* return error or data_offset */
int frame_info::init(LPCVOID buf, size_t bufsize)
{
  int hr = 0;

  m_header.headerSize = 0;
  m_block.offset = 0;
  size_t ret = ZSTD_getFrameHeader_advanced(get_ptr(), buf, bufsize, ZSTD_f_zstd1);
  FIN_IF(ZSTD_isError(ret), -2);
  FIN_IF(ret > 0, -3);   /* buffer is too small */
  FIN_IF(m_header.headerSize == 0, -4);
  m_block.offset = m_header.headerSize;
  if (m_header.frameType == ZSTD_skippableFrame) {
    FIN_IF(m_header.headerSize + 4 > bufsize, -5);
    m_block.type = bt_raw;
    m_block.size = *(PUINT32)((PBYTE)buf + m_block.offset);
    m_block.data_size = m_block.size;
    m_header.frameContentSize = m_block.size;
    m_block.offset += 4;
  } else {
    FIN_IF(m_header.headerSize + ZSTD_BLOCKHEADERSIZE > bufsize, -5);
    UINT32 blk_header = *(PUINT32)((PBYTE)buf + m_block.offset - 1);
    blk_header >>= 8;
    m_block.is_latest = (blk_header & 1) == 1;
    m_block.type = (BlockType)((blk_header >> 1) & 3);
    m_block.data_size = blk_header >> 3;
    m_block.size = (m_block.type == bt_rle) ? 1 : m_block.data_size;
    m_block.offset += ZSTD_BLOCKHEADERSIZE;
  }
  hr = (int)m_block.offset;

fin:
  return hr;
}

/* return negative error, zero or size of readed data */
int frame_info::read_frame(HANDLE hFile, LPVOID buf, size_t buf_size, bool skip_magic)
{
  int hr = 0;
  PBYTE pbuf = (PBYTE)buf;
  bool is_latest_block = false;
  size_t nbReaded = 0;
  size_t pos;

  m_header.headerSize = 0;
  m_block.offset = 0;
  FIN_IF(skip_magic && *(PDWORD)pbuf != MAGICNUMBER, -1);
  pos = skip_magic ? 4 : 0;

  size_t sz = ZSTD_FRAMEHEADERSIZE_PREFIX(ZSTD_f_zstd1);
  FIN_IF(buf_size <= sz, -5);
  sz -= pos;
  BOOL x = ReadFile(hFile, pbuf + pos, (DWORD)sz, (PDWORD)&nbReaded, NULL);
  FIN_IF(!x || nbReaded != sz, -6);
  pos += sz;
  size_t ret = ZSTD_getFrameHeader_advanced(get_ptr(), pbuf, pos, ZSTD_f_zstd1);
  FIN_IF(ZSTD_isError(ret), -7);
  sz = (ret > 0) ? ret : m_header.headerSize;
  sz += ZSTD_BLOCKHEADERSIZE;
  FIN_IF(sz < pos, -8);
  sz -= pos;
  if (sz) {
    FIN_IF(pos + sz > buf_size, -9);
    BOOL x = ReadFile(hFile, pbuf + pos, (DWORD)sz, (PDWORD)&nbReaded, NULL);
    FIN_IF(!x, -10);
    FIN_IF(nbReaded == 0, 0);   /* EOF */
    FIN_IF(nbReaded != sz, -12);
    pos += sz;
  }
  hr = init(pbuf, pos);
  FIN_IF(hr <= 0, -17);
  FIN_IF(m_header.frameType != ZSTD_frame, -18);
  FIN_IF(m_block.offset != pos, -19);

  is_latest_block = m_block.is_latest;
  sz = m_block.size;
  if (is_latest_block) {
    sz += m_header.checksumFlag ? 4 : 0;
  } else {
    sz += ZSTD_BLOCKHEADERSIZE;
  }
  while (1) {
    FIN_IF(pos + sz > buf_size, -25);
    if (sz) {
      BOOL x = ReadFile(hFile, pbuf + pos, (DWORD)sz, (PDWORD)&nbReaded, NULL);
      FIN_IF(!x, -26);
      FIN_IF(nbReaded == 0, 0);   /* EOF */
      FIN_IF(nbReaded != sz, -28);
      pos += sz;
    } else {
      FIN_IF(!is_latest_block, -29);
    }
    if (is_latest_block) {
      break;
    }
    DWORD blk_header = *(PDWORD)(pbuf + pos - 4);
    blk_header >>= 8;
    is_latest_block = (blk_header & 1) == 1;
    BlockType blk_type = (BlockType)((blk_header >> 1) & 3);
    sz = (blk_type == bt_rle) ? 1 : blk_header >> 3;
    if (is_latest_block) {
      sz += m_header.checksumFlag ? 4 : 0;
    } else {
      sz += ZSTD_BLOCKHEADERSIZE;
    }
  }

  hr = (int)pos;

fin:
  return hr;
}

/* return negative error or zero */
int frame_info::read_frame(HANDLE hFile, UINT64 * frame_size, bool skip_magic)
{
  int hr = 0;
  BYTE buf[FRAMEHEADER_SIZE_MAX + 16];
  bool is_latest_block = false;
  size_t nbReaded = 0;
  UINT64 frame_beg;
  union {
    LARGE_INTEGER fpos;
    UINT64 pos;
  };
  size_t doffset = 0;

  m_header.headerSize = 0;
  m_block.offset = 0;
  fpos.QuadPart = nt::GetFileCurrentPos(hFile);
  FIN_IF(fpos.QuadPart < 0, -2);

  size_t sz = ZSTD_FRAMEHEADERSIZE_PREFIX(ZSTD_f_zstd1);
  frame_beg = pos;
  if (skip_magic) {
    FIN_IF(fpos.QuadPart < 4, -3);
    *(PDWORD)buf = zst::MAGICNUMBER;
    doffset = 4;
    frame_beg -= 4;
    sz -= 4;
  }
  BOOL x = ReadFile(hFile, buf + doffset, (DWORD)sz, (PDWORD)&nbReaded, NULL);
  FIN_IF(!x || nbReaded != sz, -6);
  pos += sz;
  doffset += sz;
  size_t ret = ZSTD_getFrameHeader_advanced(get_ptr(), buf, doffset, ZSTD_f_zstd1);
  FIN_IF(ZSTD_isError(ret), -7);
  sz = (ret > 0) ? ret : m_header.headerSize;
  sz += ZSTD_BLOCKHEADERSIZE;
  FIN_IF(sz < doffset, -8);
  sz -= doffset;
  if (sz) {
    BOOL x = ReadFile(hFile, buf + doffset, (DWORD)sz, (PDWORD)&nbReaded, NULL);
    FIN_IF(!x, -10);
    FIN_IF(nbReaded == 0, -11);   /* EOF */
    FIN_IF(nbReaded != sz, -12);
    pos += sz;
    doffset += sz;
  }
  hr = init(buf, doffset);
  FIN_IF(hr <= 0, -17);
  FIN_IF(m_header.frameType != ZSTD_frame, -18);
  FIN_IF(m_block.offset != doffset, -19);

  is_latest_block = m_block.is_latest;
  pos += m_block.size;
  if (is_latest_block) {
    sz = m_header.checksumFlag ? 4 : 0;
  } else {
    sz = ZSTD_BLOCKHEADERSIZE;
  }
  while (1) {
    BOOL xp = SetFilePointerEx(hFile, fpos, NULL, FILE_BEGIN);
    FIN_IF(xp == FALSE, -22);
    DWORD blk_header = 0;
    if (sz) {
      BOOL x = ReadFile(hFile, &blk_header, (DWORD)sz, (PDWORD)&nbReaded, NULL);
      FIN_IF(!x, -26);
      FIN_IF(nbReaded == 0, -27);   /* EOF */
      FIN_IF(nbReaded != sz, -28);
      pos += sz;
    } else {
      FIN_IF(!is_latest_block, -29);
    }
    if (is_latest_block) {
      break;
    }
    is_latest_block = (blk_header & 1) == 1;
    BlockType blk_type = (BlockType)((blk_header >> 1) & 3);
    pos += (blk_type == bt_rle) ? 1 : blk_header >> 3;
    if (is_latest_block) {
      sz = m_header.checksumFlag ? 4 : 0;
    } else {
      sz = ZSTD_BLOCKHEADERSIZE;
    }
  }

  hr = 0;
  if (frame_size)
    *frame_size = pos - frame_beg;

fin:
  return hr;
}

// ========================================================================================

int decode_context::reset(bool full_reset)
{
  if (!m_ctx)
    return -1;
  ZSTD_DCtx_reset(m_ctx, full_reset ? ZSTD_reset_session_and_parameters : ZSTD_reset_session_only);
  return 0;
}

int decode_context::init()
{
  if (!m_ctx) {
    m_ctx = ZSTD_createDCtx();
    if (!m_ctx)
      return -1;
  }
  reset();
  return 0;
}

int decode_context::get_stage()
{
  ZSTD_DCtx_s * ctx = (ZSTD_DCtx_s *)m_ctx;
  return ctx ? ctx->stage : -1;
}

ZSTD_frameHeader * decode_context::get_frame_info()
{
  ZSTD_DCtx_s * ctx = (ZSTD_DCtx_s *)m_ctx;
  return ctx ? &ctx->fParams : NULL;
}

/* src must contain the whole frame!!! */  
int decode_context::frame_decode(LPCVOID src, size_t srcSize, LPVOID dst, size_t dstSize)
{
  int hr = 0;
  if (!m_ctx) {
    if (init() != ERROR_SUCCESS)
      return -1;
  } else {
    reset();
  }
  ZSTD_inBuffer  input  = { src, srcSize, 0 };
  ZSTD_outBuffer output = { dst, dstSize, 0 };
  size_t ret = ZSTD_decompressStream(m_ctx, &output, &input);
  FIN_IF(ZSTD_isError(ret), -21);
  FIN_IF(ret != 0, -22);
  hr = (int)output.pos;
fin:
  return hr;
}

int decode_context::init_file(HANDLE hFile, size_t chunk_size)
{
  int hr = 0;

  m_file = hFile;
  m_readed_size = 0;
  m_out_pos = 0;
  if (chunk_size < 4096)
    chunk_size = 4096;

  if (m_buf.capacity() != chunk_size) {
    m_buf.destroy();
    FIN_IF(!m_buf.reserve(chunk_size), -2);
  }
  m_buf.resize(0);
  if (!m_ctx) {
    FIN_IF(init() != 0, -4);
  }
  reset();
fin:
  return hr;
}

int decode_context::read_file(LPVOID dst_buf, size_t dst_buf_size, size_t req_size, frame_info * frame)
{
  int hr = 0;
  DWORD dw;
  size_t ret = m_buf.capacity();

  FIN_IF(req_size == 0, -1);
  FIN_IF(req_size == m_out_pos, (int)req_size);
  FIN_IF(req_size < m_out_pos, -2);
   
  while (1) {
    size_t bsize = m_buf.size();    
    size_t toDecode = BST_MIN(ret, m_buf.capacity());
    if (bsize < toDecode) {
      size_t sz = toDecode - bsize;
      BOOL x = ReadFile(m_file, m_buf.data() + bsize, (DWORD)sz, &dw, NULL);
      FIN_IF(!x, -10);
      FIN_IF(dw == 0, 0);
      if (m_readed_size == 0 && frame) {
        FIN_IF(m_buf.size(), -15);
        FIN_IF(dw < ZSTD_FRAMEHEADERSIZE_MAX, -16);
        hr = frame->init(m_buf.data(), dw);
        FIN_IF(hr <= 0, -17);
      }
      m_readed_size += dw;
      m_buf.resize(bsize + dw);
    }
    ZSTD_inBuffer  input  = { m_buf.data(), m_buf.size(), 0 };
    ZSTD_outBuffer output = { dst_buf, dst_buf_size, m_out_pos };
    size_t ret = ZSTD_decompressStream(m_ctx, &output, &input);
    FIN_IF(ZSTD_isError(ret), -21);
    //FIN_IF(m_out_pos == output.pos, -22);
    FIN_IF(input.pos == 0, -23);
    if (input.pos < input.size) {
      size_t sz = input.size - input.pos;
      memmove(m_buf.data(), m_buf.data() + input.pos, sz);
      m_buf.resize(sz);
    } else {
      m_buf.resize(0);
    }
    m_out_pos = output.pos;
    FIN_IF(m_out_pos >= req_size, (int)m_out_pos);
    FIN_IF(ret == 0, 0);   /* end of frame */
  }
fin:
  return hr;
}

// =========================================================================================

int encode_context::reset(bool full_reset)
{
  if (!m_ctx)
    return -1;
  ZSTD_CCtx_reset(m_ctx, full_reset ? ZSTD_reset_session_and_parameters : ZSTD_reset_session_only);
  return 0;
}

int encode_context::init(int clevel, ZSTD_parameters * params)
{
  if (!m_ctx) {
    m_ctx = ZSTD_createCCtx();
    if (!m_ctx)
      return -1;
  }
  reset();

  /* compression level */
  ZSTD_CCtx_setParameter(m_ctx, ZSTD_c_compressionLevel, clevel);
  if (!params) {
    ZSTD_CCtx_setParameter(m_ctx, ZSTD_c_contentSizeFlag, TRUE);
    ZSTD_CCtx_setParameter(m_ctx, ZSTD_c_checksumFlag, TRUE);
  } else {
    ZSTD_CCtx_setParameter(m_ctx, ZSTD_c_contentSizeFlag, params->fParams.contentSizeFlag);
    ZSTD_CCtx_setParameter(m_ctx, ZSTD_c_checksumFlag, params->fParams.checksumFlag);
    /* compression parameters */
    ZSTD_CCtx_setParameter(m_ctx, ZSTD_c_windowLog, (int)params->cParams.windowLog);
    ZSTD_CCtx_setParameter(m_ctx, ZSTD_c_chainLog, (int)params->cParams.chainLog);
    ZSTD_CCtx_setParameter(m_ctx, ZSTD_c_hashLog, (int)params->cParams.hashLog);
    ZSTD_CCtx_setParameter(m_ctx, ZSTD_c_searchLog, (int)params->cParams.searchLog);
    ZSTD_CCtx_setParameter(m_ctx, ZSTD_c_minMatch, (int)params->cParams.minMatch);
    ZSTD_CCtx_setParameter(m_ctx, ZSTD_c_targetLength, (int)params->cParams.targetLength);
    ZSTD_CCtx_setParameter(m_ctx, ZSTD_c_strategy, (int)params->cParams.strategy);
  }
  return 0;
}

int encode_context::set_param(ZSTD_cParameter param, int value)
{
  if (!m_ctx)
    return -1;
  size_t x = ZSTD_CCtx_setParameter(m_ctx, param, value);
  return ZSTD_isError(x) ? -1 : 0;
}

int encode_context::get_stage()
{
  ZSTD_CCtx_s * ctx = (ZSTD_CCtx_s *)m_ctx;
  return ctx ? ctx->stage : -1;
}

ZSTD_CCtx_params_s * encode_context::get_prefs()
{
  ZSTD_CCtx_s * ctx = (ZSTD_CCtx_s *)m_ctx;
  return ctx ? &ctx->appliedParams : NULL;
}

INT64 encode_context::get_consumedSrcSize()
{
  ZSTD_CCtx_s * ctx = (ZSTD_CCtx_s *)m_ctx;
  return ctx ? ctx->consumedSrcSize : -1;
}

} /* namespace */
