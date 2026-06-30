/*
 * Copyright (c) 2015-2019 Dubalu LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#pragma once

// The file-transfer codec for the connection FSM's FILE_FOLLOWS path: stream a
// file as length-prefixed LZ4 blocks with a trailing XXH32 checksum, and the
// inverse on the receiving side. Built on the Kronuz/compressors LZ4 classes
// (LZ4CompressFile / LZ4DecompressData), whose get_digest() is compressors' own
// canonical XXH32 — so there is no separate (heap) xxHash dependency.
//
// Writer interface:  writer.write(std::string) , writer.write(const char*, size_t)
// Reader interface:  reader.on_read_file(const char*, ssize_t)

#include <string>

#include "compressor_lz4.h"      // for LZ4CompressFile, LZ4DecompressData, LZ4_BLOCK_SIZE
#include "length.h"              // for serialise_length

// Logging hooks (no-op by default; host redirects via SERVER_TRACE_HEADER).
#ifdef SERVER_TRACE_HEADER
#  include SERVER_TRACE_HEADER
#else
#  include "server_trace.h"
#endif


#define COMPRESSION_SEED 0xCEED


template <typename Writer>
class ClientLZ4Compressor : public LZ4CompressFile {
	Writer& writer;

public:
	ClientLZ4Compressor(Writer& writer, int fd, size_t offset = 0) :
		LZ4CompressFile(fd, offset, -1, COMPRESSION_SEED),
		writer(writer) {}

	ssize_t compress() {
		L_CALL("compress()");

		try {
			auto it = begin();
			while (it) {
				std::string length(serialise_length(it.size()));
				if (!writer.write(length) || !writer.write(it->data(), it.size())) {
					L_ERR("Write failed!");
					return -1;
				}
				++it;
			}
		} catch (const std::exception& e) {
			L_ERR("{}", e.what());
			return -1;
		}

		if (!writer.write(serialise_length(0)) || !writer.write(serialise_length(get_digest()))) {
			L_ERR("Write Footer failed!");
			return -1;
		}

		return size();
	}
};


template <typename Reader>
class ClientLZ4Decompressor : public LZ4DecompressData {
	Reader& reader;

	std::string input;

public:
	explicit ClientLZ4Decompressor(Reader& reader) :
		LZ4DecompressData(nullptr, 0, COMPRESSION_SEED),
		reader(reader) {}

	void clear() noexcept {
		L_CALL("clear()");

		input.clear();
	}

	void append(const char* buf, size_t size) {
		L_CALL("append(<buf>, {})", size);

		input.append(buf, size);
	}

	ssize_t decompress() {
		L_CALL("decompress()");

		add_data(input.data(), input.size());
		auto it = begin();
		while (it) {
			reader.on_read_file(it->data(), it.size());
			++it;
		}
		return size();
	}

	bool verify(uint32_t checksum_) noexcept {
		L_CALL("verify({:#06x})", checksum_);

		return get_digest() == checksum_;
	}
};
