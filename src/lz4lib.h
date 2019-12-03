#pragma once

#include "stdafx.h"

#define LZ4_DISABLE_DEPRECATE_WARNINGS
#define LZ4_STATIC_LINKING_ONLY
#define LZ4_HC_STATIC_LINKING_ONLY
#define XXH_STATIC_LINKING_ONLY
#include "lz4\lib\lz4hc.h"
#include "lz4\lib\lz4frame_static.h"
#include "lz4\lib\xxhash.h"


namespace lz4 {

const DWORD MAGICNUMBER_SIZE    = 4;
const DWORD LZ4IO_MAGICNUMBER   = 0x184D2204;
const DWORD LZ4IO_SKIPPABLE0    = 0x184D2A50;
const DWORD LZ4IO_SKIPPABLEMASK = 0xFFFFFFF0;
const DWORD LZ4IO_SKIPPABLE_FS  = 4;            // Size of field "Frame Size"
const DWORD LEGACY_MAGICNUMBER  = 0x184C2102; 
const DWORD LEGACY_BLOCKSIZE    = 8*1024*1024;  // 8 MiB
const DWORD BLOCKUNCOMPRES_FLAG = 0x80000000UL; // LZ4F_BLOCKUNCOMPRESSED_FLAG


enum Format : char {
  ffUnknown = 0,
  ffLegacy  = 1,    // legacy LZ4 frame
  ffActual  = 2,    // actual LZ4 frame
};

__forceinline
bool is_legacy_frame(DWORD magic)
{
  return (magic == LEGACY_MAGICNUMBER) ? true : false;
}

Format check_frame_magic(DWORD magic);
int check_file_header(HANDLE hFile, UINT64 file_size);
int check_file_header(LPCWSTR filename);


struct frame_info;

int get_frame_info(LPCVOID buf, size_t bufsize, frame_info * finfo);
int decode_data_partial(LPCVOID src, size_t srcSize, LPVOID dst, size_t dstSize, size_t dstCapacity);

struct frame_info : public LZ4F_frameInfo_t
{
  size_t  header_size;
  size_t  data_offset;
  bool    is_compressed;
  size_t  data_size;
  
  int init(LPCVOID buf, size_t bufsize) { header_size = 0; return get_frame_info(buf, bufsize, this); }
  LZ4F_frameInfo_t * get_ptr() { return (LZ4F_frameInfo_t *)this; }
  size_t get_header_size() { return header_size; }
};


class decode_context
{
public:
  decode_context();
  ~decode_context();
  int reset();
  int init();
  LZ4F_dctx * get_ctx() { return (LZ4F_dctx *)m_ctx; }
  int get_stage();
  LZ4F_frameInfo_t * get_frame_info();

private:
  LPVOID m_ctx;
  size_t m_ctx_size;
};

class encode_context
{
public:
  static const size_t buffer_size = 5*1024*1024;

  encode_context();
  ~encode_context();
  int reset();
  int init(const LZ4F_preferences_t & prefs);
  LZ4F_cctx * get_ctx() { return (LZ4F_cctx *)m_ctx; }
  int get_stage();
  LZ4F_preferences_t * get_prefs();
  INT64 get_totalInSize();

private:
  LPVOID m_ctx;
  size_t m_ctx_size;
};

} /* namespace */
