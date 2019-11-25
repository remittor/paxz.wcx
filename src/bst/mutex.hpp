#pragma once

#include "scoped_lock.hpp"

namespace bst {

class mutex : private noncopyable
{
public:
  static const unsigned int def_spin_count = 0x1000;

  typedef scoped_lock<mutex> scoped_lock;

  mutex()
  {
    InitializeCriticalSectionAndSpinCount(&m_crit_section, def_spin_count);
  }
  
  explicit mutex(unsigned int spincount)
  {
    InitializeCriticalSectionAndSpinCount(&m_crit_section, spincount);
  }  

  ~mutex()
  {
    DeleteCriticalSection(&m_crit_section);
  }

  void lock()
  {
    EnterCriticalSection(&m_crit_section);
  }

  void unlock()
  {
    LeaveCriticalSection(&m_crit_section);
  }

private:

  CRITICAL_SECTION m_crit_section;
};

} /* namespace */
