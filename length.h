/*
 * Minimal length varint codec for the standalone `server` engine.
 *
 * The connection FSM's file-transfer path (FILE_FOLLOWS) frames each chunk with
 * a length prefix. Xapiand's length.h carries the full (de)serialisation surface
 * plus a GPL `serialise_double` from Xapian and throws `Xapian::SerialisationError`
 * on malformed input. The engine only needs the unsigned-length varint, so this
 * vendors exactly serialise_length / unserialise_length (byte-compatible with the
 * original) and throws its own `server::SerialisationError` — no Xapian, no GPL.
 */

#pragma once

#include <cstddef>
#include <stdexcept>
#include <string>


namespace server {
class SerialisationError : public std::runtime_error {
public:
	explicit SerialisationError(const char* what) : std::runtime_error(what) {}
};
}  // namespace server


inline std::string serialise_length(unsigned long long len) {
	std::string result;
	if (len < 255) {
		result += static_cast<char>(static_cast<unsigned char>(len));
		return result;
	}
	result += '\xff';
	len -= 255;
	while (true) {
		unsigned char b = static_cast<unsigned char>(len & 0x7f);
		len >>= 7;
		if (!len) {
			result += static_cast<char>(b | static_cast<unsigned char>(0x80));
			break;
		}
		result += static_cast<char>(b);
	}
	return result;
}


inline unsigned long long unserialise_length(const char** p, const char* end) {
	if (*p == end) {
		throw server::SerialisationError("Bad encoded length: no data");
	}
	unsigned long long len = static_cast<unsigned char>(*(*p)++);
	if (len == 0xff) {
		len = 0;
		unsigned char ch;
		unsigned shift = 0;
		do {
			if (*p == end || shift > (sizeof(unsigned long long) * 8 / 7 * 7)) {
				throw server::SerialisationError("Bad encoded length: insufficient data");
			}
			ch = static_cast<unsigned char>(*(*p)++);
			len |= static_cast<unsigned long long>(ch & 0x7f) << shift;
			shift += 7;
		} while ((ch & 0x80) == 0);
		len += 255;
	}
	return len;
}
