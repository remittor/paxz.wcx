#pragma once

namespace bst {

namespace noncopyable_ { // protection from unintended ADL

  class noncopyable
  {
    protected:
      noncopyable() {}
      ~noncopyable() {}

    private:
      noncopyable( const noncopyable& );
      const noncopyable& operator=( const noncopyable& );
  };

}

typedef noncopyable_::noncopyable noncopyable;

} /* namespace */
