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

#include <cstdint>
#include <list>
#include <mutex>
#include <string>

#include <nlohmann/json.hpp>

#include "dynamic_queue.hpp"
#include "lsvm.hpp"
#include "utils.hpp"
#include "vm_instructions_base.hpp"
#include "vm_instructions.hpp"
#include "vm_instructions_list.hpp"

namespace lserver {

  enum ProgramParseStatus { SUCCESS, NEED_MORE_DATA, FAILED };

  /*
   * Represents an instance of a VScript, including a priority queue of
   * instructions of the vscript to be executed.
   */
  class Program {
    class BadProgram : public std::exception { };

    /*
     * Represent a summary of the execution result of a VScript.
     */
    struct ProgResponse {
      /*
       * The result code returned by the VScript.
       */
      int code;
      /*
       * The length of data that this VScript has generated.
       */
      std::size_t download_size;
    };

  public:
    Program() = default;
    ~Program();
    Program(std::string_view json_str);
    Program& operator=(Program&& other);
    /*
     * Try to parse a program from the data stream.
     *
     * The second tuple element indicates the number of exec_points
     * consumed by the parser
     *
     * The third tuple element is true if a permanent failure
     * occures, otherwise the client should retry when more
     * data is available in the stream.
     */
    static ProgramParseStatus try_parse(Program& program,
                                         std::size_t& consume_len,
                                         uint8_t* data, std::size_t len);
    /*
     * Instantiate a simple program that just sinkhols all incomming
     * traffic and returns nothing.
     */
    static Program sinkhole();
    /*
     * Feed len exec_points of from the stream data to the program.
     * @param eof is set to true to indicate that the data stream
     * has finished.
     */
    bool feed(uint8_t* data, std::size_t len, bool eof);

    /*
     * Set the result code and download size of the program.
     * These methods are usually used by 'Op' derivatives.
     */
    void set_result_code(int sz);
    void set_downloaded_size(std::size_t sz);

    void set_vm(LSVirtualMachine* vm);
    /*
     * Returns information about the overall execution result of the
     * VScript.
     */
    ProgResponse get_response() const;
    /*
     * Returns true if the output stream of the VScript has outstanding
     * data to be sent.
     */
    bool has_more_data();
    /*
     * Fill 'd' with some data from the output stream of the VScript
     */
    void get_data(DynamicString* d);
    void stop();
    std::atomic_bool const& cancellation_request_ref() const;
    /*
     * Indicates whether the program is empty or not.
     * False == empty program
     */
    operator bool();
    void reset();

  private:
    /*
     * Returns a unique identifier by which this program can be
     * distinguished from other programs currently running on the
     * same VM.
     * This can be used by the VM for resource management.
     */
    uintptr_t session_id();

    static constexpr inline std::size_t kSendBufferSz = 64 * 1024;
    static inline std::string const kUrlHead_ = "/program/";
    static inline std::string const PHeaderEndMarker = "\n";
    /*
     * The amount of data left that this program wants to send
     * to the client.
     */
    std::atomic<std::size_t> download_size_ = 0;
    /*
     * This code indicates the overall result of the execution of
     * the VScript.
     */
    int result_code_ = 200;
    /*
     * Set to true, when the excution of the VScript is finished
     */
    bool finished_ = false;
    /*
     * Instructions to be executed sorted in ascending order of
     * execution trigger points. (See comment on OpCompare)
     */
    std::priority_queue<OpPtr, std::vector<OpPtr>, OpCompare> instructions_;
    std::size_t bytes_processed_cnt_ = 0;
    /*
     * The VM on which the instructions of this program should be
     * executed. This is generally provided by the Session object.
     */
    LSVirtualMachine* vm_ = nullptr;
    std::atomic_bool cancellation_request_ = false;
  };

  inline Program::Program(std::string_view json_str)
  {
    try {
      auto prog_text = nlohmann::json::parse(json_str);
      for (auto const& inst: prog_text) {

        std::map<std::string, nlohmann::json> prog_line = inst;
        /*
         * Format of a program line:
         * {"EXEC_POINT": {"INSTRUCTION" : "OPERAND"}}
         */
        for (auto const& kv: prog_line) {
          auto exec_point = std::stoul(kv.first);
          auto inst_json = kv.second;
          std::map<std::string, std::string> inst = inst_json;
          auto opcode = inst.begin()->first;
          auto operand = std::stoul(inst.begin()->second);

          instructions_.emplace(
              LSVMOps::instantiate(opcode, exec_point, operand));
        }
      }

    } catch (std::exception) {
      throw BadProgram{};
    }
  }

  inline Program::~Program()
  {
    reset();
  }

  inline Program&
  Program::operator=(Program&& other)
  {
    download_size_ = 0;
    result_code_ = 200;
    finished_ = false;
    instructions_ = std::move(other.instructions_);
    bytes_processed_cnt_ = 0;
    vm_ = nullptr;
    cancellation_request_ = false;
    return *this;
  }

  inline void
  Program::set_vm(LSVirtualMachine* vm)
  {
    vm_ = vm;
  }

  inline void
  Program::reset()
  {
    if (vm_)
      vm_->cleanup(session_id());

    decltype(instructions_) empty;
    swap(instructions_, empty);

    vm_ = nullptr;
  }

  inline ProgramParseStatus
  Program::try_parse(Program& program, std::size_t& consume_len, uint8_t* data,
                     std::size_t len)
  {
    std::string pheader;
    std::size_t prog_len;
    char const* prog_start;

    char const* const s = reinterpret_cast<const char*>(data);
    /*
     * First try to find the first line containing a single number indicating
     * the size of the program code.
     */
    auto instream = std::string_view{s, len};
    auto pheader_end = instream.find(PHeaderEndMarker);
    if (pheader_end == std::string_view::npos) {
      /*
       * We need to see more data, retry later.
       */
      goto retry_later;
    }

    /*
     * Parse program length from the first line.
     */
    pheader = std::string{instream.substr(0, pheader_end)};
    try {
      prog_len = std::stoul(pheader);
    } catch (std::exception) {
      goto failed;
    }

    prog_start = s + pheader_end + 1;

    /*
     * Now we have the size of the program in prog_len, continue to parse
     * the program.
     */

    if (prog_len > len) {
      /*
       * We need to see more data, retry later.
       */
      goto retry_later;
    }

    if (prog_len == 0) {
      lslog(0, "Invalid program size: 0");
      goto failed;
    }

    /*
     * Eventually we are ready to try to parse the program json code
     */
    try {

      auto prog_json_str = std::string_view{prog_start, prog_len};
      program = Program{prog_json_str};
      consume_len = pheader_end + 1 + prog_len; /* size of pheader + program*/
      return SUCCESS;

    } catch (BadProgram& ex) {
      lslog(0, "Invalid program text");
      goto failed;
    }

    __builtin_unreachable();

  retry_later:
    consume_len = 0;
    return NEED_MORE_DATA;

  failed:
    consume_len = 0;
    return FAILED;
  }

  inline Program
  Program::sinkhole()
  {
    return Program{};
  }

  inline uintptr_t
  Program::session_id()
  {
    return reinterpret_cast<uintptr_t>(this);
  }

  inline void
  Program::set_result_code(int code)
  {
    result_code_ = code;
  }

  inline void
  Program::set_downloaded_size(std::size_t sz)
  {
    download_size_.store(sz);
  }

  inline bool
  Program::has_more_data()
  {
    assert(finished_);

    return (download_size_.load() > 0);
  }

  inline void
  Program::get_data(DynamicString* d)
  {
    std::size_t bytes_cnt_to_send = kSendBufferSz;

    assert(download_size_.load() > 0);

    if (kSendBufferSz > download_size_.load())
      bytes_cnt_to_send = download_size_.load();

    d->fill(bytes_cnt_to_send);
    download_size_.fetch_sub(bytes_cnt_to_send);
  }

  inline void
  Program::stop()
  {
    cancellation_request_.store(true);
  }

  inline std::atomic_bool const&
  Program::cancellation_request_ref() const
  {
    return cancellation_request_;
  }

  inline Program::operator bool() { return (vm_ != nullptr); }

  inline bool
  Program::feed(uint8_t* data, std::size_t len, bool eof)
  {
    bytes_processed_cnt_ += len;

    while (!cancellation_request_ && instructions_.size() > 0) {
      auto& next_instr = instructions_.top();
      if (bytes_processed_cnt_ >= next_instr->get_exec_point()) {
        next_instr->run(*this, session_id(), *vm_);
        instructions_.pop();
      }
    }

    return (finished_ = eof);
  }

  inline auto
  Program::get_response() const -> ProgResponse
  {
    ProgResponse resp{result_code_, download_size_.load()};
    return resp;
  }

} // namespace lserver