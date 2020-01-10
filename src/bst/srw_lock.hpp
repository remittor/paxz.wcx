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
  srw_lock();
  ~srw_lock();

  void lock_shared();
  void unlock_shared();
  
  void lock_exclusive();
  void unlock_exclusive();
  
  void unlock();

  void read_lock()
  {
    lock_shared();
  }

  void read_unlock()
  {
    unlock_shared();
  }

  void write_lock()
  {
    lock_exclusive();
  }

  void write_unlock()
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

  explicit scoped_read_lock(srw_mutex & mutex) : m_mutex(mutex)
  {
    m_mutex.read_lock();
  }
  
  ~scoped_read_lock()
  {
    m_mutex.read_unlock();
  }

  void lock()
  {
    m_mutex.read_lock();
  }

  void unlock()
  {
    m_mutex.read_unlock();
  }

  srw_mutex & get_mutex()
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

  explicit scoped_write_lock(srw_mutex & mutex) : m_mutex(mutex)
  {
    m_mutex.write_lock();
  }

  ~scoped_write_lock()
  {
    m_mutex.write_unlock();
  }

  void lock()
  {
    m_mutex.write_lock();
  }

  void unlock()
  {
    m_mutex.write_unlock();
  }

  srw_mutex & get_mutex()
  {
    return m_mutex;
  }

private:
  srw_mutex & m_mutex;
};


} /* namespace */
