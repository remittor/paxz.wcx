#pragma once

#include "core.hpp"

namespace bst {

namespace detail { // protection from unintended ADL

  class noncopyable
  {
  protected:
    noncopyable() { }
    ~noncopyable() { }

  private:
    noncopyable( const noncopyable& );
    const noncopyable& operator=( const noncopyable& );
  };

}

typedef detail::noncopyable noncopyable;

} /* namespace */
