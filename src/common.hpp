/*
 * BSD 2-Clause License
 * 
 * Copyright (c) 2021, Amin Saba
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <exception>

#define CACHE_LINE_SIZE (64)
#define ALIGN_DESTRUCTIVE alignas(CACHE_LINE_SIZE)

#if ENABLE_LS_SANITIZE
#define LS_SANITIZE bool engaged_ = true;
#else
#define LS_SANITIZE
#endif

#include <filesystem>
#include <iostream>

#define log_error(...) log_(__FILE__, __func__, __LINE__, 0, __VA_ARGS__);

#ifdef DIAGNOSTICS
#define lslog(...) log_(__FILE__, __func__, __LINE__, __VA_ARGS__);
#else
#define lslog(...)
#endif

#define lslog_note(...) log_note_(__VA_ARGS__);

namespace lserver {

  inline int log_level = 1;

  template <class... Args>
  void
  log_note_(int level, Args const&... args)
  {
    if (log_level >= level) {
      ((std::cerr << args << " "), ...) << "\n";
    }
  }

  template <class... Args>
  void
  log_(char const* file_name, char const* func_name, int lineno, int level,
       Args const&... args)
  {
    auto fpath = std::filesystem::path{file_name}.filename();
    std::string fname = fpath;
    fname = fname.substr(0, fname.size() - 2);

    if (log_level >= level) {
      std::cerr << fname << " [" << func_name << ":" << lineno << "]: ";
      ((std::cerr << args << " "), ...) << "\n";
    }
  }

  extern bool is_debugger_attached();
  extern inline void debugger_break();

  class InvalidArgs : public std::exception { };

} // namespace lserver