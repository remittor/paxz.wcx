#pragma once

#include <windows.h>
#include "core.hpp"

namespace bst {

BST_FORCEINLINE
void * malloc(size_t size)
{
  return ::malloc(size);
}

BST_FORCEINLINE
void * calloc(size_t elem_count, size_t elem_size)
{
  return ::calloc(elem_count, elem_size);
}

BST_FORCEINLINE
void * realloc(void * ptr, size_t newsize)
{
  return ::realloc(ptr, newsize);
}

BST_FORCEINLINE
void free(void * ptr)
{
  ::free(ptr);
}

struct nothrow_t { };
const nothrow_t nothrow = {};

} /* namespace bst */


inline void * operator new(size_t size, const bst::nothrow_t &) BST_NOEXCEPT
{
  return ::malloc(size);
}

/* without this delete operator MSVC output warning:
   no matching operator delete found; memory will not be freed if initialization throws an exception */

inline void operator delete(void * ptr, const bst::nothrow_t &) BST_NOEXCEPT
{
  ::free(ptr);
}
