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
 * this list of conditions and the following disclaimer.
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

#include <yaml-cpp/yaml.h>

namespace lserver {
  struct ConfigParseError : std::exception { };

  struct LSConfig {
  public:
    /*
     * Construct a config object based program command line arguments
     */
    LSConfig(int argc, char* argv[]);

    std::string listen_address_;
    std::string control_listen_address_;
    std::size_t num_workers_;
    std::size_t max_num_workers_;
    std::size_t num_threads_per_worker_;
    std::size_t max_session_pool_size_;
    std::size_t max_transfer_sz_;
    std::size_t max_connections_per_source_;
    std::size_t header_interval_;
    uint16_t listen_port_;
    uint16_t control_listen_port_;
    bool reuse_address_;
    bool socket_close_linger_;
    bool socket_close_linger_timeout_;
    bool eager_session_pool_;
    bool separate_acceptor_thread_;

  private:
    /*
     * Read indivisual items from the config YAML file.
     */
    void parse_config();
    /*
     * Parse an item of type T with level 1 key L1 and level 2 key L2
     */
    template <class T>
    T read_config(std::string L1, std::string L2);

    YAML::Node config_;
  };
} // namespace lserver