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

#include <iostream>
#include <system_error>

#include <asio.hpp>

#include "common.hpp"

namespace lserver {

  using namespace std::placeholders;

  /*
   * Registers signal listener for SIGINT and SIGTERM and runs the
   * user-provided callbacks.
   */
  class SignalManager {
  public:
    template <class F>
    SignalManager(F&& exit_cb);
    void run();
    void handler(const asio::error_code& error, int signal_number);
    void wait();

  private:
    using work_guard_t =
        asio::executor_work_guard<asio::io_context::executor_type>;

    asio::io_context ioc_;
    work_guard_t work_guard{ioc_.get_executor()};
    asio::signal_set signals_{ioc_, SIGINT, SIGTERM};
    std::function<void(void)> exit_cb_;
    std::thread t_{std::bind(&SignalManager::run, this)};
  };

  template <class F>
  inline SignalManager::SignalManager(F&& exit_cb)
      : exit_cb_{exit_cb}
  { }

  inline void
  SignalManager::run()
  {
    signals_.async_wait(std::bind(&SignalManager::handler, this, _1, _2));
    ioc_.run();
  }

  inline void
  SignalManager::handler(const std::error_code& error, int signal_number)
  {
    if (error) {
      lslog(0, "Error in signal handler: ",
            std::error_condition{error.value(), std::system_category()}
                .message());
      return;
    }

    if (signal_number == SIGINT || signal_number == SIGTERM) {
      lslog_note(1, "Shutting down signal manager");
      exit_cb_();
      return;
    }

    signals_.async_wait(std::bind(&SignalManager::handler, this, _1, _2));
  }

  inline void
  SignalManager::wait()
  {
    ioc_.stop();
    t_.join();
  }

} // namespace lserver