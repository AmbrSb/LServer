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
#include <cassert>
#include <iomanip>
#include <tuple>
#include <variant>
#include <vector>

#include "timing.hpp"

namespace lserver {

  struct ContextInfo {
    std::size_t context_index_;
    std::size_t threads_cnt_;
    std::size_t active_sessions_cnt_;
    std::size_t strand_pool_size_;
    std::size_t strand_pool_flight_;
    bool active_;
  };

  struct ServerInfo {
    std::vector<ContextInfo> contexts_info_;
  };

  struct PoolStats {
    std::atomic<std::size_t> num_items_total_ = 0;
    std::atomic<std::size_t> num_items_in_flight_ = 0;

    void
    clear()
    {
      num_items_total_ = 0;
      num_items_in_flight_ = 0;
    }
  };

  struct ServerStats {
    /*
     * This value is field is updated by the acceptor of the corresponding
     * server, so there is not multithreaded access and race condition in
     * updating it. However if STATISTICS is enabled the portal will
     * periodically poll this value, so synchronization and atomic access
     * is required.
     */
    std::atomic<std::size_t> stats_accepted_cnt = 0;

    void
    clear()
    {
      stats_accepted_cnt.store(0);
    }
  };

  struct SessionPoolStats {
    /*
     * Allocation and borrowing of new Session instances is done a single
     * thread, but re-pooling them is done by multiple concurrent threads,
     * so synchronized and atoic access if rquired.
     */
    std::atomic<std::size_t> num_sessions_total_ = 0;
    std::atomic<std::size_t> num_sessions_in_flight_ = 0;

    void
    clear()
    {
      num_sessions_total_ = 0;
      num_sessions_in_flight_ = 0;
    }
  };

  /*
   * Internal session statistics with "delta" semantics. This enables the
   * session to resets its main statistics at will without falsifying the
   * collective statistics of the system.
   */
  struct SessionStatsDelta {
    /*
     * Synchronized and atomic access to these fields is required in case
     * an io_context has multiple threads running.
     */
    std::atomic<std::size_t> stats_transactions_cnt_delta_ = 0;
    std::atomic<std::size_t> stats_bytes_received_delta_ = 0;
    std::atomic<std::size_t> stats_bytes_sent_delta_ = 0;
  };

  struct SessionStats {
    /*
     * Synchronized and atomic access to these fields is required in case
     * an io_context has multiple threads running.
     */
    std::atomic<std::size_t> stats_transactions_cnt_delta_ = 0;
    std::atomic<std::size_t> stats_bytes_received_delta_ = 0;
    std::atomic<std::size_t> stats_bytes_sent_delta_ = 0;

    void
    clear()
    {
      stats_transactions_cnt_delta_ = 0;
      stats_bytes_received_delta_ = 0;
      stats_bytes_sent_delta_ = 0;
    }
  };

  /*
   * Represents a sample of the statisitcs of the a single server at some
   * point in time.
   */
  class LSStats {
    template <int N>
    friend auto const& get(LSStats const& stats);
    using FieldWidth_t = size_t;
    using FieldName_t = char const*;
    using FieldValue_t = std::variant<size_t, int64_t, double, lstime_t>;
    using Field_t = std::tuple<FieldWidth_t, FieldName_t, FieldValue_t>;
    constexpr static std::size_t kFieldWidth = 0;
    constexpr static std::size_t kFieldName = 1;
    constexpr static std::size_t kFieldValue = 2;
    using UnpackedRecord = std::vector<Field_t>;

  public:
    LSStats(ServerStats const& server_stats,
            PoolStats const& session_pool_stats,
            SessionStats const& session_stats);
    /*
     * Print out this sample as a single row of statistics. The header row
     * will printed out in the first call, and then on every 'header_interval'
     * calls to this function. Setting "header_interval" to zero disables
     * printing of the header row.
     */
    template <class STRM>
    void print_rec(STRM& stream, std::size_t header_interval) const;
    UnpackedRecord generate_rec() const;

  private:
    ServerStats const& server_stats_;
    PoolStats const& session_pool_stats_;
    SessionStats const& session_stats_;
    lstime_t const time_;

    bool row_number(std::size_t r) const;
    /*
     * The time point at which this sample was taken
     */
  };

  inline LSStats::LSStats(ServerStats const& server_stats,
                          PoolStats const& session_pool_stats,
                          SessionStats const& session_stats)
      : server_stats_{server_stats}
      , session_pool_stats_{session_pool_stats}
      , session_stats_{session_stats}
      , time_{now_micros()}
  { }

  inline auto
  LSStats::generate_rec() const -> UnpackedRecord
  {
    UnpackedRecord rec{
        {16, "t", time_},
        {10, "Accepted", server_stats_.stats_accepted_cnt},
        {10, "Total", session_pool_stats_.num_items_total_},
        {11, "In flight", session_pool_stats_.num_items_in_flight_},
        {10, "Trans", session_stats_.stats_transactions_cnt_delta_},
        {19, "Received", session_stats_.stats_bytes_received_delta_},
        {15, "Sent", session_stats_.stats_bytes_sent_delta_}};

    return rec;
  }

  template <class STRM>
  inline void
  LSStats::print_rec(STRM& stream, std::size_t header_interval) const
  {
    using std::setprecision;
    using std::setw;

    stream << std::fixed;
    stream << std::setprecision(3);

    auto rec = generate_rec();
    /*
     * Print header row every "header_interval" rows
     */
    if (header_interval && row_number(header_interval)) {
      stream << "\n";
      for (auto const& field: rec) {
        stream << setw(std::get<kFieldWidth>(field));
        stream << std::get<kFieldName>(field);
      }
      stream << "\n";
    }

    /*
     * Print value fields
     */
    for (auto const& field: rec) {
      stream << setw(std::get<kFieldWidth>(field));
      auto const& val = std::get<kFieldValue>(field);

      if (std::holds_alternative<std::size_t>(val))
        stream << std::get<std::size_t>(val);
      else if (std::holds_alternative<double>(val))
        stream << std::get<double>(val);
      else if (std::holds_alternative<int64_t>(val))
        stream << std::get<int64_t>(val);
      else if (std::holds_alternative<lstime_t>(val)) {
        auto t = std::get<lstime_t>(val);
        stream << timepoint_to_micros(t);
      }
    }

    stream << "\n";
  }

  inline bool
  LSStats::row_number(std::size_t r) const
  {
    assert(r > 0);
    static int row = -1;
    if (row++ == r - 1)
      row = 0;
    return row == 0;
  }

  /*
   * get<N>() implementation to support "structured binding" for LSStats
   */
  template <int N>
  inline auto const&
  get(LSStats const& stats)
  {
    if constexpr (N == 0)
      return stats.time_;
    else if constexpr (N == 1)
      return stats.server_stats_;
    else if constexpr (N == 2)
      return stats.session_pool_stats_;
    else if constexpr (N == 3)
      return stats.session_stats_;
  }
} // namespace lserver

namespace std {

  using ::lserver::LSStats;

  /*
   * std::tuple_size specializations to support "structured binding" for
   * LSStats
   */

  template <>
  struct tuple_size<LSStats> : std::integral_constant<std::size_t, 4> { };

  template <>
  struct tuple_element<0, LSStats> {
    using type = decltype(get<0>(std::declval<LSStats>()));
  };
  template <>
  struct tuple_element<1, LSStats> {
    using type = decltype(get<1>(std::declval<LSStats>()));
  };
  template <>
  struct tuple_element<2, LSStats> {
    using type = decltype(get<2>(std::declval<LSStats>()));
  };
  template <>
  struct tuple_element<3, LSStats> {
    using type = decltype(get<3>(std::declval<LSStats>()));
  };
}; // namespace std