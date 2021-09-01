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

#include <cstdint>
#include <string>

#include "common.hpp"
#include "config.hpp"

namespace lserver {
  LSConfig::LSConfig(int argc, char* argv[])
      : config_{YAML::LoadFile(argv[1])}
  {
    parse_config();
  }

  void
  LSConfig::parse_config()
  {
    using std::size_t;
    using std::string;

    control_listen_address_ = read_config<string>("control_server", "ip");
    control_listen_port_ = read_config<uint16_t>("control_server", "port");

    listen_address_ = read_config<string>("listen", "ip");
    listen_port_ = read_config<uint16_t>("listen", "port");

    reuse_address_ = read_config<bool>("listen", "reuse_address");
    separate_acceptor_thread_ =
        read_config<bool>("listen", "separate_acceptor_thread");

    socket_close_linger_ =
        read_config<bool>("networking", "socket_close_linger");

    socket_close_linger_timeout_ =
        read_config<size_t>("networking", "socket_close_linger_timeout");

    max_connections_per_source_ =
        read_config<size_t>("networking", "max_connections_per_source");

    num_workers_ = read_config<size_t>("concurrency", "num_workers");

    max_num_workers_ = read_config<size_t>("concurrency", "max_num_workers");

    num_threads_per_worker_ =
        read_config<size_t>("concurrency", "num_threads_per_worker");

    max_session_pool_size_ =
        read_config<size_t>("sessions", "max_session_pool_size");

    max_transfer_sz_ = read_config<size_t>("sessions", "max_transfer_size");

    eager_session_pool_ = read_config<bool>("sessions", "eager_session_pool");

    header_interval_ = read_config<size_t>("logging", "header_interval");
  }

  template <class T>
  T
  LSConfig::read_config(std::string L1, std::string L2)
  {
    try {
      return config_[L1][L2].as<T>();
    } catch (...) {
      lslog(0, "Error while parsing config line:", L1, L2);
      throw ConfigParseError{};
    }
  }
} // namespace lserver