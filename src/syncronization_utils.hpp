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

#include <atomic>
#include <condition_variable>
#include <list>
#include <mutex>


namespace lserver {
/*
 * Define and hold a scoped_guard from the TriggerGuard 'gv' in the current
 * scope. Return immediately if shutdown_guard is already triggered.
 * This prevents triggering of the TriggerGuard 'gv' while the guard is in
 * scope.
 */
#define SCOPED_GUARD_OR_RETURN(gv)                                             \
  auto scoped_guard = gv.acquire_scoped_guard();                               \
  if (!scoped_guard)                                                           \
    return;

  /*
   * This class can be used to prevent a trigger from going off in certain
   * time periods. For example, we may want to prevent server shutdown while
   * certain operation is in progress.
   * Each invocation of 'acquire_scoped_guard()' creates a scoped guard that
   * blocks all trigger attempts during its lifetime. Trigger request will
   * unblock and continue when there are no scoped guards in scope.
   */
  class TriggerGuard {
    class ScopedGuard;
    friend class ScopedGuard;
    class InactiveTriggerGuardInvoked : public std::exception { };

  public:
    void trigger();
    bool triggered();
    ScopedGuard acquire_scoped_guard();

  private:
    void release_scoped_guard();
    std::atomic<bool> triggered_ = false;
    std::atomic<std::size_t> ref_cnt_ = 0;
    mutable std::mutex mtx_;
    mutable std::condition_variable cv_;
  };

  /*
   * Each instance of 'ScopedGuard', during its lifetime, blocks tirggering
   * of its associated TriggerGuard instance.
   */
  class TriggerGuard::ScopedGuard {
  public:
    ScopedGuard(TriggerGuard& trigger_guard, bool triggered)
        : trigger_guard_{trigger_guard}
        , triggered_{triggered}
    { }

    ~ScopedGuard() noexcept
    {
      /*
       * If the associated guard was not triggered at the time of creation of
       * this guard instance, then it has bestowed upon us a lock, we should
       * now release.
       */
      if (!triggered_)
        trigger_guard_.release_scoped_guard();
    }

    operator bool() { return !trigger_guard_.triggered_.load(); }

  private:
    TriggerGuard& trigger_guard_;
    bool triggered_;
  };

  /*
   * Similar functionality to std::once_flag/call_once, but is also reusable.
   */
  class ResetableOnceFlag {
  public:
    void
    reset()
    {
      /*
       * Mutex is just needed to enforce "synchronizes with" with
       * calls to run_once().
       */
      std::scoped_lock _{mtx_};
      invoked_ = false;
    }

    void
    run_once(auto&& F)
    {
      std::scoped_lock _{mtx_};
      if (!invoked_)
        F();
      invoked_ = true;
    }

  private:
    std::mutex mtx_;
    bool invoked_;
  };

  inline void
  TriggerGuard::trigger()
  {
    std::unique_lock lk{mtx_};
    if (triggered_)
      throw InactiveTriggerGuardInvoked{};
    cv_.wait(lk, [this] { return (ref_cnt_.load() == 0); });
    triggered_.store(true);
  }

  inline auto
  TriggerGuard::acquire_scoped_guard() -> ScopedGuard
  {
    std::unique_lock _{mtx_};
    if (!triggered_.load())
      ref_cnt_.fetch_add(1);
    return ScopedGuard{*this, triggered_.load()};
  }

  inline void
  TriggerGuard::release_scoped_guard()
  {
    ref_cnt_.fetch_sub(1);
    cv_.notify_all();
  }
} // namespace lserver