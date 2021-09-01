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

#include "control_server.hpp"
#include "common.hpp"
#include "stats.hpp"
#include "timing.hpp"

namespace lserver {

  ControlServer::ControlServer(ServerManager& manager,
                               std::string const& bind_address)
      : manager_{manager}
  {
    start(bind_address);
  }

  ControlServer::~ControlServer() { stop(); }

  void
  ControlServer::start(std::string const& bind_address)
  {
    ServerBuilder builder_;
    builder_.AddListeningPort(bind_address, grpc::InsecureServerCredentials());
    builder_.RegisterService(this);
    grpc_server_ = std::unique_ptr(builder_.BuildAndStart());
    thread_ = std::thread{[this]() { this->grpc_server_->Wait(); }};
    lslog_note(1, "LS control server listening on ", bind_address);
  }

  void
  ControlServer::stop()
  {
    lslog_note(1, "Shutting down LS control service");
    grpc_server_->Shutdown();
    thread_.join();
  }

  Status
  ControlServer::GetStats(ServerContext* context, const StatsRequest* request,
                          StatsReply* reply)
  {
    auto recs = manager_.get_stats();

    for (auto const& rec: recs) {
      auto const& [rec_time, server_stats, session_pool_stats, session_stats] =
          rec;

      /*
       * Create 1 StatsRec instance per server
       */
      auto stats_rec = reply->add_stats_rec();
      stats_rec->set_time(timepoint_to_micros(rec_time));
      stats_rec->set_stats_accepted_cnt(server_stats.stats_accepted_cnt);
      stats_rec->set_num_items_total(session_pool_stats.num_items_total_);
      stats_rec->set_num_items_in_flight(
          session_pool_stats.num_items_in_flight_);
      stats_rec->set_stats_transactions_cnt_delta(
          session_stats.stats_transactions_cnt_delta_);
      stats_rec->set_stats_bytes_received_delta(
          session_stats.stats_bytes_received_delta_);
      stats_rec->set_stats_bytes_sent_delta(
          session_stats.stats_bytes_sent_delta_);
    }

    return Status::OK;
  }

  Status
  ControlServer::AddContext(ServerContext* context,
                            const AddContextRequest* request,
                            AddContextReply* reply)
  {
    manager_.get_server(request->server_id())
        ->add_context(request->num_threads());
    return Status::OK;
  }

  Status
  ControlServer::DeactivateContext(ServerContext* context,
                                   const DeactivateContextRequest* request,
                                   DeactivateContextReply* reply)
  {
    int rc = manager_.get_server(request->server_id())
                 ->deactivate_context(request->context_index());
    reply->set_status_code(rc);
    return Status::OK;
  }

  Status
  ControlServer::GetContextsInfo(ServerContext* context,
                                 const GetContextInfoRequest* request,
                                 GetContextInfoReply* reply)
  {
    auto servers_info = manager_.get_servers_info();

    for (auto const& server_info: servers_info) {
      /*
       * 1 ServerInfo instance for each server
       */
      auto si = reply->add_server_info();
      for (auto const& context_info: server_info.contexts_info_) {
        /*
         * 1 ContextInfo instance for each LSContext in this server
         */
        auto ci = si->add_contexts_info();
        ci->set_context_index(context_info.context_index_);
        ci->set_threads_cnt(context_info.threads_cnt_);
        ci->set_active_sessions_cnt(context_info.active_sessions_cnt_);
        ci->set_strand_pool_size(context_info.strand_pool_size_);
        ci->set_strand_pool_flight(context_info.strand_pool_flight_);
        ci->set_active(context_info.active_);
      }
    }
    return Status::OK;
  }
} // namespace lserver