/*
 * Minimal `io` namespace for the standalone `server` engine.
 *
 * Vendored, trimmed copy of Xapiand's io.hh: just the EINTR-safe socket + file
 * wrappers the reactor/TCP/client substrate calls (socket/bind/listen/accept/
 * connect/setsockopt/.../read/write/close/lseek/open/unlink/ignored_errno), with
 * none of the opts/config/error-injection machinery. A host that owns an
 * instrumented io layer (Xapiand) can redirect to it; standalone, this is the
 * default.
 */

#pragma once

#include <atomic>
#include <cerrno>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>


namespace io {

// Whether EINTR is retried. Always true here (a host could expose a knob).
inline std::atomic_bool& ignore_eintr() noexcept {
	static std::atomic_bool _ignore_eintr{true};
	return _ignore_eintr;
}

inline bool ignored_errno(int e, bool again, bool tcp, bool udp) noexcept {
	switch (e) {
		case EINTR:
			return ignore_eintr().load();
		case EAGAIN:
#if EAGAIN != EWOULDBLOCK
		case EWOULDBLOCK:
#endif
			return again;
		case EALREADY:
		case EINPROGRESS:
			return tcp;
		case ENETDOWN:
		case EPROTO:
		case ENOPROTOOPT:
		case EHOSTDOWN:
#ifdef ENONET
		case ENONET:
#endif
		case EHOSTUNREACH:
		case EOPNOTSUPP:
		case ENETUNREACH:
		case ECONNRESET:
			return udp;
		default:
			return false;
	}
}

template <typename Fun, typename... Args>
inline auto RetryAfterSignal(const Fun& F, const Args&... As) noexcept -> decltype(F(As...)) {
	decltype(F(As...)) _err;
	do {
		errno = 0;
		_err = F(As...);
	} while (_err == -1 && errno == EINTR && ignore_eintr().load());
	return _err;
}

// ---- sockets (thin EINTR-safe passthroughs) ----
inline int socket(int domain, int type, int protocol) noexcept { return RetryAfterSignal(::socket, domain, type, protocol); }
inline int bind(int s, const struct sockaddr* addr, socklen_t len) noexcept { return RetryAfterSignal(::bind, s, addr, len); }
inline int listen(int s, int backlog) noexcept { return RetryAfterSignal(::listen, s, backlog); }
inline int accept(int s, struct sockaddr* addr, socklen_t* len) noexcept { return RetryAfterSignal(::accept, s, addr, len); }
inline int connect(int s, const struct sockaddr* addr, socklen_t len) noexcept { return RetryAfterSignal(::connect, s, addr, len); }
inline int setsockopt(int s, int level, int name, const void* val, socklen_t len) noexcept { return RetryAfterSignal(::setsockopt, s, level, name, val, len); }
inline int getsockopt(int s, int level, int name, void* val, socklen_t* len) noexcept { return RetryAfterSignal(::getsockopt, s, level, name, val, len); }
inline int shutdown(int s, int how) noexcept { return RetryAfterSignal(::shutdown, s, how); }
inline ssize_t send(int s, const void* buf, size_t len, int flags) noexcept { return RetryAfterSignal(::send, s, buf, len, flags); }
inline int dup(int fd) noexcept { return RetryAfterSignal(::dup, fd); }
inline int dup2(int fd, int fd2) noexcept { return RetryAfterSignal(::dup2, fd, fd2); }

template <typename... Args>
inline int fcntl(int fd, int cmd, Args&&... args) noexcept { return RetryAfterSignal(::fcntl, fd, cmd, std::forward<Args>(args)...); }

inline int unlink(const char* path) noexcept { return RetryAfterSignal(::unlink, path); }

// ---- files ----
inline int open(const char* path, int oflag = O_RDONLY, int mode = 0644) noexcept {
	int fd;
	do { fd = ::open(path, oflag | O_CLOEXEC, mode); } while (fd == -1 && errno == EINTR && ignore_eintr().load());
	return fd;
}

inline int close(int fd) noexcept {
	// Do NOT retry on EINTR (the fd is already closed on most systems).
	return ::close(fd);
}

inline off_t lseek(int fd, off_t offset, int whence) noexcept { return ::lseek(fd, offset, whence); }

inline ssize_t write(int fd, const void* buf, size_t nbyte) noexcept {
	const auto* p = static_cast<const char*>(buf);
	while (nbyte != 0) {
		ssize_t c = ::write(fd, p, nbyte);
		if (c == -1) {
			if (errno == EINTR && ignore_eintr().load()) { continue; }
			size_t w = p - static_cast<const char*>(buf);
			return w == 0 ? -1 : static_cast<ssize_t>(w);
		}
		p += c;
		if (c == static_cast<ssize_t>(nbyte)) { break; }
		nbyte -= c;
	}
	return p - static_cast<const char*>(buf);
}

inline ssize_t read(int fd, void* buf, size_t nbyte) noexcept {
	auto* p = static_cast<char*>(buf);
	while (nbyte != 0) {
		ssize_t c = ::read(fd, p, nbyte);
		if (c == -1) {
			if (errno == EINTR && ignore_eintr().load()) { continue; }
			size_t r = p - static_cast<char*>(buf);
			return r == 0 ? -1 : static_cast<ssize_t>(r);
		}
		if (c == 0) { break; }
		p += c;
		if (c == static_cast<ssize_t>(nbyte)) { break; }
		nbyte -= c;
	}
	return p - static_cast<char*>(buf);
}

}  // namespace io
