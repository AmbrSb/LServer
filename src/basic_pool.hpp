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

namespace lserver {

  /*
   * BasicPool is the simplest form of a pool made from Pool<> CRTP base.
   * It uses default new/delete for lifetime management of the items.
   */
  template <class T>
  class BasicPool : public Pool<T, BasicPool<T>> {
    using Base = Pool<T, BasicPool<T>>;

  public:
    BasicPool(std::size_t max_size, bool eager);
    /*
     * create/destroy functions to be used by the CRTP base
     */
    T* create();
    void destroy(T* p) noexcept(std::is_nothrow_destructible_v<T>);
  };

  template <class T>
  inline BasicPool<T>::BasicPool(std::size_t max_size, bool eager)
      : Base(max_size, eager)
  { }

  template <class T>
  inline T*
  BasicPool<T>::create()
  {
    return new T{};
  }

  template <class T>
  inline void
  BasicPool<T>::destroy(T* p) noexcept(std::is_nothrow_destructible_v<T>)
  {
    delete p;
  }

} // namespace lserver