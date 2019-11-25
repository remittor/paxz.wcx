#pragma once
 
#include "noncopyable.hpp"

namespace bst {

class nondynamic: noncopyable
{
public:
  nondynamic() {}
  ~nondynamic() {}

private:

  void * operator new(size_t size)
  {
    (void)size;
    return 0;
  }

  void operator delete(void * p)
  {
    delete p;
  }

};

} /* namespace */