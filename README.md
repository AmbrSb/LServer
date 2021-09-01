# Introduction
LServer is a **dynamically reconfigureable**, scalable, lightweight, multithreaded, network server, implemented in C++20, and is based on Asio. It also provides per-session dynamic server-side load simulation capability, based on a custom scripting language.
## Purpose
* LServer can be used as a **Template** for efficient concurrent network server implementation in C++. 
* **Teaching Tool**: LServer has a clean architecture and is well documented, making it suitable for teaching design and implementation of scalable concurrent software.
* **Load Simulation** LServer has a builtin dynamic scripting capability that can be used by the clients to simulate flexiable per-request server-side load simulation.
## Dependencies
You can use the provided Dockerfile to build and run LServer in a docker container.  The main build/run dependencies of LServer are as follows:
* GCC-10.3+
* Asio
* Protobuf
* gRPC
* Yaml-Cpp
* Google Test
* Google Mock
* TBB (optional)
# Building
## CMake options
* **NO_RTTI**: Disalbe RTTI in project compilation. This will also automatically disable RTTI in TBB. However, RTTI is required for gRPC server which will be compiled as a separate static library with RTTI.
* **USE_TBB**: Enable using TBB scalable allocator for containers used in various pool objects.
* **DIAGNOSTICS**: Enable printing various runtime diagnostic messages.
* **LS_SANITIZE**: enable various sanity checks in operation of some modules in the system. This is usefull for debugging.
* **USE_PMR_POOL**: Enable using pmr pool resources in pool containers.
* **STATISTICS**: Enable collection of runtime operational statistics which will be periodically printed to the console.
## Configure and Build
## With Docker
Optionally, you can use the provided Dockerfile to build and run lserver in a docker container. 
```Bash
cd lserver
docker -t lserver build .
docker run -it --network host lserver
```
Or you can perform the build directly as follows:
```Bash
git clone https://github.com/AmbrSb/LServer lserver
mkdir lserver/.build
cd lserver/.build
cmake -H.. -B. -DCMAKE_CXX_COMPILER=`which g++-10` -DCMAKE_BUILD_TYPE=Release
make all
ctest
```
Then you can run the server executable lserver with a config file path as the command line parameter. A sample config file `config.yaml` is provided in the project root directory.
```BASH
./lserver ../config.yaml
```
Once the server starts up, if the `STATISTICS` cmake option is set, the server prints a line containing overall server operational statistics once a second:
```Bash
LS control server listening on  127.0.0.1:5050 

               t  Accepted     Total  In flight     Trans           Received           Sent
1630311561928762        35        35         35      7298             541088         452290
1630311562930478       846       846        846    802364           61151984       49726852
1630311563931063       846       846        846    825922           62997950       51210016
1630311564931846       846       846        846    826535           63017530       51243744
1630311565932812       846       846        846    828868           63194380       51390808
1630311566933567       846       846        846    827996           63118010       51336000
1630311567934152       846       846        846    826889           63038682       51266932
1630311568934824       846       846        846    830318           63296566       51477980
1630311569935420       846       846        846    826023           63064128       51212930
1630311570936213       846       846        846    824202           62945432       51103438
1630311571936982       846       846        846    830626           63465294       51499122
1630311572937808       846       846        846    828953           63328478       51395830
1630311573938452       846       846        846    828074           63264678       51339906
1630311574938856       846       846        846    836059           63947338       51839874
1630311575939257       846       846        846    836724           64008300       51876888
1630311576940090       846       846        846    837245           64039954       51909190
1630311577940859       846       846        846    836535           63987594       51867650
1630311578941631       846       846        846    833888           63751048       51696964
1630311579942024       846       846        846    836623           63965806       51870998
1630311580942468       846       846        846    837001           63993978       51892264
1630311581942882       846       846        846    835915           63903082       51826358
1630311582943294       846       846        846    835206           63855674       51782896
1630311583943664       846       846        846    831822           63632696       51578606
^C
Shutting down signal manager 
Workers pool stopped 
Shutting down LS control service 
Destroying Session Pool 
All servers destroyed 
```
# Architecture Notes
* Proactor-style scalable I/O architecture implemented using Asio.
* Dynamic memory allocation is minimized: All major objects used in the system are managed by custom pools. Each pool is CERTP-derived from `Pool` template and defined customized allocation/deallocation operations for the specific managed type.
* Use of dynamic callbacks is avoided, by using a CRTP-based hierarchy design to enabled better function inlining.
## Dynamic Reconfiguration
In addition to the static configuration file, the user can change the concurrency/parallelism characteristics of individual servers dynamically at runtime, and the server gracefully converges to the given config.

The two attributes the user can dynamically modify are:
* **the number of I/O contexts** in a server
* **the number of threads in each I/O context**

LServer is specifically designed to have dynamically reconfigurable parallelism and concurrency attributes. That is you can add/reomve I/O contexts to/from a server while is it under load. Each context can have any number of threads, and LServer will automatically enforce appropriate synchrnoization based on the number of contexts and the number of threads in each.
# VScript
A VScript is a simple program that allows a client to simulate various types of workload on the server. This can be used for general research, and also testing and optimization of different aspects of the server implementation. It is also useful for studying concurrency-related behavior of the system with different parallelism attributes alongside different types of workload.
## VScript Syntax
The first line of a VScript contains a number representing the number of bytes in the VScript, excluding the first line itself.

A VScript, at the top level, consists of a JSON array of individual operations. Each operation consists of a `BYTE` number, an `INSTRUCTION`, and an `OPERAND`:
```JSON
N{LF}
[
{BYTE, {INSTRUCTION: OPERAND}},
...
]{LF}
[DATA]
```
The `BYTE` number, indicates at what point in the upload stream should the instruction be executed. A 0 `BYTE` indicates that the instruction should always be executed as soon as a transaction starts. If two operations have equal `BYTE` numbers, it is unspecified in what order they will be executed.

After the JSON array, there can optionally be any number of data bytes that will be fed to the VScript program.

## Instructions
Currently 5 types of instructions are supported:
* **LOCK** *res_id*: Exclusively lock the resource `res_id`. All other transactions that want to lock the same resource, should wait for the transaction that is currently holding it, to either `UNLOCK` it, or to finish.
* **UNLOCK** *res_id*: Unlock the resource `res_id`.
* **SLEEP** *nano_seconds*: Literaly sleep the thread that is currently executing this transaction. (Multiple threads might execute different instructions of the same transactions at different times). This might be useful for simulating blocking I/O operations on the server side.
* **LOOP** *cycles*: Busy loop the thread that is currently running the transaction. This is useful for simulating CPU-bound operations on the server side.
* **DOWNLOAD** *bytes*: Download `bytes` bytes of random data as the result of this transaction. There should be only one `DOWNLOAD` instruction in a VScript.

All resources acquired by a VScript are automatically released when it finishes executing, event if it does not explicitly `UNLOCK` them.

## Example
The following program locks resource `1` as soon as the transaction starts, and then sleeps for 1 second, and then unlocks resource `1`. The program would return 1 MB of data to the client.
```JSON
114
[
{"0": {"LOCK" : "1"}},
{"1": {"SLEEP" : "1000000"}},
{"2": {"UNLOCK" : "1"}},
{"3": {"DOWNLOAD" : "1048576"}}
]
ABCD
```
If we run 10 concurrent requests for this VScript on an LServer, we expect the 10 concurrent transactions to get serialized by the exclusive lock, and thus the overall execution should take nearly 10 seconds instead of 1 second:
```
>> ab -c 10 -n 10 -p ./slp1 http://127.0.0.1:15001/vscript/

This is ApacheBench, Version 2.3 <$Revision: 1843412 $>
Copyright 1996 Adam Twiss, Zeus Technology Ltd, http://www.zeustech.net/
Licensed to The Apache Software Foundation, http://www.apache.org/

Benchmarking 127.0.0.1 (be patient).....done


Server Software:        
Server Hostname:        127.0.0.1
Server Port:            15001

Document Path:          /vscript/
Document Length:        1048576 bytes

Concurrency Level:      10
Time taken for tests:   10.004 seconds
Complete requests:      10
Failed requests:        0
Keep-Alive requests:    10
Total transferred:      10486440 bytes
Total body sent:        2860
HTML transferred:       10485760 bytes
Requests per second:    1.00 [#/sec] (mean)
Time per request:       10004.278 [ms] (mean)
Time per request:       1000.428 [ms] (mean, across all concurrent requests)
Transfer rate:          1023.63 [Kbytes/sec] received
                        0.28 kb/s sent
                        1023.91 kb/s total

Connection Times (ms)
              min  mean[+/-sd] median   max
Connect:        0    1   0.3      1       1
Processing:  1001 5802 2616.6   5002    9003
Waiting:     1000 4601 2875.6   5001    9001
Total:       1001 5803 2616.7   5003    9004

Percentage of the requests served within a certain time (ms)
  50%   5003
  66%   8003
  75%   8003
  80%   9004
  90%   9004
  95%   9004
  98%   9004
  99%   9004
 100%   9004 (longest request)
```

## SinkHole
LServer supports a builtin VScript called SinkHole which is effectvely a `NOOP`. It just accepts all uploaded data, and returns a `200 OK` response with a zero-length response body. The SinkHole VScript is available at url `/sinkhole/`:
```
>> curl -v -d "some string"  http://127.0.0.1:15001/sinkhole/

*   Trying 127.0.0.1:15001...
* Connected to 127.0.0.1 (127.0.0.1) port 15001 (#0)
> POST /sinkhole/ HTTP/1.1
> Host: 127.0.0.1:15001
> User-Agent: curl/7.74.0
> Accept: */*
> Content-Length: 11
> Content-Type: application/x-www-form-urlencoded
> 
* upload completely sent off: 11 out of 11 bytes
* Mark bundle as not supporting multiuse
< HTTP/1.1 200 OK
< Content-Length: 0
< Connection: Close
< 
* Closing connection 0

```

# Configuration File
The configuration file is in YAML format. A sample is provided in the project root directory. The path to the configuration file should be supplied in the program command line:
```Bash
./lserver config.yaml`
```
* **listen**
  * **ip**: Server bind address
  * **port**: Server bind TCP port
  * **reuse_address**: Allow the socket to be bound to an address that is already in use. 
  * **separate_acceptor_thread**: Use a dedicated thread and context for the acceptor. If this is false one of the worker threads will be used by the acceptor.
* **control_server**
  * **ip**: Control server bind address
  * **port**: Control server bind TCP port
* **networking**
  * **socket_close_linger**: Whether the server sockets linger on close if unsent data is present.
  * **socket_close_linger_timeout**: Socket linger period
* **concurrency**
  * **num_workers**: Number of active LSContexts in the server
  * **max_num_workers**: Max number of LSContexts that the server can have. (LSContext can be added via the control server at runtime.)
  * **num_threads_per_worker**: Number of active threads in each LSContext at startup.
* **sessions**
  * **max_session_pool_size**: Maximum number of active sessions in each server. This effectively limits the maximum number of concurrent connections to the server.
  * **max_transfer_size**: Size of the incomming buffer supplied to Asio socket.
  * **eager_session_pool**: If true, the session pool will eagerly initializes maximum allowed number of session objects.
* **logging**
  * **header_interval**: The frequency of printing output header in the Portal console. This is meaningfull only if the `STATISTICS` option in the cmake file is set.

# Control Server / Embdded gRPC Server 
A gRPC server is embedded in LServer 
* Extract operational statistics of servers
* Dynamically change configuration of servers

Currently, the control server supports 4 commands:
* **Deactivate the specified LSContext** in the specified server. The LSContext will continue to service the ongoing session, but will not accept new sessions. For performance reasons, one an LSContext is deactivated, but kept in the server context pool and can be later reactivated with new confiuration parameters.
```Bash
grpc_cli call 127.0.0.1:5050 DeactivateContext "server_id:0;context_index:2;"
```
* **Add an LSContext** to one of the active servers. This command instructs the specified server to either create/add a new LSContext, or recycle and reactivate a previsouly deactivated LSContext. We can specify the number of threads in the new LSContext as a parameter as follow:
```Bash
grpc_cli call 127.0.0.1:5050 AddContext "server_id:0;num_threads:4;"
```
* **Extract operation statistics of LSContexts**: this returns an array of context statistics, 1 per LSContext:
```Bash
>> grpc_cli call 127.0.0.1:5050 GetContextsInfo ""

server_info {
  contexts_info {
    threads_cnt: 1
    active_sessions_cnt: 20
    active: true
  }
  contexts_info {
    threads_cnt: 4
    active_sessions_cnt: 17
    strand_pool_size: 62
    strand_pool_flight: 17
    active: true
  }
  contexts_info {
    threads_cnt: 4
    active_sessions_cnt: 16
    strand_pool_size: 62
    strand_pool_flight: 16
    active: true
  }
  contexts_info {
    threads_cnt: 4
    active_sessions_cnt: 17
    strand_pool_size: 62
    strand_pool_flight: 17
    active: true
  }
}
Rpc succeeded with OK status
```

* **Extract operational statistics of servers**
```Bash
>> grpc_cli call 127.0.0.1:5050 GetStats ""

stats_rec {
  time: 1630325063926825
  stats_accepted_cnt: 249
  num_items_total: 248
  num_items_in_flight: 248
  stats_transactions_cnt_delta: 269038
  stats_bytes_received_delta: 19909034
  stats_bytes_sent_delta: 16680294
}
Rpc succeeded with OK status
```
# Benchnarks
* Server hardware:
  * Dual CPU Xeon E5 2620
  * 24 Logical cores
  * Two 16 GB memory banks

`wrk` and `ab` have been used for the sake of benchmarking LServer. For Keep-Alive connection types:
```
>> ./wrk -t 8 -c 200 -d 100s --latency --header "connection: keep-alive" http://127.0.0.1:15001/sinkhole/
```
For making one TCP connection per HTTP request:
```
>> ./wrk -t 8 -c 200 -d 100s --latency --header "connection: close" http://127.0.0.1:15001/sinkhole/
```
## Variable Number of I/O Contexts
In this scenario we increase the number of I/O contexts from 1 to 24. Once with 1 thread per context and once with 2 threads with context. As expected with this simple workload (sinkhole), best performance is achieved with 1 thread per I/O context which minimizes contention between threads in each session.
### Connectoin Type: Keep-Alive
### Connectoin Type: Close
## A Single I/O Context - Variable Number of Threads
# Roadmap
* Loading application protocol from user-supplied DSO
* Scheduler plugins
* Make LSContext/Session pool allocations NUMA-aware
* Experiment with per-LSContext session pooling
* Add server performance regression tests
