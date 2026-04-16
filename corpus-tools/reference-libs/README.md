# reference-libs

Local working copies of the OpenIGTLink reference implementations that
`corpus-tools/` builds against for corpus generation and differential
testing.

**Not checked into git.** The `.gitignore` at the repo root excludes all
subdirectory contents here except this README. Each reference library
is managed as its own clone, with the pinned commit recorded in
[`../../spec/corpus/ORACLE_VERSION.md`](../../spec/corpus/ORACLE_VERSION.md).

## Expected contents

```
reference-libs/
├── README.md                  this file
├── openigtlink-upstream/      upstream openigtlink/OpenIGTLink (read-only reference)
└── openigtlink-patched/       (future) hardened fork with security fixes
```

## Roles

**`openigtlink-upstream/`** — read-only reference pointing at the
canonical `openigtlink/OpenIGTLink` repository. Used to answer
correctness questions ("what does the deployed library actually do
for input X?"). This is the **compat reference**: deployed peers in
the field run this code (or closely related releases of it).

**`openigtlink-patched/`** — reserved slot for the hardened fork
once its uncommitted changes are committed and the branch is a
reproducible pin. Will serve as the **positive corpus oracle** once
added. See [`../../spec/corpus/ORACLE_VERSION.md`](../../spec/corpus/ORACLE_VERSION.md)
for the plan.

## Session policy (current)

Until the patched library is pinned and available:

- Positive-corpus oracle: **not yet generated**. Requires patched library.
- Correctness questions during development: answered against the
  **upstream** library only.
- If upstream's behavior looks wrong, flag inline and record in
  `../../spec/protocol/v3.md` as an upstream-correctness finding.
  Do not silently work around upstream bugs.

## Re-cloning

Each subdirectory is a fresh clone from its canonical remote. Anyone
can regenerate this directory by running the setup commands documented
in [`ORACLE_VERSION.md`](../../spec/corpus/ORACLE_VERSION.md) — the
pinned SHA in that file is the authoritative reference.
