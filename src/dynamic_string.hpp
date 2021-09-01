/*
 * BSD 2-Clause License
 *
 * Copyright (c) 2021, Amin Saba
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <cassert>
#include <cstring>
#include <exception>
#ifdef USE_PMR_POOL_RESOURCE
#include <memory_resource>
#endif
#include <string>

#include "common.hpp"

namespace lserver {

  template <class>
  class QueueBufferPool;

  /*
   * A string-like class that supports a printf-like
   * operation. It will dynamically resize at required
   * by the printf operation.
   */
  class DynamicString final {
    friend class QueueBufferPool<DynamicString>;

  public:
    LS_SANITIZE

    void resize(std::size_t);
    void clear() noexcept;
    std::size_t size() const noexcept;
    std::size_t capacity() const noexcept;
    char* data() noexcept;
    char const* data() const noexcept;
    void fill(std::size_t count);

    /*
     * The first argument is the format string. Caller
     * is responsible for sanitization of the format string.
     */
    template <class... Args>
    std::size_t printf(Args... args);
    /*
     * Making constructor/destructor private, because only
     * DynamicQueue<DynamicString> is allowed to create/destroy
     * instances of DynamicString
     */
#ifdef USE_PMR_POOL_RESOURCE
    DynamicString(std::size_t capacity, std::pmr::memory_resource& mr);
#else
    DynamicString(std::size_t capacity);
#endif
    ~DynamicString() noexcept;

  private:
    char* allocate(std::size_t sz);
    void deallocate(char* p, std::size_t n);

#ifdef USE_PMR_POOL_RESOURCE
    std::pmr::memory_resource& mr_;
#endif
    std::size_t capacity_;
    char* buffer_start_;
    char* buffer_end_;
  };

#ifdef USE_PMR_POOL_RESOURCE
  inline DynamicString::DynamicString(std::size_t capacity,
                                      std::pmr::memory_resource& mr)
      : mr_{mr}
      , capacity_{capacity}
      , buffer_start_{(char*)mr_.allocate(capacity_)}
      , buffer_end_{buffer_start_}
  { }
#else
  inline DynamicString::DynamicString(std::size_t capacity)
      : capacity_{capacity}
      , buffer_start_{new char[capacity_]}
      , buffer_end_{buffer_start_}
  { }
#endif

  inline DynamicString::~DynamicString() noexcept
  {
    deallocate(buffer_start_, capacity_);
  }

  inline char*
  DynamicString::allocate(std::size_t sz)
  {
    char* b;
#ifdef USE_PMR_POOL_RESOURCE
    b = reinterpret_cast<char*>(mr_.allocate(sz));
#else
    b = new char[sz];
#endif
    return b;
  }

  inline void
  DynamicString::deallocate(char* p, std::size_t n)
  {
#ifdef USE_PMR_POOL_RESOURCE
    mr_.deallocate(p, n);
#else
    delete[] buffer_start_;
#endif
  }

  inline std::size_t
  DynamicString::size() const noexcept
  {
    return buffer_end_ - buffer_start_;
  }

  inline std::size_t
  DynamicString::capacity() const noexcept
  {
    return capacity_;
  }

  inline char*
  DynamicString::data() noexcept
  {
    return buffer_start_;
  }

  inline char const*
  DynamicString::data() const noexcept
  {
    return buffer_start_;
  }

  inline void
  DynamicString::fill(std::size_t count)
  {
    assert(count <= capacity_);
    buffer_end_ = buffer_start_ + count;
  }

  inline void
  DynamicString::resize(std::size_t sz)
  {
    assert(sz > size());

    char* b;
    std::size_t current_sz = size();

    b = allocate(sz);
    if (!b) LS_UNLIKELY
      throw std::bad_alloc{};
    memcpy(b, buffer_start_, current_sz);
    deallocate(buffer_start_, capacity_);
    buffer_start_ = b;
    buffer_end_ = buffer_start_ + current_sz;
    capacity_ = sz;
  }

  inline void
  DynamicString::clear() noexcept
  {
    buffer_end_ = buffer_start_;
  }

  template <class... Args>
  inline std::size_t
  DynamicString::printf(Args... args)
  {
    std::size_t nchars;

  retry:
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-security"
    nchars = snprintf(buffer_end_, capacity_ - size(), args...);
#pragma GCC diagnostic pop

    if (nchars < 0) LS_UNLIKELY
      throw std::logic_error{
          "snprintf failed in dynamic string: " +
          std::error_condition{errno, std::system_category()}.message()};

    if (nchars >= capacity_ - size()) LS_UNLIKELY {
      /*
       * snprintf was not able to write the whole string
       * into the buffer.
       * Calculate the new capacity, resize, and retry.
       */
      std::size_t new_sz = nchars + size();
      if (new_sz < capacity_ * 2)
        if (capacity_ <= 512)
          new_sz = capacity_ * 2;
      resize(new_sz);
      goto retry;
    }

    buffer_end_ += nchars;

    return nchars;
  }
} // namespace lserver
