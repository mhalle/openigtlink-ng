# API coverage: upstream `igtl::` vs. openigtlink-ng's shim

Class-by-class reference. This is the companion to
[`MIGRATION.md`](MIGRATION.md); go there first if you're reading
for onboarding.

**Coverage key:**

- **✓ Full** — source-compatible, method-for-method. Wire output
  matches upstream byte-for-byte.
- **◐ Partial** — compiles and covers every upstream example's
  usage pattern; some rarely-used methods are stubs. Listed
  explicitly below.
- **✗ Missing** — not present; consumer code referencing these
  will fail to compile. Listed with rationale.

---

## Message classes

### Data-carrying (20/20 full)

| Upstream class                         | Status | Notes |
|----------------------------------------|:------:|-------|
| `igtl::MessageBase`                    | ✓ | Full v1+v2+v3 framing. |
| `igtl::MessageHeader`                  | ✓ | |
| `igtl::TransformMessage`               | ✓ | Column-major 3×4 matrix layout (matches upstream). |
| `igtl::ImageMessage`                   | ✓ | 72-byte header + pixel body. All scalar types. |
| `igtl::StatusMessage`                  | ✓ | Full enum including `STATUS_PANICK_MODE`. |
| `igtl::StringMessage`                  | ✓ | UTF-8 + IANA encoding tag. |
| `igtl::PositionMessage`                | ✓ | All three pack-type variants (POSITION_ONLY, POSITION_QUATERNION3, POSITION_ALL). |
| `igtl::PointMessage`                   | ✓ | 136-byte records, full `PointElement` API. |
| `igtl::TrackingDataMessage`            | ✓ | 70-byte records, full `TrackingDataElement`. |
| `igtl::QuaternionTrackingDataMessage`  | ✓ | 50-byte records. |
| `igtl::TrajectoryMessage`              | ✓ | 150-byte records, `TYPE_ENTRY_TARGET` + alias. |
| `igtl::ImageMetaMessage`               | ✓ | 260-byte records, `ImageMetaElement`. |
| `igtl::LabelMetaMessage`               | ✓ | 116-byte records, `LabelMetaElement`. |
| `igtl::SensorMessage`                  | ✓ | Length + typed array. |
| `igtl::CapabilityMessage`              | ✓ | |
| `igtl::CommandMessage`                 | ✓ | Incl. `RTSCommandMessage`. |
| `igtl::QueryMessage`                   | ✓ | |
| `igtl::NDArrayMessage`                 | ✓ | `ArrayBase` + `Array<T>` template + `NDArrayMessage`. Type tags mirrored at both scopes for source compat. |
| `igtl::ColorTableMessage`              | ✓ | Auto-allocates on Pack() (upstream segfaults here). |
| `igtl::PolyDataMessage`                | ◐ | See [PolyDataMessage below](#polydata). |

#### <a name="polydata"></a>`PolyDataMessage`

Fully functional. One caveat: upstream has `m_Points`, `m_Polygons`,
etc. as `protected` members. Consumers who subclass and touch
those directly need an accessor. We expose public setters
(`SetPoints`, `SetPolygons`, `SetVerts`, `SetLines`, `SetStrips`,
`SetAttributes`); getters already exist on upstream.

Associated class `RTSPolyDataMessage` — ✓ full.

### Header-only (13/13 full)

| Upstream class                           | Status |
|------------------------------------------|:------:|
| `GetColorTableMessage`                   | ✓ |
| `GetImageMessage`                        | ✓ |
| `GetImageMetaMessage`                    | ✓ |
| `GetLabelMetaMessage`                    | ✓ |
| `GetPointMessage`                        | ✓ |
| `GetPolyDataMessage`                     | ✓ |
| `GetStatusMessage`                       | ✓ |
| `GetTrajectoryMessage`                   | ✓ |
| `GetTransformMessage`                    | ✓ |
| `StartPolyDataMessage`                   | ✓ |
| `StartQuaternionTrackingDataMessage`     | ✓ |
| `StartTrackingDataMessage`               | ✓ |
| `StopImageMessage`                       | ✓ |
| `StopPolyDataMessage`                    | ✓ |
| `StopQuaternionTrackingDataMessage`      | ✓ |
| `StopTrackingDataMessage`                | ✓ |
| `RTSQuaternionTrackingDataMessage`       | ✓ |
| `RTSTrackingDataMessage`                 | ✓ |

### Body-carrying query messages

| Upstream class                  | Status | Notes |
|---------------------------------|:------:|-------|
| `BindMessage`                   | ◐ | See below. |
| `GetBindMessage`                | ✓ | |
| `StartBindMessage`              | ✓ | 8-byte resolution body. |
| `StopBindMessage`               | ✓ | |
| `RTSBindMessage`                | ✓ | 1-byte status body. |
| `RTSCommandMessage`             | ✓ | |
| `RTSPolyDataMessage`            | ✓ | |

#### `BindMessage` partial

- **`GetChildMessage(unsigned int i, MessageBase* child)`** returns
  `0` (not implemented). Upstream's version re-parses the child
  body into the caller-supplied `child` — mechanically doable in
  our shim, just not ported because nothing we've observed in the
  wild calls it. Workaround: use `GetChildBody(i)` to pull raw
  bytes, then call `SetMessageHeader()` + `AllocatePack()` +
  `memcpy` + `Unpack()` on your own `TransformMessage::New()` (or
  whatever the child type is).
- Everything else — `AppendChildMessage`, `SetChildMessage`,
  `GetChildMessageType`, `Pack`, `Unpack` — works fully.

---

## Socket classes

| Upstream class        | Status | Notes |
|-----------------------|:------:|-------|
| `igtl::Socket`        | ✓ | Over our async `oigtl::transport::Connection`. Buffers framed messages on receive; serves byte-level reads from that buffer. |
| `igtl::ClientSocket`  | ✓ | `ConnectToServer(host, port, logErrorIfFailed)`. |
| `igtl::ServerSocket`  | ✓ | `CreateServer(port)` (0 = ephemeral), `WaitForConnection(msec)`, `GetServerPort()`. Also exposes opt-in access restrictions beyond the upstream API — see below. |

### Extensions beyond upstream (ServerSocket)

Not part of the upstream API — opt-in to tighten who can connect
and how much traffic the server accepts. POSIX only today;
Windows support coming. See [`MIGRATION.md` §Restrictions](MIGRATION.md#restrictions)
for researcher-facing prose + examples.

| Method | Purpose |
|---|---|
| `RestrictToThisMachineOnly()` | Loopback only |
| `RestrictToLocalSubnet()` | Same IP subnet as any local NIC (snapshot at call time) |
| `RestrictToLocalSubnet(ifname)` | Restrict to one named interface |
| `AllowPeer(ip_or_host)` | Add IP / CIDR / range / hostname to allow-list |
| `AllowPeerRange(first_ip, last_ip)` | Add inclusive address range (no CIDR required) |
| `SetMaxSimultaneousClients(n)` | Cap concurrent peers |
| `DisconnectIfSilentFor(seconds)` | Close idle peers |
| `SetMaxMessageSizeBytes(n)` | Reject oversized messages before body allocation |

### Socket method coverage

| Method                                      | Status |
|---------------------------------------------|:------:|
| `Send(const void*, igtlUint64)`             | ✓ |
| `Receive(void*, igtlUint64, bool&, int)`    | ✓ |
| `Skip(igtlUint64, int)`                     | ✓ |
| `SetTimeout(int)`                           | ✓ |
| `SetReceiveTimeout(int)`                    | ✓ |
| `SetSendTimeout(int)`                       | ✓ |
| `SetReceiveBlocking(int)`                   | ✓ |
| `SetSendBlocking(int)`                      | ✓ |
| `CloseSocket()`                             | ✓ |
| `GetConnected()`                            | ✓ |
| `GetSocketAddressAndPort(string&, int&)`    | ✓ |

---

## Object model

| Upstream class               | Status | Notes |
|------------------------------|:------:|-------|
| `igtl::LightObject`          | ✓ | Lock-free refcount (std::atomic<int>). |
| `igtl::Object`               | ✓ | |
| `igtl::SmartPointer<T>`      | ✓ | ITK-style intrusive pointer. |
| `igtl::ObjectFactory`        | ◐ | Header exists, trivial fallback (no dynamic registration). Upstream examples don't exercise runtime factory dispatch. |

---

## TimeStamp

| Upstream method                                     | Status | Notes |
|-----------------------------------------------------|:------:|-------|
| `SetTime(igtlUint32 sec, igtlUint32 nanosec)`       | ✓ | |
| `SetTime(double seconds)`                           | ✓ | |
| `SetTime(igtlUint64 stamp_32_32)` (our addition)    | ✓ | |
| `GetTime()` (populates from wall clock)             | ✓ | Uses `std::chrono::system_clock`. |
| `GetTimeStamp()` → double                           | ✓ | |
| `GetTimeStamp(igtlUint32* sec, igtlUint32* nano)`   | ✓ | |
| `GetSecond()` / `GetNanosecond()`                   | ✓ | |
| `GetFrequency()`, `SetFrequency(...)`               | ✗ | Clock-frequency helpers. Never observed in consumer code. Open an issue if you need them — trivial to add. |

---

## Math helpers (`igtl::` namespace scope)

| Upstream helper           | Status |
|---------------------------|:------:|
| `IdentityMatrix(Matrix4x4&)` | ✓ |
| `PrintMatrix(Matrix4x4&)`    | ✓ |
| `QuaternionToMatrix(float*, Matrix4x4&)` | ✓ |
| `MatrixToQuaternion(Matrix4x4&, float*)` | ✓ |
| `PrintVector3(float[3])`     | ✓ |
| `PrintVector3(float, float, float)` | ✓ |
| `PrintVector4(float[4])`     | ✓ |
| `PrintVector4(float, float, float, float)` | ✓ |

---

## OSUtil

| Upstream helper                     | Status |
|-------------------------------------|:------:|
| `igtl::Sleep(int milliseconds)`     | ✓ |
| `igtl::Strnlen(const char*, size_t)`| ✓ |

---

## Not ported (intentional)

| Upstream subsystem                  | Rationale |
|-------------------------------------|-----------|
| `Source/VideoStreaming/` (H.264/H.265/VP9) | Wire library, not a media stack. Byte layout still works — supply your own encoder/decoder. |
| VTK converter classes               | No VTK dependency in the shim. Use a standalone IGTL↔VTK bridge if needed. |
| ITK-style `ExceptionObject` class   | We use `std::exception` descendants. Catch-all-exceptions error-handling code still works. |
| `igtl::MessageFactory` dynamic registration beyond what's statically provided | Rarely used; open an issue if you need it. |
| `UDPClientSocket` / `UDPServerSocket` (upstream v3) | Not in examples we've seen. Open an issue if you need these — our transport layer has the primitives. |
| `MessageRTPWrapper` | Upstream RTP video integration. See video streaming note above. |
| `SessionManager` | Upstream v3 addition. Our transport is session-aware by design; if you need the exact `igtl::SessionManager` API surface, open an issue. |

---

## Known compile-time compatibility shims

These aren't APIs per se — they're the definitions our install
provides so upstream consumer code compiles without grep/sed.

- `OpenIGTLink_PROTOCOL_VERSION=3` compile definition (public)
- `IANA_TYPE_US_ASCII`, `IANA_TYPE_UTF_8` at global scope
- `igtlTypeMacro` / `igtlNewMacro` / `igtlSetMacro` / `igtlGetMacro`
- `MessageBase::UNPACK_{UNDEF,HEADER,BODY}` class-scope static
  constants (upstream exposes these both as enum and as class members)
- `HeaderOnlyMessageBase` as an alias for `MessageHeader`
- `IGTLCommon_EXPORT`, `IGTL_EXPORT` as empty macros (we build
  static-only)
- Transitively-included headers: `igtlMessageBase.h` includes
  `igtlTimeStamp.h`, same as upstream

---

## Coverage changelog

| Date       | Event |
|------------|-------|
| Phase 5    | Object model (LightObject / SmartPointer / macros) landed. |
| Phase 6a   | First 8 facades hand-written; codegen for header-only variants. |
| Phase 6b   | Remaining 11 data-carrying facades completed. 25 parity tests. |
| Phase 7    | Socket / ClientSocket / ServerSocket shim. |
| Phase 8    | Every upstream Example (`Tracker*.cxx`, `Receive*.cxx`, `Status*.cxx`) compiles + live interop. |
| Phase 9    | Install + `find_package(oigtl)` + merged `liboigtl.a`. |
| post-9     | `fuzz_compat` + two DoS fixes in `PolyData` and `Bind` parsers. |

---

## Found something missing?

If your code won't compile against `oigtl::igtl_compat`:

1. Check this doc to confirm the class/method isn't already listed
   as missing.
2. If it IS listed missing, the rationale field tells you whether
   it's a firm "no" (VTK bridges) or an "open an issue if you
   need it" (most partial stubs).
3. If it's NOT listed, it's a bug. File at
   https://github.com/mhalle/openigtlink-ng/issues with the
   upstream class/method name and a minimal repro.
