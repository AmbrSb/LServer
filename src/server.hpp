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

#include <atomic>
#include <memory>
#include <optional>
#include <type_traits>

#include <asio.hpp>

#include "common.hpp"
#include "config.hpp"
#include "io_context_pool.hpp"
#include "session.hpp"
#include "session_pool.hpp"
#include "syncronization_utils.hpp"
#ifdef ENABLE_STATISTICS
#include "stats.hpp"
#endif

namespace lserver {

  using tcp = asio::ip::tcp;
  using namespace std::placeholders;
  using namespace std::literals;

#ifdef __cpp_concepts
#define SESSION_CONCEPT requires IsSession<P>
#else
#define SESSION_CONCEPT
#endif

  /*
   * The abstract base for all instances of the Server<Protocol> template.
   */
  struct AbstractServer {
    AbstractServer() = default;
    virtual ~AbstractServer() = default;
    AbstractServer(AbstractServer const&) = delete;
    AbstractServer(AbstractServer&&) = delete;
    AbstractServer& operator=(AbstractServer const&) = delete;
    AbstractServer& operator=(AbstractServer&&) = delete;

    virtual void stop() = 0;
    virtual void wait() = 0;
    /*
     * Add a new LSContext to the set of active contexts of this server.
     * It may recover and activate an already deactivated LSContext.
     */
    virtual void add_context(std::size_t thread_cnt) = 0;
    /*
     * Deactivate the LSContext with id 'context_id'.
     */
    virtual int deactivate_context(std::size_t context_index) = 0;
    virtual ServerInfo get_server_info() const = 0;
#ifdef ENABLE_STATISTICS
    virtual LSStats get_stats() const = 0;
#endif
  };

  template <class P>
  SESSION_CONCEPT class Server final : public AbstractServer {
  public:
    Server(LSConfig config);
    ~Server() override = default;
    void stop() override;
    void wait() override;
    /*
     * Acquires an LSContext and calls async_accept() to accept a new
     * connection. This method does not block and returns immediately.
     */
    void dispatch();
    void add_context(std::size_t thread_cnt) override;
    int deactivate_context(std::size_t context_index) override;
    ServerInfo get_server_info() const override;
#ifdef ENABLE_STATISTICS
    LSStats get_stats() const override;
#endif

  private:
    LSConfig config_;
    LSContextPool workers_pool_;
    SessionPool<P> pool_;
    LSContextPool acceptor_pool_;
    /*
     * Holds the socket for the next incomming connection. This is built on
     * the next chosen LSContext and is passed to the acceptor.
     */
    std::optional<tcp::socket> socket_;
    tcp::acceptor acceptor_;
    TriggerGuard shutdown_guard_;
#ifdef ENABLE_STATISTICS
    ServerStats stats_;
#endif
  };

  template <class P>
  SESSION_CONCEPT
  Server<P>::Server(LSConfig config)
      : config_{config}
      , workers_pool_{config_.num_workers_, config_.max_num_workers_,
                      config_.num_threads_per_worker_}
      , pool_(config_.max_session_pool_size_, config_.eager_session_pool_)
      , acceptor_pool_{1, 1, 1}
      , acceptor_{config_.separate_acceptor_thread_
                      ? std::get<0>(acceptor_pool_.get_context_round_robin())
                            ->get_io_context()
                      : std::get<0>(workers_pool_.get_context_round_robin())
                            ->get_io_context()}
  {
    asio::ip::tcp::endpoint ep(asio::ip::tcp::v4(), config_.listen_port_);
    acceptor_.open(ep.protocol());
    acceptor_.set_option(
        asio::ip::tcp::acceptor::reuse_address(config_.reuse_address_));
    acceptor_.set_option(asio::socket_base::linger(
        config_.socket_close_linger_, config_.socket_close_linger_timeout_));
    acceptor_.bind(ep);
    acceptor_.listen();
  }

#ifdef ENABLE_STATISTICS
  template <class P>
  SESSION_CONCEPT LSStats
  Server<P>::get_stats() const
  {
    auto const& [pool_stats, session_stats] = pool_.get_stats();
    return LSStats(stats_, pool_stats, session_stats);
  }
#endif

  template <class P>
  SESSION_CONCEPT void
  Server<P>::stop()
  {
    /*
     * Block and wait on the shutdown_guard until its clear.
     * This prevents us from shutting down the server while it's
     * registering/processing an async accept.
     */
    shutdown_guard_.trigger();

    socket_ = std::nullopt;
    acceptor_.close();
    if (config_.separate_acceptor_thread_)
      acceptor_pool_.stop();
    workers_pool_.stop();
    lslog_note(0, "Workers pool stopped");
  }

  template <class P>
  SESSION_CONCEPT void
  Server<P>::add_context(std::size_t thread_cnt)
  {
    workers_pool_.add_context(thread_cnt);
  }

  template <class P>
  SESSION_CONCEPT int
  Server<P>::deactivate_context(std::size_t context_index)
  {
    int rc = workers_pool_.deactivate_context(context_index);
    return (rc);
  }

  template <class P>
  SESSION_CONCEPT void
  Server<P>::dispatch()
  {
    auto [lscontext, id] = workers_pool_.get_context_round_robin();
    socket_.emplace(lscontext->get_io_context());

    /*
     * Prevent triggering of server shutdown while we are registering an
     * asyncronous accept.
     */
    SCOPED_GUARD_OR_RETURN(shutdown_guard_);

    acceptor_.async_accept(*socket_, [this, lscontext = lscontext,
                                      id = id](std::error_code error) {
      P* protocol;

      /*
       * Prevent triggering of server shutdown while we are starting a
       * new session.
       */
      SCOPED_GUARD_OR_RETURN(shutdown_guard_);

      if (!error && (protocol = pool_.borrow(id))) {
        protocol->setup(*lscontext, std::move(*socket_));
        protocol->session_start();
#ifdef ENABLE_STATISTICS
        stats_.stats_accepted_cnt.fetch_add(1);
#endif
      } else {
        /*
         * Normally this is done by the Session instance. We need to do
         * it manually in the unhappy path.
         */
        lscontext->unhold();
      }

      this->dispatch();
    });
  }

  template <class P>
  SESSION_CONCEPT void
  Server<P>::wait()
  {
    if (config_.separate_acceptor_thread_)
      acceptor_pool_.wait();
    workers_pool_.wait();
  }

  template <class P>
  SESSION_CONCEPT ServerInfo
  Server<P>::get_server_info() const
  {
    ServerInfo si;
    si.contexts_info_ = workers_pool_.get_contexts_info();
    return si;
  }
} // namespace lserver