# Windows support — plan

Goal: `cmake -S core-cpp -B build -G "Visual Studio 17 2022"
&& cmake --build build` produces a clean build, all ctest passes,
CI enforces it on `windows-latest` with MSVC.

Covers the library, the compat shim (including the new network
restrictions), the examples/benchmarks, and the fuzz smoke (runtime
permitting — libFuzzer on MSVC is flaky; we'll skip if needed).

---

## Status (progress marker)

| Step | State |
|---|---|
| 1 — net_compat abstraction + POSIX extraction | ✅ done (commit 9558800) |
| 2 — Winsock backend (net_compat_winsock.cpp) | pending |
| 3 — Build-system Windows quirks | pending |
| 4 — Source-level Windows quirks | pending |
| 5 — Binary fixtures (.gitattributes) | pending |
| 6 — Portable interop test | pending |
| 7 — CI matrix entry | pending |
| 8 — Validation (external-consumer smoke) | pending |

Step 1 landed as a pure refactor with no behavior change; 36/36
ctest stayed green on the existing POSIX matrix. Remaining steps
below are the Windows-specific work.

---

## Scope + non-goals

### In scope

- Build + link cleanly on MSVC 2022 (and ClangCL if it falls out).
- All ctest passes on `windows-latest`.
- New network restrictions (`RestrictToLocalSubnet`,
  `AllowPeerRange`, `SetMaxSimultaneousClients`, etc.) work on
  Windows.
- `OIGTL_DROP_IN_NAME` produces `OpenIGTLink.lib` alongside
  `oigtl.lib`.
- CI matrix entry: `windows-latest` / MSVC 2022.
- Documentation updates (MIGRATION.md, API_COVERAGE.md,
  README.md) drop "POSIX only" caveats where resolved.

### Out of scope (tracked separately)

- MinGW / Cygwin builds. MSVC only.
- 32-bit Windows — SOCKET / size_t width assumptions need audit;
  deferred.
- Windows-specific performance tuning (overlapped I/O, IOCP
  direct use). asio uses IOCP under the hood already.
- DLL / shared-library export audit (`__declspec(dllexport)`).
  Static linking only.

---

## Platform abstraction — status

**Done.** `include/oigtl/transport/detail/net_compat.hpp` now
wraps every POSIX-ism the transport code used directly:

- Socket descriptor type (`socket_t`) that's `int` on POSIX and
  `std::uintptr_t` on Windows (matching the real `SOCKET` width
  on x64 — upstream's `int m_SocketDescriptor` is technically UB
  there, we don't carry that wart forward).
- Send/recv with SIGPIPE suppression baked in.
- `poll_one` for one-fd readiness checks.
- `set_recv_timeout`, `suppress_sigpipe`, `close_socket`.
- `parse_ip_literal`, `format_ip`, `resolve_hostname`,
  `enumerate_interfaces`, `ensure_initialized`.

`connection_tcp.cpp`, `policy.cpp`, `igtlServerSocket.cxx` are all
through the abstraction. No `#ifdef _WIN32` sprinkled in business
logic — just in the two backend files.

Step 2 = implement the same API against Winsock + iphlpapi.

---

## Surveyed gaps (beyond sockets)

Grepped for POSIX-isms, shell scripts, bash-in-CMake, binary
file handling, and MSVC-specific build-system knobs. The list
below is exhaustive from that audit.

### Build-system

1. **Merged-archive recipe is POSIX-`ar`-only.**
   `CMakeLists.txt:693-705` uses `ar -x` (extract) + `ar qcs`
   (quick-create-sorted). MSVC's `lib.exe` has neither flag.
   Fix: Windows branch uses `lib.exe /OUT:oigtl.lib in1.lib
   in2.lib ...` (one command, no extraction). Wrap in
   `if(MSVC) ... else() ... endif()`.

2. **`bash -c` in parity-test commands.**
   `CMakeLists.txt:400` runs
   `bash -c "cmp <(shim) <(upstream)"` for each parity case.
   Process-substitution is a bash-only feature. Fix:
   rewrite as a CMake `-P` helper script that writes each
   emitter's stdout to a temp file and invokes
   `${CMAKE_COMMAND} -E compare_files`.

3. **Bash interop test.** `upstream_examples_interop.sh`. Rewrite
   as a CMake `-P` script using `execute_process` for the two
   subprocesses and stdout capture.

4. **MSVC hygiene defines.** Must be set before any Windows
   header is transitively included:
   - `NOMINMAX` — prevents `<windows.h>` from defining `min` /
     `max` macros that clobber `std::min` / `std::max`.
   - `WIN32_LEAN_AND_MEAN` — excludes GDI/COM/etc; speeds
     compile, avoids symbol collisions.
   - `_WIN32_WINNT=0x0A00` — Windows 10 baseline. Buys us
     `WSAPoll`, `inet_ntop`, modern `GetAdaptersAddresses`.
   - `/Zc:__cplusplus` — MSVC reports `__cplusplus=199711L`
     by default for backward-compat. This flag makes the
     macro reflect actual standard mode. Critical for any
     consumer code that feature-detects via `__cplusplus`.

5. **Winsock link libraries.** `ws2_32.lib` + `iphlpapi.lib` on
   `oigtl_transport` when `_WIN32` is set.

6. **MAX_PATH / long-path support.** Default Windows path limit
   is 260 chars. Asio's FetchContent source tree is deep; a build
   dir like `C:\Users\...\Dropbox\...\build\_deps\asio-src\...`
   can blow the limit. Fix: enable long-path support via
   registry setting in CI runner (well-documented pattern) OR
   keep build dir paths short. Cheapest: document "use short
   build dir on Windows" and hope for the best; if it breaks,
   add the registry tweak to CI.

### Source-level

7. **MSVC `/W4 /WX` warning fallout.** Our current config is
   `-Wall -Wextra -Wpedantic -Werror` for GCC/Clang; MSVC
   equivalent is `/W4 /WX`. Different rulesets will fire
   different warnings:
   - C4244 (possible loss of data in narrowing conversion)
   - C4267 (size_t → int, common in loops)
   - C4996 (deprecated function, e.g. Winsock's `gethostbyname`
     — we don't use it but transitive includes may warn)
   - C4324 (struct padding for alignment)
   - C4146 (unary minus on unsigned)
   - C4702 (unreachable code)
   No code audit has been done. Budget 0.5-1 day for
   whack-a-mole.

8. **MSVC `/permissive-`.** Strict-conformance mode, default ON
   in MSVC 2022. Catches missing `typename`, two-phase lookup
   issues, implicit conversions clang-strict tolerates, etc.
   Our code passes clang's strict mode; probable but not
   certain this survives MSVC strict mode untouched.

9. **Binary-mode file I/O audit.**
   `tests/negative_corpus_test.cpp:67` opens JSON in text mode;
   line 116 opens `.bin` fixtures in binary mode. With
   `.gitattributes` forcing JSON to LF-only across all
   checkouts, text-mode JSON is safe even on Windows. No-op
   if item 11 (below) is done.

10. **Path separator in test helpers.**
    `tests/negative_corpus_test.cpp:128` uses
    `p.find_last_of('/')`. On Windows, paths may contain
    backslashes. Fix: `find_last_of("/\\")` or use
    `std::filesystem::path`.

### Binary fixtures + line endings

11. **`.gitattributes` file.** Without it, git-for-Windows with
    default `autocrlf=true` will CRLF-mangle text files on
    checkout, potentially corrupting JSON and — worse — the
    committed `.bin` fixtures under
    `spec/corpus/negative/content/`. Needs:
    ```
    * text=auto
    *.bin binary
    *.wire binary
    *.golden binary
    *.json -text
    *.h binary
    *.hpp binary
    ```
    Yes, `.h`/`.hpp` as binary — protects the committed
    upstream-fixture `.h` files under
    `corpus-tools/reference-libs/openigtlink-upstream/Testing/
    igtlutil/`. Our in-repo `.h`/`.hpp` files use LF too;
    keeping them binary means no accidental CRLF anywhere.

### CI / workflow

12. **Windows matrix entry.** `.github/workflows/ci.yml` gets a
    new job: `windows-latest` / MSVC 2022.

13. **Upstream clone on Windows.** The clone step already uses
    plain `git clone` + `git checkout` — portable across
    POSIX and PowerShell shells. No change needed except
    possibly quoting the pinned SHA argument.

14. **libFuzzer on Windows.** MSVC libFuzzer exists under
    ClangCL but is flaky on plain MSVC. Plan: guard fuzz
    targets behind `if(NOT WIN32)` or
    `if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")` in the fuzz
    target block; add only to the Linux CI. Windows fuzz
    coverage is a documented gap.

### Runtime-level concerns

15. **`WSAStartup` refcount.** The abstraction's
    `ensure_initialized()` needs to call `WSAStartup(MAKEWORD(2,2))`
    once per process and `WSACleanup()` at teardown. Pattern:
    `std::atomic<int>` refcount + `std::once_flag` for the
    initial call; cleanup via a global sentinel dtor when the
    refcount drops to 0. Upstream OpenIGTLink calls WSAStartup
    per-socket-create and never calls WSACleanup — a leak we
    don't carry forward.

16. **`char*` vs `void*` for Winsock setsockopt.** Winsock's
    `setsockopt` takes `const char*`, POSIX takes `const void*`.
    Already abstracted — all our setsockopt calls go through
    `net_compat` typed helpers.

17. **Error retrieval.** POSIX uses `errno`; Winsock uses
    `WSAGetLastError()`. Both backends report via exception
    messages, so the abstraction just internalises the
    difference.

---

## Revised execution order

### Step 2 — Winsock backend (~1.5 days, including MSVC warning audit)

Implement `src/transport/net_compat_winsock.cpp`:
- `ensure_initialized()` / `close_socket()` with WSA refcount
- `safe_send_all` / `safe_recv` via `::send` / `::recv`
  (Winsock send never sets SIGPIPE; WSAECONNRESET is the signal)
- `poll_one` via `WSAPoll` (Windows 10+)
- `set_recv_timeout` using `DWORD milliseconds` form
- `suppress_sigpipe` is a no-op on Windows
- `parse_ip_literal` / `format_ip` — `inet_pton` / `inet_ntop`
  from `<ws2tcpip.h>`
- `resolve_hostname` via `getaddrinfo` (same API as POSIX)
- `enumerate_interfaces` via `GetAdaptersAddresses` from
  `<iphlpapi.h>`

### Step 3 — Build-system Windows quirks (~0.5 day)

- CMake: Windows branch of the merged-archive recipe using
  `lib.exe /OUT:...`
- Rewrite `upstream_examples_interop.sh` + parity
  `bash -c "cmp"` as CMake `-P` helper scripts with
  `execute_process` + `compare_files`.
- Add `NOMINMAX`, `WIN32_LEAN_AND_MEAN`, `_WIN32_WINNT=0x0A00`,
  `/Zc:__cplusplus` as public compile definitions on
  `oigtl_transport` / `igtl_compat` for Windows.
- Link `ws2_32.lib` + `iphlpapi.lib` on `_WIN32`.

### Step 4 — Source-level Windows quirks (~0.5-1 day, unpredictable)

- MSVC `/W4 /WX` warning audit. Fix narrowing conversions,
  deprecated-API warnings.
- MSVC `/permissive-` compliance. Fix whatever trips.
- Path-separator fix in `negative_corpus_test.cpp`.

### Step 5 — Binary fixtures (~15 minutes)

- Add `.gitattributes` at repo root. Force LF for text,
  binary for fixtures, explicit for `.json`.
- `git add --renormalize .` to re-stage any files that need
  their CRLF stripped.

### Step 6 — CI matrix (~0.5 day)

- Add `windows-latest` / MSVC 2022 entry to `ci.yml`.
- Windows-specific upstream clone step (same git commands,
  PowerShell-friendly).
- Verify all jobs on the matrix pass on at least two runs.

### Step 7 — Documentation (~0.5 day)

- MIGRATION.md: drop "POSIX only today" caveat from
  §Restrictions. Add Windows-specific interface-name note
  (`"Ethernet"`, `"Tailscale"` with spaces).
- API_COVERAGE.md: same.
- core-cpp/README.md: add Windows to the tested-platforms list.

### Step 8 — Validation (~0.5 day)

- External-consumer smoke: tiny CMake project under Windows
  that does `find_package(oigtl)` and builds a legacy_app +
  modern_app (parallel to the `/tmp/oigtl_consumer` tests
  we ran for POSIX). Document exact command.
- Optional: `OIGTL_DROP_IN_NAME=ON` produces
  `OpenIGTLink.lib` + symlink parity.

**Total remaining: ~3-4 days.**

---

## Acceptance criteria

- [ ] CI matrix includes `windows-latest / MSVC 2022`, green.
- [ ] All ctest passes on Windows (at most 1-2 explicitly-skipped
      tests for upstream-libOpenIGTLink-dependent parity cases,
      documented).
- [ ] `compat_server_restrictions` passes on Windows — new
      network-policy features work.
- [ ] `cmake --install` produces `oigtl.lib` and optional
      `OpenIGTLink.lib` under Windows.
- [ ] External-consumer project on Windows builds via
      `find_package(oigtl)`.
- [ ] MIGRATION.md + API_COVERAGE.md drop POSIX-only caveats.
- [ ] `.gitattributes` in place; verified `.bin` fixtures survive
      a Windows checkout byte-identical.

---

## Risks + open questions

### MSVC warning/permissive fallout

Genuinely unpredictable. `/permissive-` and `/W4` catch a
different set of issues than GCC's `-Wall -Wextra`. Budget
for iteration; worst case, targeted `#pragma warning(disable:
NNNN)` in one or two places if a fix would be invasive.

### Interface names differ on Windows

POSIX: short (`eth0`, `en0`, `tailscale0`). Windows:
human-friendly with spaces (`"Ethernet 2"`, `"Tailscale"`).
Our `RestrictToLocalSubnet(const std::string&)` accepts
arbitrary strings, so nothing breaks. Docs need a platform-
specific note.

### asio version compatibility

We pin asio-1-30-2 via FetchContent. That version supports
MSVC 2019+ and Windows 10+. No bump needed unless forced.

### `/MD` vs `/MT` runtime

MSVC defaults to `/MD` (DLL runtime). Our `liboigtl.a` built
with `/MD` works for consumers also using `/MD`. If a
consumer needs `/MT`, they rebuild from source. No CMake
option for 0.2.0; add if requested.

### Windows Defender false positives

Some AV software flags libFuzzer-built binaries as suspicious.
If this bites CI, we may need to skip fuzz on Windows
entirely (already planned as "guarded behind `if(NOT WIN32)`"
in step 2).
