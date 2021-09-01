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

#include "config.hpp"
#include <thread>

#include "portal.hpp"
#include "stats.hpp"

namespace lserver {

  Portal::Portal(ServerManager& manager, std::size_t header_interval,
                 std::string control_server_bind_address,
                 uint16_t control_server_bind_port)
      : manager_{manager}
      , header_interval_{header_interval}
      , control_server_{manager_, control_server_bind_address + ":" +
                                      std::to_string(control_server_bind_port)}
  { }

  void
  Portal::service_func()
  {
#ifdef ENABLE_STATISTICS
    print_stats(std::cout);
#endif
    std::this_thread::sleep_for(1s);
  }

#ifdef ENABLE_STATISTICS
  template <class STRM>
  void
  Portal::print_stats(STRM& stream)
  {
    for (auto const& item: manager_.get_stats())
      item.print_rec(stream, header_interval_);
  }
#endif
} // namespace lserver