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

#include "common.hpp"
#include "server.hpp"
#ifdef ENABLE_STATISTICS
#include "stats.hpp"
#endif

namespace lserver {
  /*
   * This class is responsible for creating servers and managing their
   * lifetime. It also provides a single point of contact for various
   * other operatios/queries on servers, like get_stats(), etc.
   */
  class ServerManager {
  public:
    using ServerHandle = int;
    class ServerCreationFailed : public std::exception { };

    ServerManager() = default;
    ~ServerManager();
    /*
     * Create a managed server using P as the Session (CRTP-base) derived
     * protocol.
     */
    template <class P>
    ServerHandle create_server(auto config);
    AbstractServer* get_server(ServerHandle sh);
    std::vector<ServerInfo> get_servers_info();
    bool validate_server_handle(ServerHandle sh);
    void wait();
    void stop();
    void stop(ServerHandle sh);
#ifdef ENABLE_STATISTICS
    std::vector<LSStats> get_stats() const;
#endif

  private:
    std::map<ServerHandle, AbstractServer*> servers_;
    ServerHandle handle_id_ = 0;
  };

  template <class P>
  inline ServerManager::ServerHandle
  ServerManager::create_server(auto config)
  {
    auto srv = new Server<P>(config);
    auto [iter, inserted] = servers_.insert(std::make_pair(handle_id_++, srv));
    if (!inserted) {
      delete srv;
      throw ServerCreationFailed{};
    }
    srv->dispatch();
    return iter->first;
  }

} // namespace lserver