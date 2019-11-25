#include "stdafx.h"
#include "lz4lib.h"

#include "..\lz4\lib\lz4frame.c"

namespace lz4 {

bool is_legacy_frame(DWORD magic)
{
  return (magic == LEGACY_MAGICNUMBER) ? true : false;
}

bool check_frame_magic(DWORD magic)
{
  return (magic == LZ4F_MAGICNUMBER || magic == LEGACY_MAGICNUMBER) ? true : false;
}

int check_file_header(HANDLE hFile, UINT64 file_size)
{
  int hr = -1;
  DWORD dw;
  LARGE_INTEGER pos;
  BOOL x;
  union {
    BYTE  buf[LZ4F_HEADER_SIZE_MAX];
    DWORD magic;
  } header;
  DWORD dwRead = 0;

  if (file_size == 0) {
    x = GetFileSizeEx(hFile, (PLARGE_INTEGER)&file_size);
    FIN_IF(!x, -2);
  }
  FIN_IF(file_size < LZ4F_HEADER_SIZE_MIN, -3);
  pos.QuadPart = 0;
  dw = SetFilePointerEx(hFile, pos, NULL, FILE_BEGIN);
  FIN_IF(dw == INVALID_SET_FILE_POINTER, -4);

  x = ReadFile(hFile, &header.magic, sizeof(header.magic), &dwRead, NULL);
  FIN_IF(!x, -5);
  FIN_IF(dwRead != sizeof(header.magic), -6);

  FIN_IF(lz4::check_frame_magic(header.magic) == FALSE, -7);

  hr = 0;

fin:
  return hr;  
}

int check_file_header(LPCWSTR filename)
{
  int hr = -1;
  
  HANDLE hFile = CreateFileW(filename, GENERIC_READ, FILE_SHARE_VALID_FLAGS, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
  hFile = (hFile == INVALID_HANDLE_VALUE) ? NULL : hFile;
  FIN_IF(!hFile, -2);
  
  hr = lz4::check_file_header(hFile, 0);

fin:
  if (hFile)
    CloseHandle(hFile);
  return hr;
}

// =========================================================================================

/* return error or data_offset */
int get_frame_info(LPCVOID buf, size_t bufsize, frame_info * finfo)
{
  size_t offset = bufsize;
  struct LZ4F_dctx_s dctx;

  memset(&dctx, 0, sizeof(dctx));
  dctx.version = LZ4F_VERSION;
  dctx.dStage = dstage_getFrameHeader;
  LZ4F_errorCode_t sz = LZ4F_getFrameInfo(&dctx, finfo->get_ptr(), buf, &offset);
  if (LZ4F_isError(sz))
    return -2;
  if (sz == 0)
    return -3;
  if (offset + 4 > bufsize)
    return -4;
  finfo->frame_size = offset;
  UINT32 data_size = *(PUINT32)((PBYTE)buf + offset);
  if (finfo->frameType == LZ4F_skippableFrame) {
    finfo->is_compressed = false;
    finfo->data_size = data_size;
    offset += 4;
  } else {
    if (data_size & LZ4F_BLOCKUNCOMPRESSED_FLAG) {
      finfo->is_compressed = false;
      finfo->data_size = data_size & (~LZ4F_BLOCKUNCOMPRESSED_FLAG);
    } else {
      finfo->is_compressed = true;
      finfo->data_size = data_size;
    }
    offset += 4;    
  }
  finfo->data_offset = offset; 
  return (int)offset;
}

int decode_data_partial(LPCVOID src, size_t srcSize, LPVOID dst, size_t dstSize, size_t dstCapacity)
{
  if (dstCapacity - dstSize < 128)
    return -101;
  int sz = LZ4_decompress_safe_partial((LPCSTR)src, (LPSTR) dst, (int)srcSize, (int)dstSize, (int)dstCapacity);
  if (sz <= 0)
    return -102;
  return (int)sz;
}

// ========================================================================================

typedef  struct LZ4F_dctx_s  lz4_dctx;

decode_context::decode_context()
{
  m_ctx = NULL;
  m_ctx_size = 0;
}

decode_context::~decode_context()
{
  lz4_dctx * ctx = (lz4_dctx *)m_ctx;
  if (ctx) {
    LZ4F_freeDecompressionContext(ctx);
  }
}

int decode_context::reset()
{
  lz4_dctx * ctx = (lz4_dctx *)m_ctx;
  if (!ctx)
    return -1;
  LZ4F_resetDecompressionContext(ctx);
  ctx->version = LZ4F_VERSION;
  return 0;
}

int decode_context::init()
{
  lz4_dctx * ctx = (lz4_dctx *)m_ctx;
  if (!ctx) {
    size_t sz = sizeof(lz4_dctx);
    ctx = (lz4_dctx *)malloc(sz);
    if (!ctx)
      return -1;
    memset(ctx, 0, sizeof(lz4_dctx));
    m_ctx = ctx;
    m_ctx_size = sz;
  }
  reset();
  return 0;
}

int decode_context::get_stage()
{
  lz4_dctx * ctx = (lz4_dctx *)m_ctx;
  if (ctx) {
    return ctx->dStage;
  }
  return -1;
}

LZ4F_frameInfo_t * decode_context::get_frame_info()
{
  lz4_dctx * ctx = (lz4_dctx *)m_ctx;
  if (!ctx)
    return NULL;
  return &ctx->frameInfo;
}

// ========================================================================================

typedef struct _lz4_cctx {
  LZ4F_cctx_t    cctx;
  union {
    LZ4_stream_t   stream1;
    LZ4_streamHC_t stream2;
  };
  size_t tmpBufSize;
  BYTE   tmpBuf[1];
} lz4_cctx;

encode_context::encode_context()
{
  m_ctx = NULL;
  m_ctx_size = 0;
}

encode_context::~encode_context()
{
  lz4_cctx * ctx = (lz4_cctx *)m_ctx;
  if (ctx) {
    LZ4F_cctx_t * cctxPtr = &ctx->cctx;
    if (cctxPtr->tmpBuff && cctxPtr->tmpBuff != ctx->tmpBuf) {
      free(cctxPtr->tmpBuff);
    }
    free(m_ctx);
  }
}

int encode_context::reset()
{
  lz4_cctx * ctx = (lz4_cctx *)m_ctx;
  if (ctx) {
    memset(ctx, 0, sizeof(lz4_cctx));
    ctx->tmpBufSize = buffer_size;
    return 0;
  }
  return -1;
}

int encode_context::init(const LZ4F_preferences_t & prefs)
{
  lz4_cctx * ctx = (lz4_cctx *)m_ctx;
  if (!ctx) {
    size_t sz = sizeof(lz4_cctx) + buffer_size;
    ctx = (lz4_cctx *)malloc(sz);
    if (!ctx)
      return -1;
    m_ctx = ctx;
    m_ctx_size = sz;
  }
  reset();

  LZ4F_cctx_t * cctxPtr = &ctx->cctx;
  
  memset(&ctx->cctx, 0, sizeof(ctx->cctx));
  cctxPtr->prefs = prefs;
  cctxPtr->cStage = 0;   /* Next stage : init stream */
  cctxPtr->version = LZ4F_VERSION;
  cctxPtr->maxBufferSize = ctx->tmpBufSize;
  cctxPtr->tmpBuff = ctx->tmpBuf;
  if (prefs.compressionLevel < LZ4HC_CLEVEL_MIN) {
    LZ4_initStream(&ctx->stream1, sizeof(ctx->stream1));
    cctxPtr->lz4CtxPtr = &ctx->stream1;
    cctxPtr->lz4CtxAlloc = 1;  /* sized for: 0 = none, 1 = lz4 ctx, 2 = lz4hc ctx */ 
    cctxPtr->lz4CtxState = 1;  /* in use as: 0 = none, 1 = lz4 ctx, 2 = lz4hc ctx */
  } else {
    LZ4_initStreamHC(&ctx->stream2, sizeof(ctx->stream2));
    cctxPtr->lz4CtxPtr = &ctx->stream2;
    cctxPtr->lz4CtxAlloc = 2;
    cctxPtr->lz4CtxState = 2;
  }
  return 0; 
}

int encode_context::get_stage()
{
  lz4_cctx * ctx = (lz4_cctx *)m_ctx;
  return ctx ? ctx->cctx.cStage : -1;
}

LZ4F_preferences_t * encode_context::get_prefs()
{
  lz4_cctx * ctx = (lz4_cctx *)m_ctx;
  return ctx ? &ctx->cctx.prefs : NULL;
}

INT64 encode_context::get_totalInSize()
{
  lz4_cctx * ctx = (lz4_cctx *)m_ctx;
  return ctx ? ctx->cctx.totalInSize : -1;
}

} /* namespace */
