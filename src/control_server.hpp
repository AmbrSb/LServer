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
#include <string>
#include <thread>

#include <grpcpp/grpcpp.h>

#include "lscomm.grpc.pb.h"
#include "lscomm.pb.h"
#include "manager.hpp"
#include "service.hpp"

using namespace std::literals;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

namespace lserver {

  /*
   * Provides a mechanism to control/query the servers at runtime.
   * - Allows extraction of operational statistics.
   * - Allows dynamically changing the configuration of the servers at
   * runtime.
   */
  class ControlServer : public StatsService::Service {
  public:
    ControlServer(ServerManager& manager, std::string const& bind_address);
    ~ControlServer();
    void start(std::string const& bind_address);
    void stop();

  private:
    /*
     * Extract operational statistics at the level if indivisual Servers.
     */
    Status GetStats(ServerContext* context, const StatsRequest* request,
                    StatsReply* reply);
    /*
     * Activate a new LSContext in a specified server, with the specified
     * number of threads.
     */
    Status AddContext(ServerContext* context, const AddContextRequest* request,
                      AddContextReply* reply);
    /*
     * Deactivate a specified LSContext in a specified server instance.
     */
    Status DeactivateContext(ServerContext* context,
                             const DeactivateContextRequest* request,
                             DeactivateContextReply* reply);
    /*
     * Extrac operation and structural data at the level of LSContexts
     * from all servers.
     */
    Status GetContextsInfo(ServerContext* context,
                           const GetContextInfoRequest* request,
                           GetContextInfoReply* reply);

    /*
     * All communication with the servers goes through a ServerManager
     * instance.
     */
    ServerManager& manager_;
    std::unique_ptr<grpc::Server> grpc_server_;
    std::thread thread_{};
  };
} // namespace lserver