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

#include "unistd.h"

#include <chrono>
#include <condition_variable>
#include <shared_mutex>
#include <unordered_map>

#include "common.hpp"

namespace lserver {
  using namespace std::chrono;

  /*
   * "Exclusive Lock" resources that VScripts running on top of an Http
   * service can acquire and hold to simulate exclusive resource in the
   * simulated workload.
   */
  struct VMResource {
    /*
     * Condition variable is used instead of mutex so that resources
     * acquired by one thread can be release by another thread.
     */
    std::condition_variable cv_;
    std::mutex mtx_;
    bool taken = false;
    /*
     * Id of the session that has acquired this resource instance. This
     * used by the clean up logic of the VM to release resource of finished
     * VScripts.
     */
    uintptr_t holder_id;
  };

  class LSVirtualMachine {
  public:
    LSVirtualMachine() = default;
    /*
     * Lock resource number 'num' on behalf of the session with id
     * 'session_id'
     */
    void lock(uintptr_t session_id, std::size_t num,
              std::atomic_bool const& cancellation_request);
    /*
     * Unlock resource number 'num' on behalf of the session with id
     * 'session_id'
     */
    void unlock(uintptr_t session_id, std::size_t num);
    /*
     * Release all resources locked by session 'session_id'
     */
    void cleanup(uintptr_t session_id);
    /*
     * Sleep the calling thread for 'operand' microseconds
     */
    void sleep(std::size_t operand);
    /*
     * Loop the calling thread for 'operand' cycles
     */
    void loop(std::size_t operand);

  private:
    /*
     * Protects the resources_ map and condition variables of individual
     * VMResource instances.
     */
    mutable std::shared_mutex mtx_;
    /*
     * VMResource instances are allocated on-demand and added to this
     * hash map.
     */
    std::unordered_map<std::size_t, VMResource> resources_;
  };

  inline void
  LSVirtualMachine::sleep(std::size_t operand)
  {
    std::this_thread::sleep_for(operand * 1us);
  }

  inline void
  LSVirtualMachine::loop(std::size_t operand)
  {
    for (std::size_t i = 0; i < operand; ++i)
      asm volatile("" : "+g"(i) : :);
  }

  inline void
  LSVirtualMachine::lock(uintptr_t session_id, std::size_t num,
                         std::atomic_bool const& cancellation_request)
  {
    /*
     * IIFE to guard access to resources_ via mtx_
     */
    auto& res = [ this, num ]() -> auto&
    {
      std::shared_lock _{mtx_};
      return resources_[num];
    }
    ();
    auto& res_mtx = res.mtx_;
    auto& res_cv = res.cv_;

    while (!cancellation_request.load()) {
      std::unique_lock<std::mutex> res_lk{res_mtx};
      auto available =
          res_cv.wait_for(res_lk, 100ms, [&res]() { return !res.taken; });

      if (available) {
        res.taken = true;
        res.holder_id = session_id;
        break;
      }
    }
  }

  inline void
  LSVirtualMachine::unlock(uintptr_t session_id, std::size_t num)
  {
    /*
     * IIFE to guard access to resources_ via mtx_
     */
    auto& res = [ this, num ]() -> auto&
    {
      std::shared_lock _{mtx_};
      return resources_[num];
    }
    ();
    auto& res_mtx = res.mtx_;
    auto& res_cv = res.cv_;

    {
      std::unique_lock<std::mutex> res_lk{res_mtx};
      res.taken = false;
      res_cv.notify_one();
    }
  }

  inline void
  LSVirtualMachine::cleanup(uintptr_t session_id)
  {
    std::unique_lock _{mtx_};

    for (auto& kv: resources_) {
      auto& res = kv.second;
      auto& res_cv = res.cv_;
      {
        std::unique_lock<std::mutex> res_lk{res.mtx_};
        if (res.holder_id == session_id) {
          res.taken = false;
          res_cv.notify_one();
        }
      }
    }
  }

}; // namespace lserver