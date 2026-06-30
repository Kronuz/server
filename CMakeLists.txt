# server: a generic, search-agnostic HTTP/TCP server engine extracted from
# Xapiand — the reactor (libev), the TCP socket layer, the accept-loop server
# base, and the per-connection client FSM. Prototype of "Leg 1": prove the
# substrate is generic by cutting the four host couplings (sig_exit, the manager
# client accounting, the Xapian length error, the CRTP instantiation list) into
# injection seams.
#
#   cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
#   ./build/http_hello   # a generic HTTP server, zero search code

cmake_minimum_required(VERSION 3.16)
project(server LANGUAGES C CXX)

include(FetchContent)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE Release)
endif()

# --- vendored libev (the event loop) ---
# Built standalone (EV_STANDALONE=1 -> libev self-configures, no config.h). Only
# ev.c is compiled; it #includes the backend files (ev_kqueue.c, ...) itself.
add_library(ev STATIC ev/ev.c)
target_compile_definitions(ev PRIVATE EV_STANDALONE=1)
target_include_directories(ev PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
# libev is C with a few warnings we don't own.
target_compile_options(ev PRIVATE -w)

# --- dependency: the bounded MPMC queue (the client write queue) ---
FetchContent_Declare(queue GIT_REPOSITORY https://github.com/Kronuz/queue.git GIT_TAG main)
FetchContent_GetProperties(queue)
if(NOT queue_POPULATED)
	FetchContent_Populate(queue)
endif()

# --- the server engine ---
# worker.cc + tcp.cc are the only out-of-line units; base_server/base_client/
# buffer are header-only (base_client.h includes base_client.tcc). Everything
# else (io, length, epoch, the trace/hooks/colors shims) is vendored header-only.
add_library(server STATIC worker.cc tcp.cc base_server.cc)
target_include_directories(server PUBLIC
	"${CMAKE_CURRENT_SOURCE_DIR}"
	"${queue_SOURCE_DIR}"        # queue.h
)
target_compile_features(server PUBLIC cxx_std_20)
target_link_libraries(server PUBLIC ev)
add_library(server::server ALIAS server)

# pthread (the worker loop / async watchers)
find_package(Threads)
if(Threads_FOUND)
	target_link_libraries(server PUBLIC Threads::Threads)
endif()

# --- demo (built only when this repo is top-level) ---
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
	add_executable(http_hello examples/http_hello.cc)
	target_link_libraries(http_hello PRIVATE server)
endif()
