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

#include <sys/time.h>

#include <any>
#include <atomic>
#include <exception>

#include <asio.hpp>

#include "common.hpp"
#include "dynamic_queue.hpp"
#include "io_context_pool.hpp"
#include "program.hpp"
#include "syncronization_utils.hpp"
#ifdef ENABLE_STATISTICS
#include "stats.hpp"
#endif

namespace lserver {

  template <class P>
  class Session;

  using tcp = asio::ip::tcp;
  using DynQue = DynamicQueue<DynamicString>;
  using namespace std::placeholders;

  /*
   * This concept verifies that type P is a CRTP-style derivative of template
   * Session. IsSession is a requirement of template parameter of the Server
   * template.
   */
#ifdef __cpp_concepts
  template <class P>
  concept IsSession = std::derived_from<P, Session<P>>;
#endif
  /*
   * The CRTP base template Session<P> provides the base networking
   * services for the upper layer 'Protocol'.
   * It also provides some common services for the 'Server' to aid
   * in efficient management of resources.
   */
  template <class P>
  class ALIGN_DESTRUCTIVE Session {
  public:
    /*
     * This should be included in the public section of any protocol
     * class. It injects sanitization facilities used for debugging
     * and sanitiy checking.
     */
    LS_SANITIZE

    class BadReceptionState : std::exception { };

    Session() = default;
    void setup(LSContext& lscontext, tcp::socket&& socket);
    void session_start();
    template <class F>
    void set_finalized_cb(F&& on_finalized_cb);
#ifdef ENABLE_STATISTICS
    SessionStatsDelta& get_stats_delta();
#endif

  protected:
    enum Feedback { kFinished, kContinue, kClose, kData };

    ~Session() noexcept = default;
    Session(Session const&) = delete;
    Session(Session&&) = delete;
    Session& operator=(Session const&) = delete;
    Session& operator=(Session&&) = delete;

    /*
     * Trigger asynchronous reception of data. A call to this function
     * will never block, but may result in subsequent calls to the CRTP
     * derived 'P' class, like on_data(), on_error(), and on_closed().
     */
    void receive();
    void send(DynQue::QueueBuffer* qb);
    /*
     * Throws away 'length' bytes of data from the input stream of this
     * session. If 'length' is zero all currently buffered data is
     * discarded
     */
    void consume(std::size_t length = 0);
    /*
     * Returns a buffer that the client can fill and later pass to the
     * 'send()' function.
     */
    DynQue::QueueBuffer* prepare_send_buffer(std::size_t n);
    void release_send_buffer(DynQue::QueueBuffer* qb);
    /*
     * Returns a raw pointer to the underlying buffer storing the sessions
     * data stream.
     */
    uint8_t* data();
    void set_expected_data_length(std::size_t len);
    std::size_t get_bytes_received();
    std::size_t data_size();
    /*
     * Resets the internal buffers and counters of the Session object,
     * and prepare it to handle a new 'transaction'.
     */
    void reset_buffers();
    /*
     * Returns true only if this Session instance has received
     * 'expected_data_chunck_sz_' bytes of data, since the last call to
     * 'reset_buffers()'.
     */
    bool check_finished();

    void transaction_started();
    void transaction_finished();

  private:
    void async_receive();
    void async_send();
    void async_close(std::error_code error);
    void receive_event_cb(std::error_code error, std::size_t bytes_transferred);
    void send_event_cb(std::error_code error, std::size_t bytes_transferred);
    /*
     * Tries to close down the current session. If called multiple times
     * in a single session, exactly one of the calls goes through and just
     * one shutdown sequence is performed for the session.
     * It may defer close down if there are still outging data queue and
     * waiting to be sent out.
     */
    void close_once();
    /*
     * Performs the shutdown sequence of the session.
     */
    void finalize();
    void report_error(std::error_code& error);
    /*
     * Get a pointer to the CRTP derived protocol instance.
     */
    P* get_protocol();

    /*
     * Queue of buffers to be sent. This is necessary because we cannot
     * have concurrent async_write operations in flight. Buffers are poped
     * from this queue and sent one-by-one.
     */
    DynQue outgoing_queue_;
    /*
     * 'ubuf_' is the underlying buffer used for reception of data in each
     * Session instance.
     */
    std::vector<uint8_t> ubuf_;
    std::optional<tcp::socket> socket_;
    /*
     * The LSContext in which this Session instance is attached. It has the
     * io_context, plus a pool of strands, a thread pool, and other items
     * associated with each io_context.
     */
    LSContext* lscontext_ = nullptr;
    /*
     * Each time a session is selected from the session pool to serve a user
     * connection, it may also request to get s trand from the LSContext. This
     * happens only when there are multiple threads running in the io_context.
     * When the session is closed, the strand should be put back in the
     * LSContext strand poool.
     */
    Strand* strand_ = nullptr;
    /*
     * This is set by the CRTP derived Protocol class to hint the Session as to
     * the amount of data it expects to see comming from over the connection.
     * Sessoin may use this to optimize the way it reads data from its
     * underlying socket.
     */
    std::size_t expected_data_chunck_sz_ = 0;
    /*
     * If this is false, then the session may assume that the input data stream
     * over the connection is "inifinite", and ignores
     * 'expected_data_chunck_sz_'.
     */
    bool expected_data_chunck_sz_set_ = false;
    std::atomic<bool> prepare_for_shutdown_ = false;
    /*
     * The session calls this callback as the last thing in its chain of
     * shutdown. A Server instance may use this callback to recycle or re-pool
     * the Session instance.
     */
    std::function<void(P*)> finalized_;
    ResetableOnceFlag close_once_flag_;

    std::size_t bytes_received_ = 0;
    std::size_t bytes_sent_ = 0;
#ifdef ENABLE_STATISTICS
    SessionStatsDelta stats_;
#endif
  };

  template <class P>
  template <class F>
  void
  Session<P>::set_finalized_cb(F&& on_finalized_cb)
  {
    finalized_ = on_finalized_cb;
  }

#ifdef ENABLE_STATISTICS
  template <class P>
  auto
  Session<P>::get_stats_delta() -> SessionStatsDelta&
  {
    return stats_;
  }
#endif

  template <class P>
  void
  Session<P>::session_start()
  {
    get_protocol()->start();
    receive();
    lscontext_->unhold();
  }

  template <class P>
  inline void
  Session<P>::setup(LSContext& lscontext, tcp::socket&& socket)
  {
    lscontext.ref();
    lscontext_ = &lscontext;
    strand_ = lscontext_->borrow_strand();
    socket_.emplace(std::move(socket));
    close_once_flag_.reset();
  }

  template <class P>
  inline void
  Session<P>::transaction_started()
  {
#ifdef ENABLE_STATISTICS
    stats_.stats_transactions_cnt_delta_.fetch_add(1);
#endif
  }

  template <class P>
  inline void
  Session<P>::transaction_finished()
  { }

  template <class P>
  inline void
  Session<P>::reset_buffers()
  {
    expected_data_chunck_sz_set_ = false;
    expected_data_chunck_sz_ = 0;
    bytes_received_ = 0;
    bytes_sent_ = 0;
    ubuf_.clear();
  }

  template <class P>
  inline void
  Session<P>::receive()
  {
    async_receive();
  }

  template <class P>
  inline void
  Session<P>::send(DynQue::QueueBuffer* qb)
  {
    bool idle = outgoing_queue_.empty();
    outgoing_queue_.push(qb);

    if (idle) LS_LIKELY {
      async_send();
    }
  }

  template <class P>
  inline void
  Session<P>::consume(std::size_t length)
  {
    if (length == 0) {
      ubuf_.clear();
    } else {
      ubuf_.erase(ubuf_.begin(), ubuf_.begin() + length);
    }
  }

  template <class P>
  inline DynQue::QueueBuffer*
  Session<P>::prepare_send_buffer(std::size_t n)
  {
    return outgoing_queue_.prepare(n);
  }

  template <class P>
  inline void
  Session<P>::release_send_buffer(DynQue::QueueBuffer* qb)
  {
    return outgoing_queue_.free(qb);
  }

  template <class P>
  inline uint8_t*
  Session<P>::data()
  {
    return std::data(ubuf_);
  }

  template <class P>
  inline void
  Session<P>::set_expected_data_length(std::size_t len)
  {
    expected_data_chunck_sz_ = len;
    expected_data_chunck_sz_set_ = true;
  }

  template <class P>
  inline std::size_t
  Session<P>::get_bytes_received()
  {
    return bytes_received_;
  }

  template <class P>
  inline std::size_t
  Session<P>::data_size()
  {
    return std::size(ubuf_);
  }

  template <class P>
  inline bool
  Session<P>::check_finished()
  {
    return (expected_data_chunck_sz_set_ &&
            bytes_received_ >= expected_data_chunck_sz_);
  }

  template <class P>
  inline void
  Session<P>::async_receive()
  {
    std::size_t next_transfer_sz = 1;
    const std::size_t max_transfer_sz_ = 256 * 1024ul;

    /*
     * If the expected_data_chunck_sz_ is not set, we will have to set
     * the next min transfer size to 1 to avoid blocking, because we don't
     * know any better. Otherwise we can calculate a larger min transfer
     * size as follow.
     */
    if (expected_data_chunck_sz_set_) LS_LIKELY  {
      auto expected_remaining_data_sz =
          expected_data_chunck_sz_ - bytes_received_;

      /*
       * async_receive should not have been called if expected remaining
       * data size is zero.
       */
      if (expected_remaining_data_sz == 0) LS_UNLIKELY
        throw BadReceptionState{};

      next_transfer_sz = std::min(expected_remaining_data_sz, max_transfer_sz_);
    }

    auto dynbuf = asio::dynamic_buffer(ubuf_, max_transfer_sz_);
    auto condition = asio::transfer_at_least(next_transfer_sz);
    auto cb = std::bind(&Session::receive_event_cb, this, _1, _2);

    if (strand_) LS_UNLIKELY
      asio::async_read(*socket_, std::move(dynbuf), condition,
                       strand_->wrap(std::move(cb)));
    else
      asio::async_read(*socket_, std::move(dynbuf), condition, std::move(cb));

    /*
     * If lscontext_ was stopped before the above calls to async_read(), we
     * need to close the session manually.
     */
    if (lscontext_->stopped()) LS_UNLIKELY
      close_once();
  }

  template <class P>
  inline P*
  Session<P>::get_protocol()
  {
    return static_cast<P*>(this);
  }

  template <class P>
  inline void
  Session<P>::receive_event_cb(std::error_code error,
                               std::size_t bytes_transferred)
  {
    if (error) LS_UNLIKELY {
      report_error(error);
      async_close(error);
      return;
    }

    bytes_received_ += bytes_transferred;

    /*
     * Notify the CRTP derived protocol of new data and decide what
     * to do next (continue or close), based on the return value.
     */
    switch (get_protocol()->on_data()) {
    LS_LIKELY case kContinue:
      async_receive();
      break;
    case kClose:
      async_close(std::error_code{});
      break;
    case kFinished:
    case kData:
      break;
    }
#ifdef ENABLE_STATISTICS
    stats_.stats_bytes_received_delta_.fetch_add(bytes_transferred);
#endif
  }

  template <class P>
  inline void
  Session<P>::async_send()
  {
    auto qb = outgoing_queue_.front();
    if (strand_) LS_UNLIKELY {
      asio::async_write(
          *socket_, asio::buffer(qb->data(), qb->size()),
          strand_->wrap(std::bind(&Session::send_event_cb, this, _1, _2)));
    } else {
      asio::async_write(*socket_, asio::buffer(qb->data(), qb->size()),
                        std::bind(&Session::send_event_cb, this, _1, _2));
    }
  }

  template <class P>
  inline void
  Session<P>::send_event_cb(std::error_code error,
                            std::size_t bytes_transferred)
  {
    bytes_sent_ += bytes_transferred;
#ifdef ENABLE_STATISTICS
    stats_.stats_bytes_sent_delta_.fetch_add(bytes_transferred);
#endif

    if (error) LS_UNLIKELY {
      outgoing_queue_.clear();
      report_error(error);
      async_close(error);
      return;
    }

    outgoing_queue_.pop();
    if (!outgoing_queue_.empty())  LS_LIKELY{
      async_send();
    } else {
      /*
       * Notify the CRTP derived protocol.
       */
      switch (get_protocol()->on_sent()) {
      case kContinue:
        async_receive();
        break;
      case kClose:
        async_close(std::error_code{});
        break;
      case kData:
        break;
      case kFinished:
        __builtin_unreachable();
        break;
      }
      /*
       * If we have pending shutdown request(s), we queue another async close
       */
      if (prepare_for_shutdown_.load()) LS_UNLIKELY {
        prepare_for_shutdown_.store(false);
        async_close(std::error_code{});
      }
    }
  }

  template <class P>
  inline void
  Session<P>::close_once()
  {
    /*
     * If the outgoing queue is not empty, when we cannot close the
     * Sessio yet. We set 'prepare_for_shutdown_' and wait until
     * the outgoing queue is empty.
     */
    if (!outgoing_queue_.empty()) LS_LIKELY {
      prepare_for_shutdown_.store(true);
      return;
    }

    close_once_flag_.run_once(std::bind(&Session<P>::finalize, this));
  }

  template <class P>
  inline void
  Session<P>::async_close(std::error_code error)
  {
    if (error) LS_UNLIKELY 
      report_error(error);

    if (strand_) LS_UNLIKELY
      asio::post(*strand_, std::bind(&Session::close_once, this));
    else
      asio::post(lscontext_->get_io_context(),
                 std::bind(&Session::close_once, this));

    /*
     * If lscontext_ was stopped before the above calls to async_close(), we
     * need to close the session manually.
     * This is needed because async_close may have been called from outside
     * of a event handler.
     */
    if (lscontext_->get_io_context().stopped()) LS_UNLIKELY
      close_once();
  }

  template <class P>
  inline void
  Session<P>::report_error(std::error_code& error)
  {
    if (error.value() == 2)
      return;

    get_protocol()->on_error(error);
  }

  template <class P>
  inline void
  Session<P>::finalize()
  {
    try {
      /*
       * Let the destructor of asio::tcp::socket take care of shutting
       * down and closing the socket.
       */
      socket_ = std::nullopt;
    } catch (std::system_error&) {
      log_error("Exception occured while closing socket");
    }

    get_protocol()->on_closed();

    /*
     * Return strand_ to strand pool of lscontext_
     */
    if (strand_) LS_UNLIKELY
      lscontext_->put_strand(strand_);
    strand_ = nullptr;

    /*
     * If we have an LSContext, we are responsible for deref()ing it.
     */
    if (lscontext_) LS_LIKELY
      lscontext_->deref();

    /*
     * Notify the caller through the finalization callback, that the session
     * if finished.
     */
    finalized_(get_protocol());
  }
} // namespace lserver