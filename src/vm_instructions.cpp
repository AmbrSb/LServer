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


#include "vm_instructions.hpp"
#include "common.hpp"
#include "program.hpp"

namespace lserver {
  
  void
  DownloadOp::run(Program& program, uintptr_t session_id, LSVirtualMachine& vm)
  {
    program.set_result_code(200);
    program.set_downloaded_size(operand_);
  }

  void
  LockOp::run(Program& program, uintptr_t session_id, LSVirtualMachine& vm)
  {
    vm.lock(session_id, operand_, program.cancellation_request_ref());
  }

  void
  UnlockOp::run(Program& program, uintptr_t session_id, LSVirtualMachine& vm)
  {
    vm.unlock(session_id, operand_);
  }

  void
  SleepOp::run(Program& program, uintptr_t session_id, LSVirtualMachine& vm)
  {
    vm.sleep(operand_);
  }

  void
  LoopOp::run(Program& program, uintptr_t session_id, LSVirtualMachine& vm)
  {
    vm.loop(operand_);
  }

} // namespace lserver