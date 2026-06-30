/*
 * Stub of Xapiand's readable_revents() for the standalone `server` engine.
 *
 * The original turns a set of libev revents flags into a human string (e.g.
 * "READ|WRITE") for L_EV debug logging, pulling in strings::join. The engine only
 * ever uses it inside L_* calls, which are no-ops by default, so this trivial stub
 * keeps the call sites compiling without a strings dependency. A host that wants
 * the readable form provides its own via SERVER_TRACE_HEADER.
 */

#pragma once

#include <string>

inline std::string readable_revents(int /*revents*/) {
	return std::string();
}
