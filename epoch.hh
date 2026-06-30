/*
 * Minimal epoch::now for the standalone `server` engine.
 *
 * Worker uses epoch::now<>() for a coarse monotonic timestamp (detach/keep-alive
 * bookkeeping). Vendored trim of the Kronuz/epoch helper: a steady-clock now in
 * seconds (as the default duration), no other surface.
 */

#pragma once

#include <chrono>

namespace epoch {

template <typename Duration = std::chrono::seconds>
inline auto now() noexcept {
	return std::chrono::duration_cast<Duration>(
		std::chrono::steady_clock::now().time_since_epoch()).count();
}

}  // namespace epoch
