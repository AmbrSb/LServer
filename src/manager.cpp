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


#include "manager.hpp"

namespace lserver {

  ServerManager::~ServerManager()
  {
    for (auto const& entry: servers_) {
      auto srv = entry.second;
      delete srv;
    }
    lslog_note(1, "All servers destroyed");
  }

  AbstractServer*
  ServerManager::get_server(ServerHandle sh)
  {
    if (!validate_server_handle(sh))
      throw std::logic_error("ServerManager::get_server(): Invalid server ID.");

    return servers_[sh];
  }

#ifdef ENABLE_STATISTICS
  auto
  ServerManager::get_stats() const -> std::vector<LSStats>
  {
    std::vector<LSStats> server_stats;

    for (auto const& hs: servers_) {
      auto const* server = hs.second;
      auto const& s = server->get_stats();
      server_stats.push_back(s);
    }

    return server_stats;
  }
#endif

  void
  ServerManager::wait()
  {
    for (auto& srv: servers_)
      srv.second->wait();
  }

  void
  ServerManager::stop(ServerHandle sh)
  {
    if (!validate_server_handle(sh))
      throw std::logic_error("ServerManager::stop: Invalid server ID.");

    servers_[sh]->stop();
  }

  void
  ServerManager::stop()
  {
    for (auto& srv: servers_)
      srv.second->stop();
  }

  std::vector<ServerInfo>
  ServerManager::get_servers_info()
  {
    std::vector<ServerInfo> servers_info;

    for (auto& server: servers_) {
      servers_info.push_back(server.second->get_server_info());
    }

    return servers_info;
  }

  bool
  ServerManager::validate_server_handle(ServerHandle sh)
  {
    return (sh < servers_.size());
  }
} // namespace lserver