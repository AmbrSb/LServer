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

#include <functional>
#include <thread>

namespace lserver {
  /*
   * CRTP base class for creating a general service type. The derived
   * type should provide 'service_func()' as the function called in
   * the service loop of the service.
   * This class starts a dedicated thread for the serivce.
   */
  template <class S>
  class Service {
  public:
    Service() = default;
    void start();
    void stop();
    /*
     * Joins and blocks on the service thread.
     */
    void wait();

  private:
    void service_loop();
    S& derived();
    S const& derived() const;

    std::thread thread_;
    std::atomic<bool> shutdown_requested_ = false;
  };

  template <class S>
  void
  Service<S>::start()
  {
    thread_ = std::thread{std::bind(&Service::service_loop, this)};
  }

  template <class S>
  void
  Service<S>::service_loop()
  {
    while (!shutdown_requested_.load()) {
      derived().service_func();
    }
  }

  template <class S>
  void
  Service<S>::stop()
  {
    shutdown_requested_.store(true);
  }

  template <class S>
  void
  Service<S>::wait()
  {
    thread_.join();
  }

  template <class S>
  inline S&
  Service<S>::derived()
  {
    return *(static_cast<S*>(this));
  }

  template <class S>
  inline S const&
  Service<S>::derived() const
  {
    return *(const_cast<S const*>(static_cast<S*>(this)));
  }

} // namespace lserver