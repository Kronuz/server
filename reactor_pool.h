/*
 * Copyright (c) 2026 Germán Méndez Bravo (Kronuz)
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

// ReactorPool -- the runtime that scales the engine across cores. It runs N
// independent reactors, each on its own thread with its own event loop (a Worker
// created with a null parent loop owns a dynamic ev loop), each hosting one Service.
// This is the shared-nothing, Tokio-style model: a connection is accepted on a
// reactor and every bit of its work -- read, dispatch, the worker-pool offload, write
// -- stays on that reactor's loop and pool, so the runtime never contends a global
// lock or bounces a connection across cores. To fan connections out you bind all N on
// the same port with SO_REUSEPORT and let the kernel spread them.
//
// It is PROTOCOL-AGNOSTIC: a Service is anything the factory builds that exposes
//   void listen(unsigned int port);   // bind; all N share the port (reuse_port)
//   void run();                       // run this reactor's loop; blocks until stop()
//   void stop();                      // break the loop (thread-safe)
// http::HttpService satisfies this, and so does a raw server root -- the pool knows
// nothing of HTTP. This is why the multi-reactor pattern lives here, at the runtime
// layer, and not inside any one protocol binding: HTTP, a future gRPC service, or a
// custom binary protocol all scale through the same pool.

#pragma once

#include <cstddef>
#include <functional>
#include <future>
#include <memory>
#include <thread>
#include <vector>

// One reactor per thread, each hosting a Service. `Service` is duck-typed
// (listen/run/stop); the factory builds one per reactor, each on its own loop and
// binding with reuse_port so the N share a port.
template <typename Service>
class ReactorPool {
public:
	using Factory = std::function<std::shared_ptr<Service>()>;

	// `reactors` runtimes (0 is treated as 1); `factory` builds one Service per
	// reactor. Each Service must be created on its own loop (a Worker with a null
	// parent loop is) and bind with reuse_port so all N can share the port.
	ReactorPool(std::size_t reactors, Factory factory)
		: reactors_(reactors != 0 ? reactors : 1),
		  factory_(std::move(factory)) {}

	~ReactorPool() { stop(); }

	ReactorPool(const ReactorPool&) = delete;
	ReactorPool& operator=(const ReactorPool&) = delete;

	// Spawn the reactors and bind `port` on each. Returns once every reactor that came
	// up is listening (so a client can connect immediately). Non-blocking: the reactors
	// run on their own threads and the caller keeps its own thread, tearing them down
	// with stop().
	void start(unsigned int port) {
		services_.assign(reactors_, nullptr);
		std::vector<std::promise<void>> ready(reactors_);
		threads_.reserve(reactors_);
		for (std::size_t i = 0; i < reactors_; ++i) {
			threads_.emplace_back([this, i, port, &ready] {
				std::shared_ptr<Service> svc;
				try {
					svc = factory_();
					svc->listen(port);
					services_[i] = svc;      // a distinct slot; the vector never resizes here
					ready[i].set_value();     // published: this reactor is bound + stored
				} catch (...) {
					// A reactor that fails to bind must not hang start(); leave its slot
					// null and let the others serve.
					try { ready[i].set_value(); } catch (...) {}
					return;
				}
				svc->run();                   // this reactor's loop, until stop()
			});
		}
		for (auto& r : ready) { r.get_future().wait(); }
	}

	// Stop every reactor and join its thread. Idempotent.
	void stop() {
		for (auto& s : services_) {
			if (s) { s->stop(); }
		}
		for (auto& t : threads_) {
			if (t.joinable()) { t.join(); }
		}
		threads_.clear();
		services_.clear();
	}

	std::size_t size() const { return reactors_; }

private:
	std::size_t reactors_;
	Factory factory_;
	std::vector<std::shared_ptr<Service>> services_;
	std::vector<std::thread> threads_;
};
