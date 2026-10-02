# AsyncAsioTCPServer

A Minecraft Java Edition TCP proxy built with C++20 and standalone Asio. It inspects the initial handshake, answers server-list status requests locally, and rewrites login handshakes before forwarding traffic to an upstream server.

The project demonstrates asynchronous I/O, C++20 coroutines, object lifetime management, bounded binary parsing, and concurrent connection handling.

## Features

- Asynchronous connection acceptance and bidirectional forwarding with separate 4 KiB buffers.
- Coroutine-based handshake and status handling, with explicit error completion.
- Per-session Asio strands that serialize socket operations and cleanup across worker threads.
- Validated VarInt decoding, bounded handshake parsing, and length-prefixed serialization.
- Configurable upstream and listening endpoints, rewritten hostname, and status metadata.
- Handshake/status and connection deadlines, a connection limit, and coordinated shutdown.
- Upstream connection attempts across the resolved endpoint list.
- CMake and Make builds, automated C++ and loopback integration tests, and CI definitions.

This is a learning project with a focused protocol scope. It is not a Minecraft server or a protocol-version translator.

## Build and test

Requirements: a C++20 compiler, CMake 3.20 or newer, and Python 3.10 or newer for the integration tests. GCC 13.3 on Linux has been tested locally. Linux GCC/Clang and Windows MSVC jobs are configured in CI; their remote results have not been verified as part of this update.

Standalone Asio 1.36.0 and JSON for Modern C++ 3.12.0 are bundled in `src/external/`. Building does not download dependencies or require Boost.

From the project root:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```

For Visual Studio's multi-configuration generator:

```powershell
cmake -S . -B build
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

The executable is `build/ProxyServer` on a single-configuration Linux build or `build/Debug/ProxyServer.exe` with Visual Studio. To build without tests or a Python dependency, configure with `-DBUILD_TESTING=OFF`.

The existing Linux Make workflow is also supported:

```bash
make -j2
make test
```

Make produces `./ProxyServer`. It now tracks header dependencies and creates the build directory automatically. `make clean` removes its generated objects, dependency files, and server executable; CMake build directories are separate.

## Run

For an upstream server on your own machine at port `25566`:

```bash
./build/ProxyServer 127.0.0.1 --upstream-port 25566 --listen-address 127.0.0.1 --listen-port 25565
```

Connect the Minecraft client to `127.0.0.1:25565`. If an upstream server expects a virtual hostname in the handshake, specify it separately:

```bash
./build/ProxyServer 127.0.0.1 --upstream-port 25566 --handshake-host minecraft.example.com
```

Use `./build/ProxyServer --help` for every option. When using Make, substitute `./ProxyServer`.

| Option | Default | Purpose |
| --- | --- | --- |
| Positional upstream host | Required | TCP destination to resolve |
| `--listen-address` | `0.0.0.0` | Listening address; default accepts connections on all IPv4 interfaces |
| `--listen-port` | `25565` | Proxy port; `0` asks the OS for an available port |
| `--upstream-port` | `25565` | Upstream connection and rewritten handshake port |
| `--handshake-host` | Upstream host | Hostname embedded in login handshakes |
| `--threads` | `2` | Total event-loop threads, including the calling thread |
| `--max-connections` | `1024` | Maximum active sessions |
| `--handshake-timeout-ms` | `10000` | Deadline for the initial handshake or complete status/ping exchange |
| `--connect-timeout-ms` | `10000` | Deadline for all upstream connection attempts combined |
| `--motd` | Original project description | Static server-list description |
| `--version-name` / `--protocol` | `1.8.9` / `47` | Advertised version metadata |
| `--max-players` / `--online-players` | `1` / `0` | Static player counts |
| `--favicon` | Original embedded image | Text file containing a PNG base64 data URI, or `none` |

`Ctrl+C` and `SIGTERM` initiate shutdown: acceptance stops, active sockets and timers are canceled, pending handlers finish, and worker threads join. This cleans up resources but does not promise to deliver outstanding application data before closing connections.

## Architecture

| Component | Responsibility |
| --- | --- |
| `Server` | Owns configuration, listener, signal handling, active-session registry, and worker lifecycle |
| `Session` | Owns two sockets, relay buffers, a strand, and the deadline timer for one client |
| `MCPacketReader` | Reads the initial wire exchange and handles local status responses |
| `Protocol` | Parses handshakes and serializes framed handshakes/pongs without socket dependencies |
| `VarInt` | Encodes and decodes bounded 32-bit variable-length integers |
| `ServerConfig` | Parses and validates command-line settings |

Once a login handshake is forwarded, traffic is treated as an opaque byte stream. Each direction alternates a read and a complete asynchronous write, so its buffer is not reused until the write finishes. Both directions can have I/O pending at the same time; the strand prevents their handlers from concurrently accessing session state.

Session callbacks and coroutine completion handlers retain shared ownership. The server removes a session from its registry when it closes, while pending handlers keep its storage alive until they complete.

## Validation

There are 17 named C++ test cases and 18 loopback integration tests. They cover VarInt boundaries and malformed input, packet length correctness, configuration, fragmented/coalesced traffic, a 1 MiB relay, 24 clients, connection limits, refusal, deadlines, and POSIX shutdown signals. Integration tests use a local echo backend and need no Minecraft account or public server.

To enable AddressSanitizer and UndefinedBehaviorSanitizer on supported GCC/Clang platforms:

```bash
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DENABLE_SANITIZERS=ON
cmake --build build-asan --parallel 2
ctest --test-dir build-asan --output-on-failure
```

ThreadSanitizer can be configured separately with `-DENABLE_THREAD_SANITIZER=ON`; do not combine it with AddressSanitizer. See [validation results](docs/VALIDATION.md) for exactly what was run and environment limitations.

## Scope and limitations

- Handshakes are limited to 4 KiB, with hostname fields limited to 255 bytes. Extended addresses containing NUL-separated forwarding/mod metadata are rejected.
- The status response is local, static metadata. It does not query upstream player counts or translate game versions.
- Startup DNS resolution is synchronous. The connection deadline begins after the client handshake, not during startup DNS resolution.
- Established relays have no idle timeout. An EOF or I/O error closes both sockets; TCP half-close forwarding is not implemented.
- End-to-end testing with real Minecraft clients/servers and performance benchmarking remain future work.

## Understanding the changes

Read [CHANGES_AND_LEARNING_GUIDE.md](docs/CHANGES_AND_LEARNING_GUIDE.md) for the file-by-file changes, design explanations, and suggested exercises. Existing C++ comment text is preserved, including historical TODOs. Some comments moved with extracted code; adjacent notes clarify superseded comments where needed. The original README is retained in [docs/ORIGINAL_README.md](docs/ORIGINAL_README.md).

Bundled third-party code and notices remain unchanged in `src/external/`. The existing deployment workflow retains its environment and destination settings and now runs tests before producing its deployment artifact.
