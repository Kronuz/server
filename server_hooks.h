/*
 * Injection seams for the standalone `server` engine.
 *
 * The substrate was lifted out of Xapiand, which reached three host singletons
 * from inside the reactor / client code. Those are now hooks with safe defaults,
 * so the engine depends on nothing host-specific; a host overrides them (Xapiand
 * maps them back onto sig_exit + its client accounting) by defining the macros
 * before including the engine headers, or by pointing SERVER_HOOKS_HEADER at a
 * header that defines them.
 *
 *   - SERVER_FATAL(code)        — an unrecoverable startup/config error (was
 *                                 sig_exit). Default: terminate the process.
 *   - SERVER_ON_CLIENT_CREATED()    \  per-connection lifecycle notifications
 *   - SERVER_ON_CLIENT_DESTROYED()  /  (was XapiandManager's total_clients
 *                                 accounting + last-client shutdown). No-op by
 *                                 default; a host counts live connections and
 *                                 drives its own drain/shutdown from them.
 */

#pragma once

#ifdef SERVER_HOOKS_HEADER
#  include SERVER_HOOKS_HEADER
#endif

#include <cstdlib>           // for std::_Exit, EXIT_FAILURE

// Unrecoverable error from the socket/bind path. The engine cannot continue
// serving, so the default terminates; a host (Xapiand) maps this to its graceful
// sig_exit so the rest of the process can wind down.
#ifndef SERVER_FATAL
#define SERVER_FATAL(code) (::std::_Exit(EXIT_FAILURE))
#endif

// A client connection was accepted / finished. No-ops by default. A host uses
// these to track live-connection counts and to decide when the last client has
// gone (Xapiand's "total_clients hit zero -> try_shutdown" logic lives here).
#ifndef SERVER_ON_CLIENT_CREATED
#define SERVER_ON_CLIENT_CREATED() ((void)0)
#endif
#ifndef SERVER_ON_CLIENT_DESTROYED
#define SERVER_ON_CLIENT_DESTROYED() ((void)0)
#endif

// The exception type the file-length decoder throws on malformed input (caught
// in the FILE_FOLLOWS path). Defaults to the engine's own server::SerialisationError
// (from length.h); a host that uses its own length codec redefines this to that
// codec's error type (Xapiand: Xapian::SerialisationError).
#ifndef SERVER_LENGTH_ERROR
#define SERVER_LENGTH_ERROR ::server::SerialisationError
#endif
