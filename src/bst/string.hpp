#pragma once

#include "bst.hpp"
#include <cstdlib>
#include <Shlwapi.h>
#include <stdio.h>
#include <vadefs.h>

namespace bst {

static const size_t npos = (size_t)(-1);    /* Non-position */

enum error_code : char {
  non_error        = 0,
  e_out_of_memory  = 2,
  e_out_of_range   = 3,
  e_max_code       = 4,
};


template <typename CharT>
class fmt_string
{
private:
  const CharT * m_ptr;
public:  
  fmt_string() BST_DELETED;
  fmt_string(const CharT & s) { m_ptr = s; }
  ~fmt_string() {  }
  explicit fmt_string(const CharT * s) { m_ptr = s; }
  const CharT * c_str() const { return m_ptr; }
};

template <typename CharT>
BST_INLINE const fmt_string<CharT> fmtstr(const CharT * s)
{
  return fmt_string<CharT>(s);
}


template <typename CharT>
class string_base
{
protected:
  CharT    * m_buf;
  size_t     m_len;
  size_t     m_capacity;
  bool       m_isdynamic;
  error_code m_last_error;

  enum { is_string  = is_same<CharT, char>::value };
  enum { is_wstring = is_same<CharT, wchar_t>::value };
  enum { is_buffer  = is_same<CharT, unsigned char>::value };

  BST_INLINE
  void set_defaults() BST_NOEXCEPT
  {
    m_buf = nullptr;
    m_len = 0;
    m_capacity = 0;
    m_isdynamic = true;
    m_last_error = non_error;
  }

public:
  typedef CharT * pointer;
  typedef const CharT * const_pointer;
  typedef CharT & reference;
  typedef const CharT & const_reference;
  
  enum char_case {
    case_insensitive = 0,
    case_sensitive   = 1,    
  };

  string_base() BST_NOEXCEPT
  {
    set_defaults();
  }

  explicit string_base(const CharT * s, size_t len = 0) BST_NOEXCEPT
  {
    set_defaults();
    assign(s, len);
  }
  
  explicit string_base(const fmt_string<CharT> fmt, ...) BST_NOEXCEPT
  {
    set_defaults();
    va_list argptr;
    va_start(argptr, fmt);
    assign_fmt_internal(fmt.c_str(), argptr);
    va_end(argptr);
  }
  
  ~string_base() BST_NOEXCEPT
  {
    destroy();
  }

  /* copy-constructor */
  string_base(const string_base & str) BST_NOEXCEPT
  {
    set_defaults();
    assign(str.m_buf, str.m_len);
  }

  bool is_dynamic() BST_NOEXCEPT
  {
    return m_isdynamic;
  }
  
  error_code get_last_error(bool reset = true) BST_NOEXCEPT
  {
    error_code err_code = m_last_error;
    if (reset)
      m_last_error = non_error;
    return err_code;
  }
  
  bool has_error(bool reset = true) BST_NOEXCEPT
  {
    return get_last_error(reset) != non_error;
  }

  void clear() BST_NOEXCEPT
  {
    m_len = 0;
    if (m_buf)
      m_buf[0] = 0;
    m_last_error = non_error;
  }
  
  void destroy() BST_NOEXCEPT
  {
    clear();
    if (is_dynamic()) {
      if (m_buf)
        bst::free(m_buf);
      m_buf = NULL;
      m_capacity = 0;
    }
  }

  static size_t get_length(const CharT * s) BST_NOEXCEPT
  {
    size_t len = 0;
    if (s)
      while (*s++) len++;
    return len;
  }
  
  /* may be return npos */
  size_t fix_length() BST_NOEXCEPT
  {
    if (m_capacity && m_buf) {
      m_buf[m_capacity] = 0;
      m_len = get_length(m_buf);
      return m_len;
    }
    return npos;
  }

  bool assign(const CharT * s, size_t len = 0) BST_NOEXCEPT
  {
    if (!s) {
      destroy();
      return true;
    }
    if (!is_buffer)
      if ((SSIZE_T)len <= 0)
        len = get_length(s);

    if (len == 0) {
      clear();
      return true;
    }
    len = request_length(len);
    if (len == npos)
      return false;

    memcpy(m_buf, s, len * sizeof(CharT));
    if (!is_buffer)
      m_buf[len] = 0;

    m_len = len;
    return true;
  }

  bool assign(const string_base & str) BST_NOEXCEPT
  {
    return assign(str.c_str(), str.length());
  }
  
  bool reserve(size_t len) BST_NOEXCEPT
  {
    return set_capacity(len, true);
  }
  
  size_t length() const
  {
    return m_len;
  }

  size_t size() const 
  {
    return m_len;
  }

  size_t get_size() const 
  {
    return m_len * sizeof(CharT);
  }
  
  size_t capacity() const
  {
    return m_capacity;
  }

  size_t get_capacity() const
  {
    return m_capacity * sizeof(CharT);
  }

  bool empty() const
  {
    return m_len == 0;
  }
  
  bool is_null() const
  {
    return m_buf == nullptr;
  }
 
  void * ptr()
  {
    return (void *)m_buf;
  }

  const void * c_ptr() const
  {
    return (const void *)m_buf;
  }

  CharT * data()
  {
    return m_buf;
  }

  const CharT * c_data() const
  {
    return m_buf;
  }

  const CharT * c_str() const
  {
    return m_buf;
  }
  
  CharT & at(size_t pos)
  {
    return m_buf[pos]; // (m_buf && pos < m_len) ? m_buf[pos] : 0;
  }
  
  const CharT & at(size_t pos) const
  {
    return m_buf[pos];
  }
  
  CharT & back()
  {
    return (m_buf && m_len > 0) ? m_buf[m_len - 1] : 0;
  }

  const CharT & back() const
  {
    return (m_buf && m_len > 0) ? m_buf[m_len - 1] : 0;
  }

  CharT & front()
  {
    return (m_buf && m_len > 0) ? m_buf[0] : 0;
  }
  
  const CharT & front() const
  {
    return (m_buf && m_len > 0) ? m_buf[0] : 0;
  }

  /* may be return npos */
  size_t resize(size_t len) BST_NOEXCEPT
  {
    return resize_internal(len, false, 0);
  }
  
  size_t resize(size_t len, CharT c) BST_NOEXCEPT
  {
    return resize_internal(len, true, c);
  }

  /* may be return null */
  CharT * expand(size_t add_len) BST_NOEXCEPT
  {
    if (add_len == 0)
      return m_buf ? &m_buf[m_len] : nullptr;

    if (resize(m_len + add_len, 0) == npos)
      return nullptr;

    return &m_buf[m_len];
  }

  string_base & append(const CharT * s, size_t slen = 0) BST_NOEXCEPT
  {
    if (!s)
      return *this;

    if (!is_buffer)
      if ((SSIZE_T)slen <= 0)
        slen = get_length(s);

    if (slen == 0)
      return *this;

    size_t len = request_length(m_len + slen, 128);
    if (len == npos)
      return *this;

    if (len > m_len) {
      memcpy(&m_buf[m_len], s, (len - m_len) * sizeof(CharT));
      m_len = len;
      if (!is_buffer)
        m_buf[m_len] = 0;
    }
    return *this;  
  }

  string_base & append(const string_base & str) BST_NOEXCEPT
  {
    return append(str.c_data(), str.length());
  }
  
  /* may be return npos */
  size_t copy(CharT * s, size_t len, size_t pos = 0) const BST_NOEXCEPT
  {
    if (!s || !m_buf || pos >= m_len)
      return npos;

    if (pos + len > m_len)
      len = m_len - pos;

    if (len > 0) {
      memcpy(s, &m_buf[pos], len * sizeof(CharT));
      if (!is_buffer)
        s[len] = 0;
    }
    return len;
  }
  
  string_base & operator= (const string_base & str) BST_NOEXCEPT
  {
    assign(str);
    return *this;
  }
  
  string_base & operator= (const CharT * s) BST_NOEXCEPT
  {
    assign(s);
    return *this;
  }
  
  string_base & operator+= (const string_base & str) BST_NOEXCEPT
  {
    append(str);
    return *this;
  }
  
  string_base & operator+= (const CharT * s) BST_NOEXCEPT
  {
    append(s);
    return *this;
  }
  
  CharT & operator[] (size_t pos)
  {
    return at(pos);
  }

  const CharT & operator[] (size_t pos) const
  {
    return at(pos);
  }

protected:
  size_t request_length(size_t new_len, size_t suffix = 0, bool save_data = true) BST_NOEXCEPT
  {
    if (is_dynamic()) {
      if (!m_buf || new_len + suffix > m_capacity)
        if (!set_capacity(new_len + suffix, save_data))
          return npos;
      return new_len;
    } else {
      if (new_len > m_capacity) {      /* ignore suffix for preallocated string */
        m_last_error = e_out_of_range;
        return m_capacity;
      }
    }  
    return new_len;
  }

  bool set_capacity(size_t new_capacity, bool save_data) BST_NOEXCEPT
  {
    if (is_dynamic() && new_capacity > m_capacity) {
      CharT * buf = reinterpret_cast<CharT *> (bst::malloc((new_capacity + 2) * sizeof(CharT)));
      if (!buf) {
        m_last_error = e_out_of_memory;
        return false;
      }
      buf[0] = 0;
      if (m_buf) {
        if (m_len && save_data) {
          memcpy(buf, m_buf, (m_len + 1) * sizeof(CharT));
        }  
        bst::free(m_buf);
      }
      m_buf = buf;
      m_capacity = new_capacity;
    }
    return true;
  }

  size_t resize_internal(size_t len, bool fill, CharT c) BST_NOEXCEPT
  {
    if (len <= m_len) {
      if (!is_buffer && m_buf)
        m_buf[len] = 0;
      m_len = len;
      return len;
    } 
    len = request_length(len);
    if (len == npos)
      return len;

    if (fill) {
      for (size_t i = m_len; i < len; i++) {
        m_buf[i] = c;
      }
    }  
    if (!is_buffer)
      m_buf[len] = 0;

    m_len = len;
    return len;
  }

public:
  size_t rfind(const string_base & str, size_t pos = npos) const BST_NOEXCEPT
  {
    return rfind_internal(str.c_str(), str.length(), pos, case_sensitive);
  }

  size_t rfind(const CharT * s, size_t pos = npos) const BST_NOEXCEPT
  {
    return rfind_internal(s, 0, pos, case_sensitive);
  }

  size_t rfind_i(const string_base & str, size_t pos = npos) const BST_NOEXCEPT
  {
    return rfind_internal(str.c_str(), str.length(), pos, case_insensitive);
  }

  size_t rfind_i(const CharT * s, size_t pos = npos) const BST_NOEXCEPT
  {
    return rfind_internal(s, 0, pos, case_insensitive);
  }

protected:
  size_t rfind_internal(const CharT * s, size_t slen, size_t pos, char_case _case) const BST_NOEXCEPT
  {
    if (is_buffer)
      return npos;

    if ((SSIZE_T)slen <= 0)
      slen = get_length(s);

    if (slen == 0 || empty() || (pos != npos && pos > length()))
      return npos;
      
    const_pointer last = (pos == npos) ? nullptr : m_buf + pos;

    size_t res;
    if (is_wstring)
      res = (size_t)StrRStrIW((LPCWSTR)m_buf, (LPCWSTR)last, (LPCWSTR)s);
    else
      res = (size_t)StrRStrIA((LPCSTR)m_buf, (LPCSTR)last, (LPCSTR)s);

    return (!res) ? npos : (res - (size_t)m_buf) / sizeof(CharT);
  }   

public:
  size_t rfind(const CharT c, size_t pos = npos) const BST_NOEXCEPT
  {
    return rfind_internal(c, pos, case_sensitive);
  }

  size_t rfind_i(const CharT c, size_t pos = npos) const BST_NOEXCEPT
  {
    return rfind_internal(c, pos, case_insensitive);
  }

protected:
  size_t rfind_internal(const CharT c, size_t pos, char_case _case) const BST_NOEXCEPT
  {
    if (is_buffer)
      return npos;

    if (empty() || (pos != npos && pos > length()))
      return npos;

    const_pointer end = (pos == npos) ? nullptr : m_buf + pos;
    
    size_t res;
    if (_case == case_sensitive) {
      if (is_wstring)
        res = (size_t)StrRChrW((LPCWSTR)m_buf, (LPCWSTR)end, (WCHAR)c);
      else
        res = (size_t)StrRChrA((LPCSTR)m_buf, (LPCSTR)end, (CHAR)c);
    } else {
      if (is_wstring)
        res = (size_t)StrRChrIW((LPCWSTR)m_buf, (LPCWSTR)end, (WCHAR)c);
      else
        res = (size_t)StrRChrIA((LPCSTR)m_buf, (LPCSTR)end, (CHAR)c);
    }  
    return (!res) ? npos : (res - (size_t)m_buf) / sizeof(CharT);
  }   

public:
  size_t find(const string_base & str, size_t pos = npos) const BST_NOEXCEPT
  {
    return find_internal(str.c_str(), str.length(), pos, case_sensitive);
  }

  size_t find(const CharT * s, size_t pos = npos) const BST_NOEXCEPT
  {
    return find_internal(s, 0, pos, case_sensitive);
  }

  size_t find_i(const string_base & str, size_t pos = npos) const BST_NOEXCEPT
  {
    return find_internal(str.c_str(), str.length(), pos, case_insensitive);
  }

  size_t find_i(const CharT * s, size_t pos = npos) const BST_NOEXCEPT
  {
    return find_internal(s, 0, pos, case_insensitive);
  }

protected:
  size_t find_internal(const CharT * s, size_t slen, size_t pos, char_case _case) const BST_NOEXCEPT
  {
    if (is_buffer)
      return npos;

    if ((SSIZE_T)slen <= 0)
      slen = get_length(s);

    if (slen == 0 || empty() || pos >= length())
      return npos;

    size_t res;
    if (_case == case_sensitive) {
      if (is_wstring)
        res = (size_t)StrStrW((LPCWSTR)m_buf + pos, (LPCWSTR)s);
      else
        res = (size_t)StrStrA((LPCSTR)m_buf + pos, (LPCSTR)s);
    } else {
      if (is_wstring)
        res = (size_t)StrStrIW((LPCWSTR)m_buf + pos, (LPCWSTR)s);
      else
        res = (size_t)StrStrIA((LPCSTR)m_buf + pos, (LPCSTR)s);
    }
    return (!res) ? npos : (res - (size_t)m_buf) / sizeof(CharT);
  }   

public:
  size_t find(const CharT c, size_t pos = 0) const BST_NOEXCEPT
  {
    return find_internal(c, pos, case_sensitive);
  }

  size_t find_i(const CharT c, size_t pos = 0) const BST_NOEXCEPT
  {
    return find_internal(c, pos, case_insensitive);
  }

protected:
  size_t find_internal(const CharT c, size_t pos, char_case _case) const BST_NOEXCEPT
  {
    if (is_buffer)
      return npos;

    if (empty() || pos >= length())
      return npos;

    size_t res;
    if (_case == case_sensitive) {
      if (is_wstring)
        res = (size_t)StrChrW((LPCWSTR)m_buf + pos, (WCHAR)c);
      else
        res = (size_t)StrChrA((LPCSTR)m_buf + pos, (CHAR)c);
    } else {
      if (is_wstring)
        res = (size_t)StrChrIW((LPCWSTR)m_buf + pos, (WCHAR)c);
      else
        res = (size_t)StrChrIA((LPCSTR)m_buf + pos, (CHAR)c);
    }  
    return (!res) ? npos : (res - (size_t)m_buf) / sizeof(CharT);
  }
  
public:
  string_base & insert(size_t pos, const CharT * s, size_t n = 0) BST_NOEXCEPT
  {
    return insert_internal(pos, s, n);
  }

  string_base & insert(size_t pos, const string_base & str) BST_NOEXCEPT
  {
    return insert_internal(pos, str.c_str, str.length());
  }

protected:
  string_base & insert_internal(size_t pos, const CharT * s, size_t slen) BST_NOEXCEPT
  {
    if (!is_buffer)
      if ((SSIZE_T)slen <= 0)
        slen = get_length(s);

    if (slen == 0 || pos > length())
      return *this;
 
    size_t len = request_length(m_len + slen);
    if (len == npos || len < m_len + slen)
      return *this;
    
    if (!empty())
      memmove(m_buf + pos + slen, m_buf + pos, m_len - pos + 1);

    memcpy(m_buf + pos, s, slen);
    if (!is_buffer)
      m_buf[len] = 0;

    m_len = len;
    return *this;  
  } 

public:
  bool assign_fmt(const CharT * fmt, ...) BST_NOEXCEPT
  {
    va_list argptr;
    va_start(argptr, fmt);
    bool res = assign_fmt_internal(fmt, argptr);
    va_end(argptr);
    return res;
  }

protected:
  BST_NOINLINE
  bool assign_fmt_internal(const CharT * fmt, va_list argptr) BST_NOEXCEPT
  {
    BYTE buf[250];
    int sz;
    if (is_wstring) {
      sz = vswprintf_s((LPWSTR)buf, sizeof(buf) / sizeof(WCHAR) - 2, (LPCWSTR)fmt, argptr);
    } else {
      sz = vsprintf_s((LPSTR)buf, sizeof(buf) - 2, (LPCSTR)fmt, argptr);
    }
    return assign((const CharT *)buf, (size_t)sz);
  }

public:
  string_base & append(const fmt_string<CharT> fmt, ...) BST_NOEXCEPT
  {
    va_list argptr;
    va_start(argptr, fmt);
    string_base & res = append_fmt_internal(fmt.c_str(), argptr);
    va_end(argptr);
    return res;
  }

  string_base & append_fmt(const CharT * fmt, ...) BST_NOEXCEPT
  {
    va_list argptr;
    va_start(argptr, fmt);
    string_base & res = append_fmt_internal(fmt, argptr);
    va_end(argptr);
    return res;
  }

protected:
  BST_NOINLINE
  string_base & append_fmt_internal(const CharT * fmt, va_list argptr) BST_NOEXCEPT
  {
    BYTE buf[250];   // TODO: may be using _vscprintf ???
    int sz;
    if (is_wstring) {
      sz = vswprintf_s((LPWSTR)buf, sizeof(buf) / sizeof(WCHAR) - 2, (LPCWSTR)fmt, argptr);
    } else {
      sz = vsprintf_s((LPSTR)buf, sizeof(buf) - 2, (LPCSTR)fmt, argptr);
    }
    return append((const CharT *)buf, (size_t)sz);
  }

};


template <typename CharT, size_t PreAllocLen>
class prealloc_string : public string_base<CharT>
{
protected:
  CharT m_content[PreAllocLen + 2];

  BST_INLINE
  void set_defaults() BST_NOEXCEPT
  {
    m_buf = m_content;
    m_len = 0;
    m_capacity = PreAllocLen;
    m_isdynamic = false;
    m_last_error = non_error;
    m_content[0] = 0;
    m_content[PreAllocLen] = 0;
  }

public:
  prealloc_string() BST_NOEXCEPT
  {
    set_defaults();
  }

  explicit prealloc_string(const CharT * s, size_t len = 0) BST_NOEXCEPT
  {
    set_defaults();
    assign(s, len);
  }
  
  explicit prealloc_string(const fmt_string<CharT> fmt, ...) BST_NOEXCEPT
  {
    set_defaults();
    va_list argptr;
    va_start(argptr, fmt);
    assign_fmt_internal(fmt.c_str(), argptr);
    va_end(argptr);
  }
  
  ~prealloc_string() BST_NOEXCEPT
  {
    //
  }

  /* copy-constructor */
  prealloc_string(const string_base & str) BST_NOEXCEPT
  {
    set_defaults();
    assign(str.c_data(), str.length());
  }

  bool reserve(size_t len) BST_NOEXCEPT
  {
    return (len <= PreAllocLen) ? true : false;
  }

  bool is_null() const
  {
    return false;
  }
};


typedef string_base<char>      str;
typedef string_base<WCHAR>     wstr;
typedef string_base<UCHAR>     buf;


typedef prealloc_string<WCHAR, BST_MAX_PATH_LEN>  filepath;
typedef prealloc_string<WCHAR, BST_MAX_NAME_LEN>  filename;

typedef prealloc_string<char, BST_MAX_PATH_LEN>  filepath_a;
typedef prealloc_string<char, BST_MAX_NAME_LEN>  filename_a;

#define prealloc_buffer(PreAllocLen)  prealloc_string<UCHAR, (PreAllocLen)>


} /* namespace */
