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

# Headers that would collide with a host's richer versions (io.hh, length.h,
# colors.h, strings.hh, epoch.hh, readable_revents.hh, likely.h, ev/) live under
# compat/. Standalone, compat/ is on the include path and supplies them; a host
# (Xapiand) does NOT add compat/, so its own versions win, and only the engine
# headers in the root are taken from this library.
set(SERVER_COMPAT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/compat")

# --- vendored libev (the event loop) ---
# Built standalone (EV_STANDALONE=1 -> libev self-configures, no config.h). Only
# ev.c is compiled; it #includes the backend files (ev_kqueue.c, ...) itself.
add_library(ev STATIC compat/ev/ev.c)
target_compile_definitions(ev PRIVATE EV_STANDALONE=1)
target_include_directories(ev PUBLIC "${SERVER_COMPAT_DIR}")
# libev is C with a few warnings we don't own.
target_compile_options(ev PRIVATE -w)

# --- dependencies (the Kronuz family) ---
#   queue       -> queue.h           (the client write queue)
#   compressors -> compressor_lz4.h  (the FILE_FOLLOWS LZ4 codec, client_compressor.h)
FetchContent_Declare(queue       GIT_REPOSITORY https://github.com/Kronuz/queue.git       GIT_TAG main)
FetchContent_Declare(compressors GIT_REPOSITORY https://github.com/Kronuz/compressors.git GIT_TAG main)
FetchContent_GetProperties(queue)
if(NOT queue_POPULATED)
	FetchContent_Populate(queue)
endif()
# compressors has compiled units + its own lz4/zstd/zlib wiring, so build it.
FetchContent_MakeAvailable(compressors)

# --- the server engine ---
# worker.cc + tcp.cc + base_server.cc are the out-of-line units; base_client is
# header-only (base_client.h includes base_client.tcc). The root holds the engine
# headers + the redirectable seams (server_hooks.h, server_trace.h); compat/ holds
# the standalone shims a host overrides.
add_library(server STATIC worker.cc tcp.cc base_server.cc)
target_include_directories(server PUBLIC
	"${CMAKE_CURRENT_SOURCE_DIR}"   # engine headers + seams
	"${SERVER_COMPAT_DIR}"          # io.hh / length.h / colors.h / strings.hh / ... (standalone)
	"${queue_SOURCE_DIR}"           # queue.h
)
target_compile_features(server PUBLIC cxx_std_20)
target_link_libraries(server PUBLIC ev compressors::compressors)
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
