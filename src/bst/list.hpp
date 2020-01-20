#pragma once

#include "bst.hpp"
#include "srw_lock.hpp"

namespace bst {

template <typename T>
class list;

template <typename T>
class list_enum;


template <typename T>
class list_node
{
public:
  list_node() BST_NOEXCEPT : m_obj(NULL)  // head node
  {
    m_prev = m_next = this;
  }
  
  list_node(list_node * prev, list_node * next, T * obj) BST_NOEXCEPT
    : m_prev(prev)
    , m_next(next)
    , m_obj(obj)
  {
    m_prev->m_next = m_next->m_prev = this;
  }

  ~list_node() BST_NOEXCEPT
  {
    m_prev->m_next = m_next;
    m_next->m_prev = m_prev;
    if (m_obj)
      delete m_obj;
  }

protected:
  friend list<T>;
  friend list_enum<T>;

  list_node * m_prev;
  list_node * m_next;
  T * m_obj;
};


template <typename T>
class list
{
public:
  list() BST_NOEXCEPT : m_base()
  {
    m_capacity = 0;
  }
  
  ~list() BST_NOEXCEPT
  { 
    clear();
  }
  
  void clear() BST_NOEXCEPT
  {
    scoped_write_lock lock(m_mutex);
    while (!is_empty_internal()) {
      delete m_base.m_next;
    }
    m_capacity = 0;
  }
  
  srw_lock & get_mutex()
  {
    return m_mutex;
  }

  size_t get_capacity() BST_NOEXCEPT
  {
    return m_capacity;
  }

  bool is_empty()
  {
    return m_capacity == 0;
  }

  bool find(T * obj) BST_NOEXCEPT
  {
    scoped_read_lock lock(m_mutex);
    return find_node(obj) ? true : false;
  }
  
  bool add(T * obj) BST_NOEXCEPT
  {
    if (obj) {
      scoped_write_lock lock(m_mutex);
      return add_internal(obj);
    }
    return false;
  }

  bool del(T * obj) BST_NOEXCEPT
  {
    if (obj) {
      scoped_write_lock write_lock(m_mutex);
      return pop_internal(obj, true);
    }
    return false;
  }

  bool pop(T * obj) BST_NOEXCEPT
  {
    if (obj) {      
      scoped_write_lock write_lock(m_mutex);
      return pop_internal(obj, false);
    }
    return false;
  }
  
protected:
  bool is_empty_internal()
  {
    return (m_base.m_next == &m_base) && (m_base.m_prev == &m_base);
  }
  
  list_node<T> * find_node(T * obj) BST_NOEXCEPT
  {
    if (obj && !is_empty())
      for (list_node<T> * node = m_base.m_next; node != &m_base; node = node->m_next)
        if (node->m_obj == obj)
          return node;
    return NULL;
  }
  
  bool add_internal(T * obj) BST_NOEXCEPT
  {
    if (!find_node(obj)) {
      list_node<T> * node = new(bst::nothrow) list_node<T>(m_base.m_prev, &m_base, obj);
      if (node) {
        m_capacity++;
        return true;
      }
    }
    return false;
  }

  bool pop_internal(T * obj, bool free_obj) BST_NOEXCEPT
  {
    list_node<T> * node = find_node(obj);
    if (node) {
      if (!free_obj)
        node->m_obj = NULL;
      delete node;
      m_capacity--;
      return true;
    }
    return false;
  }

protected:
  friend class list_enum<T>;
  
  list_node<T> m_base;
  srw_lock     m_mutex;
  size_t       m_capacity;
};



template <typename T>
class list_enum : private noncopyable
{
public:
  list_enum() BST_DELETED;  // disable default ctor
  
  list_enum(list<T> & list, read_lock_t) BST_NOEXCEPT : m_list(&list)
  {
    m_write_lock = false;
    m_list->m_mutex.read_lock();
    reset();
  }

  list_enum(list<T> & list, write_lock_t) BST_NOEXCEPT : m_list(&list)
  {
    m_write_lock = true;
    m_list->m_mutex.write_lock();
    reset();
  }
  
  ~list_enum() BST_NOEXCEPT
  {
    m_list->m_mutex.unlock();
  }

  list_node<T> * begin() BST_NOEXCEPT
  {
    return &(m_list->m_base);
  }

  void reset() BST_NOEXCEPT
  {
    m_node = begin();
  }
  
  T * get_next() BST_NOEXCEPT
  {
    m_node = m_node->m_next;
    return (m_node == begin()) ? NULL : m_node->m_obj;
  }
  
  bool find(T * obj) BST_NOEXCEPT
  {
    return m_list->find_node(obj) ? true : false;
  }
  
  bool add(T * obj) BST_NOEXCEPT
  {
    return m_write_lock ? m_list->add_internal(obj) : false;
  }

  bool del(T * obj) BST_NOEXCEPT
  {
    return m_write_lock ? m_list->pop_internal(obj, true) : false;
  }

  bool pop(T * obj) BST_NOEXCEPT
  {
    return m_write_lock ? m_list->pop_internal(obj, false) : false;
  }
  
  T & operator*()
  {
    return m_node->m_obj;
  }

  list_enum & operator++()
  {
    m_node = m_node->m_next;
    return *this;
  }

  list_enum & operator--()
  {
    m_node = m_node->m_prev;
    return *this;
  }

  bool operator==(const list_enum & other)
  {
    return m_node == other.m_node;
  }

  bool operator!=(const list_enum & other)
  {
    return m_node != other.m_node;
  }

private:
  list<T>      * m_list;
  list_node<T> * m_node;
  bool           m_write_lock;
};


} /* namespace */
