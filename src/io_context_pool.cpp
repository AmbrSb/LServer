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

#include <exception>
#include <stdexcept>

#include "io_context_pool.hpp"

namespace lserver {

  LSContextPool::LSContextPool(std::size_t pool_size, std::size_t max_pool_size,
                               std::size_t thread_multiplier)
  {
    /*
     * This reservation is needed because LSContext instances should not
     * be moved. See the comment above declaration of the move operations
     * of LSContext.
     */
    lscontexts_.reserve(max_pool_size);
    for (std::size_t i = 0; i < pool_size; ++i)
      add_context(thread_multiplier);

    next_context_ = std::begin(lscontexts_);
  }

  void
  LSContextPool::stop()
  {
    std::unique_lock _{smtx_};

    for (auto& lscontext: lscontexts_)
      lscontext.stop(true);
  }

  void
  LSContextPool::wait()
  {
    for (auto& lscontext: lscontexts_)
      lscontext.wait();
  }

  void
  LSContextPool::add_context(std::size_t num_threads)
  {
    std::unique_lock _{smtx_};

    for (auto& lscontext: lscontexts_) {
      if (lscontext.reusable()) {
        lscontext.reuse(num_threads);
        return;
      }
    }

    /*
     * We didn't find a reusable LSContext, let's create a new one
     */
    if (lscontexts_.size() == lscontexts_.capacity())
      throw std::logic_error{"Max contexts count will be exceeded."};

    auto& context = lscontexts_.emplace_back();
    context.set_num_threads(num_threads);
    context.run_threads();
  }

  std::size_t
  LSContextPool::active_contexts_count()
  {
    std::size_t count = 0;

    for (std::size_t i = 0; i < lscontexts_.size(); ++i)
      if (lscontexts_[i].is_active())
        count++;

    return count;
  }

  int
  LSContextPool::deactivate_context(std::size_t index)
  {
    std::unique_lock _{smtx_};

    /*
     * Perform some sanity check before trying to stop the LSContext
     * /////////////////////////////////////////////////////////////
     */
    if (index >= lscontexts_.size())
      throw std::logic_error{"Bad context index"};

    if (!lscontexts_[index].is_active())
      throw std::logic_error{"Context is not active"};

    /*
     * Verify that LSContext with index 'index' is not the only active
     * LSContext.
     */
    if (active_contexts_count() < 2)
      throw std::logic_error{"There should be at least one active context"};
    /*
     * /////////////////////////////////////////////////////////////
     * Sanity checks passed
     */

    int rc = lscontexts_[index].stop(false);
    return (rc);
  }

  std::vector<ContextInfo>
  LSContextPool::get_contexts_info() const
  {
    std::vector<ContextInfo> contexts_info;

    for (auto const& lscontext: lscontexts_)
      contexts_info.push_back(lscontext.get_context_info());

    return contexts_info;
  }

} // namespace lserver