cmake_minimum_required(VERSION 3.13 FATAL_ERROR)

project(lserver VERSION 0.0.1 DESCRIPTION "Lightning Server")

# Currently C++20 standard level is required for Concepts only,
# otherwise C++17 would be sufficient.
set(CMAKE_CXX_STANDARD 20)

# This will also disable RTTI for the control server module
# which is required by the gRPC library.
option(${PROJECT_NAME}_NO_RTTI "Disable RTTI" OFF)
option(${PROJECT_NAME}_USE_TBB "Use TBB scalable allocator" OFF)
option(${PROJECT_NAME}_LS_SANITIZE "Sanity checks on internal state" OFF)
option(${PROJECT_NAME}_USE_PMR_POOL "PMR pool resource for pool objects" ON)
option(${PROJECT_NAME}_STATISTICS "Statistics printing enable" ON)
option(${PROJECT_NAME}_DIAGNOSTICS "Debug printing enable" ON)

find_package(Protobuf REQUIRED)
find_package(Git)
find_program(GIT_EXE NAMES git)
include("${CMAKE_CURRENT_LIST_DIR}/third_party/nlohman_json.cmake")

add_compile_options(
    "$<$<CONFIG:Debug>:-O0;-g3;-gdwarf;-ggdb;-fsanitize=address>"
    "$<$<CONFIG:Release>:-O2;-g3>"
)

add_link_options(
  -Wl,--copy-dt-needed-entries
  "$<$<CONFIG:Debug>:-fsanitize=address>"
  "$<$<CONFIG:Release>:-Wl,--no-as-needed>"
)

set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)

if(GIT_FOUND AND NOT EXISTS "${CMAKE_SOURCE_DIR}/.git")
    message(WARNING "Configre Git!")
elseif(GIT_FOUND)
    execute_process(
            COMMAND ${GIT_EXECUTABLE} rev-list --max-count=1 HEAD
            OUTPUT_VARIABLE GIT_REVISION
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
            ERROR_QUIET
            )
    if(NOT ${GIT_REVISION} STREQUAL "")
            string(STRIP ${GIT_REVISION} GIT_REVISION)
    endif()
endif()

if (${PROJECT_NAME}_NO_RTTI)
  add_definitions(-DGOOGLE_PROTOBUF_NO_RTTI)
endif()

if (${PROJECT_NAME}_USE_TBB)
  if (${PROJECT_NAME}_NO_RTTI)
    message(FATAL_ERROR "Cannot enable TBB when NO-RTTI option is set!")
  endif()
  add_definitions(-DUSE_TBB)
endif()

if (${PROJECT_NAME}_DIAGNOSTICS)
  add_definitions(-DDIAGNOSTICS)
endif()

if (${PROJECT_NAME}_LS_SANITIZE)
  add_definitions(-DENABLE_LS_SANITIZE)
endif()

if (${PROJECT_NAME}_USE_PMR_POOL)
  add_definitions(-DUSE_PMR_POOL_RESOURCE)
endif()

if (${PROJECT_NAME}_STATISTICS)
  add_definitions(-DENABLE_STATISTICS)
endif()

include_directories(/usr/local/include include src ${json_SOURCE_DIR}/include/)
link_directories(/usr/local/lib)

set(HTTP_PARSER_SOURCES
  src/http_parser.cpp
)

add_library(http_parser ${HTTP_PARSER_SOURCES})

get_filename_component(proto "${CMAKE_SOURCE_DIR}/src/lscomm.proto" ABSOLUTE)
get_filename_component(proto_path "${proto}" PATH)

set(proto_srcs "${CMAKE_SOURCE_DIR}/src//lscomm.pb.cc")
set(proto_hdrs "${CMAKE_SOURCE_DIR}/src/lscomm.pb.h")
set(grpc_srcs "${CMAKE_SOURCE_DIR}/src/lscomm.grpc.pb.cc")
set(grpc_hdrs "${CMAKE_SOURCE_DIR}/src/lscomm.grpc.pb.h")

# Generate Protobuf/gRPC files
find_path(grpc_plugin_path NAMES grpc_cpp_plugin)
add_custom_command(
      OUTPUT "${proto_srcs}" "${proto_hdrs}" "${grpc_srcs}" "${grpc_hdrs}"
      COMMAND protoc
      ARGS --grpc_out "${proto_path}"
        --cpp_out "${proto_path}"
        -I "${proto_path}"
        --plugin=protoc-gen-grpc=${grpc_plugin_path}/grpc_cpp_plugin
        "${proto}"
      DEPENDS "${proto}")

set(${PROJECT_NAME}_CONTROL_SERVER_SOURCES
    src/control_server.cpp
    src/vm_instructions.cpp
    ${proto_srcs}
    ${grpc_srcs}
)
add_library(${PROJECT_NAME}_CONTROL_SERVER SHARED ${${PROJECT_NAME}_CONTROL_SERVER_SOURCES})
target_link_libraries(${PROJECT_NAME}_CONTROL_SERVER
    pthread
    grpc
    grpc++
    grpc++_reflection
    protobuf
)

set(${PROJECT_NAME}_SOURCES
    src/io_context_pool.cpp
    src/portal.cpp
    src/config.cpp
    src/manager.cpp
    src/vm_instructions.cpp
)
add_executable(${PROJECT_NAME} src/main.cpp ${${PROJECT_NAME}_SOURCES})
if (${PROJECT_NAME}_NO_RTTI AND ${PROJECT_NAME}_USE_TBB)
  target_compile_options(${PROJECT_NAME} PUBLIC "-fno-rtti")
endif()
target_link_libraries(${PROJECT_NAME}
    ${PROJECT_NAME}_CONTROL_SERVER
)
target_link_libraries(${PROJECT_NAME}
    pthread
    tbb
    tbbmalloc
    http_parser
    yaml-cpp
)

add_executable(dynamic_string_test
    tests/dynamic_string_test.cpp
    ${${PROJECT_NAME}_SOURCES}
)
add_executable(pool_test
    tests/pool_test.cpp
    ${${PROJECT_NAME}_SOURCES}
)
set (TEST_LINK_LIST
    gtest
    gtest_main
    gmock
    pthread
    tbb
    tbbmalloc
    http_parser
    yaml-cpp
    ${PROJECT_NAME}_CONTROL_SERVER
)
target_link_libraries(dynamic_string_test ${TEST_LINK_LIST})
target_link_libraries(pool_test ${TEST_LINK_LIST})
enable_testing()
add_test(DYNAMIC_STRING_TEST dynamic_string_test)
add_test(POOL_TEST pool_test)
include(CTest)
