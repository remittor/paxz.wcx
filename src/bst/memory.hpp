#pragma once

#include <windows.h>

namespace bst {

inline void * malloc(size_t size)
{
  return ::malloc(size);
}

inline void * calloc(size_t elem_count, size_t elem_size)
{
  return ::calloc(elem_count, elem_size);
}

inline void * realloc(void * ptr, size_t newsize)
{
  return ::realloc(ptr, newsize);
}

inline void free(void * ptr)
{
  ::free(ptr);
}

} /* namespace */
