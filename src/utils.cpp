#include "stdafx.h"
#include "utils.h"
#include <vadefs.h>


int get_full_filename(LPCWSTR path, LPCWSTR name, LPWSTR fullname, size_t fullname_max_len)
{
  size_t pathlen = path ? wcslen(path) : 0;
  size_t namelen = wcslen(name);
  size_t fnlen = pathlen + namelen;
  LPCWSTR fn = path ? path : name;
  LPWSTR p = fullname;

  p[0] = 0;
  if (fnlen >= UNICODE_STRING_MAX_CHARS)
    return -9;

  if (fnlen >= fullname_max_len)
    return -1;

  if (fnlen >= 250) {
    if (fn[1] == L':' && fn[2] == L'\\') {
      if (fnlen + 4 >= fullname_max_len)
        return -2;
      memcpy(p, L"\\\\?\\", 4*sizeof(WCHAR));
      p += 4;
      if (path) {
        memcpy(p, path, pathlen * sizeof(WCHAR));
        p += pathlen;
      }
      memcpy(p, name, namelen * sizeof(WCHAR));
      p += namelen;
      *p = 0;
      return 0;
    }  
    if (fn[0] == L'\\' && fn[1] == L'\\' && fn[2] != '?') {
      if (fnlen + 8 >= fullname_max_len)
        return -3;
      wcscpy(p, L"\\\\?\\UNC\\");
      if (path) {
        wcscat(p, path + 2);
        wcscat(p, name);
      } else {
        wcscat(p, name + 2);
      }
      return 0;
    }
    return -4;
  }

  if (path) {
    memcpy(p, path, pathlen * sizeof(WCHAR));
    p += pathlen;
  }
  memcpy(p, name, namelen * sizeof(WCHAR));
  p += namelen;
  *p = 0;
  return 0;
}

int get_full_filename(LPCWSTR fn, LPWSTR fullname, size_t fullname_max_len)
{
  return get_full_filename(NULL, fn, fullname, fullname_max_len);
}

int get_full_filename(LPCWSTR fn, LPWSTR * fullname)
{
  int hr = 0;
  size_t fnlen = wcslen(fn);
  LPWSTR buf = (LPWSTR) calloc(fnlen + 32, sizeof(WCHAR));
  FIN_IF(!buf, -10);
  hr = get_full_filename(NULL, fn, buf, fnlen + 15);
  FIN_IF(hr, hr);
  *fullname = buf;

fin:
  if (hr && buf) free(buf);
  return hr;
}

int get_full_filename(LPCWSTR path, LPCWSTR name, bst::wstr & fullname)
{
  size_t fnlen = path ? wcslen(path) : 0;
  fnlen += wcslen(name);
  size_t cap = fullname.reserve(fnlen + 32);
  if (cap == bst::npos)
    return -10;
  int hr = get_full_filename(path, name, fullname.data(), fnlen + 15);
  if (hr) {
    fullname.clear();
    return hr;
  }
  fullname.fix_length();
  return 0;
}

int get_full_filename(LPCWSTR fn, bst::wstr & fullname)
{
  return get_full_filename(NULL, fn, fullname);
}

int get_full_filename(LPCWSTR path, LPCWSTR name, bst::filepath & fullname)
{
  size_t fnlen = path ? wcslen(path) : 0;
  fnlen += wcslen(name);
  if (fnlen + 32 > fullname.capacity())
    return -10;
  int hr = get_full_filename(path, name, fullname.data(), fnlen + 16);
  if (hr) {
    fullname.clear();
    return hr;
  }
  fullname.fix_length();
  return 0;
}

int get_full_filename(LPCWSTR fn, bst::filepath & fullname)
{
  return get_full_filename(NULL, fn, fullname);
}

