# Host Migration and Suite Recovery — 2026-11-25

## Provenance and status

This record exists because the working tree moved to a different machine by **file copy**, not by a
fresh checkout. It records what the copy preserved, what it could not preserve, the one product
defect a clean rebuild exposed, and the state the next pass starts from.

Written while the repair was in progress: the findings, the root-cause chain and the planned
verification are recorded first, and the verification table is filled from the gates actually run
(see **Verification**). Nothing in this session was recorded to version control.

**Environment of record:** Gentoo Linux, `gcc 14.3.1_p20250801`, `ld 2.45.1`, `glibc 2.43`,
Intel i5-4200M (Haswell, AVX2, **no AVX-512**), 4 cores, tree on local ext4 (`/dev/sdb`).

## What the file copy preserved

- **History, intact.** The copy is a strict descendant of the older tree at
  `/bigdisk/programming/C/terminal-style-game`, whose head `bd2c772` is the recorded upstream
  baseline; the copy is **39 commits ahead** of it with no divergence, and the object store
  reported no faults.
- **Every tracked file.** The tracked-file list differs only by **additions**: `src/glyph_*`,
  `src/font_*`, `src/sha256`, `src/mapgen*`, `src/platform_memory`, nine test sources, three tools,
  five documents. No pre-existing file was lost. `LICENSE`, `config.ini` and `user.ini` are
  byte-identical to the older tree; they differed only in mtime and mode.
- **Nothing untracked but ignorable.** The only non-tracked entries are the six expected ignored
  paths (`build/`, `vendor/`, `.cache/`, `.clangd`, `compile_commands.json`,
  `docs/smc_benchmark_logs/`).

## What the file copy could not preserve

| Artifact | Effect | Disposition |
|---|---|---|
| `build/*` (92 binaries) | compiled where `gcc` defaults to a wider ISA; every binary carries `x86 ISA needed: … x86-64-v3, x86-64-v4` and the host loader refuses them: `CPU ISA level is lower than required` | **Discarded and rebuilt here.** Not corruption — an artifact of the source host's compiler defaults. The Makefile pins no `-march`, so nothing in the project asked for v4. |
| `compile_commands.json` | 110 references to `/nas/contents/Projects/...`, zero to this tree | Regenerated for this host (see **Editor database**). |
| `.cache/clangd` | index of the previous host's paths | Stale by definition; left for the editor to replace. |
| file/dir modes | 1,112 directories arrived as `drwxrwx---` (the older tree has none); source files lost stray `+x` | Cosmetic. The source-file change is an improvement; the group-writable directories are noted, not corrected. |

The vendored libraries are **not** implicated: `vendor/dist/lib64/*.so*` carry no ISA-level note,
so only the project's own binaries failed to load.

**Consequence worth stating:** every gate result reported before this session was produced on the
source host. On this machine, none of those binaries had ever executed.

## The defect the clean rebuild exposed

A full clean rebuild made one runner fail:

```
./build/test-r9-optical-semantics
[  ERROR   ] --- 2097152 != 131072
[   LINE   ] --- tests/test_r9_optical_semantics.c:100
[  FAILED  ] test_exact_memory_costs_and_break_even
```

**Cause.** Commit `53b67e9` (*"raise the scene size ceiling to 2048x1024"*, 2026-11-07) moved
`SCENE_MAX_WIDTH 512 → 2048` and `SCENE_MAX_HEIGHT 256 → 1024` — 131,072 cells → 2,097,152, exactly
16× — and did not update the test's hand-written expectations. The test source has not been touched
since `48cd73f`. It asserts the *outputs of a computed report*, so only the expectations were stale;
`r9_optical_memory_report` derives everything from its inputs and needed no change.

**Why it hid for eighteen days.** The failing runner's rule lists its own `.c` files and no headers:

```make
$(TEST_R9_OPTICAL_SEMANTICS_RUNNER): tests/test_r9_optical_semantics.c src/r9_optical_semantics.c $(SRC_CHECKED_SIZE) | dirs
```

`53b67e9` changed headers only, so this runner was never rebuilt and its stale binary kept asserting
against the old constants. This is the same stale-binary class `02e7ac9` documents ("rebuilt only 18
of ~90 runners … the pass was incomplete evidence"); `01c0bc6` later attached `.headers.stamp` to
every `%_RUNNER`/`%_TOOL` target and closed the mechanism. `b632998` (head) correctly declined to
claim the aggregate. No one had run the suite from clean in between.

**Two recorded claims therefore cannot be true, and are corrected here:**

| Record | Claim | Why it cannot hold |
|---|---|---|
| `53b67e9` message | "the whole suite is green at the new limits (81 runners / 927 tests, 0 failures)" | the runner was in the `test` recipe and asserted `131072` |
| `02e7ac9` message and `V1_5_CELL_IDENTITY_DECISION_2026-11-22.md` | "`make test` — 88 suites, `TEST=0`, zero failures" | same stale binary, same assertion |

The mechanism `01c0bc6` introduced is confirmed effective: after touching `src/scene_types.h`,
`make -n build/test-r9-optical-semantics` emitted a rebuild — so a header-only change now reaches
every binary.

### Corrected expectations

Measured with `make r9-p1-memory-report` and reproduced by hand from
`r9_optical_memory_report`; the eight values that derive from `ASSET_ID_CAPACITY` or the sparse
override count are unchanged.

| Field | Was | Now |
|---|---|---|
| `cell_count` | 131072 | **2097152** |
| `dense_cell_extension_bytes` | 1048576 | **16777216** |
| `dense_break_even_sparse_count` | 87381 | **1398101** |
| `material_plus_sparse_break_even_count` | 43690 | **1354410** |
| `dense_cell_text_payload_bytes` | 2228224 | **35651584** |

## What was changed

Deliberately minimal: **one test's expectations, and documentation.** No production source changed,
no behaviour changed, no build flags changed.

- `tests/test_r9_optical_semantics.c` — the five constants above. The test's intent (the report's
  byte costs and break-even points at the accepted ceiling) is unchanged; only the numbers that the
  2026-11-07 ceiling decision re-derived.
- `docs/V1_5_CELL_IDENTITY_DECISION_2026-11-22.md` and
  [`2026-11-22-font-programme-position.md`](2026-11-22-font-programme-position.md) — correction notes
  on the two `TEST=0` claims.
- `docs/TODO.md` — two build-hygiene items recorded under *Build and performance maintenance*.
- This record.

`SCENE_MAX_WIDTH`/`SCENE_MAX_HEIGHT` stay at 2048×1024. That is the accepted owner decision recorded
in `53b67e9` and the map-size plan; the defect was the un-updated expectation, not the ceiling.

## Editor database

`compile_commands.json` is ignored by the repository and is regenerated per host rather than shared.
The copy carried the source host's, whose paths (`/nas/contents/Projects/...`) do not exist here. It
was regenerated from the actual build with `intercept-build --cdb compile_commands.json make all`:
**126 per-file entries**, `directory` set to this tree, and **zero** references to the source host.
`.clangd` already supplies the three include roots and is host-independent, so it was left alone.

## Verification

Verification follows [`VERIFICATION_POLICY.md`](../VERIFICATION_POLICY.md) outcome names.
All results below are from **this host**, after `make clean`, on the tree as it stands.

| Gate | Command | Outcome | Evidence |
|---|---|---|---|
| Application build | `make all` | **PASS** | `build/ascii-fps`, 856,664 B, clean under `-Werror` |
| Smoke | `./build/ascii-fps --smoke-test` | **PASS** | `{"smoke":"ok","map_width":10,"map_height":6}`, exit 0 |
| Complete functional aggregate | `make test` | **PASS** | **91 runners, 1005 tests, 0 failures**, exit 0 |
| Tracker parity matrix | `make matrix` | **PASS** | 8/8 modes, `MATRIX_RC=0` |
| Standards — repository guards | `make standards-core` | **PASS** | 6/6 guards |
| Standards — analysis policy | `make check-static-analysis-policy` | **PASS** | suppressions limited to analysis-scope information |
| Standards — cppcheck defects | `make style` | **PASS with a runtime caveat** | see below |
| Binary portability | `readelf -n build/ascii-fps` | **PASS** | `x86 ISA needed: … x86-64-v2, x86-64-v3` — runs on this host |

**cppcheck caveat, stated precisely.** `make style` runs one cppcheck invocation over `src/`
(270 inputs: 123 `.c`, 147 `.h`) and needs more than 285 s here. It is not hung: the cost is
concentrated in a single file. Split into bounded invocations with the **identical** flags
(`--quiet --error-exitcode=100 --std=c11 --suppress=normalCheckLevelMaxBranches
--suppress=toomanyconfigs`), **269 of the 270 inputs returned exit 0 in the current state**. The one
input not re-measured is `src/ui_document.c`, which alone exceeds 285 s; removing it from its batch
drops that batch from >100 s to 1 s, which identifies it as the sole cause. For that file the
evidence is the complete `make standards` invocation earlier in this session, which returned exit 0
with `PASS: cppcheck static analysis passed` — and `src/` has not been modified since; this session's
only source edit is in `tests/`, which cppcheck does not analyse.

The `make matrix` runs interleave `make clean`, so the artifacts present at the end were rebuilt
afterwards and the aggregate re-run; the numbers above are from that final rebuild.

## What remains unverified, and why

- **`make asan` / `make ubsan` / `make leak`** were not run. `make leak` is the canonical container
  gate and needs Docker; the sanitizer targets are separate evidence. Nothing here is a memory-safety
  claim, and none should be inferred from a green functional aggregate.
- **`make matrix`** exercises `test-core` in eight tracker modes with a clean rebuild each. It is the
  parity gate for the tracker paths, **not** a clean-rebuild pass over all 91 runners; the full clean
  rebuild performed here is the broader evidence.
- **Native display acceptance** (`make display-acceptance-linux`, `make stability`,
  `make stability-headless`) needs a video environment and was not attempted.
- **`make coverage` and the headless benchmarks** (`make benchmark-headless`) were not run; they are
  performance/coverage evidence with their own baselines, not correctness gates.
- **`make font-determinism`** needs an installed face manifest; no converted set is committed
  (`assets/fonts/` is ignored), so the loader's built-in-face fallback is what the smoke test covers.

## Where the next pass starts

1. If any further source change lands, re-run `make clean && make test` plus `make standards` on this
   host; the header stamp makes the rebuild coarse and correct, at the cost of a full rebuild.
2. **V1-5 is the active phase, and the cell-identity gate is behind it.** The decision record's steps
   1–4 have landed, step 6 (drawing through `glyph_paint_resolve`) landed in `6672ddc`, and what step 5
   covers is now closed: the SMC stream field sizes are derived from the types rather than written as
   literals (`CELL_STREAM_GLYPH_BYTES = sizeof(Cell.glyph)`, `pack_cell_state` at bit 0 with 16 bits),
   and the clean rebuild plus the eight-mode matrix above are the evidence that was missing.
   What remains in V1-5 is **not** step 5. Inspecting the tree leaves three open items:
   - **Requirement 4 — the cache is not generation-aware.** `src/glyph_block_cache.h` keys on
     `uint32_t glyph_id` with **no generation or epoch**, and its field comment still reads
     "ASCII character (0-255)". Installing or clearing a glyph set cannot invalidate cached masks, so
     a changed face can serve the previous face's pixels. This is the concrete next increment.
   - **Requirement 6 — re-test logical cells and UI accessibility-scale independence**, explicitly
     required "after step 4" and not recorded as re-run anywhere.
   - **Requirement 2's segmentation/normalization/shaping** is `utf8proc` (D3), which the V1-4 record
     and the font handoff both schedule as its own increment; `src/grid.c:264` says so in place. It
     also gates V1-6. Do not fold it into the cache work.
   Then the V1-5 exit gate itself: reference/optimized parity, all SMC modes, fallback exceptional,
   ASCII and multilingual benchmark/stability budgets, and a Q4 review of the representation change.
   See [`V1_5_CELL_IDENTITY_DECISION_2026-11-22.md`](../V1_5_CELL_IDENTITY_DECISION_2026-11-22.md),
   [`2026-11-22-font-programme-position.md`](2026-11-22-font-programme-position.md) and
   [`FEATURE_ROADMAP.md`](../FEATURE_ROADMAP.md).
3. **V1-G is a parallel enabler, not the next phase.** M1 is complete (adopted-map seam, deterministic
   generator, seed sidecar, editor entry on Ctrl+Shift+N); its recorded follow-ups are the seed/size
   entry UI and the deliberately deferred level/portal format slice.
4. Do not treat `CURRENT_STATUS.md`'s suite counts (81/927) or the V1-5 record's (88) as current; the
   measured inventory on this host is recorded above.

## Reproducing this position from scratch

```sh
cd <tree>
make clean
make all
make test            # builds and runs every runner in the recipe
make standards
make matrix
./build/ascii-fps --smoke-test
```

The full rebuild is minutes, not the hour the V1-5 record estimated for a network-mounted workspace:
this tree sits on local ext4.

