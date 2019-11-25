#pragma once

#include <windows.h>
#include "config.hpp"
#include "memory.hpp"
#include "nondynamic.hpp"
#include "noncopyable.hpp"

#define BST_INLINE  __forceinline

#define BST_NOINLINE  __declspec(noinline)

#if _MSC_VER < 1900
#define __func__   __FUNCTION__
#endif

#if _MSC_VER < 1600
#define nullptr  NULL
#endif


#if _MSC_VER < 1600
#define BST_DELETED 
#define BST_NOEXCEPT throw()
#else
#define BST_DELETED  =delete
#define BST_NOEXCEPT noexcept
#endif 


#define BST_MAX(a,b)    (((a) > (b)) ? (a) : (b))
#define BST_MIN(a,b)    (((a) < (b)) ? (a) : (b))


#if _MSC_VER < 1600

namespace bst {
  namespace detail {

    template <bool>
    struct StaticAssertion;

    template <>
    struct StaticAssertion<true>
    {
    };

    template<int>
    struct StaticAssertionTest
    {
    };
  }
}

#define BST_CONCATENATE(arg1, arg2)   BST_CONCATENATE1(arg1, arg2)
#define BST_CONCATENATE1(arg1, arg2)  BST_CONCATENATE2(arg1, arg2)
#define BST_CONCATENATE2(arg1, arg2)  arg1##arg2

#define BST_STATIC_ASSERT(expression, message)\
struct BST_CONCATENATE(__static_assertion_at_line_, __LINE__)\
{\
  bst::detail::StaticAssertion<static_cast<bool>((expression))> \
    BST_CONCATENATE(BST_CONCATENATE(BST_CONCATENATE(STATIC_ASSERTION_FAILED_AT_LINE_, __LINE__), _WITH__), message);\
};\
typedef bst::detail::StaticAssertionTest<sizeof(BST_CONCATENATE(__static_assertion_at_line_, __LINE__))> \
  BST_CONCATENATE(__static_assertion_test_at_line_, __LINE__)

#define static_assert(expression, message) BST_STATIC_ASSERT((expression), _Error_MessageAAA_)

#endif /* _MSC_VER < 1600 */



namespace bst {

template <class T1, class T2>
struct is_same;

template <class T>
struct is_same<T,T>
{
  static const bool value = true;
};

template <class T1, class T2>
struct is_same
{
  static const bool value = false;
};

} /* namespace */
