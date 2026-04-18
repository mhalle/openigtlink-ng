# Windows support — plan

Goal: `cmake -S core-cpp -B build -G "Visual Studio 17 2022"
&& cmake --build build` produces a clean build, all ctest passes,
CI enforces it on `windows-latest` with MSVC.

Covers the library, the compat shim (including the new network
restrictions), the examples/benchmarks, and the fuzz smoke
(runtime permitting — libFuzzer on MSVC is flaky; ClangCL may
replace it).

---

## Scope + non-goals

### In scope for this phase

- Build + link cleanly on MSVC 2022 and ClangCL.
- All 36 current ctest passes on `windows-latest`.
- New network restrictions (Phase post-0.1.0 — `RestrictToLocalSubnet`
  etc.) work on Windows.
- `OIGTL_DROP_IN_NAME` produces `OpenIGTLink.lib` alongside
  `oigtl.lib`.
- CI matrix entry: `windows-latest` / MSVC 2022.
- Documentation updates (MIGRATION.md, API_COVERAGE.md,
  README.md) drop "POSIX only" caveats where resolved.

### Out of scope (tracked separately)

- MinGW / Cygwin builds. MSVC only; downstream is welcome to
  port themselves once the POSIX-abstraction headers exist.
- 32-bit Windows. `size_t` and `SOCKET` width assumptions need
  audit; deferred.
- Windows-specific performance tuning (overlapped I/O, IOCP
  direct use). asio already uses IOCP under the hood; we're
  not hand-optimising.
- libFuzzer on Windows. If it doesn't compile on the MSVC/ClangCL
  matrix entry, the fuzz target is POSIX-only with a clear
  `#if defined(_WIN32)` guard and a CI skip — not blocking.

---

## Survey of POSIX-isms in the tree today

Grepped for POSIX headers, socket primitives, and existing
`#ifdef _WIN32` blocks. Results:

### Already-guarded (safe)

| File | Notes |
|---|---|
| `tests/oracle_parity_test.cpp` | Has `_popen` fallback. |
| `src/transport/policy.cpp` | Has `#error` stub for Windows. |
| `src/transport/net_iface.cpp` | Has `#error` stub for Windows. |
| `compat/src/igtlServerSocket.cxx` | `#ifndef _WIN32` around `AllowPeer` hostname resolution. |

### POSIX-only, needs work

| File | POSIX use | Windows equivalent |
|---|---|---|
| `src/transport/connection_tcp.cpp` | `::send`, `::recv` with `MSG_NOSIGNAL` (line 325+), `::poll` for EAGAIN recovery (line 250+), `SO_NOSIGPIPE`, `struct timeval` for `SO_RCVTIMEO`, `native_handle()` returns `int` | `WSASend`/`WSARecv`, `WSAPoll`, Winsock doesn't generate SIGPIPE at all (no workaround needed), `DWORD milliseconds` for `SO_RCVTIMEO`, `native_handle()` returns `SOCKET` |
| `compat/tests/upstream_examples_interop.sh` | Bash script as a ctest | Need a cross-platform equivalent (CMake `-P` script, or C++ test runner); see §Strategy below |
| `compat/tests/emitter_upstream.cxx`, `emitter_shim.cxx` | Links upstream's `libOpenIGTLink.a` for byte-parity. Upstream also has the same POSIX-isms. | These tests are gated by `IGTL_UPSTREAM_BUILD` existing. On Windows CI, don't build them. Skip. |
| `src/client.cpp`, `src/server.cpp` | One `::close` reference (false positive — it's `Client::close`, a member function). | No action. |
| `tests/negative_corpus_test.cpp` | Reads fixture files; uses `popen` for hex-decode of negative corpus. | Replace with in-process hex decode (cleanest) or add `_popen` branch. Simple. |

### Build-system

| Concern | Status |
|---|---|
| CMake | Already multi-generator; asio FetchContent works on MSVC. |
| Thread / atomic / chrono | C++17 standard — portable. |
| `<netinet/in.h>` type aliases (`in_addr`, `sockaddr_in`) | Need `<ws2tcpip.h>` with `WSAStartup` on Windows. Abstract behind a new header. |
| `<sys/time.h>` | Only used for `struct timeval`; on Windows, `<winsock2.h>` defines it. Abstract. |
| WSAStartup / WSACleanup | Must be called exactly once per process. Currently not called at all (asio handles it internally), but our DIRECT POSIX calls in `connection_tcp.cpp` bypass asio's init. Need a sentinel singleton at library init time. |
| igtl_compat export for DLL builds | Our install ships `.a` only today. For Windows `.lib` is the same model (static). If a consumer wants `.dll`/`.so` we'll need to audit `IGTLCommon_EXPORT`. Deferred. |

---

## The key abstraction: `oigtl/runtime/net_compat.hpp`

One private header with a small POSIX-like API, implemented twice
(POSIX / Winsock). All the `connection_tcp.cpp` direct socket code
goes through this. Public API stays unchanged.

```cpp
// Not installed; purely internal.
namespace oigtl::runtime::net_compat {

// Native socket descriptor type.
#ifdef _WIN32
    using socket_t = std::uintptr_t;    // SOCKET
    inline constexpr socket_t invalid_socket = ~socket_t{0};  // INVALID_SOCKET
#else
    using socket_t = int;
    inline constexpr socket_t invalid_socket = -1;
#endif

// One-time process init / teardown. Reference-counted.
void ensure_initialized();

// Send/recv with SIGPIPE-safe semantics. Returns bytes sent /
// received, or -1 on error (errno / WSAGetLastError set).
std::ptrdiff_t safe_send(socket_t s, const void* buf,
                         std::size_t len);
std::ptrdiff_t safe_recv(socket_t s, void* buf, std::size_t len);

// poll() wrapper returning events. Negative timeout == block.
// One-fd convenience form — all our call sites are one-fd.
struct PollResult { bool readable; bool writable; bool error; };
PollResult poll_one(socket_t s, int timeout_ms,
                    bool want_read, bool want_write);

// Set SO_RCVTIMEO portably. `ms == 0` means "no timeout".
void set_recv_timeout(socket_t s, std::chrono::milliseconds ms);

// Close-on-error for a socket. Doesn't throw.
void close_socket(socket_t s);

// Translate the last socket error (errno / WSAGetLastError) into
// a std::string. Used only for error-path diagnostics.
std::string last_error_string();

// Query local interface table. Replaces the current POSIX-only
// implementation in net_iface.cpp. POSIX: getifaddrs. Windows:
// GetAdaptersAddresses.
std::vector<InterfaceAddress> enumerate_interfaces();

}  // namespace oigtl::runtime::net_compat
```

### Why this header rather than peppering `#ifdef` everywhere

- One place to read when porting to a new platform.
- `connection_tcp.cpp` stays readable — stays focused on the
  asio / framer / policy dance instead of branching on OS.
- The compile-time fallback for Windows in `policy.cpp` today
  (`#error`) gets replaced with a real backend.
- Unit-testable: we can write a tiny POSIX-vs-Windows smoke that
  each backend produces the same behaviour for simple inputs.

---

## Execution order (proposed)

### Step 1: Plumbing (~1 day)

1. Create `net_compat.hpp` + POSIX backend in `net_compat_posix.cpp`.
   Refactor `connection_tcp.cpp` to use it; confirm macOS + Linux
   ctest still 36/36 green. No behaviour change.
2. Merge `net_iface.cpp`'s logic into `enumerate_interfaces()` in
   the new module. Keep the same API in `policy.hpp` — just the
   backing implementation moves.

### Step 2: Windows backend (~1 day)

3. `net_compat_winsock.cpp`: implement each function. Handles
   `WSAStartup` via a local `std::once_flag`.
4. `igtlServerSocket.cxx`'s `AllowPeer(hostname)` — use
   `getaddrinfo` (available on Windows — same API). Drop the
   `#ifndef _WIN32` guard once includes are right.
5. `tests/negative_corpus_test.cpp`: move hex decode in-process
   (no more popen dependency). Drop POSIX-ism.

### Step 3: CMake + Winsock quirks (~half day)

6. Link `ws2_32.lib` and `iphlpapi.lib` on `_WIN32`.
7. `NOMINMAX` / `WIN32_LEAN_AND_MEAN` on Windows builds (prevent
   `<windows.h>` from polluting `min`/`max`).
8. `target_compile_definitions(oigtl_transport PRIVATE
   _WIN32_WINNT=0x0A00)` — require Windows 10 (covers
   `WSAPoll`, `inet_ntop`, `GetAdaptersAddresses` with modern
   flags).

### Step 4: Interop test portability (~half day)

9. Replace `upstream_examples_interop.sh` with a CMake `-P`
   script, or rewrite as a C++ test driver. CMake `-P` is
   simplest — it's already in the build dependency graph.
   Spawn the two executables via `execute_process`; gate on
   stdout count.

### Step 5: CI matrix (~quarter day)

10. Add `windows-latest / MSVC 2022` to `.github/workflows/ci.yml`.
11. Add Windows-specific upstream-clone step (git clone is the
    same; PowerShell path separators differ).
12. Drop "POSIX only" caveats from MIGRATION.md + API_COVERAGE.md
    for the restrictions section.

### Step 6: Bonus — validate the whole story (~half day)

13. Build the 0.2.0 merged archive on Windows as
    `oigtl.lib` + optional `OpenIGTLink.lib` (the Windows
    equivalent of the drop-in filename alias).
14. Smoke test an external consumer project (equivalent of
    `/tmp/oigtl_consumer` under bash, but a tiny CMake project
    under Windows).

**Total estimated work: ~3-4 days of focused effort.**

---

## Risks + open questions

### libFuzzer on Windows

MSVC's libFuzzer support exists under ClangCL but is less mature
than Linux/macOS. If the fuzz-smoke job fails to compile, I'll
guard it behind `if(NOT WIN32 OR USE_CLANG_CL)` and accept that
Windows fuzz coverage is a gap. Not blocking.

### `SOCKET` vs `int` for `native_handle()`

asio's `basic_socket::native_handle()` returns `SOCKET` on Windows
and `int` on POSIX. Our `net_compat::socket_t` typedef covers
this — but any code path that passes a native handle across
functions must use `socket_t`, not `int`. Audit tbd during step 1.

### No SIGPIPE on Windows

Winsock `send` on a closed peer returns `WSAECONNRESET`; there's
no signal to suppress. Our `safe_send` just calls `send` and
returns the error — no equivalent of `MSG_NOSIGNAL` or
`SO_NOSIGPIPE` needed. Good; one fewer thing to translate.

### Interface names

POSIX interface names are short (`eth0`, `en0`, `tailscale0`).
Windows uses human-friendly names with spaces (`"Ethernet 2"`,
`"Tailscale"`). Our `RestrictToLocalSubnet(const std::string&)`
accepts arbitrary strings, so nothing breaks — but docs should
note the per-platform naming convention clearly.

### asio version compatibility

We pin asio-1-30-2 via FetchContent. That version supports MSVC
2019+ and Windows 10+. No action needed unless we want to bump
the pin.

### Static runtime vs DLL runtime

MSVC defaults to `/MD` (DLL runtime). Our `liboigtl.a` (static)
built with `/MD` works fine for consumers also using `/MD`. If a
consumer needs `/MT`, they rebuild from source. We could offer
both flavours via a CMake option, but I don't think it's warranted
for 0.2.0.

---

## Acceptance criteria

At the end of this work:

- [ ] CI matrix includes `windows-latest / MSVC 2022`, green.
- [ ] Optional: `windows-latest / ClangCL`, green.
- [ ] 36/36 ctest passes on Windows (or explicitly skipped tests
      are documented; expect at most 1-2 gated on upstream libs
      not being built on Windows CI).
- [ ] New network restrictions work (`compat_server_restrictions`
      passes on Windows).
- [ ] MIGRATION.md + API_COVERAGE.md drop POSIX-only caveats.
- [ ] `cmake --install` produces `oigtl.lib` + optional
      `OpenIGTLink.lib`.
- [ ] A tiny external-consumer project builds and links
      against our install on Windows via `find_package(oigtl)`.
