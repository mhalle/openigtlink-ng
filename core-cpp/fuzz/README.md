# core-cpp/fuzz — libFuzzer memory-safety gate

Part of the security harness — see
[`../../security/README.md`](../../security/README.md) for the
top-level fuzzer story. Two libFuzzer entry points run
under ASan + UBSan, built by an opt-in CMake option.

## Why C++ specifically

The differential oracle fuzzer (`security/README.md`, Phase 2)
already catches any *logical* divergence across the four codecs. It
does not catch **memory-safety bugs that happen to produce plausible
output** — e.g. a silent OOB read past a buffer end whose contents
decode to the same value another codec would have reported. Python
and TS are memory-safe by construction, so their in-process fuzzers
would only catch "unexpected exception class escapes the codec" —
already covered by the subprocess-based differential runner. libFuzzer
on C++ is the only tool that flags silent OOB / UAF / UMR / integer
overflow bugs in this codec set.

A fuzzer-found bug was landed with this harness (2026-04-17): the
C++ `parse_wire` bounds check computed `kHeaderSize + body_size` as
a `std::size_t` and wrapped when `body_size` was near `UINT64_MAX`,
letting malformed 58-byte headers slip past the bounds check and
drive `crc64` off the end of the buffer. A 58-byte input was enough
to trigger remote DoS. The fix is in `src/runtime/oracle.cpp`; the
regression is pinned via `spec/corpus/negative/framing_header/body_size_uint64_max.bin`.

## Targets

- **`fuzz_header`** — drives `unpack_header` + `verify_crc`. Fast;
  ~10M runs/minute on an M-series MacBook.
- **`fuzz_oracle`** — drives `verify_wire_bytes` with
  `check_crc=false` so libFuzzer can reach per-message content
  decode paths without having to brute-force CRC-64. The CRC path
  itself is exercised by `fuzz_header`.

## Local build (Linux or Homebrew LLVM on macOS)

Apple's system clang does **not** ship libFuzzer; Homebrew LLVM
does. On macOS, point CMake at `/opt/homebrew/opt/llvm/bin/clang++`
and add the Homebrew libc++ rpath so the sanitizer runtime finds
matching C++ symbols:

```bash
cmake -S core-cpp -B core-cpp/build-fuzz \
      -DBUILD_FUZZERS=ON \
      -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_CXX_COMPILER=/opt/homebrew/opt/llvm/bin/clang++ \
      -DCMAKE_EXE_LINKER_FLAGS="-L/opt/homebrew/opt/llvm/lib/c++ -Wl,-rpath,/opt/homebrew/opt/llvm/lib/c++"
cmake --build core-cpp/build-fuzz --target fuzz_header fuzz_oracle fuzz_seed_corpus
```

On Linux with `clang-15` from apt, no extra flags needed:

```bash
cmake -S core-cpp -B core-cpp/build-fuzz \
      -DBUILD_FUZZERS=ON \
      -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_CXX_COMPILER=clang++-15
cmake --build core-cpp/build-fuzz --target fuzz_header fuzz_oracle fuzz_seed_corpus
```

## Run

```bash
# 60 s smoke on seed corpus
core-cpp/build-fuzz/fuzz_header \
    core-cpp/build-fuzz/seeds/header \
    -max_total_time=60 -max_len=4096

core-cpp/build-fuzz/fuzz_oracle \
    core-cpp/build-fuzz/seeds/oracle \
    -max_total_time=60 -max_len=8192
```

A crash writes `./crash-<sha1>` in the cwd. Reproduce with
`fuzz_<target> ./crash-<sha1>`. Minimize with `-minimize_crash=1`.

## Seed corpus

The `fuzz_seed_corpus` CMake target stages seeds from the
on-disk negative corpus into `build-fuzz/seeds/{header,oracle}/`.
libFuzzer recurses, so dropping additional `.bin` files in under
those paths is enough to extend coverage. For a deeper run, also
point at upstream fixtures by exporting them:

```bash
# (planned) uv run oigtl-corpus corpus export-fuzz-seeds --out core-cpp/build-fuzz/seeds/oracle
```

## CI

`.github/workflows/ci.yml` has a `fuzz-smoke` job that runs each
target for 60 s plus a 50 000-iteration differential pass on every
PR. Any crash or ASan / UBSan report fails the job; artifacts
(crash repros + disagreement logs) are uploaded on failure.
