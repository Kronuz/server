# server (Leg-1 prototype)

A generic, search-agnostic HTTP/TCP server engine extracted from
[Xapiand](https://github.com/Kronuz/Xapiand): the libev reactor, the TCP socket
layer, the accept-loop server base, and the per-connection client state machine.

This is the **Leg-1 prototype** from the "invert Xapiand" assessment — proof that
the networking substrate is genuinely generic and lifts out cleanly, with the
search engine able to sit on top as an application rather than underneath.

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build
./build/http_hello 8080      # a complete HTTP server, zero search code
curl localhost:8080/anything # -> "Hello from Kronuz/server ..."
```

`examples/http_hello.cc` is 142 lines: an app supplies a client type
(`BaseClient<DemoClient>`, an `on_read` that writes a response) and an accept
handler (`MetaBaseServer<DemoServer>`), and the engine runs everything. Nothing
in the app — or the engine — references Xapiand, Xapian, or search.

## What it contains

| File | Role |
|---|---|
| `worker.{h,cc}` | The reactor base: owns the ev loop + the async start/stop/destroy/detach control watchers + the worker tree. |
| `ev/` | Vendored libev, built standalone (`EV_STANDALONE`). |
| `tcp.{h,cc}` | Socket layer: bind / listen / accept / connect / sockopts. |
| `base_server.{h,cc}` | `MetaBaseServer<ServerImpl>` — the CRTP accept-loop base. |
| `base_client.{h,.tcc}` | `BaseClient<ClientImpl>` — the per-connection read/write FSM (header-only). |
| `buffer.h`, `client_compressor.h` | Write buffering + the file-transfer codec (the codec is a prototype pass-through; see below). |

## The four couplings that were cut (the actual Leg-1 work)

The substrate reached three host singletons plus a CRTP instantiation list. Each
became an injection seam with a safe default, so the engine depends on nothing
host-specific. A host (Xapiand) overrides them via `server_hooks.h` /
`SERVER_TRACE_HEADER`.

1. **`sig_exit` → `SERVER_FATAL(code)`** (`server_hooks.h`). Default terminates;
   Xapiand maps it to its graceful exit.
2. **`XapiandManager::total_clients` accounting → `SERVER_ON_CLIENT_CREATED()` /
   `SERVER_ON_CLIENT_DESTROYED()`** hooks (`server_hooks.h`). No-op by default;
   the host tracks live connections and drives its own drain/shutdown.
3. **`Xapian::SerialisationError` → `server::SerialisationError`** — the
   file-length decoder throws its own error (vendored `length.h`); no Xapian, no
   GPL.
4. **The explicit `template class BaseClient<HttpClient>; ...` list → header-only
   `.tcc`.** The application now instantiates `BaseClient<ItsClient>` just by
   including the header; the engine never names a concrete client.

Logging is the no-op `L_*` family (`server_trace.h`); the `__repr__` debug
helpers use empty-string color shims. All of these are `SERVER_*_HEADER`
redirects so a host restores its real logger/colors.

## Host integration (root vs compat/)

The library is split so a host can drop its own richer versions of the
boundary headers in:

- **Root** — the engine itself (`worker`, `tcp`, `base_server`, `base_client`,
  `buffer`, `client_compressor`) plus the redirectable seams (`server_hooks.h`,
  `server_trace.h`). A host always takes these.
- **`compat/`** — standalone shims for headers that would collide with a host's
  own: `io.hh`, `length.h`, `colors.h`, `strings.hh`, `epoch.hh`,
  `readable_revents.hh`, `likely.h`, and vendored `ev/`. Standalone, `compat/` is
  on the include path. A host (Xapiand) does **not** add it, so its own `io.hh` /
  `length.h` / `colors.h` / `strings.hh` / `ev/` win, and only the engine headers
  come from this library.

Dependencies (via FetchContent): `Kronuz/queue` (the client write queue) and
`Kronuz/compressors` (the LZ4 file-transfer codec in `client_compressor.h`,
real — used by replication's whole-database copy). UDP and the cluster protocols
are out of scope here; they layer on the same base the same way HTTP does and are
a later leg.
