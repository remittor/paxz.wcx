#pragma once

#include "core.hpp" 
#include "noncopyable.hpp"

namespace bst {

class nondynamic: noncopyable
{
private:
  // disallow operator new
  static void * operator new(size_t);
  static void * operator new[](size_t);

  // disallow operator delete
  static void operator delete(void *) { };
  static void operator delete[](void *) { };

  // disallow address-of
  const nondynamic* operator&() const;
  /* */ nondynamic* operator&() /* */;

  // disallow ->
  const nondynamic* operator->() const;
  /* */ nondynamic* operator->() /* */;
};

} /* namespace */