/*
 * Minimal strings::format shim for the standalone `server` engine.
 *
 * The substrate uses exactly one thing from Xapiand's strings toolkit:
 * strings::format(fmt, args...) inside __repr__() debug builders. This forwards
 * it to std::vformat (C++20), so the engine needs neither the full Kronuz/strings
 * library nor its dependency chain. A host with the real toolkit shadows this by
 * putting its strings.hh earlier on the include path.
 */

#pragma once

#include <format>
#include <string>
#include <string_view>
#include <utility>

namespace strings {

template <typename... Args>
inline std::string format(std::string_view fmt, Args&&... args) {
	return std::vformat(fmt, std::make_format_args(args...));
}

}  // namespace strings
