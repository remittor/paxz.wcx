#pragma once

#include "bst.hpp"

namespace bst {

#ifndef RTL_SRWLOCK_INIT
typedef struct _RTL_SRWLOCK {
  PVOID Ptr;
} RTL_SRWLOCK, *PRTL_SRWLOCK;
#define RTL_SRWLOCK_INIT {0}
#endif

class srw_lock : private noncopyable
{
public:
  srw_lock() BST_NOEXCEPT;
  ~srw_lock() BST_NOEXCEPT;

  void lock_shared() BST_NOEXCEPT;
  void unlock_shared() BST_NOEXCEPT;
  
  void lock_exclusive() BST_NOEXCEPT;
  void unlock_exclusive() BST_NOEXCEPT;
  
  void unlock() BST_NOEXCEPT;

  void read_lock() BST_NOEXCEPT
  {
    lock_shared();
  }

  void read_unlock() BST_NOEXCEPT
  {
    unlock_shared();
  }

  void write_lock() BST_NOEXCEPT
  {
    lock_exclusive();
  }

  void write_unlock() BST_NOEXCEPT
  {
    unlock_exclusive();
  }

private:
  RTL_SRWLOCK m_lock;
};

typedef srw_lock  srw_mutex;


struct read_lock_t { };
const read_lock_t read_lock = {};

struct write_lock_t { };
const write_lock_t write_lock = {};


class scoped_read_lock : private noncopyable
{
public:
  scoped_read_lock() BST_DELETED;  // disable default ctor

  explicit scoped_read_lock(srw_mutex & mutex) BST_NOEXCEPT : m_mutex(mutex)
  {
    m_mutex.read_lock();
  }
  
  ~scoped_read_lock() BST_NOEXCEPT
  {
    m_mutex.read_unlock();
  }

  void lock() BST_NOEXCEPT
  {
    m_mutex.read_lock();
  }

  void unlock() BST_NOEXCEPT
  {
    m_mutex.read_unlock();
  }

  srw_mutex & get_mutex() BST_NOEXCEPT
  {
    return m_mutex;
  }

private:
  srw_mutex & m_mutex;
};


class scoped_write_lock : private noncopyable
{
public:
  scoped_write_lock() BST_DELETED;  // disable default ctor

  explicit scoped_write_lock(srw_mutex & mutex) BST_NOEXCEPT : m_mutex(mutex)
  {
    m_mutex.write_lock();
  }

  ~scoped_write_lock() BST_NOEXCEPT
  {
    m_mutex.write_unlock();
  }

  void lock() BST_NOEXCEPT
  {
    m_mutex.write_lock();
  }

  void unlock() BST_NOEXCEPT
  {
    m_mutex.write_unlock();
  }

  srw_mutex & get_mutex() BST_NOEXCEPT
  {
    return m_mutex;
  }

private:
  srw_mutex & m_mutex;
};


} /* namespace */
