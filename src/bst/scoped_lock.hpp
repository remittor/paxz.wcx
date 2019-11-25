#pragma once

#include "noncopyable.hpp"

namespace bst {

// Helper class to lock and unlock a mutex automatically.
template <typename Mutex>
class scoped_lock : private noncopyable
{
public:
  // Constructor acquires the lock.
  explicit scoped_lock(Mutex & m, bool initially_locked = true) : m_mutex(m)
  {
    m_locked = false;
    if (initially_locked) {
      m_mutex.lock();
      m_locked = true;
    }
  }

  // Destructor releases the lock.
  ~scoped_lock()
  {
    if (m_locked)
      m_mutex.unlock();
  }

  // Explicitly acquire the lock.
  void lock()
  {
    if (!m_locked) {
      m_mutex.lock();
      m_locked = true;
    }
  }

  // Explicitly release the lock.
  void unlock()
  {
    if (m_locked) {
      m_mutex.unlock();
      m_locked = false;
    }
  }

  // Test whether the lock is held.
  bool locked() const
  {
    return m_locked;
  }

  // Get the underlying mutex.
  Mutex & mutex()
  {
    return m_mutex;
  }

private:
  // The underlying mutex.
  Mutex & m_mutex;

  // Whether the mutex is currently locked or unlocked.
  bool m_locked;
};

} /* namespace */
