#pragma once

#include <windows.h>
#include "lz4lib.h"
#include "tar.h"


namespace paxz { 

const char  CURRENT_VERSION      = 0;       /* current version on PAXZ global header format */

const DWORD FRAME_MAGIC          = 0x184D2A55;
const DWORD FRAME_SIZE_SIZE      = 4;
const DWORD FRAME_NAME_LEN       = 3;
const char  FRAME_GLB_NAME[]     = "PAX";   /* Global paxz frame */
const char  FRAME_DIC_NAME[]     = "DIC";   /* paxz frame for dictionary */
const char  FRAME_END_NAME[]     = "END";   /* Footer paxz frame */

const UINT16 FLAG_DICT_FOR_HEADER  = 0x0002;
const UINT16 FLAG_DICT_FOR_CONTENT = 0x0004;
const UINT16 FLAG_CIPHER_NO_SALT   = 0x0100;
const UINT16 FLAG_CIPHER_HEADER    = 0x0200;

const char  SALT_PREFIX[]        = "Salted__";      /* from OpenSSL */
const DWORD SALT_PREFIX_SIZE     = 8;
const DWORD SALT_BODY_SIZE       = 8;               /* PKCS5_SALT_LEN = 8 */
const DWORD SALT_SIZE            = SALT_PREFIX_SIZE + SALT_BODY_SIZE;

/* ASN.1 types */
const char SN_chacha20[]         = "ChaCha20";
const char LN_chacha20[]         = "chacha20";
const UINT16 NID_chacha20        = 1019; 


#pragma pack(push, 1)

struct frame_prefix
{
  DWORD    magic;         /* see FRAME_ROOT */
  DWORD    size;          /* see FRAME_SIZE_SIZE */
};

struct frame_pax : public frame_prefix
{
  char     name[FRAME_NAME_LEN];
  BYTE     version;
  UINT16   flags;
  UINT16   cipher_algo;   /* 0 = cipher not used */
  UINT16   pbkdf2_iter;   /* iteration count for PBKDF2 algo */
  DWORD    checksum;      /* XXHASH32 checksum of frame_root */
  
  int init(char aVersion, int aFlags, bool aCipher = false);
  bool is_valid(char min_version, char max_version);
};

struct frame_end : public frame_pax
{
  int init(char aVersion);
};

struct frame_dic : public frame_prefix {
  char     name[FRAME_NAME_LEN];
  BYTE     version;
  UINT16   flags;
  // ???????      // TODO: support packing with dictionary 
};

#pragma pack(pop)

inline int frame_pax::init(char aVersion, int aFlags, bool aCipher)
{
  magic = FRAME_MAGIC;
  {
    size = sizeof(frame_pax) - sizeof(frame_prefix);
    memcpy(name, FRAME_GLB_NAME, FRAME_NAME_LEN);
    version = aVersion;
    flags = (UINT16)aFlags;
    if (aCipher) {
      cipher_algo = NID_chacha20;
      pbkdf2_iter = 0;   // TODO: support PBKDF2 algo
    } else {
      cipher_algo = 0;
      pbkdf2_iter = 0;
    }
  }
  checksum = XXH32(&magic, sizeof(frame_pax) - sizeof(checksum), 0);
  return (int)sizeof(frame_pax);
}

inline bool frame_pax::is_valid(char min_version, char max_version)
{
  if (magic != FRAME_MAGIC)
    return false;
  if (size != sizeof(paxz::frame_pax) - sizeof(paxz::frame_prefix))
    return false;
  if (memcmp(name, paxz::FRAME_GLB_NAME, paxz::FRAME_NAME_LEN) != 0)
    return false;
  if (version < (BYTE)min_version || version > (BYTE)max_version)
    return false;
  if (checksum != XXH32(&magic, sizeof(paxz::frame_pax) - sizeof(checksum), 0))
    return false;

  return true;
}

inline int frame_end::init(char aVersion)
{
  magic = FRAME_MAGIC;
  {
    size = sizeof(frame_end) - sizeof(frame_prefix);
    memcpy(name, paxz::FRAME_END_NAME, paxz::FRAME_NAME_LEN);
    version = aVersion;
    flags = 0;
    cipher_algo = 0;
    pbkdf2_iter = 0;
  }
  checksum = XXH32(&magic, sizeof(frame_end) - sizeof(checksum), 0);
  return (int)sizeof(frame_end);
}

} /* namespace */
