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

#include "dynamic_string.hpp"
#include "pool.hpp"

namespace lserver {
  /*
   * A pool of dynamically sized buffers used to minimize use of dynamic
   * memory allocation.
   */
  template <class T>
  class QueueBufferPool : public Pool<T, QueueBufferPool<T>, std::size_t> {
    using QueueBufferType = T;
    using Base = Pool<T, QueueBufferPool<T>, std::size_t>;

  public:
    QueueBufferPool(std::size_t max_size, bool eager);
    /*
     * create/destroy functions to be used by the CRTP base
     */
    T* create(std::size_t n);
    void destroy(T* p);

  private:
#ifdef USE_PMR_POOL_RESOURCE
    std::pmr::synchronized_pool_resource spr_;
#endif
  };

  template <class T>
  inline QueueBufferPool<T>::QueueBufferPool(std::size_t max_size, bool eager)
      : Base{max_size, eager}
  { }

  template <class T>
  inline auto
  QueueBufferPool<T>::create(std::size_t n) -> T*
  {
    return
#ifdef USE_PMR_POOL_RESOURCE
        new T{n, spr_};
#else
        new T{n};
#endif
  }

  template <class T>
  inline void
  QueueBufferPool<T>::destroy(T* p)
  {
    delete p;
  }

} // namespace lserver