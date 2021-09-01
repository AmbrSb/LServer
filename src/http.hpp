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

#include <asio.hpp>

#include "common.hpp"
#include "http_header.hpp"
#include "program.hpp"
#include "session.hpp"

namespace lserver {

  using DynQue = DynamicQueue<>;

  class Http final : public Session<Http> {
    using BaseSession = Session<Http>;

  public:
    Http();
    ~Http() = default;
    /*
     * Primes the state of the protocol instance and starts
     * the main session loop.
     */
    void start();
    /*
     * operator new is overriden to observe the alignment
     * requirement of Http/Session
     */
    void* operator new(std::size_t n);
    void operator delete(void* ptr, std::size_t n);

    /*
     * "callbacks" called by the CRTP base (Session)
     */
    void on_error(std::error_code error);
    void on_closed();
    auto on_sent();
    auto on_data();
    bool try_handle_header();
    uintptr_t get_id();

  private:
    /*
     * Send back the HTTP response headers [+ body]
     * @param headers Additional HTTP headers
     */
    void respond(int code, bool keep_alive, std::size_t size,
                 std::initializer_list<std::string> headers);
    char const* get_config_name();
    /*
     * Resets the internal state of the protocol and
     * prepares it to parse a new HTTP session anew.
     */
    void reset();

    /*
     * We keep around a pair of request/reponse headers to
     * avoid dynamic allocation for each session.
     */
    static inline LSVirtualMachine vm_;
    HttpRequestHeader request_header_;
    HttpResponseHeader response_header_;
    char const* config_name_ = "http";
    static inline std::string const vscript_url = "/vscript/";
    static inline std::string const sinkhole_url = "/sinkhole/";
    Program program_;
    DynamicString* d_;
  };

  Http::Http()
      : response_header_{BaseSession::prepare_send_buffer(64)}
      , d_{BaseSession::prepare_send_buffer(256 * 1024)}
  { }

  char const*
  Http::get_config_name()
  {
    return config_name_;
  }

  void
  Http::start()
  {
    reset();
  }

  void*
  Http::operator new(std::size_t n)
  {
    void* ptr = std::aligned_alloc(
        std::max(alignof(BaseSession), alignof(Http)), n * sizeof(BaseSession));

    if (!ptr)
      LS_UNLIKELY
      {
        if (auto new_hndl = std::get_new_handler())
          new_hndl();
        else
          throw std::bad_alloc{};
      }

    return ptr;
  }

  void
  Http::operator delete(void* ptr, std::size_t n)
  {
    free(ptr);
  }

  inline void
  Http::on_error(std::error_code error)
  {
    lslog(
        3, "Http service: ",
        std::error_condition{error.value(), std::system_category()}.message());
  }

  inline uintptr_t
  Http::get_id()
  {
    return reinterpret_cast<uintptr_t>(this);
  }

  inline void
  Http::on_closed()
  {
    program_.reset();
  }

  inline auto
  Http::on_sent()
  {
    if (program_.has_more_data())
      LS_LIKELY
      {
        program_.get_data(d_);
        BaseSession::send(d_);
        return BaseSession::kData;
      }

    /*
     * Output stream is finished and we are not going to send more data.
     */

    transaction_finished();
    auto keep_alive = request_header_.get_keep_alive();
    if (keep_alive)
      LS_LIKELY
      {
        this->reset();
        /*
         * Continue to read the HTTP headers for the next chunk
         */
        return BaseSession::kContinue;
      }
    else {
      return BaseSession::kClose;
    }
  }

  inline bool
  Http::try_handle_header()
  {
    auto header_end_offset = request_header_.try_parse(
        reinterpret_cast<char const*>(BaseSession::data()),
        BaseSession::data_size());

    if (header_end_offset)
      LS_LIKELY
      {
        BaseSession::set_expected_data_length(
            request_header_.get_content_length());

        /*
         * Consume header size bytes from the input stream, so that
         * the stream head points to the first byte of body (if any).
         */
        BaseSession::consume(*header_end_offset);
        return true;
      }

    return false;
  }

  inline auto
  Http::on_data()
  {
#if ENABLE_LS_SANITIZE
    assert(engaged_);
#endif

    if (!request_header_.is_ready())
      LS_UNLIKELY
      {
        BaseSession::transaction_started();
        if (!try_handle_header()) {
          /*
           * Header is not ready yet...
           */
          return BaseSession::kContinue;
        }
      }

    if (!program_)
      LS_UNLIKELY
      {
        // Decide on the type of program based on the request URL
        auto url = request_header_.get_url();
        if (url_prefix(vscript_url, url)) {
          /*
           * Minimum Program length is 2 bytes (i.e "0<LF>")
           */
          if (request_header_.get_content_length() < 2)
            return BaseSession::kClose;

          std::size_t consume_len;
          auto status =
              Program::try_parse(program_, consume_len, BaseSession::data(),
                                 BaseSession::data_size());

          switch (status) {
          case SUCCESS:
            BaseSession::consume(consume_len);
            break;

          case NEED_MORE_DATA:
            return BaseSession::kContinue;
            break;

          case FAILED:
            return BaseSession::kClose;
            break;

          default:
            __builtin_unreachable();
          }

        } else if (url_prefix(sinkhole_url, url)) {
          /*
           * The sinkhole program just accepts all uploaded data and returns a
           * minimal "200 OK" response of length zero.
           */
          program_ = Program::sinkhole();

        } else {
          // TODO replace this with a specialized Error program
          return BaseSession::kClose;
        }
      }

    /*
     * A program requires a VM.
     * Set this program to run on the shared static VM of Http service class
     */
    program_.set_vm(&vm_);

    /*
     * Start feeding the data stream into the program.
     */
    auto finished = program_.feed(BaseSession::data(), BaseSession::data_size(),
                                  BaseSession::check_finished());
    BaseSession::consume();

    /*
     * Currently we assume that Program should signal finish if and only if
     * session data chunk has finished.
     */
    assert(BaseSession::check_finished() == finished);

    if (finished)
      LS_UNLIKELY
      {
        /*
         * All data is fed into the program, now we check the response from the
         * program.
         */
        auto prog_resp = program_.get_response();
        respond(prog_resp.code, request_header_.get_keep_alive(),
                prog_resp.download_size, {});
        /*
         * Inform the session that the input stream is finished and we don't
         * expect more data. (Output stream may still be active)
         */
        return BaseSession::kFinished;
      }

    /*
     * Current data chunck is fully received yet ... continue
     */
    return BaseSession::kContinue;
  }

  void
  Http::respond(int code, bool keep_alive, std::size_t size,
                std::initializer_list<std::string> headers)
  {
    assert(!response_header_.is_sent());

    response_header_.prepare(code, size, keep_alive);
    BaseSession::send(response_header_.get_buffer());
    response_header_.set_sent();
  }

  void
  Http::reset()
  {
    // TODO clean it up and reuse rather than delete/new
    program_.reset();
    request_header_.reset();
    response_header_.reset();
    BaseSession::reset_buffers();
  }
} // namespace lserver
