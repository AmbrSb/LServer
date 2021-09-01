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

#include <map>
#include <string.h>

#include "dynamic_queue.hpp"
#include "http_parser.h"
#include "utils.hpp"

namespace lserver {

  using namespace std::literals;

  using http_parser = proxygen::http_parser;
  using http_parser_settings = proxygen::http_parser_settings;
  using DynQue = DynamicQueue<>;

  namespace http_parser_internal {

    constexpr static inline char const* stats = "HTTP/1.1 %d %s";
    constexpr static inline char const* cntln = "Content-Length: %d";
    constexpr static inline char const* ctype = "Content-Type: %s";
    constexpr static inline char const* cencd = "Content-Encoding: %s";
    constexpr static inline char const* conct = "Connection: %s";
    constexpr static inline char const* close = "Close";
    constexpr static inline char const* kpalv = "Keep-Alive";
    constexpr static inline char const* newln = "\r\n";
    constexpr static inline char const* hdrfn = "\r\n\r\n";
    static inline std::size_t newln_sz = std::string{newln}.size();
    static inline std::size_t hdrfn_sz = std::string{hdrfn}.size();

    /*
     * These are free proxy functions used as intermediate callbacks passed
     * to the http_parser library. They proxy the calls to member functions
     * of the HttpRequestHeader object that is the 'data' field of the
     * http_parser object.
     */
    inline int message_begin_cb(http_parser* p);
    inline int header_field_cb(http_parser* p, char const* buf, size_t len);
    inline int header_value_cb(http_parser* p, char const* buf, size_t len);
    inline int body_cb(http_parser* p, const char* buf, size_t len);
    inline int request_url_cb(http_parser* p, const char* buf, size_t len);
    inline int headers_complete_cb(http_parser* p, const char* buf, size_t len);
    inline int message_complete_cb(http_parser* p);
    inline int response_reason_cb(http_parser* p, const char* buf, size_t len);
    inline int chunk_header_cb(http_parser* p);
    inline int chunk_complete_cb(http_parser* p);

  } // namespace http_parser_internal

  namespace hpi = http_parser_internal;

  class HttpRequestHeader {

  public:
    enum class HeaderState { kNone, kConnection };

    /*
     * Tries to find the full HTTP header in buffer 'data', and if
     * successfull, it tries to parse the HTTP header lines.
     *
     * @retruns true of it finds and parses the header, false otherwise.
     */
    std::optional<std::size_t> try_parse(char const* data, std::size_t len);
    /*
     * Resets the internal state and fields of the request header
     * and the parser, so that it can be used on a new HTTP transaction.
     */
    void reset();
    /*
     * Callbacks proxied back from the http_parser.
     * Called once for each header line.
     */
    void set_field(char const* buf, std::size_t len);
    void set_value(char const* buf, std::size_t len);
    void set_url(char const* buf, std::size_t len);
    /*
     * Returns HTTP content length if set in the header, and returns
     * zero otherwise.
     */
    std::size_t get_content_length();
    /*
     * Has the HTTP client sent keep-alive request?
     */
    bool get_keep_alive();
    /*
     * Retruns true only if the full HTTP header is seen and parsed.
     */
    bool is_ready();
    std::string const& get_url() const;

  private:
    std::optional<std::size_t>
    find_request_header_end_offset(char const* data_raw, std::size_t len);

    static inline std::map<std::string_view, HeaderState, nocase_compare>
        header_names{{"connection"sv, HeaderState::kConnection}};

    bool keep_alive_ = false;
    bool ready_ = false;
    HeaderState header_state_ = HeaderState::kNone;
    http_parser parser_;
    std::string url_;
    http_parser_settings settings = {
        .on_message_begin = hpi::message_begin_cb,
        .on_url = hpi::request_url_cb,
        .on_header_field = hpi::header_field_cb,
        .on_header_value = hpi::header_value_cb,
        .on_headers_complete = hpi::headers_complete_cb,
        .on_body = hpi::body_cb,
        .on_message_complete = hpi::message_complete_cb,
        .on_reason = hpi::response_reason_cb,
        .on_chunk_header = hpi::chunk_header_cb,
        .on_chunk_complete = hpi::chunk_complete_cb};
  };

  std::optional<std::size_t>
  HttpRequestHeader::try_parse(char const* data, std::size_t len)
  {
    auto header_end = find_request_header_end_offset(data, len);
    if (header_end)
      LS_LIKELY
      {
        proxygen::http_parser_execute(&parser_, &settings, data, *header_end);
        ready_ = true;
      }

    return header_end;
  }

  void
  HttpRequestHeader::reset()
  {
    auto empty = HttpRequestHeader{};
    std::swap(*this, empty);
    proxygen::http_parser_init(&parser_, proxygen::HTTP_REQUEST);
    parser_.data = this;
  }

  void
  HttpRequestHeader::set_field(char const* buf, std::size_t len)
  {
    auto it = header_names.find(std::string_view{buf, len});
    if (it != header_names.end())
      header_state_ = it->second;
    else
      header_state_ = HeaderState::kNone;
  }

  void
  HttpRequestHeader::set_value(char const* buf, std::size_t len)
  {
    switch (header_state_) {
      LS_LIKELY case HeaderState::kNone : return;
    case HeaderState::kConnection:
      if (0 == strncasecmp(buf, "close", len)) {
        keep_alive_ = false;
      } else if (0 == strncasecmp(buf, "keep-alive", len)) {
        keep_alive_ = true;
      }
      break;
    }
    header_state_ = HeaderState::kNone;
  }

  void
  HttpRequestHeader::set_url(char const* buf, std::size_t len)
  {
    url_ = std::string{buf, len};
  }

  inline std::optional<std::size_t>
  HttpRequestHeader::find_request_header_end_offset(char const* data_raw,
                                                    std::size_t len)
  {
    assert(!ready_);
    auto header_end = memmem(data_raw, len, hpi::hdrfn, hpi::hdrfn_sz);

    if (header_end != nullptr)
      return std::optional{reinterpret_cast<uintptr_t>(header_end) -
                           reinterpret_cast<uintptr_t>(data_raw) +
                           hpi::hdrfn_sz};
    return std::nullopt;
  }

  inline std::size_t
  HttpRequestHeader::get_content_length()
  {
    assert(ready_);
    return std::max(parser_.content_length, (int64_t)0);
  }

  inline bool
  HttpRequestHeader::get_keep_alive()
  {
    assert(ready_);
    return keep_alive_;
  }

  inline bool
  HttpRequestHeader::is_ready()
  {
    return ready_;
  }

  inline std::string const&
  HttpRequestHeader::get_url() const
  {
    return url_;
  }

  namespace http_parser_internal {

    inline int
    message_begin_cb(http_parser* p)
    {
      return 0;
    }

    inline int
    header_field_cb(http_parser* p, char const* buf, size_t len)
    {
      auto http_header = static_cast<HttpRequestHeader*>(p->data);
      http_header->set_field(buf, len);
      return 0;
    }

    inline int
    header_value_cb(http_parser* p, char const* buf, size_t len)
    {
      auto http_header = static_cast<HttpRequestHeader*>(p->data);
      http_header->set_value(buf, len);
      return 0;
    }

    inline int
    body_cb(http_parser* p, const char* buf, size_t len)
    {
      return 0;
    }

    inline int
    request_url_cb(http_parser* p, const char* buf, size_t len)
    {
      if (len > 0) {
        auto http_header = static_cast<HttpRequestHeader*>(p->data);
        http_header->set_url(buf, len);
      }
      return 0;
    }

    inline int
    headers_complete_cb(http_parser* p, const char* buf, size_t len)
    {
      return 0;
    }

    inline int
    message_complete_cb(http_parser* p)
    {
      return 0;
    }

    inline int
    response_reason_cb(http_parser* p, const char* buf, size_t len)
    {
      return 0;
    }

    inline int
    chunk_header_cb(http_parser* p)
    {
      return 0;
    }

    inline int
    chunk_complete_cb(http_parser* p)
    {
      return 0;
    }

  }; // namespace http_parser_internal


  //////////////////////////////////////////////////////////////////////


  class HttpResponseHeader {

  public:
    HttpResponseHeader(DynQue::QueueBuffer* buffer);
    /*
     * Generate an HTTP response header using the provided values.
     */
    void prepare(int code, std::size_t length, bool keep_alive);
    DynQue::QueueBuffer* get_buffer();
    void reset();
    void set_sent();
    bool is_sent();

    std::size_t content_length_;
    /* HTTP response code */
    int code_;
    bool keep_alive_;

  private:
    void line_break();
    void status_line();
    void content_length_line();
    void connection_line();
    void generate_header();
    template <class... Args>
    void append(Args... args);

    static inline std::unordered_map<int, char const*> status_code_reason_{
        {100, "Continue"},
        {101, "Switching Protocols"},
        {200, "OK"},
        {201, "Created"},
        {202, "Accepted"},
        {203, "Non-Authoritative Information"},
        {204, "No Content"},
        {205, "Reset Content"},
        {206, "Partial Content"},
        {300, "Multiple Choices"},
        {301, "Moved Permanently"},
        {302, "Found"},
        {303, "See Other"},
        {304, "Not Modified"},
        {305, "Use Proxy"},
        {307, "Temporary Redirect"},
        {400, "Bad Request"},
        {401, "Unauthorized"},
        {402, "Payment Required"},
        {403, "Forbidden"},
        {404, "Not Found"},
        {405, "Method Not Allowed"},
        {406, "Not Acceptable"},
        {407, "Proxy Authentication Required"},
        {408, "Request Time-out"},
        {409, "Conflict"},
        {410, "Gone"},
        {411, "Length Required"},
        {412, "Precondition Failed"},
        {413, "Request Entity Too Large"},
        {414, "Request-URI Too Large"},
        {415, "Unsupported Media Type"},
        {416, "Requested range not satisfiable"},
        {417, "Expectation Failed"},
        {500, "Internal Server Error"},
        {501, "Not Implemented"},
        {502, "Bad Gateway"},
        {503, "Service Unavailable"},
        {504, "Gateway Time-out"},
        {505, "HTTP Version not supported"}};

    DynQue::QueueBuffer* const buffer_;
    bool sent_ = false;
  };

  HttpResponseHeader::HttpResponseHeader(DynQue::QueueBuffer* buffer)
      : buffer_{buffer}
  { }

  inline void
  HttpResponseHeader::prepare(int code, std::size_t length, bool keep_alive)
  {
    code_ = code;
    content_length_ = length;
    keep_alive_ = keep_alive;

    generate_header();
  }

  inline void
  HttpResponseHeader::generate_header()
  {
    buffer_->clear();

    status_line();
    line_break();

    content_length_line();
    line_break();

    connection_line();
    line_break();
    line_break();
  }

  template <class... Args>
  inline void
  HttpResponseHeader::append(Args... args)
  {
    buffer_->printf(args...);
  }

  inline void
  HttpResponseHeader::line_break()
  {
    append(hpi::newln);
  }

  inline void
  HttpResponseHeader::status_line()
  {
    auto reason = status_code_reason_.find(code_);
    auto reason_str =
        (reason == status_code_reason_.end()) ? "" : reason->second;
    append(hpi::stats, code_, reason_str);
  }

  inline void
  HttpResponseHeader::content_length_line()
  {
    append(hpi::cntln, content_length_);
  }

  inline void
  HttpResponseHeader::connection_line()
  {
    append(hpi::conct, keep_alive_ ? hpi::kpalv : hpi::close);
  }

  inline DynQue::QueueBuffer*
  HttpResponseHeader::get_buffer()
  {
    return buffer_;
  }

  void
  HttpResponseHeader::set_sent()
  {
    sent_ = true;
  }

  bool
  HttpResponseHeader::is_sent()
  {
    return sent_;
  }

  void
  HttpResponseHeader::reset()
  {
    sent_ = false;
  }

  inline bool
  url_prefix(std::string const& pref, std::string const& url)
  {
    auto iter = std::mismatch(pref.begin(), pref.end(), url.begin()).first;
    return (iter == pref.end());
  }

} // namespace lserver
