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

const UINT64 CONTENTSIZE_UNKNOWN = ZSTD_CONTENTSIZE_UNKNOWN;
const DWORD BLOCKHEADERSIZE  = 3;  // ZSTD_BLOCKHEADERSIZE

const DWORD BLOCKSIZELOG_MAX = ZSTD_BLOCKSIZELOG_MAX;
const DWORD BLOCKSIZE_MAX    = ZSTD_BLOCKSIZE_MAX;     /* 128 KB */

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


enum BlockType : char {
  bt_raw         = 0,
  bt_rle         = 1,
  bt_compressed  = 2,
  bt_reserved    = 3,
};

struct frame_info
{
  ZSTD_frameHeader m_header;
  struct {
    size_t    offset;
    size_t    size;
    size_t    data_size;
    BlockType type;
    bool      is_latest;
  } m_block;

  int init(LPCVOID buf, size_t bufsize);
  int read_frame(HANDLE hFile, LPVOID buf, size_t buf_size, bool skip_magic);
  int read_frame(HANDLE hFile, UINT64 * frame_size, bool skip_magic);

  ZSTD_frameHeader * get_ptr() { return &m_header; }
  ZSTD_frameType_e get_type() { return m_header.frameType; }
  size_t get_header_size() { return m_header.headerSize; }
  bool is_skippable_frame() { return m_header.frameType == ZSTD_skippableFrame; }
  bool is_unknown_content_size() { return m_header.frameContentSize == CONTENTSIZE_UNKNOWN; }
  UINT64 get_content_size() { return m_header.frameContentSize; }
};


class decode_context
{
public:
  decode_context() : m_ctx(NULL), m_file(NULL), m_readed_size(0), m_out_pos(0)
  {
    //
  }

  ~decode_context()
  {
    ZSTD_freeDCtx(m_ctx);
  }

  int reset(bool full_reset = false);
  int init();
  ZSTD_DCtx * get_ctx() { return (ZSTD_DCtx *)m_ctx; }
  int get_stage();
  ZSTD_frameHeader * get_frame_info();
  int frame_decode(LPCVOID src, size_t srcSize, LPVOID dst, size_t dstSize);
  int init_file(HANDLE hFile, size_t chunk_size = 0);
  int read_file(LPVOID dst_buf, size_t dst_buf_size, size_t req_size, frame_info * frame);
  size_t get_readed_size() { return m_readed_size; }

private:
  ZSTD_DCtx * m_ctx;
  HANDLE      m_file;
  bst::buf    m_buf;
  size_t      m_readed_size;
  size_t      m_out_pos;
};


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

