/*
 * http_hello — a generic HTTP server built entirely on the extracted `server`
 * engine (the reactor + TCP + accept-loop + connection FSM), with ZERO search
 * code. It proves the Leg-1 inversion: an application supplies a client type and
 * an accept handler, and the engine does the rest. Nothing here knows what
 * Xapiand is.
 *
 *   ./http_hello [port]      # default 8080
 *   curl localhost:8080/anything
 */

#include <atomic>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>

#include "base_client.h"     // BaseClient<ClientImpl> (header-only, includes base_client.tcc)
#include "base_server.h"     // MetaBaseServer<ServerImpl>
#include "worker.h"          // Worker (the reactor)


// ---- the connection handler: read a request, write a canned response --------
class DemoClient : public BaseClient<DemoClient> {
	friend BaseClient<DemoClient>;   // the base drives our on_* callbacks

	std::string request;

public:
	DemoClient(const std::shared_ptr<Worker>& parent, ev::loop_ref* loop, unsigned int flags)
		: BaseClient<DemoClient>(parent, loop, flags) {}

	// Called by the engine whenever bytes arrive. Returns bytes consumed.
	ssize_t on_read(const char* buf, ssize_t received) {
		if (received <= 0) {
			return received;
		}
		request.append(buf, static_cast<size_t>(received));
		// Respond once the request headers are complete.
		if (request.find("\r\n\r\n") != std::string::npos) {
			static const std::string body =
				"Hello from Kronuz/server -- a generic HTTP engine, no search in sight.\n";
			std::string resp =
				"HTTP/1.1 200 OK\r\n"
				"Content-Type: text/plain\r\n"
				"Content-Length: " + std::to_string(body.size()) + "\r\n"
				"Connection: close\r\n"
				"\r\n" + body;
			write(resp);
			close();
		}
		return received;
	}

	// The file-transfer path is unused by an HTTP server.
	void on_read_file(const char* /*buf*/, ssize_t /*received*/) {}
	void on_read_file_done() {}

	bool is_idle() { return request.empty(); }
};


// ---- the accept loop: spawn a DemoClient per connection ---------------------
class DemoServer : public MetaBaseServer<DemoServer> {
	friend Worker;

public:
	DemoServer(const std::shared_ptr<Worker>& parent, ev::loop_ref* loop, unsigned int flags)
		: MetaBaseServer<DemoServer>(parent, loop, flags, "demo-http", 0) {}

	// bind() is protected on TCP, so we expose listening from inside the subclass.
	void listen(unsigned int port) {
		bind(nullptr, port, 1);
	}

	void start_impl() override {
		Worker::start_impl();
		io.start(sock, ev::READ);   // arm the accept watcher (set to io_accept_cb by MetaBaseServer)
	}

	void io_accept_cb(ev::io& /*watcher*/, int revents) {
		if ((EV_ERROR & revents) != 0) {
			return;
		}
		int client_sock = accept();
		if (client_sock != -1) {
			auto client = Worker::make_shared<DemoClient>(share_this<DemoServer>(), ev_loop, ev_flags);
			if (!client->init(client_sock)) {
				client->detach();
				return;
			}
			client->start();
		}
	}
};


// ---- the root of the worker tree: owns the event loop -----------------------
class DemoRoot : public Worker {
public:
	DemoRoot(const std::shared_ptr<Worker>& parent, ev::loop_ref* loop, unsigned int flags)
		: Worker(parent, loop, flags) {}

	// Create + bind + start a server child (done here so it shares our ev loop,
	// which is a protected member).
	std::shared_ptr<DemoServer> serve(unsigned int port) {
		auto server = Worker::make_shared<DemoServer>(share_this<DemoRoot>(), ev_loop, ev_flags);
		server->listen(port);
		server->start();
		return server;
	}

	void run() { run_loop(); }
};


static std::weak_ptr<DemoRoot> g_root;
static void on_signal(int) {
	if (auto r = g_root.lock()) {
		r->break_loop();
	}
}

int main(int argc, char** argv) {
	unsigned int port = (argc > 1) ? static_cast<unsigned int>(std::atoi(argv[1])) : 8080;

	auto root = Worker::make_shared<DemoRoot>(std::shared_ptr<Worker>(), static_cast<ev::loop_ref*>(nullptr), static_cast<unsigned int>(ev::AUTO));
	g_root = root;

	std::signal(SIGINT, on_signal);
	std::signal(SIGTERM, on_signal);

	auto server = root->serve(port);
	std::printf("http_hello: generic server engine listening on http://127.0.0.1:%u/ (Ctrl-C to stop)\n", port);
	std::fflush(stdout);

	root->run();

	std::printf("http_hello: stopped.\n");
	return 0;
}
