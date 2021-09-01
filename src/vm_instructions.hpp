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

#include <memory>
#include <thread>

#include "basic_pool.hpp"
#include "lsvm.hpp"
#include "vm_instructions_base.hpp"
#include "vm_instructions_list.hpp"

namespace lserver {

  /////////////////////////////////////////////////////////
  /// One derivative of Op for each type of instruction ///
  /////////////////////////////////////////////////////////

  /*
   * 'DOWNLOAD'
   * Set the volume of data to be downloaded as a result of
   * running this VScript.
   */
  class DownloadOp : public Op<DownloadOp> {
    template <class...>
    friend class OpList;

  public:
    LS_SANITIZE

    void run(Program& program, uintptr_t session_id,
             LSVirtualMachine& vm) override;

  private:
    static inline const opname_t name_ = "DOWNLOAD";
  };

  /*
   * 'LOCK'
   * Acquire and exclusively lock a resource in the VM. all
   * other session trying to acquire the same resource will
   * be blocked until this session either 'UNLOCK's this
   * resource or the session is closed which makes the vm
   * to automatically release all acquired resources.
   */
  class LockOp : public Op<LockOp> {
    template <class...>
    friend class OpList;

  public:
    LS_SANITIZE

    void run(Program& program, uintptr_t session_id,
             LSVirtualMachine& vm) override;

  private:
    static inline const opname_t name_ = "LOCK";
  };

  /*
   * 'UNLOCK'
   * Unlock a previously acquire resource by this VScript.
   * If the resource was not already acquired, this operation
   * will have not effect.
   */
  class UnlockOp : public Op<UnlockOp> {
    template <class...>
    friend class OpList;

  public:
    LS_SANITIZE

    void run(Program& program, uintptr_t session_id,
             LSVirtualMachine& vm) override;

  private:
    static inline const opname_t name_ = "UNLOCK";
  };

  /*
   * 'SLEEP'
   * Sleep and block the current thread for 'operand' microseconds.
   * This can be used to simulate a busy I/O-bound thread.
   */
  class SleepOp : public Op<SleepOp> {
    template <class...>
    friend class OpList;

  public:
    LS_SANITIZE

    void run(Program& program, uintptr_t session_id,
             LSVirtualMachine& vm) override;

  private:
    static inline const opname_t name_ = "SLEEP";
  };

  /*
   * 'LOOP'
   * Force the current thread to perform a spin loop of 'operand' cycles.
   * This can be used to simulate a busy CPU-bound thread.
   */
  class LoopOp : public Op<LoopOp> {
    template <class...>
    friend class OpList;

  public:
    LS_SANITIZE

    void run(Program& program, uintptr_t session_id,
             LSVirtualMachine& vm) override;

  private:
    static inline const opname_t name_ = "LOOP";
  };

  /*
   * Every Op derivative should be added to the following type list
   */
  using LSVMOps = OpList<DownloadOp, LockOp, UnlockOp, SleepOp, LoopOp>;

} // namespace lserver