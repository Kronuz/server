/*
 * Color-constant shim for the standalone `server` engine.
 *
 * The substrate's __repr__() debug methods embed Xapiand's named ANSI colors
 * (STEEL_BLUE, ...) into the strings they build. Those strings are only ever
 * consumed by L_* logging, which is no-op by default, but the method bodies must
 * still compile. This defines the handful of names used as empty strings, so the
 * repr text builds with no color codes and no term-color dependency. A host that
 * wants real colors provides its own colors.h ahead of the engine headers.
 */

#pragma once

#include <string>

#ifndef SERVER_COLORS_DEFINED
#define SERVER_COLORS_DEFINED
inline const std::string STEEL_BLUE;
inline const std::string DARK_STEEL_BLUE;
inline const std::string ORANGE;
inline const std::string FOREST_GREEN;
#endif
