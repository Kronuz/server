/*
 * Default (no-op) logging hooks for the standalone `server` engine.
 *
 * The reactor / TCP / client substrate instruments itself through a family of
 * L_* logging macros (and a couple of color helpers) that are no-ops by default,
 * so the engine builds with no dependency on any logger. A host restores real,
 * colored, leveled logging by defining SERVER_TRACE_HEADER to a header that
 * provides these macros (Xapiand points it at its log.h), or by defining the
 * macros before including the engine headers.
 *
 * The engine's behavior never depends on these: they only emit diagnostics.
 */

#pragma once

#ifndef L_NOTHING
#define L_NOTHING(...)
#endif

// The full L_* surface the substrate uses. All no-ops by default; each is
// #ifndef-guarded so a host can override any subset.
#define SERVER_TRACE_NOOP(name) \
	ifndef_##name

#ifndef L_CALL
#define L_CALL(...)
#endif
#ifndef L_DEBUG
#define L_DEBUG(...)
#endif
#ifndef L_DEBUG_HOOK
#define L_DEBUG_HOOK(...)
#endif
#ifndef L_ERR
#define L_ERR(...)
#endif
#ifndef L_CRIT
#define L_CRIT(...)
#endif
#ifndef L_WARNING
#define L_WARNING(...)
#endif
#ifndef L_WARNING_ONCE
#define L_WARNING_ONCE(...)
#endif
#ifndef L_EXC
#define L_EXC(...)
#endif
#ifndef L_OBJ
#define L_OBJ(...)
#endif
#ifndef L_CONN
#define L_CONN(...)
#endif
#ifndef L_EV
#define L_EV(...)
#endif
#ifndef L_EV_BEGIN
#define L_EV_BEGIN(...)
#endif
#ifndef L_EV_END
#define L_EV_END(...)
#endif
#ifndef L_WORKER
#define L_WORKER(...)
#endif
#ifndef L_TCP_ENQUEUE
#define L_TCP_ENQUEUE(...)
#endif
#ifndef L_TCP_WIRE
#define L_TCP_WIRE(...)
#endif
#ifndef L_DELAYED_
#define L_DELAYED_(...)
#endif
#ifndef L_DELAYED_N_UNLOG
#define L_DELAYED_N_UNLOG(...)
#endif

// Color-tagged variants the substrate sprinkles into L_* calls. As no-ops they
// just swallow their arguments.
#ifndef L_RED
#define L_RED(...)
#endif
#ifndef L_GREEN
#define L_GREEN(...)
#endif
#ifndef L_LIGHT_GREEN
#define L_LIGHT_GREEN(...)
#endif
#ifndef L_GREY
#define L_GREY(...)
#endif
#ifndef L_WHITE
#define L_WHITE(...)
#endif
#ifndef L_SALMON
#define L_SALMON(...)
#endif
#ifndef L_MEDIUM_PURPLE
#define L_MEDIUM_PURPLE(...)
#endif
#ifndef L_STACKED_DIM_GREY
#define L_STACKED_DIM_GREY(...)
#endif
