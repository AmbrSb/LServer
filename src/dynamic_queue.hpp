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

#include <memory_resource>
#include <queue>

#include "dynamic_string.hpp"
#include "pool.hpp"
#include "queue_buffer_pool.hpp"


namespace lserver {

  /*
   * DynamicQueue is a thin wrapper around std::queue.
   * It also provides a couple of methods that encapsulate the logic of
   * constructing and destructing QBs that are actually the items
   * that are queued. It uses a single pool resource to allocate the
   * QueuBuffer instances.
   */
  template <class QB = DynamicString>
  class DynamicQueue {
  public:
    using QueueBuffer = QB;

    DynamicQueue();
    ~DynamicQueue() noexcept = default;
    /*
     * Returns a free QueueBuffer of capacity at least n from the buffer
     * pool. This buffer should eventually be returned back via free()
     * method of this class.
     */
    QB* prepare(std::size_t n);
    /*
     * @param qb is a QueueBuffer previously provided by the prepare()
     * method.
     */
    void free(QB* qb) noexcept(std::is_nothrow_destructible_v<QB>);
    void push(QB* qb);
    void pop();
    void clear();
    QB* front();
    std::size_t size() const noexcept;
    bool empty() const noexcept;

  private:
    static inline QueueBufferPool<QB> queue_buffer_pool_{0, false};
    std::queue<QB*> q_{};
    mutable std::mutex mtx_;
  };

  template <class QB>
  DynamicQueue<QB>::DynamicQueue()
  { }

  template <class QB>
  inline auto
  DynamicQueue<QB>::prepare(std::size_t n) -> QB*
  {
    return queue_buffer_pool_.borrow(n);
  }

  template <class QB>
  inline void
  DynamicQueue<QB>::push(QB* qb)
  {
    std::scoped_lock _{mtx_};
    q_.push(qb);
  }

  template <class QB>
  inline auto
  DynamicQueue<QB>::front() -> QB*
  {
    std::scoped_lock _{mtx_};
    return q_.front();
  }

  template <class QB>
  inline void
  DynamicQueue<QB>::free(QB* qb) noexcept(std::is_nothrow_destructible_v<QB>)
  {
    queue_buffer_pool_.put_back(qb);
  }

  template <class QB>
  inline void
  DynamicQueue<QB>::pop()
  {
    std::scoped_lock _{mtx_};
    q_.pop();
  }

  template <class QB>
  inline void
  DynamicQueue<QB>::clear()
  {
    std::scoped_lock _{mtx_};
    decltype(q_) empty;
    std::swap(q_, empty);
  }

  template <class QB>
  inline std::size_t
  DynamicQueue<QB>::size() const noexcept
  {
    std::scoped_lock _{mtx_};
    return q_.size();
  }

  template <class QB>
  inline bool
  DynamicQueue<QB>::empty() const noexcept
  {
    std::scoped_lock _{mtx_};
    return q_.empty();
  }

} // namespace lserver