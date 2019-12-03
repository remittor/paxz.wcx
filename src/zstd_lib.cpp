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
  DWORD dw;
  LARGE_INTEGER pos;
  BOOL x;
  union {
    BYTE  buf[FRAMEHEADER_SIZE_MIN];
    DWORD magic;
  } header;
  DWORD dwRead = 0;

  if (file_size == 0) {
    x = GetFileSizeEx(hFile, (PLARGE_INTEGER)&file_size);
    FIN_IF(!x, -2);
  }
  FIN_IF(file_size < FRAMEHEADER_SIZE_MIN, -3);
  pos.QuadPart = 0;
  dw = SetFilePointerEx(hFile, pos, NULL, FILE_BEGIN);
  FIN_IF(dw == INVALID_SET_FILE_POINTER, -4);

  x = ReadFile(hFile, &header.magic, sizeof(header.magic), &dwRead, NULL);
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
