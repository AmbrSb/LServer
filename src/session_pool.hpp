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

#include "pool.hpp"

#ifdef ENABLE_STATISTICS
#include "stats.hpp"
#endif

namespace lserver {

  template <class T>
  class SessionPool : public Pool<T, SessionPool<T>> {
    using SessionType = T;
    using Base = Pool<T, SessionPool<T>>;

  public:
    SessionPool(std::size_t max_size, bool eager);
    ~SessionPool() = default;
#ifdef ENABLE_STATISTICS
    std::tuple<PoolStats const&, SessionStats const&>
    get_stats() const noexcept;
#endif
    /*
     * create/destroy functions to be used by the CRTP base
     */
    SessionType* create();
    void destroy(SessionType* p);
    /*
     * Optional 'name' to be used by the CRTP base.
     */
    char const* name();

  private:
    mutable SessionStats session_stats_;
  };

  template <class T>
  inline SessionPool<T>::SessionPool(std::size_t max_size, bool eager)
      : Base(max_size, eager)
  { }

  template <class T>
  inline auto
  SessionPool<T>::create() -> SessionType*
  {
    /*
     * Any Session instance requires a release callback that puts it
     * back into the session pool, when it's done processing the
     * assigned connection.
     */
    auto session_release_cb = [this](SessionType* protocol) {
      this->put_back(protocol);
    };

    auto p = new T{};
    p->set_finalized_cb(session_release_cb);
    return p;
  }

  template <class T>
  inline void
  SessionPool<T>::destroy(SessionType* p)
  {
    delete p;
  }

#ifdef ENABLE_STATISTICS
  template <class T>
  std::tuple<PoolStats const&, SessionStats const&>
  SessionPool<T>::get_stats() const noexcept
  {
    session_stats_.clear();

    for (auto const& kv: Base::all_items()) {
      auto& delta = kv.first->get_stats_delta();

      std::size_t transaction_cnt_delta =
          delta.stats_transactions_cnt_delta_.exchange(0);
      std::size_t bytes_received_delta =
          delta.stats_bytes_received_delta_.exchange(0);
      std::size_t bytes_sent_delta = delta.stats_bytes_sent_delta_.exchange(0);

      session_stats_.stats_transactions_cnt_delta_.fetch_add(
          transaction_cnt_delta);
      session_stats_.stats_bytes_received_delta_.fetch_add(
          bytes_received_delta);
      session_stats_.stats_bytes_sent_delta_.fetch_add(bytes_sent_delta);
    }

    return std::tie(Base::get_stats(), session_stats_);
  }
#endif

  template <class T>
  char const*
  SessionPool<T>::name()
  {
    return "Session Pool";
  }

} // namespace lserver