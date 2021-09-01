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

#include "args_parser.hpp"
#include "common.hpp"
#include "config.hpp"
#include "http.hpp"
#include "ls_error.hpp"
#include "manager.hpp"
#include "portal.hpp"
#include "signal_manager.hpp"

using namespace lserver;

int
main(int argc, char* argv[])
try {
  args_sanity_check(argc, argv); // May throw InvalidArgs
  LSConfig config{argc, argv};   // May throw ConfigParseError

  /*
   * Startup sequence
   * 1- Create a server manager
   *    The server manager is responsible for create/destroy/control server
   *    instances.
   * 2- Add one or more Server instances s to the server manager.
   * 3- Optionally create a Portal which allows communication with/control of
   *    the servers.
   * 4- Create a signal manager which allows gracefull shutdown of the server.
   */

  ServerManager server_manager;
  server_manager.create_server<Http>(config);

  Portal portal{server_manager, config.header_interval_,
                config.control_listen_address_, config.control_listen_port_};
  portal.start();

  SignalManager sigman{[&]() {
    server_manager.stop();
    portal.stop();
  }};

  portal.wait();
  sigman.wait();

  return (0);

} catch (InvalidArgsError&) {
  lslog(0, "Invalid command line arguments.");
  usage(argc, argv);
  exit(EC_INVALID_COMMANDLINE_ARGS);

} catch (ConfigParseError&) {
  lslog(0, "Could not load config.");
  exit(EC_INVALID_CONFIG_FILE);
}
