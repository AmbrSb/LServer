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

#include <list>
#include <shared_mutex>
#include <vector>

#include <asio.hpp>

#include "lscontext.hpp"
#include "pool.hpp"
#include "strand_pool.hpp"

namespace lserver {
  /*
   * LSContextPool eagerly creates a fixed number of of LSContext
   * instances and hands them out in a round-robin style through its
   * get_context_round_robin() method.
   */
  class LSContextPool final {
  public:
    LSContextPool(std::size_t pool_size, std::size_t max_pool_size,
                  std::size_t thread_multiplier);
    LSContextPool(LSContextPool const&) = delete;
    LSContextPool(LSContextPool&&) = delete;
    LSContextPool& operator=(LSContextPool const&) = delete;
    LSContextPool& operator=(LSContextPool&&) = delete;

    ~LSContextPool() noexcept = default;

    /*
     * Returns a reference to one of its LSContext instances.
     */
    std::tuple<LSContext*, POI> get_context_round_robin() noexcept;
    /*
     * Executes the stop methods of its owned LSContexts.
     */
    void stop();
    /*
     * Blocks and joins on all of the threads running in all LSContexts.
     */
    void wait();
    /*
     * Adds a new LSContext to this server, or recycles an already
     * deactivated LSContext in this server.
     *
     * throws std::logic_error if there are no recyclable LSContexts
     * and there are already as many as the max number of allowable
     * LSContexts.
     */
    void add_context(std::size_t num_threads);
    /*
     * Deactivate LSContext with specified index.
     * Returns 0 on success, and returns EBUSY if the LSContext cannot
     * be currently removed.
     */
    int deactivate_context(std::size_t index);
    /*
     * Returns the number of active LSContexts in this pool
     */
    std::size_t active_contexts_count();
    std::vector<ContextInfo> get_contexts_info() const;

  private:
    mutable std::shared_mutex smtx_;
    /*
     * A vector of active and inactive LSContexts of this.
     * This vector has to be reserved with proper size in the constructor
     * of this Server, so that push_back() does not cause copy/move of
     * LSContext instances.
     * This limitation can be circumvented by using a non-contiguous
     * container (e.g. std::list). But benchmarks show a considerable
     * drop in performance in those cases.
     */
    std::vector<LSContext> lscontexts_;
    /*
     * The next lscontext that will potentially be dispatched in the next
     * call to get_context_round_robin()
     */
    decltype(lscontexts_)::iterator next_context_;
  };

  inline std::tuple<LSContext*, POI>
  LSContextPool::get_context_round_robin() noexcept
  {
    std::shared_lock _{smtx_};

    /*
     * Find the next acitve LSContext
     */
    while (true) {
      auto chosen_context = next_context_++;

      /*
       * Wrap around
       */
      if (next_context_ == lscontexts_.end())
        next_context_ = lscontexts_.begin();

      if (chosen_context->is_active()) {
        POI id = (&(*chosen_context) - data(lscontexts_));
        chosen_context->hold();
        return std::make_tuple(&(*chosen_context), id);
      }
    }
    __builtin_unreachable();
  }

} // namespace lserver