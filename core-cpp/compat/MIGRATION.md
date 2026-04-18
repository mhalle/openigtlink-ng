# Migrating from `libOpenIGTLink` to openigtlink-ng

openigtlink-ng ships `liboigtl.a` — a single static archive containing
both a modern `oigtl::` C++ API and a **complete source-level drop-in
shim for the upstream `igtl::` API**. If you have C++ code written
against `igtl::TransformMessage`, `igtl::ClientSocket`,
`igtl::ServerSocket`, and friends, it compiles and runs unchanged
against our library.

Every example program shipped under
`reference-libs/openigtlink-upstream/Examples/` builds and interops
with our library as part of CI (see `compat_upstream_examples_interop`
in the test matrix).

## TL;DR

Replace upstream's library on your link line. That's it.

```
-L/path/to/upstream/lib -lOpenIGTLink
```

becomes

```
-L/path/to/openigtlink-ng/install/lib -loigtl
```

Nothing else changes. Your `#include "igtlTransformMessage.h"`,
`igtl::ClientSocket::New()`, `socket->Receive(...)` — all of it —
keeps working.

## CMake consumers

```cmake
find_package(oigtl REQUIRED)
target_link_libraries(your_app PRIVATE oigtl::igtl_compat)
```

If you also want to reach for our modern API in new code:

```cmake
target_link_libraries(your_app PRIVATE oigtl::oigtl)
```

`oigtl::oigtl` is the umbrella that covers everything
(`runtime + messages + transport + ergo + igtl_compat`). Fine-grained
targets are available if you want only a layer:

| Imported target           | What's inside                                |
|---------------------------|----------------------------------------------|
| `oigtl::oigtl_runtime`    | codec primitives (crc, header, metadata)     |
| `oigtl::oigtl_messages`   | typed codecs, all 20+ IGTL message types     |
| `oigtl::oigtl_transport`  | async `Connection`, framer, TCP              |
| `oigtl::oigtl_ergo`       | ergonomic `Client`/`Server`                  |
| `oigtl::igtl_compat`      | upstream `igtl::` shim                       |
| `oigtl::oigtl`            | umbrella (all of the above)                  |

All of them resolve to the same `liboigtl.a` on disk; the linker's
`--gc-sections` pass drops code your binary doesn't reach.

## autotools / Makefile consumers

A `pkg-config` file is installed:

```
$(pkg-config --cflags oigtl)
$(pkg-config --libs   oigtl)
```

Or link directly: `-loigtl -lpthread`.

## "I really need the file to be called `libOpenIGTLink.a`"

Configure with `-DOIGTL_DROP_IN_NAME=ON`. An additional copy of the
merged archive gets installed under that name, so you can swap the
file in place of upstream's without touching a Makefile.

```
cmake -S . -B build -DOIGTL_DROP_IN_NAME=ON
cmake --build build
cmake --install build
# now have both /lib/liboigtl.a AND /lib/libOpenIGTLink.a
```

**Do not link both libraries in the same binary.** Our shim defines
the `igtl::` namespace — linking against both produces duplicate
symbols (ODR violation). Pick one.

## What the shim provides

Every upstream class that matters for message I/O:

- `igtl::MessageBase`, `igtl::MessageHeader`
- `igtl::TransformMessage`, `igtl::ImageMessage`, `igtl::StatusMessage`,
  `igtl::StringMessage`, `igtl::PositionMessage`, `igtl::PointMessage`,
  `igtl::TrackingDataMessage`, `igtl::QuaternionTrackingDataMessage`,
  `igtl::TrajectoryMessage`, `igtl::ImageMetaMessage`,
  `igtl::LabelMetaMessage`, `igtl::SensorMessage`,
  `igtl::CapabilityMessage`, `igtl::CommandMessage`,
  `igtl::QueryMessage`, `igtl::NDArrayMessage`,
  `igtl::ColorTableMessage`, `igtl::PolyDataMessage`,
  `igtl::BindMessage` (+ `GetBindMessage`, `StartBindMessage`,
  `StopBindMessage`, `RTSBindMessage`)
- Header-only variants: `GET_*`, `STT_*`, `STP_*`, `RTS_*`
- `igtl::Socket`, `igtl::ClientSocket`, `igtl::ServerSocket`
- `igtl::TimeStamp`, `igtl::LightObject`, `igtl::Object`,
  `igtl::SmartPointer<T>`
- Math helpers: `IdentityMatrix`, `PrintMatrix`, `QuaternionToMatrix`,
  `MatrixToQuaternion`, `PrintVector3`, `PrintVector4`
- OS helpers: `igtl::Sleep`, `igtl::Strnlen`

The wire format is byte-exact compatible with upstream. A server
built with openigtlink-ng accepts connections from an
upstream-built client and vice versa.

## Intentional differences from upstream

**Strictly friendlier than upstream on the decode path.** We
fixed a class of null-pointer / short-body crashes that upstream
segfaults on. Your old error-handling code still works — we just
don't crash where they did. If you depend on a particular
upstream crash, you have a different problem.

**Lock-free refcounting.** Upstream's `LightObject` uses
`SimpleFastMutexLock` around a plain `int`. Ours uses
`std::atomic<int>`. Same contract, slightly faster, thread-safe
under the same rules. No API change.

**New: hardened parsers.** The shim's `UnpackContent()` paths
are continuously fuzzed (`fuzz_compat` under `BUILD_FUZZERS=ON`).
Two DoS-class bugs in `PolyData` and `Bind` parsers were fixed
during this process; upstream's equivalent code still has them.

## What the shim *doesn't* provide

- `igtl::BindMessage::GetChildMessage(i, child)` — returns `0`
  (not implemented). Use the side accessor `GetChildBody(i)` to
  pull raw bytes, then construct the child message yourself.
- A few of upstream's more featureful `TimeStamp` conversion
  helpers (clock frequency, etc). Our `TimeStamp` wraps the
  IGTL 32.32 fixed-point uint64 and offers set/get by
  `(sec, nanosec)` or `double`. That's what every example needs.
- VTK / ITK bridges. Never shipped; never will.
- Video streaming codecs (H.264/H.265/VP9 under
  upstream's `Source/VideoStreaming/`). Out of scope for the
  port — we're a wire library, not a media stack.

## Coexisting with new code

You can mix `igtl::` and `oigtl::` in the same translation unit.
Different namespaces, different headers, different instantiation:

```cpp
#include "igtlTransformMessage.h"           // legacy
#include "oigtl/ergo/client.hpp"            // modern

int main() {
    // Legacy wire path:
    auto msg = igtl::TransformMessage::New();
    // ... pack + send via igtl::ClientSocket

    // Modern wire path:
    oigtl::Client client("tracker.local", 18944);
    client.run();
}
```

No symbol collisions, no header collisions, no link order
gotchas. Use whichever fits the code you're writing.

## Getting help

If you hit a source-level incompatibility that isn't documented
here, open an issue with the file + line from upstream's API that
doesn't work — and please include a minimal repro. We triage
those as bug reports, not feature requests.
