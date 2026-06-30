/*
 * Pass-through file-transfer codec for the standalone `server` engine.
 *
 * PROTOTYPE NOTE. The connection FSM (base_client) has a FILE_FOLLOWS path that
 * streams a file over the wire, optionally LZ4-compressed with a running XXH32
 * checksum. In Xapiand that path backs replication's whole-database copy; a
 * generic HTTP server never uses it. The real codec pulls in lz4 + the heap
 * xxHash API, so for the prototype this provides the same class surface
 * (ClientLZ4Compressor / ClientLZ4Decompressor) as a RAW pass-through: it frames
 * the bytes length-prefixed but does not compress, and verify() is a no-op. The
 * productionized library would swap this for the real Kronuz/compressors-backed
 * codec.
 *
 * Writer interface used:  writer.write(std::string) , writer.write(const char*, size_t)
 * Reader interface used:  reader.on_read_file(const char*, ssize_t)
 */

#pragma once

#include <cstdint>
#include <string>
#include <unistd.h>          // for off_t

#include "io.hh"             // for io::read, io::lseek
#include "length.h"          // for serialise_length

#ifndef LZ4_BLOCK_SIZE
#define LZ4_BLOCK_SIZE (1024 * 8)
#endif


template <typename Writer>
class ClientLZ4Compressor {
	Writer& writer;
	off_t offset;
	int fd;

public:
	ClientLZ4Compressor(Writer& writer, int fd, off_t offset = 0)
		: writer(writer), offset(offset), fd(fd) {}

	// Frame the file at `fd` as length-prefixed raw blocks (no compression),
	// terminated by a zero-length marker + a zero "checksum". Returns the number
	// of payload bytes sent, or -1 on IO error.
	ssize_t compress() {
		if (io::lseek(fd, offset, SEEK_SET) != offset) {
			return -1;
		}
		char buffer[LZ4_BLOCK_SIZE];
		ssize_t size = 0;
		ssize_t r;
		while ((r = io::read(fd, buffer, sizeof(buffer))) > 0) {
			if (!writer.write(serialise_length(static_cast<unsigned long long>(r))) || !writer.write(buffer, static_cast<size_t>(r))) {
				return -1;
			}
			size += r;
		}
		if (r < 0) {
			return -1;
		}
		if (!writer.write(serialise_length(0)) || !writer.write(serialise_length(0))) {
			return -1;
		}
		return size;
	}
};


template <typename Reader>
class ClientLZ4Decompressor {
	Reader& reader;
	std::string input;

public:
	explicit ClientLZ4Decompressor(Reader& reader) : reader(reader) {}

	void clear() noexcept { input.clear(); }

	void append(const char* buf, size_t size) { input.append(buf, size); }

	// Hand the accumulated bytes straight to the reader (no decompression).
	ssize_t decompress() {
		ssize_t size = static_cast<ssize_t>(input.size());
		reader.on_read_file(input.data(), size);
		return size;
	}

	// No checksum in the pass-through; accept whatever the sender claimed.
	bool verify(uint32_t /*checksum*/) noexcept { return true; }
};
