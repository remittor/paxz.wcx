#pragma once

#include <windows.h>
#include "bst\string.hpp"

__forceinline
int GetTickBetween(DWORD prev, DWORD now)
{
  if (prev == now)
    return 0;
  if (prev < now)
    return (int)(now - prev);
  return (int)((0xFFFFFFFF - prev) + now); 
}


int get_full_filename(LPCWSTR path, LPCWSTR name, LPWSTR fullname, size_t fullname_max_len);
int get_full_filename(LPCWSTR fn, LPWSTR fullname, size_t fullname_max_len);
int get_full_filename(LPCWSTR fn, LPWSTR * fullname);

int get_full_filename(LPCWSTR path, LPCWSTR name, bst::wstr & fullname);
int get_full_filename(LPCWSTR fn, bst::wstr & fullname);

int get_full_filename(LPCWSTR path, LPCWSTR name, bst::filepath & fullname);
int get_full_filename(LPCWSTR fn, bst::filepath & fullname);

