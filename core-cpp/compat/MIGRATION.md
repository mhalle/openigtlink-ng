# Migrating from `libOpenIGTLink` to openigtlink-ng

openigtlink-ng ships `liboigtl.a` — a single static archive that
includes a **complete source-level drop-in shim for the upstream
`igtl::` API** alongside a new modern `oigtl::` API. If you have a
C++ codebase written against upstream's library, it compiles and
runs unchanged against ours.

Every example program shipped under
`reference-libs/openigtlink-upstream/Examples/` builds and interops
with our library as part of CI (see `compat_upstream_examples_interop`
in the test matrix). We run upstream's unmodified
`TrackerClient.cxx` against upstream's unmodified `ReceiveServer.cxx`,
both linked only against `oigtl::igtl_compat`, and they talk to each
other correctly.

## Table of contents

1. [TL;DR](#tldr)
2. [Quickstart: your first compat-mode program](#quickstart)
3. [Build recipes](#build-recipes)
4. [Restricting which peers may connect](#restrictions)
5. [Verifying you linked the right library](#verifying)
6. [Behavioral differences from upstream](#behavioral-differences)
7. [Troubleshooting](#troubleshooting)
8. [Performance expectations](#performance)
9. [Interoperability with upstream peers](#interop)
10. [Mixing compat and modern API](#mixing)
11. [API coverage reference](#api-coverage)
12. [FAQ](#faq)
13. [Getting help](#help)

---

## TL;DR <a name="tldr"></a>

Replace upstream's library on your link line:

```
-L/path/to/upstream/lib -lOpenIGTLink     # OLD
-L/path/to/oigtl/lib    -loigtl           # NEW
```

Nothing else changes. Every `#include "igtlTransformMessage.h"`,
`igtl::ClientSocket::New()`, `socket->Receive(...)` keeps working.

---

## Quickstart: your first compat-mode program <a name="quickstart"></a>

Copy-paste runnable. Requires a local `oigtl` install — see
[Build recipes](#build-recipes) below to produce one.

**`hello.cxx`** — sends a TRANSFORM, reads a STATUS reply:

```cpp
#include "igtlClientSocket.h"
#include "igtlMessageHeader.h"
#include "igtlStatusMessage.h"
#include "igtlTransformMessage.h"
#include <iostream>

int main() {
    auto sock = igtl::ClientSocket::New();
    if (sock->ConnectToServer("127.0.0.1", 18944) != 0) {
        std::cerr << "could not connect\n"; return 1;
    }

    // Send a TRANSFORM with a rotation + translation.
    auto tx = igtl::TransformMessage::New();
    tx->SetDeviceName("demo");
    tx->SetHeaderVersion(2);
    igtl::Matrix4x4 m;
    igtl::IdentityMatrix(m);
    m[0][3] = 10; m[1][3] = 20; m[2][3] = 30;
    tx->SetMatrix(m);
    tx->Pack();
    sock->Send(tx->GetPackPointer(), tx->GetPackSize());

    // Wait for a reply (one framed message).
    auto hdr = igtl::MessageHeader::New();
    hdr->InitPack();
    bool timed_out = false;
    auto n = sock->Receive(hdr->GetPackPointer(),
                           hdr->GetPackSize(), timed_out);
    if (timed_out || n != hdr->GetPackSize()) {
        std::cerr << "no reply\n"; return 2;
    }
    hdr->Unpack();

    if (std::string(hdr->GetDeviceType()) == "STATUS") {
        auto st = igtl::StatusMessage::New();
        st->SetMessageHeader(hdr);
        st->AllocatePack();
        sock->Receive(st->GetPackBodyPointer(),
                      st->GetPackBodySize(), timed_out);
        st->Unpack(1);
        std::cout << "got STATUS code=" << st->GetCode()
                  << " msg='" << st->GetStatusString() << "'\n";
    }
    sock->CloseSocket();
}
```

Build:

```bash
c++ -std=c++17 hello.cxx \
    $(pkg-config --cflags --libs oigtl) -o hello
./hello
```

That source file is valid `libOpenIGTLink` consumer code verbatim.
The only thing that changed is the library on the link line.

---

## Build recipes <a name="build-recipes"></a>

### CMake + find_package

```cmake
cmake_minimum_required(VERSION 3.16)
project(my_app CXX)

find_package(oigtl REQUIRED)

add_executable(my_app src/app.cxx)
target_link_libraries(my_app PRIVATE oigtl::igtl_compat)
```

Configure with `-DCMAKE_PREFIX_PATH=/path/to/oigtl/install`.

Granular imported targets (all resolve to the same `liboigtl.a`;
linker `--gc-sections` filters what your binary actually pulls in):

| Imported target           | What's inside                                |
|---------------------------|----------------------------------------------|
| `oigtl::oigtl_runtime`    | codec primitives (crc, header, metadata)     |
| `oigtl::oigtl_messages`   | typed codecs, all 20+ IGTL message types     |
| `oigtl::oigtl_transport`  | async `Connection`, framer, TCP              |
| `oigtl::oigtl_ergo`       | ergonomic `Client`/`Server`                  |
| `oigtl::igtl_compat`      | upstream `igtl::` shim (what you want)       |
| `oigtl::oigtl`            | umbrella (all of the above)                  |

### pkg-config

```bash
c++ -std=c++17 app.cxx $(pkg-config --cflags --libs oigtl) -o app
```

### Plain Makefile

```make
CXX      := c++
CXXFLAGS := -std=c++17 -O2
OIGTL    := /path/to/oigtl/install
INCLUDES := -I$(OIGTL)/include
LIBS     := -L$(OIGTL)/lib -loigtl -lpthread

my_app: src/app.cxx
	$(CXX) $(CXXFLAGS) $(INCLUDES) $< $(LIBS) -o $@
```

### autotools

In `configure.ac`:

```m4
PKG_CHECK_MODULES([OIGTL], [oigtl >= 0.1])
```

In `Makefile.am`:

```make
my_app_CPPFLAGS = $(OIGTL_CFLAGS)
my_app_LDADD    = $(OIGTL_LIBS)
```

### Swapping `libOpenIGTLink.a` in place (no build-system changes)

Configure the oigtl build with `-DOIGTL_DROP_IN_NAME=ON`:

```bash
cmake -S core-cpp -B build \
      -DCMAKE_BUILD_TYPE=Release \
      -DOIGTL_DROP_IN_NAME=ON \
      -DCMAKE_INSTALL_PREFIX=/your/prefix
cmake --build build -j
cmake --install build
# installs BOTH /your/prefix/lib/liboigtl.a
# AND      /your/prefix/lib/libOpenIGTLink.a
```

Then replace upstream's `libOpenIGTLink.a` with ours — same
filename, same header set (install also puts upstream-named
headers under `include/oigtl-compat/` for consumers writing bare
`#include "igtlFoo.h"`).

**Warning: don't leave both libraries on the link line.** See
[Troubleshooting](#troubleshooting) below — diagnostic one-liner
included.

### Slicer, PLUS, MITK — the big named consumers

These projects pull OpenIGTLink in as a superbuild submodule.
The cleanest swap is at the superbuild level:

**3D Slicer** (`Modules/Remote/OpenIGTLinkIF` pulls in OpenIGTLink
via `ExternalProject_Add`): point the superbuild at our install
prefix and let it find `oigtlConfig.cmake`. Tested manually on
Slicer 5.8; no patches required. A clean integration patch is
pending; track `#slicer-integration` in the issues tab.

**PLUS** (`PlusLib` links `OpenIGTLink::igtlioDevice`): same
approach — set `OpenIGTLink_DIR` in the superbuild cache to point
at `<oigtl-install>/lib/cmake/oigtl`. Caveat: PLUS uses
`igtl::Socket*` raw pointer returns from `ServerSocket::WaitForConnection`
in a couple of places; our shim returns `ClientSocket::Pointer`
(matching upstream's post-2020 API). If you're on an old PLUS
branch that predates the pointer change, cherry-pick that.

**MITK** (`MITK/Modules/OpenIGTLink`): straightforward —
MITK's `find_package(OpenIGTLink)` call resolves against
`oigtlConfig.cmake` when `CMAKE_PREFIX_PATH` includes our install.

---

## Restricting which peers may connect <a name="restrictions"></a>

All of the methods below are **opt-in**. A `ServerSocket` that
doesn't call any of them behaves exactly like upstream: accept
any peer, unlimited concurrency, no timeouts. Call them before
`CreateServer()` (restrictions active from the first connection)
or after it (active on the next `WaitForConnection`).

**Platform note:** Linux, macOS, and Windows are all supported.
`RestrictToLocalSubnet("Ethernet 2")` with a human-friendly,
possibly space-containing interface name works on Windows where
POSIX would use `"eth0"` / `"en0"` / `"tailscale0"`.

### Who can connect

```cpp
// Option A: only the computer running the server.
//
// Good for: single-machine setups, dev work, "I don't want this
// server reachable from the network at all."
srv->RestrictToThisMachineOnly();


// Option B: peers on the same IP subnet as one of my interfaces.
//
// Reads the server's own network interfaces (via getifaddrs on
// POSIX) and allows any peer on the same subnet. Single-NIC
// machine: accepts peers on your physical segment. Multi-NIC:
// accepts peers on any of your subnets (see Option C to narrow).
//
// Prints a line to stderr so operators can confirm what got
// allowed:
//     [igtl] RestrictToLocalSubnet: allowing 10.1.2.0/24 (eth0),
//            127.0.0.0/8 (lo loopback)
//
// Snapshot semantics: computed at call time. Plugging in a new
// NIC later does not widen the allow-list — restart the server
// to pick it up. This is deliberate; an unexpected USB ethernet
// shouldn't silently enlarge your perimeter.
srv->RestrictToLocalSubnet();


// Option C: restrict to a specific named network interface.
//
// Useful when a server machine has one NIC to the "device
// network" and another to the outside world, and you only want
// the device NIC.
//
// Interface names are platform-specific:
//     Linux:   "eth0", "enp3s0", "wlan0", "tailscale0"
//     macOS:   "en0", "utun4"
//     (Windows: coming soon)
srv->RestrictToLocalSubnet("eth0");


// Option D: add a specific host, CIDR, or address range.
//
// Each AllowPeer* call ADDS to the allow-list (union). Calling
// them doesn't remove anything, and calling none leaves the
// server accepting any peer.
//
// Accepted forms for AllowPeer():
//     "10.1.2.42"          single IP address (IPv4 or IPv6)
//     "10.1.2.0/24"        CIDR — "every address sharing this
//                          first-24-bit prefix" = 256 addresses.
//                          Prefer AllowPeerRange if this
//                          notation is unfamiliar.
//     "10.1.2.1-10.1.2.254" dash-separated range
//     "tracker.lab.local"   a hostname, resolved here via DNS
srv->AllowPeer("10.1.2.42");          // one machine
srv->AllowPeer("tracker.lab.local");  // resolved on add
srv->AllowPeer("10.1.2.0/24");        // a whole subnet

// Or, the explicit first/last form — no CIDR required:
srv->AllowPeerRange("10.1.2.1", "10.1.2.254");
```

### How much traffic to allow

```cpp
// Cap the number of peers connected at once.
// A new connection over the cap is accepted briefly then closed;
// your server sees no event for it. Default: unlimited.
srv->SetMaxSimultaneousClients(4);


// Hang up on peers that go silent. Measured from the last
// received byte. Default: 0 = never, keep connections open
// indefinitely.
//
// Typical setting for a live tracker stream at 50 Hz: a few
// seconds. For polling clients that might pause, larger.
srv->DisconnectIfSilentFor(std::chrono::seconds(15));


// Reject any single message whose body is bigger than `n` bytes.
// Check runs BEFORE body bytes are allocated, so this is also a
// pre-parse DoS defence. Default: 0 = no extra cap (body_size
// is only bounded by its 64-bit wire field).
//
// Rough IGTL body sizes for reference:
//     TRANSFORM / POSITION / STATUS   under 1 KB
//     POINT / TDATA with a few items  a few KB
//     IMAGE 256×256×50 int16          6.4 MB
//     full CT/MRI volume              100 MB+
srv->SetMaxMessageSizeBytes(256 * 1024 * 1024);  // 256 MB cap
```

### Worked example: typical lab setup

```cpp
auto srv = igtl::ServerSocket::New();
srv->CreateServer(18944);

// Imaging-suite devices live on 10.1.2.0/24.
srv->AllowPeerRange("10.1.2.1", "10.1.2.254");
srv->AllowPeer("127.0.0.1");        // local viewer on the same box

// Operational limits.
srv->SetMaxSimultaneousClients(4);                         // tracker + 2 imagers + viewer
srv->DisconnectIfSilentFor(std::chrono::seconds(15));      // catch dropped cables
srv->SetMaxMessageSizeBytes(256 * 1024 * 1024);            // 256 MB per msg

while (auto peer = srv->WaitForConnection(1000)) {
    // handle peer …
}
```

### What these restrictions do NOT replace

- **Authentication.** An IP-based allow-list trusts the network —
  any device on the allowed range is assumed legitimate. If you
  need "only *my* tracker, not any tracker on the same subnet,"
  you need a cryptographic identity mechanism. Our forthcoming
  Noise-protocol transport (see `../docs/history/transport_plan.md`) is the
  planned answer; until then, a WireGuard-based mesh VPN like
  Tailscale provides the same guarantee with an external daemon.

- **A properly-configured network firewall.** These knobs are
  defense-in-depth if a firewall rule goes wrong. They don't
  substitute for one.

- **Encryption.** Nothing in the restrictions encrypts the wire.
  IGTL bytes still travel in the clear on the network. Again:
  Noise, or a VPN.

## Verifying you linked the right library <a name="verifying"></a>

`nm` on your binary will show whose symbols got pulled in. Our
shim's objects are named `igtlMessageBase.cxx.o` and similar
(upstream's are named `igtlMessageBase.cpp.o`). Inspect:

```bash
nm your_app 2>/dev/null | grep -c 'igtlMessageBase\.cxx'  # ours
nm your_app 2>/dev/null | grep -c 'igtlMessageBase\.cpp'  # upstream
```

Exactly one of those should be nonzero. If both are: you're linking
against both libraries. See [Troubleshooting](#troubleshooting).

Ours also embeds a provenance string. You can grep for it:

```bash
strings your_app | grep oigtl | head
# oigtl compat shim — openigtlink-ng
# ...
```

If that string is present, you're on ours.

---

## Behavioral differences from upstream <a name="behavioral-differences"></a>

**The wire format is byte-exact.** A server built with oigtl
accepts connections from an upstream-built client and vice versa.
CI verifies this on every commit for 25+ message types.

The subtle differences are all in local behavior:

### Decode path is strictly friendlier

Upstream's `ColorTableMessage::Pack()` segfaults if you forgot to
call `AllocateTable()` first. Ours self-allocates on demand. Your
existing error-handling code still works — we just don't crash
where they did. If anywhere in your codebase you depended on an
upstream crash (yes, people do this — `try/catch` around a
segfault via a signal handler), you have a different problem.

### Refcounting is lock-free

Upstream's `LightObject` uses `SimpleFastMutexLock` around a plain
`int`. Ours uses `std::atomic<int>`. Same contract, same thread-safety
rules, slightly faster. No API change.

### Parsers are hardened

The shim's `UnpackContent()` paths are continuously fuzzed
(`fuzz_compat` under `BUILD_FUZZERS=ON`). Two DoS-class bugs
(4 GB OOM in `PolyData::unpack_cells`, uncaught `length_error` in
`Bind::unpack_impl`) were fixed during initial fuzzing.
Upstream's equivalent parsers still have both.

This means: a maliciously-crafted peer message that would crash
upstream does not crash us. We return a failure status and leave
the message in an unusable-but-safe state (the documented
contract).

### Sub-microsecond accessor timing

Our `SmartPointer<T>` atomic ops are lock-free; upstream's take a
mutex. If your code hot-loops on `GetPointer()` / `Register()` /
`UnRegister()` you'll see marginally lower latency.

### Threading rules are the same

Both libraries: an individual message object is NOT thread-safe.
The same socket must not have concurrent `Send()` and `Receive()`
calls. Our `igtl::Socket` inherits this contract.

### Features we explicitly dropped

- **`igtl::BindMessage::GetChildMessage(i, child)`** returns 0
  (not implemented). Use `GetChildBody(i)` to pull raw bytes,
  then construct the child yourself.
- **`TimeStamp` clock-frequency helpers.** Our `TimeStamp` wraps
  the IGTL 32.32 fixed-point uint64 and offers set/get by
  `(sec, nanosec)` or `double`. Enough for every known consumer.
- **VTK / ITK bridges.** Never shipped; never will. Use a
  separate VTK-↔-IGTL converter if you need one.
- **Video streaming codecs** (H.264/H.265/VP9 under upstream's
  `Source/VideoStreaming/`). Out of scope — we're a wire
  library, not a media stack. The OpenIGTLink-Video
  super-RFC's byte layout still works; you just need to supply
  your own encoder/decoder.

---

## Troubleshooting <a name="troubleshooting"></a>

### "`igtlXxxMessage.h`: file not found"

You have our `include/oigtl-compat/` install layout but your build
system isn't adding it to the include path. Either:
- use the CMake imported target (`target_link_libraries(…
  oigtl::igtl_compat)` — it handles `-I` automatically), or
- use `pkg-config --cflags oigtl` in your CXXFLAGS, or
- add `-I$(OIGTL_PREFIX)/include/oigtl-compat` manually.

### "No member named 'SensorMessage' in namespace 'igtl'"

Upstream gates v2-only message types behind a compile-time
`OpenIGTLink_PROTOCOL_VERSION >= 2` macro in `igtlConfigure.h`.
Our install advertises `OpenIGTLink_PROTOCOL_VERSION=3` via the
`oigtl::igtl_compat` target's compile definitions, so CMake
consumers get this automatically. If you're building without
CMake, add `-DOpenIGTLink_PROTOCOL_VERSION=3` to your CXXFLAGS.

### "duplicate symbol `igtl::TransformMessage::Pack()`"

You're linking against both our library and upstream's. Remove
one. Check your link line for both `-lOpenIGTLink` and `-loigtl`,
or check for stray `libOpenIGTLink.a` files in paths earlier than
ours.

If you built with `OIGTL_DROP_IN_NAME=ON` and then set
`CMAKE_PREFIX_PATH` to include both directories, CMake may resolve
`find_package(OpenIGTLink)` to upstream's config file. Set
`OpenIGTLink_DIR` explicitly to point at ours:

```cmake
set(OpenIGTLink_DIR "/your/prefix/lib/cmake/oigtl")
```

### "`ld: -lOpenIGTLink not found`" (after removing upstream)

You removed upstream's library but kept `-lOpenIGTLink` on the
link line. Build with `OIGTL_DROP_IN_NAME=ON` so the renamed
alias installs, or change the link line to `-loigtl`.

### "undefined reference to `asio::…`"

You're linking against an oigtl build that wasn't cleanly built.
Our `oigtl_transport` library is supposed to keep asio internal,
but if you somehow end up with asio headers leaking into your
TU, the link will fail on asio symbols we didn't ship. Fix: make
sure you're including `<oigtl/transport/...>` headers (which only
expose forward-declared types), not the asio headers directly.

If you built from source, verify `CMAKE_INSTALL_LIBDIR` matches
the `-L` on your link line.

### Interop works over loopback but not over LAN

Check MTU and firewalls. Our TCP backend sets `TCP_NODELAY` by
default (for the benefit of round-trip-sensitive workloads);
upstream does not. Some old kernel setups misbehave with NODELAY
on certain NICs. Try `socket->SetReceiveTimeout(1000)` to
disambiguate "slow" from "broken."

### Server consumes 100% CPU idling

You're calling `ServerSocket::WaitForConnection(0)` (zero
timeout = block indefinitely) from a tight loop, with a
`continue` on nullptr. Upstream has the same footgun; ours
doesn't help. Pass a positive timeout (`WaitForConnection(100)`)
and sleep between retries.

---

## Performance expectations <a name="performance"></a>

Measured on macOS arm64, Release mode, localhost TCP:

| Workload                              | Upstream | Ours    |
|---------------------------------------|---------:|--------:|
| TRANSFORM round-trip latency (p50)    | ~19 µs   | ~16 µs  |
| TRANSFORM throughput (one-way)        | 560k/s   | 490k/s  |
| TRANSFORM throughput (req/resp)       | 26k/s    | 31k/s   |
| `liboigtl.a` vs `libOpenIGTLink.a` size | 3.3 MB   | 1.6 MB  |
| Typical app binary (stripped)         | 583 KB   | 318 KB  |

Throughput for pure one-way TRANSFORM spray is slightly below
upstream because our async framer adds a thread hop. Round-trip
latency (more common in real IGTL use — tracker → viewer → ack)
is better because our direct-POSIX send/recv path avoids the hop.

If you need more one-way throughput and are OK using a new API,
use `oigtl::Client` from the modern layer — it amortizes framing
across batches.

---

## Interoperability with upstream peers <a name="interop"></a>

Byte-exact on the wire. Mix-and-match tested in CI:

| Sender          | Receiver        | Status |
|-----------------|-----------------|--------|
| upstream-built  | upstream-built  | ✓ baseline |
| upstream-built  | oigtl-built     | ✓ |
| oigtl-built     | upstream-built  | ✓ |
| oigtl-built     | oigtl-built     | ✓ |

For each of the 25+ parity-tested message types, CI compares
our packed wire bytes against upstream's character-for-character.
A single mismatch fails the build.

### Gotchas

- **CRC:** we compute CRC-64 ECMA-182 the same way upstream does,
  including the same treatment of 0-byte bodies (CRC == 0).
- **Header version:** we default new messages to `HeaderVersion=2`
  (v2 with extended header). Upstream defaults to 1. If you need
  v1 output for an ancient peer, call `SetHeaderVersion(1)`
  before `Pack()`.
- **TimeStamp:** both libraries use IGTL's 32.32 fixed-point uint64
  representation. Wall-clock translation is the same.
- **Metadata (v2+):** byte-exact, same IANA encoding table.

---

## Mixing compat and modern API <a name="mixing"></a>

You can use both in the same translation unit:

```cpp
#include "igtlTransformMessage.h"             // legacy
#include "oigtl/client.hpp"                   // modern
#include "oigtl/messages/transform.hpp"       // modern typed codec

int main() {
    // Legacy socket reading a TRANSFORM:
    auto sock = igtl::ClientSocket::New();
    sock->ConnectToServer("tracker.local", 18944);
    // ... use igtl:: API ...

    // Modern client for a different server:
    oigtl::Client client("imager.local", 18945);
    client.on<oigtl::messages::Transform>([](auto& m) {
        std::cout << "got matrix[0]=" << m.matrix[0] << "\n";
    });
    client.run();
}
```

Different namespaces, different headers, different instantiation.
No collisions, no link order surprises. Useful as a migration
path: drop in the compat shim first, then incrementally rewrite
call sites against the modern API.

---

## API coverage reference <a name="api-coverage"></a>

For a class-by-class breakdown of what's supported, partial, and
missing, see [`API_COVERAGE.md`](API_COVERAGE.md) in this
directory. Summary:

- **Message classes:** 20/20 data-carrying + 13/13 header-only. ✓
- **Sockets:** `Socket`, `ClientSocket`, `ServerSocket`. ✓
- **Object model:** `LightObject`, `Object`, `SmartPointer<T>`,
  `ObjectFactory` (trivial fallback). ✓
- **TimeStamp:** core set/get + wall-clock. ✓
- **Math helpers:** ✓
- **OSUtil:** ✓
- **VTK bridges, video codecs, RTP transport:** ✗ (out of scope)

---

## FAQ <a name="faq"></a>

**Q: Will this run on Windows?**
A: Yes. CI builds and tests `windows-latest` / MSVC 2022
alongside Linux (gcc + clang) and macOS on every push. The
transport layer uses a pimpl'd platform abstraction — POSIX
sockets on Linux/macOS, Winsock2 + iphlpapi on Windows — so
the same source code links on all three. The network
restrictions (`RestrictToLocalSubnet`, `AllowPeerRange`, etc.)
work identically on all platforms; the only Windows-specific
nuance is that interface names are human-friendly strings
("Ethernet 2", "Tailscale") rather than short POSIX names
("eth0", "tailscale0").

**Q: Does `std::atomic<int>` refcounting break my multithreaded
assumptions?**
A: No. Upstream's contract says an individual `LightObject`
instance is safe to share-ref from multiple threads (that's what
the mutex was for). We honor the same contract with lock-free ops.
Your code doesn't need to change.

**Q: Can I link `liboigtl.a` into a shared library?**
A: Yes. The archive is built with `-fPIC` on Linux/macOS. Wrap
it in your own `.so` / `.dylib` and re-export the `igtl::`
symbols however you want.

**Q: What's the ABI guarantee?**
A: Pre-1.0: none. Any minor version can change private members
of public classes (we use pimpl where it matters, but not
everywhere). Post-1.0: standard SemVer. Static linking is the
recommended deployment mode for now.

**Q: What about the Python and TypeScript cores?**
A: Separate archives, separate install trees — see
[`../core-py/`](../../core-py/) and
[`../core-ts/`](../../core-ts/). They share the same schema
and corpus; wire-level parity is enforced cross-language in CI.

**Q: Is this a fork of upstream?**
A: No. Clean-sheet C++17 implementation driven by the schemas in
[`../spec/`](../../spec/). The wire format matches upstream
because the spec is machine-derived from upstream's observable
behavior, not from its source.

**Q: Why is the merged archive smaller than upstream's despite
having *more* code (new API + compat shim)?**
A: Upstream includes VTK-style factory boilerplate, an internal
`itk::ExceptionObject` hierarchy, some dead code paths from the
VTK-heritage era, and the `Source/VideoStreaming/` encoder stubs.
We generate our codecs from schemas (denser), don't ship VTK,
and don't ship video codecs.

**Q: Can I contribute bug reports / fixes upstream instead?**
A: Upstream is in security-maintenance mode. Fixes there benefit
the existing ecosystem; fixes here benefit the successor. If it's
a wire-format question, fix the schema
(in [`../spec/schemas/`](../../spec/schemas/)) — that's the
single source of truth for both projects.

---

## Getting help <a name="help"></a>

If you hit a source-level incompatibility that isn't documented
here:

1. **Check [`API_COVERAGE.md`](API_COVERAGE.md)** for the class /
   method you're using.
2. **Run the verification one-liner** from the
   [Verifying](#verifying) section — half of all "my code broke"
   reports turn out to be a stale `libOpenIGTLink.a` lingering
   on the link path.
3. **Open an issue** at
   https://github.com/mhalle/openigtlink-ng/issues with:
   - The exact upstream class / method that doesn't work
   - A minimal repro (ideally under 30 lines)
   - Your build system (CMake / Make / autotools)
   - Your OS + compiler version

We triage compat bugs as regressions, not feature requests.
