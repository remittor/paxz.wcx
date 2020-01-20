#pragma once

#include <windows.h>
#include "wcx_simplemm.h"


namespace wcx {

/* https://docs.microsoft.com/windows/win32/api/fileapi/nf-fileapi-getvolumeinformationw */
const int max_path_component_len = 255;

const UINT16 EFLAG_DIRECTORY      = 0x0001;
const UINT16 EFLAG_FILE_ADDR      = 0x0002;     /* file elem have addr */
const UINT16 EFLAG_NAME_CASE_SENS = 0x0004;     /* elem name is case sensitive (ala UNIX) */
const UINT16 EFLAG_CONT_CASE_SENS = 0x0008;     /* dir content is case sensitive (ala UNIX) */
const UINT16 EFLAG_ROOT           = 0x0010;


#pragma pack(push, 1)

struct TTreeElem;   /* forward declaration */
typedef TTreeElem * PTreeElem;


struct TElemList {
  TTreeElem   * head;
  TTreeElem   * tail;
};
typedef TElemList * PElemList;


struct TTreeNode {
  TElemList     dir;      /* subdir list */
  TElemList     file;     /* file list */

  TElemList * get_list(bool dirlist) { return dirlist ? &dir : &file; }
};
typedef TTreeNode * PTreeNode;


struct TFileAddr {
  UINT64    fileid;
};

struct TElemInfo {
  struct {
    UINT64   pos;
    UINT32   hdr_p_size;   /* size of packed_header */
    UINT32   hdr_size;     /* size of orig PAX header */
    UINT64   realsize;     /* SCHILY.realsize */
  } pax;
  UINT64     data_size;    /* orig content size */
  UINT64     pack_size;    /* total_size = packed_header + packed_content */
  DWORD      attr;         /* windows file attributes */
  INT64      ctime;        /* creation time */
  INT64      mtime;        /* modification time */
};

struct TTreeElem {
  TTreeElem     * next;         /* NULL for last element */
  TTreeElem     * owner;        /* link to owner dir */
  union {
    TTreeNode     node;         /* only for directories */
    TFileAddr     addr;         /* only for files */
  } content;
  UINT16          flags;
  TElemInfo       info;
  SIZE_T          name_hash;    /* hash for original name or hash for lower case name (see EFLAG_NAME_CASE_SENS) */
  UINT16          name_len;     /* length of name (number of character without zero-termination) */
  WCHAR           name[1];      /* renaming possible, but required realloc struct for longer names */

  bool is_root() { return (flags & EFLAG_ROOT) != 0; }
  bool is_dir()  { return (flags & EFLAG_DIRECTORY) != 0; }
  bool is_file() { return (flags & EFLAG_DIRECTORY) == 0; }
  bool is_name_case_sens() { return (flags & EFLAG_NAME_CASE_SENS) != 0; }
  bool is_content_case_sens() { return (flags & EFLAG_CONT_CASE_SENS) != 0; }
  TElemList * get_elem_list(bool dirlist) { return is_dir() ? content.node.get_list(dirlist) : NULL; }
  TElemList * get_elem_list(TTreeElem * base) { return get_elem_list(base->is_dir()); }
  void push_subelem(PTreeElem elem) noexcept;
  int set_info(TElemInfo * elem_info) noexcept;
  int set_name(LPCWSTR elem_name, size_t elem_name_len) noexcept;
  int get_dir_num_of_items() noexcept;
  int get_path(LPWSTR path, size_t path_cap, WCHAR delimiter = L'\\') noexcept;
  TTreeElem * get_prev() noexcept;
  TTreeElem * get_next() noexcept { return next; }
  bool unlink() noexcept;
  size_t get_size() noexcept { return sizeof(TTreeElem) + name_len * sizeof(WCHAR); }
};

#pragma pack(pop)

// ==================================================================================

struct TDirEnum;   /* forward declaration */
typedef TDirEnum * PDirEnum;

class TTreeEnum;  /* forward declaration */


class FileTree
{
public:
  friend class TTreeEnum;

  FileTree() noexcept;
  ~FileTree() noexcept;

  void clear() noexcept;
  void set_case_sensitive(bool is_case_sensitive) noexcept;
  void set_internal_mm(bool enabled) { m_use_mm = enabled; }

  int add_file(LPCWSTR fullname, DWORD attr, TElemInfo * info, OUT PTreeElem * lpElem) noexcept;
  TTreeElem * find_directory(LPCWSTR curdir, WCHAR delimiter = L'/') noexcept;
  bool find_directory(TDirEnum & direnum, LPCWSTR curdir, WCHAR delimiter = L'/') noexcept;
  bool find_directory(TTreeEnum & tenum, LPCWSTR curdir, WCHAR delimiter = L'/', size_t max_depth = 0) noexcept;

  size_t get_num_elem() { return m_elem_count; }
  size_t get_capacity() { return m_capacity; }
  bool is_overfill() { return m_elem_count == SIZE_MAX - 1; }

protected:
  TTreeElem * get_root() { return &m_root; }

private:
  void reset_root_elem() noexcept;
  void destroy_elem(PTreeElem elem, bool total_destroy = false) noexcept;
  void destroy_content(PTreeElem elem, bool total_destroy = false) noexcept;
  bool unlink_elem(PTreeElem elem) noexcept;

  TTreeElem * find_subelem(PTreeElem base, LPCWSTR name, size_t name_len, bool is_dir) noexcept;
  TTreeElem * find_subdir(PTreeElem base, LPCWSTR name, size_t name_len) noexcept;
  TTreeElem * find_file(PTreeElem base, LPCWSTR name, size_t name_len) noexcept;

  TTreeElem * create_elem(PTreeElem owner, LPCWSTR name, size_t name_len, UINT16 flags) noexcept;
  TTreeElem * add_dir(PTreeElem owner, LPCWSTR name, size_t name_len, DWORD attr) noexcept;
  TTreeElem * add_file(PTreeElem owner, LPCWSTR name, size_t name_len, TElemInfo * info) noexcept;

  TTreeElem * get_last_elem(PTreeElem base, bool dirlist) { return base->is_dir() ? base->content.node.get_list(dirlist)->tail : NULL; }

  TTreeElem   m_root;
  size_t      m_elem_count;
  size_t      m_capacity;
  simplemm    m_mm;

  bool        m_case_sensitive;
  bool        m_use_mm;    /* use simple memory manager */
};

// ==================================================================================

#pragma pack(push, 1)

struct TDirEnum {
  TTreeElem * owner;  /* cur owner-dir */
  TTreeElem * dir;    /* cur subdir in owner */
  TTreeElem * file;   /* cur file in owner */

  void init(TTreeElem * base) noexcept { owner = base; reset(); }
  void reset() noexcept { dir = NULL; file = NULL; }
  bool is_inited() { return owner != NULL; }
  int get_num_of_items() noexcept { return owner ? owner->get_dir_num_of_items() : -1; }
  TTreeElem * get_next() noexcept;
};

#pragma pack(pop)


class TTreeEnum
{
private:
  static const size_t prealloc_path_depth = 256;

  void set_defaults() noexcept
  {
    m_root = NULL;
    m_path = m_buf;
    m_path_cap = prealloc_path_depth;
  }

public:
  TTreeEnum() noexcept
  {
    set_defaults();
  }

  TTreeEnum(TTreeElem * root) noexcept
  {
    set_defaults();
    init(root);
  }

  ~TTreeEnum() noexcept
  {
    if (m_path != m_buf)
      free(m_path);
  }

  bool init(TTreeElem * root, size_t max_depth = 0) noexcept
  {
    m_root = (root && root->is_dir()) ? root : NULL;
    reset(max_depth);
    return is_inited();
  }

  bool reset(size_t max_depth = 0) noexcept
  {
    m_max_depth = (max_depth == 0) ? INT_MAX : max_depth - 1;
    m_cur_depth = 0;
    memset(m_path, 0, m_path_cap * sizeof(TDirEnum));
    m_path[0].init(m_root);
    return is_inited();
  }

  bool is_inited() { return m_root != NULL; }
  TTreeElem * TTreeEnum::get_next() noexcept;

private:
  TTreeElem * m_root;
  TDirEnum  * m_path;
  size_t      m_path_cap;
  TDirEnum    m_buf[prealloc_path_depth];
  size_t      m_max_depth;
  size_t      m_cur_depth;
};


} /* namespace */
