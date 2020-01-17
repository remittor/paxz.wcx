#pragma once

#include <windows.h>

namespace wcx {

#define MM_SIZE_ALIGN16_UP(_size_)   (((size_t)(_size_) + 0xF) & ~0xF)

class simplemm
{
public:
  static const size_t mm_alloc_size = 256*1024;  // 256 KiB

  struct mem_block_header {
    mem_block_header * prev;
    mem_block_header * next;        /* link to next mem block */
    size_t             capacity;    /* data capacity = mm_alloc_size - sizeof(mem_block_header) */
    size_t             size;        /* used data size */
  };

  static_assert(sizeof(mem_block_header) % 16 == 0, "error_mem_block_header");

private:
  mem_block_header * m_head;
  mem_block_header * m_tail;
  bool m_zero_fill;

public:
  simplemm() noexcept : m_head(0), m_tail(0)
  {
    m_zero_fill = false;
  }

  ~simplemm() noexcept
  {
    destroy();
  }

  void set_zero_fill(bool zero_fill) { m_zero_fill = zero_fill; }

  void destroy() noexcept
  {
    mem_block_header * blk = m_head;
    while (blk) {
      mem_block_header * next = blk->next;
      mem_block_destroy(blk);
      blk = next;
    }
    m_head = NULL;
    m_tail = NULL;
  }

  LPVOID alloc(size_t elem_size, bool zero_fill = false) noexcept
  {
    if (!m_head) {
      m_head = mem_block_create(NULL, mm_alloc_size);
      if (!m_head)
        return NULL;
      m_tail = m_head;
    }
    LPVOID elem = alloc_internal(elem_size, zero_fill);
    if (!elem) {
      mem_block_header * new_blk = create_next();
      if (new_blk) {
        elem = alloc_internal(elem_size, zero_fill);
      }
    }
    return elem;
  }

private:
  __forceinline
  void mem_block_destroy(mem_block_header * blk) noexcept
  {
    if (blk)
      VirtualFree(blk, 0, MEM_RELEASE);
  }

  __forceinline
  mem_block_header * simplemm::mem_block_create(mem_block_header * prev, size_t blk_size) noexcept
  {
    mem_block_header * blk = (mem_block_header *) VirtualAlloc(NULL, blk_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (blk) {
      blk->prev = prev;
      blk->next = NULL;
      blk->capacity = blk_size - sizeof(mem_block_header) - 2;
      blk->size = 0;
      if (m_zero_fill)
        memset((PBYTE)blk + sizeof(mem_block_header), 0, blk_size - sizeof(mem_block_header));
      return blk;
    }
    return NULL;
  }

  mem_block_header * simplemm::create_next() noexcept
  {
    if (!m_tail->next) {
      mem_block_header * new_blk = mem_block_create(m_tail, mm_alloc_size);
      if (new_blk) {
        m_tail->next = new_blk;
        m_tail = new_blk;
        return new_blk;
      }
    }
    return NULL;
  }

  LPVOID alloc_internal(size_t elem_size, bool zero_fill) noexcept
  {
    size_t new_size = m_tail->size + MM_SIZE_ALIGN16_UP(elem_size);
    if (new_size < m_tail->capacity) {
      PBYTE elem = (PBYTE)m_tail + sizeof(mem_block_header) + m_tail->size;
      if (zero_fill)
        memset(elem, 0, elem_size);
      m_tail->size = new_size;
      return (LPVOID)elem;
    }
    return NULL;
  }
};

} /* namespace */
