# core-cpp

Typed C++17 wire codec for the OpenIGTLink protocol. Generated from
the schemas under [`../spec/schemas/`](../spec/schemas/) via the
Python codegen in [`../corpus-tools/`](../corpus-tools/).

Symmetric to [`../core-py/`](../core-py/): same 84 message types,
same registry pattern, same drift-checked codegen. Downstream C++
applications depend on this package; the Python codec in
`corpus-tools/` is the reference / oracle, not meant for production
use.

## Status

**Codec: complete.** Round-trips 23 of 24 upstream fixtures
byte-for-byte, plus cross-language oracle parity against the Python
reference codec. Transport layer (ASIO-based TCP/UDP, TLS, session
management) is future work; see the parent repo's `README.md`.

## Layout

```
core-cpp/
├── include/oigtl/
│   ├── runtime/        hand-written, ~700 LoC, stdlib-only
│   │   ├── byte_order.hpp      inline BE read/write helpers
│   │   ├── crc64.hpp           CRC-64 ECMA-182 (slice-by-8, ~1.4 GB/s)
│   │   ├── error.hpp           typed exception hierarchy
│   │   ├── header.hpp          58-byte OpenIGTLink header
│   │   ├── extended_header.hpp v3 extended header (12-byte)
│   │   ├── metadata.hpp        v3 metadata block (index + body)
│   │   ├── dispatch.hpp        Registry<type_id → round-trip fn>
│   │   └── oracle.hpp          parse_wire + verify_wire_bytes
│   └── messages/       generated, one file per type_id (84 total)
│       ├── transform.hpp
│       ├── ...
│       └── register_all.hpp    generated; populates a Registry
├── src/
│   ├── runtime/        hand-written .cpp counterparts
│   └── messages/       generated .cpp (84 + register_all.cpp)
├── tests/
│   ├── upstream_fixtures_test.cpp   23 fixtures via the registry
│   └── oracle_parity_test.cpp       shells out to the Python oracle
├── benches/
│   └── bench_codec.cpp             microbenchmark
└── CMakeLists.txt
```

## Usage

### Codec only

```cpp
#include "oigtl/messages/register_all.hpp"
#include "oigtl/runtime/oracle.hpp"

// Option 1 — you know the type_id, just use the struct directly:
#include "oigtl/messages/transform.hpp"
auto tx = oigtl::messages::Transform::unpack(body_ptr, body_len);
std::vector<std::uint8_t> body = tx.pack();

// Option 2 — type-erased verify + round-trip via the registry:
auto registry = oigtl::messages::default_registry();  // all 84 types
auto result = oigtl::runtime::oracle::verify_wire_bytes(
    wire_ptr, wire_len, registry);
if (!result.ok) {
    /* result.error describes why */
}
```

### TCP Client + Server — the ergonomic API

```cpp
#include "oigtl/client.hpp"
#include "oigtl/server.hpp"
#include "oigtl/messages/transform.hpp"

// Client: connect, send, receive.
auto client = oigtl::Client::connect("tracker.lab", 18944);
client.send(oigtl::messages::Transform{ .matrix = { ... } });
auto reply = client.receive<oigtl::messages::Status>();

// Server: listen, dispatch by type, run.
oigtl::Server::listen(18944)
    .on<oigtl::messages::Transform>([&](auto& env) {
        process_pose(env.body.matrix);
    })
    .restrict_to_local_subnet()     // opt-in network policy
    .set_max_simultaneous_clients(4)
    .run();
```

For resilient client configurations (auto-reconnect, offline
buffer, TCP keepalive), see **[CLIENT_GUIDE.md](CLIENT_GUIDE.md)**
and the runnable **[examples/resilient_client.cpp](examples/resilient_client.cpp)**.

## Build + test

```bash
cmake -S core-cpp -B core-cpp/build
cmake --build core-cpp/build
ctest --test-dir core-cpp/build --output-on-failure
```

CTest runs two suites:

1. **`upstream_fixtures`** — every supported upstream fixture
   (TRANSFORM, STATUS, STRING, SENSOR, POSITION, IMAGE, COLORTABLE,
   COMMAND, CAPABILITY, POINT, TRAJ, TDATA, IMGMETA, LBMETA, BIND,
   POLYDATA, NDARRAY, plus Format2/v3 variants and VIDEOMETA)
   round-trips byte-for-byte through the registry-backed oracle.

2. **`oracle_parity`** — for each fixture, shells out to `uv run
   oigtl-corpus oracle verify --fixture <name>` (the Python
   reference codec) and asserts both sides agree on the framing
   summary (`ok`, `type_id`, `version`, `body_size`,
   `ext_header_size`, `metadata_count`, `round_trip_ok`). Skips
   at runtime with a clear message if `uv` isn't on PATH; CI
   installs it so the test always exercises.

## Performance

Release build on Apple Silicon:

| Operation | Time | Throughput |
|---|---|---|
| TRANSFORM unpack (48 B body) | 41 ns | 1.1 GB/s |
| TRANSFORM pack | 42 ns | 1.1 GB/s |
| IMAGE unpack (2.5 KB body) | 970 ns | 2.5 GB/s |
| IMAGE pack | 860 ns | 2.8 GB/s |
| IMAGE oracle round-trip (incl. CRC verify) | 2.84 µs | 884 MB/s |
| CRC-64 ECMA-182 (2.5 KB, slice-by-8) | 1.75 µs | 1.4 GB/s |

Run the benchmark with:

```bash
cmake -S core-cpp -B core-cpp/build-release -DCMAKE_BUILD_TYPE=Release
cmake --build core-cpp/build-release --target oigtl_bench
./core-cpp/build-release/oigtl_bench
```

## Compiler floor

C++17 in the public API. CI exercises:

- Ubuntu / GCC 13
- Ubuntu / Clang 15
- macOS / Apple Clang
- Windows / MSVC 2022 (x64)

The transport layer uses a pimpl'd platform abstraction — POSIX
sockets on Linux/macOS, Winsock2 + iphlpapi on Windows — so
the same public headers work everywhere. MinGW, Cygwin, and
32-bit Windows are out of scope.

Internal `.cpp` code may use later-standard features if guarded
behind feature-test macros, but nothing in the codec currently
needs that.

## Dependencies

**None at runtime.** Standard library only. The generated message
classes use `std::vector`, `std::string`, `std::array` — no external
libraries. Transport-layer dependencies (ASIO, OpenSSL) land when
the transport layer lands.

## Regenerating

The contents of `include/oigtl/messages/` and `src/messages/` are
generated. To regenerate after a schema change:

```bash
uv run --project corpus-tools oigtl-corpus codegen cpp
```

CI enforces drift via `oigtl-corpus codegen cpp --check`.

## Install & consume

```bash
cmake -S core-cpp -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build --prefix /your/prefix
```

Installs five granular static archives, a merged `liboigtl.a`,
`oigtlConfig.cmake` with imported targets (`oigtl::runtime`,
`oigtl::messages`, `oigtl::transport`, `oigtl::ergo`, `oigtl::igtl_compat`,
`oigtl::oigtl`), and a pkg-config file.

CMake consumer:

```cmake
find_package(oigtl REQUIRED)
target_link_libraries(your_app PRIVATE oigtl::oigtl)
```

Makefile / autotools consumer:

```
$(pkg-config --cflags oigtl)
$(pkg-config --libs   oigtl)
```

### Drop-in for upstream libOpenIGTLink

If you have a C++ codebase written against the upstream
OpenIGTLink library (`igtl::TransformMessage`, `igtl::ClientSocket`,
etc.), you can relink against our library unchanged — the
`igtl::` namespace is provided in full by `oigtl::igtl_compat`.
See [`compat/MIGRATION.md`](compat/MIGRATION.md) for specifics.

Configure with `-DOIGTL_DROP_IN_NAME=ON` to also install the
merged archive under the literal filename `libOpenIGTLink.a` for
consumers who can't easily change a Makefile.

## License

Apache 2.0. See [`../LICENSE`](../LICENSE).
