#define XXHASH_H_5627135585666179
#include "stdafx.h"
#include "wcx_filetree.h"
#undef XXHASH_H_5627135585666179

#include <cassert>


namespace wcx {

#define XXH_INLINE_ALL
#include "xxh3.h"

__forceinline
void TTreeElem::push_subelem(PTreeElem elem) noexcept
{
  if (this->is_dir()) {
    TElemList * elist = this->get_elem_list(elem);
    if (!elist->head)
      elist->head = elem;    /* first elem */

    if (elist->tail)
      elist->tail->next = elem;

    elist->tail = elem;
  }
}

__forceinline
int TTreeElem::set_info(TElemInfo * elem_info) noexcept
{
  if (elem_info)
    info = *elem_info;
  return 0;
}

int TTreeElem::get_dir_num_of_items() noexcept
{
  if (!this->is_dir())
    return -1;

  int count = 0;
  for (TTreeElem * elem = content.node.dir.head; elem != NULL; elem = elem->next) {
    count++;
  }
  for (TTreeElem * elem = content.node.file.head; elem != NULL; elem = elem->next) {
    count++;
  }
  return count;
}

int TTreeElem::get_path(LPWSTR path, size_t path_cap, WCHAR delimiter) noexcept
{
  int hr = -1;
  const size_t prealloc_branch_cap = 512;
  PTreeElem pbuf[prealloc_branch_cap];
  PTreeElem * branch = pbuf;
  size_t branch_cap = prealloc_branch_cap;

  FIN_IF(this->is_root(), -2);  /* path returned without root name */

  size_t path_len = 0;
  size_t depth = 0;
  TTreeElem * e = this;
  do { 
    if (e->is_root())
      break;
    if (depth >= branch_cap - 2) {
      const size_t new_cap = branch_cap + prealloc_branch_cap;
      LPVOID buf = calloc(new_cap, sizeof(TDirEnum));
      FIN_IF(!buf, -4);
      memcpy(buf, branch, branch_cap * sizeof(TDirEnum));
      if (branch != pbuf) {
        free(branch);
      }
      branch = (PTreeElem *)buf;
      branch_cap = new_cap;
    }
    branch[depth++] = e;
    path_len += e->name_len + 1;
  } while (e = e->owner);

  FIN_IF(depth == 0, -6);
  depth--;
  FIN_IF(path_len == 0, -7);
  path_len--;
  if (path) {
    FIN_IF(path_len >= path_cap, -8);
    do {
      TTreeElem * e = branch[depth];
      const size_t nlen = e->name_len;
      memcpy(path, e->name, nlen * sizeof(WCHAR));
      path += nlen;
      *path++ = delimiter;
    } while(depth--);

    path[-1] = 0;
  }
  hr = (int)path_len;

fin:
  if (branch != pbuf)
    free(branch);
  return hr;
}

TTreeElem * TTreeElem::get_prev() noexcept
{
  if (this->is_root())
    return NULL;

  if (!this->owner)
    return NULL;  /* critical error !!! */

  TElemList * elist = this->owner->get_elem_list(this);
  TTreeElem * e_prev = NULL;
  for (TTreeElem * elem = elist->head; elem != NULL; elem = elem->next) {
    if (elem == this)
      return e_prev;
    e_prev = elem;
  }
  return NULL;
}

bool TTreeElem::unlink() noexcept
{
  if (this->is_root())
    return false;

  if (!this->owner)
    return false;  /* critical error !!! */

  TElemList * elist = this->owner->get_elem_list(this);
  if (elist->head == elist->tail) {
    assert(this == elist->head);
    elist->head = NULL;
    elist->tail = NULL;
  } else {
    TTreeElem * e_prev = get_prev();
    if (e_prev) {
      e_prev->next = this->next;
      if (elist->tail == this) {
        assert(this->next == NULL);
        elist->tail = e_prev;
      }
    } else {
      elist->head = this->next;
    }
  }
  return true;
}

// =====================================================================================

FileTree::FileTree() noexcept
{
  m_case_sensitive = false;
  m_use_mm = false;
  m_mm.set_zero_fill(true);
  reset_root_elem();
}

FileTree::~FileTree() noexcept
{
  clear();
}

bool FileTree::unlink_elem(PTreeElem elem) noexcept
{
  if (elem) {
    bool x = elem->unlink();
    if (x && !m_use_mm && !elem->is_root()) {
      free(elem);
    }
    return x;
  }
  return false;
}

void FileTree::destroy_elem(PTreeElem elem, bool total_destroy) noexcept
{
  if (elem) {
    destroy_content(elem, total_destroy);
    m_elem_count--;
    m_capacity -= elem->get_size();
    if (!total_destroy) {
      elem->unlink();      /* remove all links to this elem */
    }
    if (!m_use_mm && !elem->is_root()) {
      free(elem);
    }
  }
}

void FileTree::destroy_content(PTreeElem elem, bool total_destroy) noexcept
{
  if (elem) {
    if (elem->is_dir()) {
      PTreeElem dir = elem->content.node.dir.head;
      while (dir) {      
        PTreeElem next = dir->next;
        destroy_elem(dir, true);
        dir = next;
      }
      PTreeElem file = elem->content.node.file.head;
      while (file) {
        PTreeElem next = file->next;
        destroy_elem(file, true);
        file = next;
      }
    }
    if (!total_destroy) {
      memset(&elem->content, 0, sizeof(elem->content));
      if (elem->is_file())
        elem->flags &= ~EFLAG_FILE_ADDR;    /* remove flag */
    }
  }
}

void FileTree::clear() noexcept
{
  if (m_use_mm) {
    m_mm.destroy();
  } else {
    destroy_content(&m_root, true);
  }
  reset_root_elem();
}

void FileTree::reset_root_elem() noexcept
{
  m_elem_count = 0;
  m_capacity = 0;
  memset(&m_root, 0, sizeof(m_root));
  m_root.flags = EFLAG_ROOT | EFLAG_DIRECTORY;
  if (m_case_sensitive)
    m_root.flags |= EFLAG_NAME_CASE_SENS | EFLAG_CONT_CASE_SENS;
}

void FileTree::set_case_sensitive(bool is_case_sensitive) noexcept
{
  m_case_sensitive = is_case_sensitive;
  clear();
}

__forceinline
static int get_lower_filename(LPCWSTR src, size_t src_len, LPWSTR dst, size_t dst_max_len) noexcept
{
  if (src_len == 0) {
    *dst = 0;
    return 0;
  }
  /* see https://docs.microsoft.com/windows/win32/intl/handling-sorting-in-your-applications#map-strings */
  int nLen = LCMapStringW(LOCALE_INVARIANT, LCMAP_LOWERCASE, src, (int)src_len, dst, (int)dst_max_len);
  if (nLen >= 0) {
    dst[nLen] = 0;
  }
  return nLen;
}

__declspec(noinline)
static SIZE_T get_hash(LPCWSTR name, size_t name_len, bool lower_case) noexcept
{
  __declspec(align(16)) WCHAR namelwr[max_path_component_len + 3];
  if (lower_case && name_len) {
    int nLen = get_lower_filename(name, name_len, namelwr, max_path_component_len);
    if (nLen != (int)name_len)
      return 0;
    name = (LPCWSTR)namelwr;
  }
  return (SIZE_T)XXH3_64bits(name, name_len);
}

TTreeElem * FileTree::find_subelem(PTreeElem base, LPCWSTR name, size_t name_len, bool is_dir) noexcept
{
  if (!base)
    return NULL;

  if (!base->is_dir())
    return NULL;

  TElemList * elist = base->content.node.get_list(is_dir);
  TTreeElem * elem = elist->head;
  if (!elem)
    return NULL;

  SIZE_T const hash_org = get_hash(name, name_len, false);   /* hash for original name */
  SIZE_T const hash_lwr = get_hash(name, name_len, true);    /* hash for lower case name */
  size_t const name_size = name_len * sizeof(WCHAR);
  do {
    if (elem->name_len == name_len) {
      if (elem->is_name_case_sens()) {
        if (elem->name_hash != hash_org)
          continue;
        if (memcmp(elem->name, name, name_size) == 0)
          return elem;
      } else {
        if (hash_lwr && elem->name_hash && elem->name_hash != hash_lwr)
          continue;
        if (StrCmpNIW(elem->name, name, (int)name_len) == 0)
          return elem;
      }
    }
  } while(elem = elem->next);

  return NULL;
}

TTreeElem * FileTree::find_subdir(PTreeElem base, LPCWSTR name, size_t name_len) noexcept
{
  return find_subelem(base, name, name_len, true);
}

TTreeElem * FileTree::find_file(PTreeElem base, LPCWSTR name, size_t name_len) noexcept
{
  return find_subelem(base, name, name_len, false);
}

__forceinline
int TTreeElem::set_name(LPCWSTR elem_name, size_t enlen) noexcept
{
  if (owner->is_content_case_sens()) {
    flags |= EFLAG_NAME_CASE_SENS;
    if (is_dir())
      flags |= EFLAG_CONT_CASE_SENS;
  }
  if (!elem_name) {
    name_len = 0;
    name[0] = 0;
    name_hash = 0;
  } else {
    name_len = (UINT16)enlen;
    memcpy(name, elem_name, enlen * sizeof(WCHAR));
    name[enlen] = 0;
    name_hash = get_hash(elem_name, enlen, is_name_case_sens() ? false : true);
  }
  return 0;
}

__forceinline
TTreeElem * FileTree::create_elem(PTreeElem owner, LPCWSTR name, size_t name_len, UINT16 flags)	noexcept
{
  size_t const elem_size = sizeof(TTreeElem) + name_len * sizeof(WCHAR);
  TTreeElem * elem;
  if (m_use_mm) {
    elem = (TTreeElem *)m_mm.alloc(elem_size);
  } else {
    elem = (TTreeElem *)calloc(1, elem_size);
  }
  if (!elem)
    return NULL;

  elem->owner = owner;
  elem->flags = flags;
  elem->set_name(name, name_len);
  owner->push_subelem(elem);
  m_elem_count++;
  m_capacity += elem->get_size();
  return elem;
}

TTreeElem * FileTree::add_dir(PTreeElem owner, LPCWSTR name, size_t name_len, DWORD attr) noexcept
{
  if (!name_len || name_len > max_path_component_len)
    return NULL;

  TTreeElem * elem = find_subdir(owner, name, name_len);
  if (elem)
    return elem;   /* subdir already exist */

  elem = create_elem(owner, name, name_len, EFLAG_DIRECTORY);
  if (elem) {
    elem->info.attr = attr | FILE_ATTRIBUTE_DIRECTORY;
  }
  return elem;
}

TTreeElem * FileTree::add_file(PTreeElem owner, LPCWSTR name, size_t name_len, TElemInfo * info) noexcept
{
  if (!name_len || name_len > max_path_component_len)
    return NULL;

  TTreeElem * elem = create_elem(owner, name, name_len, 0);
  if (elem) {
    //elem->content.addr.fileid = 0;    file addr not supported !!!
    elem->set_info(info);
  }
  return elem;
}

int FileTree::add_file(LPCWSTR fullname, DWORD attr, TElemInfo * info, OUT PTreeElem * lpElem) noexcept
{
  int hr = -1;
  TTreeElem * elem = &m_root;

  FIN_IF(is_overfill(), -1);
  FIN_IF(!fullname, -2);
  FIN_IF(fullname[0] == 0, -3);

  //WLOGd(L"%S: '%s'", __func__, fullname);
  size_t nlen = 0;
  LPCWSTR name = fullname;
  for (LPCWSTR fn = name; /*nothing*/; fn++) {
    WCHAR const symbol = *fn;
    if (symbol == L'/') {
      if (nlen) {
        elem = add_dir(elem, name, nlen, 0);
        FIN_IF(!elem, -10);
      }
      name = fn + 1;
      nlen = 0;
      continue;   /* skip delimiter */
    }
    if (symbol == 0) {
      FIN_IF(nlen == 0, -20);
      if (attr & FILE_ATTRIBUTE_DIRECTORY) {
        elem = add_dir(elem, name, nlen, 0);
        FIN_IF(!elem, -21);
        elem->set_info(info);
      } else {
        elem = add_file(elem, name, nlen, info);
        FIN_IF(!elem, -25);
      }
      if (lpElem)
        *lpElem = elem;
      break;
    }
    nlen++;
  }
  hr = 0;  

fin:
  LOGe_IF(hr, "%s: ERROR = %d", __func__, hr);
  return hr;
}

TTreeElem * FileTree::find_directory(LPCWSTR curdir, WCHAR delimiter) noexcept
{
  int hr = -1;
  TTreeElem * elem = &m_root;

  if (curdir && curdir[0]) {
    size_t nlen = 0;
    LPCWSTR name = curdir;
    for (LPCWSTR fn = curdir; /*nothing*/; fn++) {
      WCHAR const symbol = *fn;
      if (symbol == delimiter || symbol == 0) {
        if (nlen) {
          elem = find_subdir(elem, name, nlen);
          FIN_IF(!elem, -10);          
        }
        if (symbol == 0)
          break;
        name = fn + 1;
        nlen = 0;
        continue;   /* skip delimiters */
      }
      nlen++;
    }
  }
  hr = 0;

fin:
  WLOGe_IF(hr, L"%S: ERROR = %d, curdir = '%s' ", __func__, hr, curdir);
  return hr ? NULL : elem;
}

bool FileTree::find_directory(TDirEnum & direnum, LPCWSTR curdir, WCHAR delimiter) noexcept
{
  direnum.reset();
  direnum.owner = find_directory(curdir, delimiter);
  return direnum.owner ? true : false;
};

bool FileTree::find_directory(TTreeEnum & tenum, LPCWSTR curdir, WCHAR delimiter, size_t max_depth) noexcept
{
  TTreeElem * dir = find_directory(curdir, delimiter);
  tenum.init(dir, max_depth);
  return dir ? true : false;
};

// ===========================================================================================

TTreeElem * TDirEnum::get_next() noexcept
{
  if (owner) {
    if (!dir) {
      dir = owner->content.node.dir.head;
      if (dir)
        return dir;
    }
    if (dir && dir->next) {
      dir = dir->next;
      return dir;
    }
    if (!file) {
      file = owner->content.node.file.head;
      if (file)
        return file;
    }
    if (file && file->next) {
      file = file->next;
      return file;
    }
  }
  return NULL;
};

// ===========================================================================================

TTreeElem * TTreeEnum::get_next() noexcept
{
  int hr = 0;
  TTreeElem * elem = NULL;

  FIN_IF(!m_root, -1);

  TDirEnum * direnum = &m_path[m_cur_depth];
  elem = direnum->get_next();
  while (!elem) {
    FIN_IF(m_cur_depth == 0, 0);  /* END */
    m_cur_depth--;
    direnum = &m_path[m_cur_depth];
    elem = direnum->get_next();
  }
  if (elem->is_dir()) {
    if (m_cur_depth < m_max_depth) {
      if (m_cur_depth >= m_path_cap - 2) {
        const size_t new_cap = m_path_cap + prealloc_path_depth;
        LPVOID buf = calloc(new_cap, sizeof(TDirEnum));
        FIN_IF(!buf, 1);   /* skip next dir level */
        memcpy(buf, m_path, m_path_cap * sizeof(TDirEnum));
        if (m_path != m_buf) {
          free(m_path);
        }
        m_path = (TDirEnum *)buf;
        m_path_cap = new_cap;
      }
      m_cur_depth++;
      m_path[m_cur_depth].init(elem);
    }
  }
  return elem;

fin:
  LOGe_IF(hr, "%s: ERROR = %d", __func__, hr);
  return (hr <= 0) ? NULL : elem;
}

} /* namespace */
