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

#include <string>

#include "vm_instructions.hpp"
#include "vm_instructions_base.hpp"


namespace lserver {

  /*
   * OpList holds a list of Op derivative and can instantiate
   * instances of them based on the operation name.
   */
  template <class... T>
  class OpList;

  /*
   * A specialization of OpList which marks the end of the recursive
   * instantiation.
   */
  template <>
  class OpList<> {
  public:
    OpList() = default;

    /*
     * This instance of 'instantiate()' will be reached only if no
     * derivative of Op has name 'name'.
     * @returns NULL OpPtr
     */
    template <class... Args>
    static inline OpPtr
    instantiate(std::string name, Args... args)
    {
      return OpPtr{nullptr, [](auto _) {}};
    }
  };

  template <class H, class... T>
  class OpList<H, T...> : public OpList<T...> {
    using Base = OpList<T...>;

  public:
    /*
     * Create an instance of a derivative of BaseOp which is named 'name'.
     * args... argument pack will be passed to the constructor of that
     * type.
     */
    template <class... Args>
    static inline OpPtr instantiate(std::string name, Args... args);
  };

  template <class H, class... T>
  template <class... Args>
  inline OpPtr
  OpList<H, T...>::instantiate(std::string name, Args... args)
  {
    if (name == H::name_) {
      auto op = H::get_instance(args...);
      return OpPtr(op, op_repooler);
    } else
      return Base::template instantiate(name, args...);
  }
} // namespace lserver
