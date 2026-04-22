# core-c fuzz targets

Opt-in libFuzzer targets under ASan + UBSan. Covers the four
field-shape categories used by the current generated C codec:

| Target           | Covers |
|------------------|--------|
| `fuzz_transform` | Fixed body + fixed-count primitive array |
| `fuzz_status`    | Primitives + null-padded fixed string + null-terminated trailing string |
| `fuzz_position`  | `body_size_set` validation + variable primitive array view |
| `fuzz_sensor`    | Sibling-count variable primitive array view |

Each target runs unpack → pack → byte-compare as one step, catching
both OOB / UB on adversarial input AND round-trip divergence in a
single 30-second pass.

## Local run

Requires Clang with libFuzzer runtime. Apple clang does **not**
ship libFuzzer; use a distro clang / Homebrew LLVM on macOS, any
`clang-15+` on Linux.

```bash
# Configure + build
CC=clang cmake -S core-c -B core-c/build-fuzz \
    -DOIGTL_C_BUILD_FUZZERS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build core-c/build-fuzz \
    --target fuzz_transform fuzz_status fuzz_position fuzz_sensor \
             fuzz_c_seed_corpus

# Run a single target with the staged seed corpus
core-c/build-fuzz/fuzz_transform \
    core-c/build-fuzz/seeds/transform \
    -max_total_time=30 -max_len=4096
```

Findings are written to `fuzz-artifact-<target>-<hash>` alongside
the interesting input. Drop the finding under
`core-c/fuzz/seeds/<target>/` (or append to the CI job's curated
set) once the underlying codec bug is fixed — that way the seed
corpus grows with every closed regression.

## Scope

This fuzzes the codec's responsibility surface only:

- OOB reads on malformed wire bytes
- Integer overflow on size math
- Round-trip divergence (`pack(unpack(x))` ≠ `x`)

It does **not** exercise caller-responsibility bugs: view-pointer
lifetime, aliasing, thread safety, misuse of return codes. Those
are documented contracts, not codec behavior.

See [`../docs/history/fuzz_plan_initial.md`](../docs/history/fuzz_plan_initial.md)
for the full original design rationale and future-round plans.
