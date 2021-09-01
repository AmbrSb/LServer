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
#include <chrono>
#include <list>
#include <stack>
#include <vector>

#include <asio.hpp>

#include "strand_pool.hpp"

using namespace std::literals;

namespace lserver {
  /*
   * Every Session instance requires a reference to an LSContext
   * instance. LSContext provides the Session with io_context,
   * work_guard, strand, threads, etc.
   */
  class LSContext final {
    using work_guard_t =
        asio::executor_work_guard<asio::io_context::executor_type>;

  public:
    LS_SANITIZE

    LSContext()
        : io_context_{std::make_unique<asio::io_context>()}
        , work_guard_{std::make_unique<work_guard_t>(
              io_context_->get_executor())}
        , strand_pool_{std::make_unique<StrandPool>(0, false, *io_context_)}
        , ref_cnt_{std::make_unique<std::atomic<std::size_t>>(0)}
        , hold_cnt_{std::make_unique<std::atomic<std::size_t>>(0)}
    { }

    LSContext(LSContext const&) = delete;
    LSContext& operator=(LSContext const&) = delete;
    /*
     * Move operations are needed to make this usable in a std::vector
     * Based on benchmarks, using a contiguous container for storage of
     * lscontexts of this pool has considerable effect on efficiency of
     * this pool.
     */
    LSContext(LSContext&&) noexcept;
    LSContext& operator=(LSContext&&) noexcept;

    ~LSContext() noexcept;

    asio::io_context& get_io_context() noexcept;
    /*
     * Set the number of threads that should run on each io_context.
     */
    void set_num_threads(std::size_t num_threads);
    void run_threads();
    int stop(bool force);
    /*
     * Blocks and joins on all of the threads running in this LSContext instance
     */
    void wait();
    /*
     * Returns true if and only if the LSContext instance can accpet new session
     * requests.
     */
    bool is_active();
    /*
     * Control ref_cnt_ which indicates the number of Session instances that
     * are currently assigned to this LSContext.
     */
    void ref();
    void deref();
    /*
     * Control hold_cnt_. If this reference count is positive then this
     * LSContext instance cannot be deactivated.
     */
    void hold();
    void unhold();
    /*
     * Returns true if and only if this LSContext is inactive but can be
     * reactivated to service session requests. An inactive instance may return
     * false, if old session mounted on it are not fully drained yet.
     */
    bool reusable();
    /*
     * Reactivate a deactivated instance so that it can start serving session
     * requests again.
     */
    void reuse(std::size_t threads_cnt);
    ContextInfo get_context_info() const;
    /*
     * Returns true if and only if this LSContext instance can be deactivated at
     * this time.
     */
    int removable();
    /*
     * Returns true if and only if the underlying asio::io_context is stopped
     */
    bool stopped();
    /*
     * Borrows a strand attached to the io_context of this LSContext.
     * The borrowed strand should be returned to this LSContext,
     * through 'put_strand'.
     * This method may return 'nullptr' if there is just a single thread
     * running in this LSContext, and thus no strand is needed anyway.
     */
    Strand* borrow_strand();
    void put_strand(Strand* s) noexcept;

  private:
    std::list<std::unique_ptr<std::thread>> threads_;
    std::unique_ptr<asio::io_context> io_context_;
    std::unique_ptr<work_guard_t> work_guard_;
    std::stack<Strand*> strands_;
    std::size_t num_threads_;
    std::unique_ptr<StrandPool> strand_pool_;
    std::unique_ptr<std::atomic<std::size_t>> ref_cnt_ = 0;
    std::unique_ptr<std::atomic<std::size_t>> hold_cnt_ = 0;
    std::atomic<bool> active_ = true;
    mutable std::mutex mtx_;
  };

  inline void
  LSContext::set_num_threads(std::size_t num_threads)
  {
    if (num_threads < 1)
      throw std::logic_error{"Thread multiplier should greater than 0"};
    if (num_threads > 64)
      throw std::logic_error{"Thread multiplier should be less than 65"};
    num_threads_ = num_threads;
  }

  inline asio::io_context&
  LSContext::get_io_context() noexcept
  {
    return *io_context_;
  }

  /*
   * The move operations of this class should not get invoked.
   * This defined to allow compilation.
   */
  inline LSContext::LSContext(LSContext&&) noexcept
  {
    __builtin_unreachable();
    abort();
  }

  /*
   * The move operations of this class should not get invoked.
   * This defined to allow compilation.
   */
  inline LSContext&
  LSContext::operator=(LSContext&&) noexcept
  {
    __builtin_unreachable();
    abort();
    return *this;
  }

  inline bool
  LSContext::stopped()
  {
    return io_context_->stopped();
  }

  inline int
  LSContext::stop(bool force)
  {
    std::scoped_lock _{mtx_};
    if (int rc = removable(); rc > 0 && !force)
      return (rc);

    active_.store(false);
    work_guard_.reset();
    wait();
    io_context_->stop();
    while (!io_context_->stopped())
      io_context_->run();
    io_context_ = std::make_unique<asio::io_context>();
    strand_pool_ = std::make_unique<StrandPool>(0, false, *io_context_);
    return (0);
  }

  inline void
  LSContext::wait()
  {
    for (auto& thread: threads_)
      if (thread->joinable())
        thread->join();
  }

  inline void
  LSContext::run_threads()
  {
    for (std::size_t i = 0; i < num_threads_; ++i) {
      threads_.emplace_back(
          std::make_unique<std::thread>([&]() { io_context_->run(); }));
    }
  }

  inline void
  LSContext::put_strand(Strand* s) noexcept
  {
    strand_pool_->put_back(s);
  }

  inline Strand*
  LSContext::borrow_strand()
  {
    std::scoped_lock _{mtx_};
    /*
     * Avoid the overhead of strand if there is just one thread
     * running in this LSContext.
     */
    if (num_threads_ == 1)
      return nullptr;

    auto s = strand_pool_->borrow();
    assert(s);
    return s;
  }

  inline bool
  LSContext::is_active()
  {
    return active_.load();
  }

  inline void
  LSContext::ref()
  {
    ref_cnt_->fetch_add(1);
  }

  inline void
  LSContext::deref()
  {
    assert(ref_cnt_->load() > 0);
    ref_cnt_->fetch_sub(1);
  }

  inline void
  LSContext::hold()
  {
    hold_cnt_->fetch_add(1);
  }

  inline void
  LSContext::unhold()
  {
    assert(hold_cnt_->load() > 0);
    hold_cnt_->fetch_sub(1);
  }

  inline bool
  LSContext::reusable()
  {
    // XXX Is ref_cnt_ == 0 constraint necessary?
    return !active_.load() && *ref_cnt_ == 0;
  }

  inline void
  LSContext::reuse(std::size_t threads_cnt)
  {
    std::scoped_lock _{mtx_};
    std::list<std::unique_ptr<std::thread>> threads;
    std::swap(threads, threads_);
    work_guard_ = std::make_unique<work_guard_t>(io_context_->get_executor());
    active_.store(true);
    num_threads_ = threads_cnt;
    run_threads();
  }

  inline LSContext::~LSContext() noexcept
  {
    stop(true);
    wait();
  }


  inline ContextInfo
  LSContext::get_context_info() const
  {
    std::vector<ContextInfo> contexts_info;
    ContextInfo context_info;

    context_info.context_index_ = 0;
    context_info.threads_cnt_ = threads_.size();
    context_info.active_sessions_cnt_ = *ref_cnt_;

    context_info.strand_pool_size_ = strand_pool_->get_size();
    context_info.strand_pool_flight_ = strand_pool_->get_in_flight_cnt();
    context_info.active_ = active_.load();

    return context_info;
  }

  inline int
  LSContext::removable()
  {
    if (hold_cnt_->load() > 0)
      return (EBUSY);
    return (0);
  }
} // namespace lserver