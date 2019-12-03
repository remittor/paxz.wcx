#pragma once

#include "stdafx.h"

#define XXH_STATIC_LINKING_ONLY
#include "lz4\lib\xxhash.h"

#define ZSTD_STATIC_LINKING_ONLY
#include "zstd\lib\zstd.h"

namespace zst {

const DWORD MAGICNUMBER      = ZSTD_MAGICNUMBER;        /* valid since v0.8.0 */
const DWORD MAGIC_DICT       = ZSTD_MAGIC_DICTIONARY;   /* valid since v0.7.0 */
const DWORD MAGIC_SKIP       = ZSTD_MAGIC_SKIPPABLE_START;    /* all 16 values, from 0x184D2A50 to 0x184D2A5F, signal the beginning of a skippable frame */
const DWORD MAGIC_SKIP_MASK  = ZSTD_MAGIC_SKIPPABLE_MASK;

const DWORD FRAMEHEADER_SIZE_MIN = ZSTD_FRAMEHEADERSIZE_MIN(ZSTD_f_zstd1);
const DWORD FRAMEHEADER_SIZE_MAX = ZSTD_FRAMEHEADERSIZE_MAX;

const DWORD BLOCKHEADERSIZE  = 3;  // ZSTD_BLOCKHEADERSIZE

const DWORD BLOCKSIZELOG_MAX = ZSTD_BLOCKSIZELOG_MAX;
const DWORD BLOCKSIZE_MAX    = ZSTD_BLOCKSIZE_MAX;

const int MIN_CLEVEL = 1;
const int MAX_CLEVEL = 22;  /* ZSTD_MAX_CLEVEL */

enum Format : char {
  ffUnknown = 0,
  ffLegacy  = 1,    // legacy Zstd frame
  ffActual  = 2,    // actual Zstd frame
};

__forceinline
bool is_legacy_frame(DWORD magic)
{
  return (magic >= 0xFD2FB522 && magic <= 0xFD2FB527) ? true : false;
}

Format check_frame_magic(DWORD magic);
int check_file_header(HANDLE hFile, UINT64 file_size);
int check_file_header(LPCWSTR filename);


class encode_context
{
public:
  encode_context() : m_ctx(NULL)
  {
    //
  }
  
  ~encode_context()
  {
    ZSTD_freeCCtx(m_ctx);
  }
  
  int reset(bool full_reset = false);
  int init(int compression_level, ZSTD_parameters * params);
  int set_param(ZSTD_cParameter param, int value);
  ZSTD_CCtx * get_ctx() { return m_ctx; }
  int get_stage();
  ZSTD_CCtx_params_s * get_prefs();
  INT64 get_consumedSrcSize();

private:
  ZSTD_CCtx * m_ctx;
};

} /* namespace */

