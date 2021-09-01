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

#include "basic_pool.hpp"
#include "lsvm.hpp"


namespace lserver {

  class Program;

  /*
   * The common dynamic base class for all Op types
   */
  class BaseOp {
    friend struct OpCompare;

  public:
    virtual ~BaseOp() noexcept = default;
    /*
     * Run this operation on LSVirtualMachine 'vm' on behalf of session
     * 'session_id'
     */
    virtual void run(Program& program, uintptr_t session_id,
                     LSVirtualMachine& vm) = 0;
    virtual void repool() = 0;

    void
    set_args(std::size_t exec_point, std::size_t operand)
    {
      exec_point_ = exec_point;
      operand_ = operand;
    }

    std::size_t
    get_exec_point() const
    {
      return exec_point_;
    }

  protected:
    using opname_t = std::string;

    std::size_t exec_point_;
    std::size_t operand_;
  };

  /*
   * Wrap Op pointer in a unique_ptr with a custom deleter that puts the
   * Op instance back in the owning pool.
   */
  inline auto op_repooler = +[](BaseOp* p) { p->repool(); };
  using OpRepooler = decltype(op_repooler);
  using OpPtr = std::unique_ptr<BaseOp, OpRepooler>;

  /*
   * CRTP Base class for all OpX types. This type is responsible for handling
   * Op type dependent common parts of Op classes, specifically the Pool type
   * and related operations are factored into this CRTP base.
   */
  template <class D>
  class Op : public BaseOp {

  public:
    D*
    get_derived()
    {
      return static_cast<D*>(this);
    }

    /*
     * Returns an instance of Op of type D taken from a pool of appropriate
     * type.
     */
    template <class... Args>
    static inline D*
    get_instance(Args... args)
    {
      auto p = pool_.borrow();
      p->set_args(args...);
      return p;
    }

    /*
     * Put this Op instance back into its corresponding pool
     */
    void
    repool() override
    {
      pool_.put_back(get_derived());
    }

  private:
    /*
     * One pool instance per Op type.
     * Note: This shared among all servers in the same process.
     */
    static inline BasicPool<D> pool_{0, false};
  };

  /*
   * Custom comparator for BaseOp derivatives.
   * Primarily used by the 'instructions_' priority queue in Program
   * instances.
   */
  struct OpCompare {
    bool
    operator()(OpPtr const& left, OpPtr const& right)
    {
      return left->exec_point_ > right->exec_point_;
    }
  };

} // namespace lserver