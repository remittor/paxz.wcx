#pragma once

#include <windows.h>
#include "lz4lib.h"
#include "tar.h"


namespace paxz { 

const DWORD FRAME_ROOT           = 0x184D2A55;
const DWORD FRAME_SIZE_SIZE      = lz4::LZ4IO_SKIPPABLE_FS;
const DWORD FRAME_NAME_LEN       = 4;
const DWORD FRAME_ROOT_NAME_LEN  = FRAME_NAME_LEN;
const char  FRAME_ROOT_NAME[]    = "PAX\0";
const DWORD FRAME_DICT_NAME_LEN  = FRAME_NAME_LEN;
const char  FRAME_DICT_NAME[]    = "DIC\0";
const DWORD FRAME_END_NAME_LEN   = FRAME_NAME_LEN;
const char  FRAME_END_NAME[]     = "END\0";

const BYTE FLAG_USE_DICTIONARY   = 0x01;
const BYTE FLAG_CIPHER_NO_SALT   = 0x02;
const BYTE FLAG_CIPHER_FILE_NAME = 0x04;

const char  SALT_PREFIX[]        = "Salted__";      /* from OpenSSL */
const DWORD SALT_PREFIX_SIZE     = 8;
const DWORD SALT_BODY_SIZE       = 8;               /* PKCS5_SALT_LEN = 8 */
const DWORD SALT_SIZE            = SALT_PREFIX_SIZE + SALT_BODY_SIZE;

/* ASN.1 types */
const char SN_chacha20[]         = "ChaCha20";
const char LN_chacha20[]         = "chacha20";
const UINT16 NID_chacha20        = 1019; 


#pragma pack(push, 1)

struct frame_prefix {
  DWORD    magic;         /* see FRAME_ROOT */
  DWORD    size;          /* see FRAME_SIZE_SIZE */
};

struct frame_root : public frame_prefix {
  char     name[FRAME_ROOT_NAME_LEN];
  BYTE     version;
  BYTE     flags;
  UINT16   cipher_algo;   /* 0 = cipher not used */
  UINT16   pbkdf2_iter;   /* iteration count for PBKDF2 algo */
  DWORD    checksum;      /* XXHASH checksum of frame_root */
  
  int init_pax(BYTE aVersion, BYTE aFlags, bool aCipher = false);
  int init_end(BYTE aVersion);
  bool is_valid(BYTE min_version);
};

struct frame_dict : public frame_prefix {
  char     name[FRAME_DICT_NAME_LEN];
  BYTE     version;
  BYTE     flags;
  // ???????      // TODO: support packing with dictionary 
};

#pragma pack(pop)

inline int frame_root::init_pax(BYTE aVersion, BYTE aFlags, bool aCipher)
{
  magic = FRAME_ROOT;
  {
    size = sizeof(frame_root) - sizeof(frame_prefix);
    memcpy(name, FRAME_ROOT_NAME, FRAME_ROOT_NAME_LEN);
    version = aVersion;
    flags = aFlags;  
    if (aCipher) {
      cipher_algo = NID_chacha20;
      pbkdf2_iter = 0;   // TODO: support PBKDF2 algo
    } else {
      cipher_algo = 0;
      pbkdf2_iter = 0;
    }
  }
  checksum = XXH32(&magic, sizeof(frame_root) - sizeof(DWORD), 0);
  return (int)sizeof(frame_root);
}

inline int frame_root::init_end(BYTE aVersion)
{
  magic = FRAME_ROOT;
  {
    size = sizeof(frame_root) - sizeof(frame_prefix);
    memcpy(name, FRAME_END_NAME, FRAME_END_NAME_LEN);
    version = aVersion;
    flags = 0;
    cipher_algo = 0;
    pbkdf2_iter = 0;
  }
  checksum = XXH32(&magic, sizeof(frame_root) - sizeof(DWORD), 0);
  return (int)sizeof(frame_root);
}

inline bool frame_root::is_valid(BYTE min_version)
{
  if (magic != FRAME_ROOT)
    return false;
  if (size != sizeof(paxz::frame_root) - sizeof(paxz::frame_prefix))
    return false;
  if (memcmp(name, paxz::FRAME_ROOT_NAME, paxz::FRAME_ROOT_NAME_LEN) != 0)
    return false;
  if (version < min_version)
    return false;
  DWORD frame_hash = XXH32(&magic, sizeof(paxz::frame_root) - sizeof(DWORD), 0);
  if (frame_hash != checksum)
    return false;

  return true;
}

} /* namespace */
