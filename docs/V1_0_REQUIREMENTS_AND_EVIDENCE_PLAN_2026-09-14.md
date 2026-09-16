# V1-0 Requirements and Evidence Plan — 2026-09-14

## Status and authority

**Verified and closed on 2026-09-16.** B0/C1, W1-W5, scoped native Windows
verification, L1 native Linux display/input acceptance, P1-P3 performance work, byte
inventory, Clang diagnostic continuation, and mirror disposition are complete. See
[`reviews/2026-09-16-v1-0-closeout.md`](reviews/2026-09-16-v1-0-closeout.md).

This is the focused execution plan for
[`FEATURE_ROADMAP.md`](FEATURE_ROADMAP.md) phase **V1-0 — Baseline,
required-platform remediation, and evidence reconciliation**. It does not broaden
later v1 feature semantics or claim that Linux/Windows release support already exists.

V1-0 ends only when required strict builds pass, baseline evidence is reproducible,
native Linux display-input evidence is established, scoped native Windows verification
passes, the mirror report is classified, and legacy non-ASCII bytes have an
evidence-based disposition.

## Purpose

Make required-platform and regression evidence trustworthy before cross-cutting UI,
Unicode/font, glyph-cell, SMC, sprite, persistence, and export work. V1-0 removes known
toolchain blockers through narrow platform boundaries; it does not implement those later
features.

## Evidence already accepted at entry

1. R0-R12 are verified foundations; their behavior and file compatibility remain in force.
2. The 2026-09-12 Gentoo baseline passed the strict application build, all 62 functional
   runners including SMC, UI standards, `standards-core`, headless benchmarks and extended
   stability, ASan/LeakSanitizer, UBSan, the eight-mode matrix, and coverage as recorded in
   [`reviews/2026-09-12-regression-coverage-audit.md`](reviews/2026-09-12-regression-coverage-audit.md).
3. Ubuntu GCC and Fedora GCC passed their complete headless profiles; Ubuntu Clang
   produced `FAIL-PRODUCT` at strict application compilation because files lacked final
   newlines. Alpine musl GCC passed as informational evidence.
4. Native Windows 10 x64 UCRT64 reached strict application compilation and produced
   `FAIL-PRODUCT` for unavailable/incompatible POSIX filesystem, directory, `mkdir`, and
   locale APIs. Later phases correctly did not run.
5. The earlier row-dependent reflected-curvature defect was corrected before this phase:
   reflected-camera Z remains the incoming camera Z. A distinct general projection defect
   was subsequently reproduced and corrected on 2026-09-16 by retaining camera-to-mirror
   depth and source-column correction through the reflected continuation ray.

Historical evidence is not silently upgraded to a current run. B0 records fresh local
outcomes and retains unavailable native/platform gates as unverified.

## Requirements ledger

### Required behavior

1. Preserve strict C11 `-Wall -Wextra -Wpedantic -Werror`; do not suppress a platform
   diagnostic to obtain a pass.
2. Preserve the complete registered runner inventory and execute `make test` as the only
   complete functional aggregate.
3. Preserve default SMC stream mode as the shipping renderer path and retain every SMC
   runner and matrix role.
4. Record exact environment, compiler, flags, architecture, dependency artifacts, command,
   outcome class, status, and unrun gates for each baseline or platform result.
5. Treat product, missing-tool, tool, and timeout outcomes according to
   [`VERIFICATION_POLICY.md`](VERIFICATION_POLICY.md).
6. Resolve cross-platform differences through narrow first-party adapters whose contracts
   are testable without the native platform where practical.
7. Preserve atomic save/replacement, durability reporting, symlink rejection, direct-child
   catalog bounds, locale-independent numeric parsing, and cleanup behavior.
8. Use genuine native Windows for Windows verification. Cross-compilation, Wine, or Linux
   containers do not prove native Windows behavior.
9. Establish native Linux display/input evidence with backend, dimensions,
   scale, devices, and physical/virtual/remote context recorded.
10. Inventory bytes at or above `0x80` separately for runtime-authored data and
    documentation; do not infer an encoding from byte values.
11. Treat a mirror issue as new only if a deterministic fixture differs from the existing
    reflected-camera-Z regression.

### Forbidden behavior

- No warning-policy weakening, warning suppression, runner omission, or expected-failure pass.
- No blanket platform `#ifdef` branches distributed through document/domain modules.
- No replacement of atomic writes with plain overwrite on Windows.
- No process-global locale mutation or host-locale-dependent persistence parsing.
- No weakening of catalog symlink/path protections for Windows compatibility.
- No writable live-repository exposure, credentials, VM disks, proprietary media, or
  machine-specific VM identity in tracked files.
- No SMC removal, bypass, demotion, generated-source editing, or unexplained performance loss.
- No glyph/font format migration in V1-0.
- No mirror geometry/cache source change without a distinct failing fixture.
- No claim that headless checks prove native display, input, clipboard, IME, or packaging.

### Preserved invariants

- Application/editor UI and authored `UiDocument` remain separate owners.
- Documents remain authoritative; registries, runtime views, caches, and platform handles
  remain derived.
- Failed load/save/refresh operations preserve accepted caller-owned state according to
  existing contracts.
- Renderer, world, editor, and authored formats retain current behavior in this phase.
- Existing ASCII runtime assets retain byte-for-byte meaning.
- The planar mirror remains one-bounce, preserves incoming camera Z, terminates reflected
  mirrors, and uses bounded deterministic fallback.

### Deferred to later phases

- Unicode semantics, normalization, fonts, glyph identity, new `Cell` layout, and SMC
  representation migration belong to V1-4/V1-5.
- Pointer semantics and major-context motion belong to V1-3.
- Final sprite-workbench behavior belongs to V1-8.
- Windows packaging and clean-machine exported-game evidence culminate in V1-19/V1-20.

## Known failures and initial diagnosis

### Ubuntu Clang strict compilation

**Observed result:** `FAIL-PRODUCT`, strict application build, Clang 18.1.3.

**Diagnostic:** `-Wnewline-eof` promoted to an error for first-party and pinned SMC source
and header files without a final newline.

**Disposition:** first-party files were corrected. The four owned-upstream SMC newline
corrections and pin update are deferred to V1-20. Ubuntu Clang is informational; warnings
remain errors. A diagnostic continuation compiles independent application/test targets and
classifies any finding outside the exact known four-file `-Wnewline-eof` set as additional.

This is a textual portability correction, not a source-logic redesign. It lands separately
from Windows API work.

### Native Windows UCRT64 strict compilation

**Observed result:** `FAIL-PRODUCT`, strict application build, GCC 16.2.0.

Known incompatibility families:

1. durability and descriptor operations: `fsync` and related POSIX APIs;
2. direct-child/symlink-safe directory inspection: `dirfd`, `fstatat`, and
   `AT_SYMLINK_NOFOLLOW`;
3. directory creation signature: POSIX `mkdir(path, mode)` versus UCRT behavior;
4. locale-independent numeric parsing: `locale_t`, `newlocale`, `uselocale`,
   `freelocale`, and `LC_NUMERIC_MASK`.

Do not fix these call sites independently. W1 first inventories each caller and defines
narrow capabilities for file durability, directory/catalog inspection, directory creation,
and C-locale numeric parsing. Each adapter reports whether an operation failed, committed
with a durability warning, or is unsupported; callers retain their domain-specific typed
results and transactional state guarantees.

### Mirror report

The known symptom—symmetric columns seeing mismatched reflected boundaries through a planar
mirror—has the same documented cause as the original report: moving reflected-camera Z to
each sampled mirror point while reusing a prepared column. The correction and regression
already exist.

V1-0 therefore assumes no second production change. If a visible defect remains, M1 requires
the exact scene, camera/mirror geometry, viewport, screenshot/grid capture, expected boundary,
and a deterministic failing grid/framebuffer test before diagnosis or source changes.

### Non-ASCII authored bytes

The initial 2026-09-14 scan found one file below `assets/` containing bytes at or above
`0x80`: `assets/README.md`. It is documentation, not runtime-authored input. No runtime asset
file with non-ASCII bytes was found. B0 must preserve the exact scan scope and result; later
Unicode migration must rescan before implementation rather than assuming the inventory stays
empty.

## B0 evidence — 2026-09-14

### Current environment

`make verification-environment` reported:

```text
system: Linux 6.6.62-gentoo-dist x86_64
compiler: gcc (Gentoo 14.3.1_p20250801 p4) 14.3.1 20250801
cflags: -std=c11 -O2 -Wall -Wextra -Wpedantic -Werror
processors: 4
sdl_artifact: vendor/dist/lib64/libSDL3.so -> libSDL3.so.0
cmocka_artifact: vendor/dist/lib64/libcmocka.so -> libcmocka.so.0
default tracker: USE_SMC_STREAM_STATE_TRACKER=1
```

`cppcheck` and `valgrind` were not found on this host's command path. This is tool
availability evidence only. `make standards` and native Valgrind were not run and are
not passed; the canonical container leak gate remains separate.

**Post-closeout cppcheck result (2026-09-16):** Cppcheck 2.18.2 later became available.
Four serializer pointer-array findings were resolved with explicit initialization. The
gate now narrowly suppresses only the tool's normal branch/configuration scope
information while retaining defect exit status 100 and a repository-owned policy guard.
Real `make standards` and complete `make -j2 check` pass. The first serial full-gate
attempt timed out after all functional runners passed and is retained as
`FAIL-TIMEOUT`. See
[`reviews/2026-09-16-cppcheck-gate-recovery.md`](reviews/2026-09-16-cppcheck-gate-recovery.md).

### Passed gates

| Command | Outcome | Evidence |
|---|---|---|
| `make all` | `PASS` | Strict default GCC application build, default SMC stream mode. |
| `make build/test-mirror-trace build/test-optical-render` | `PASS` | Both focused owners compiled strictly. |
| `./build/test-mirror-trace` | `PASS` | 4/4 tests. |
| `./build/test-optical-render` | `PASS` | 19/19 tests, including straight reflected edges, stable vertical viewpoint, one-bounce/cache/fallback behavior. |
| `make test-build` | `PASS` | Complete registered 62-runner inventory built after bounded timeout recovery. |
| `make test` | `PASS` | Complete functional aggregate, including SMC runners. |
| `make test-ui-standards` | `PASS` | All nine focused UI owner groups. |
| `make standards-core` | `PASS` | Unsafe-call, structure, inventory, legacy-call, and current-renderer guards. |
| `make smoke` | `PASS` | `{"smoke":"ok","map_width":10,"map_height":6}`. |
| `make -B CC=clang all` | `PASS` | Local Clang 22 strict application build; not equivalent to Ubuntu Clang 18 informational diagnostic evidence. |

### Timeout and recovery

The first `timeout 120s make test` invocation ended with status 124 while compiling
`build/test-ui-menu-runtime`. No test failure had occurred. It is classified
`FAIL-TIMEOUT`, not `FAIL-PRODUCT` and not a pass.

The command was not retried unchanged. The narrower recovery first completed the
orchestration-only `make test-build`, then reran `make test` with current prerequisites;
both passed. The timeout remains part of the evidence and suggests clean full-suite build
duration needs a larger explicitly approved harness or staged profile execution.

### Headless benchmark product failure

`timeout 120s make benchmark-headless` stopped at the deterministic authored-surface
workload:

```text
benchmark-editor-highlight:
  avg_scenario_ms: 0.188778
  pass_budget_ms: 1.000
  deterministic: true
  result: pass

benchmark-surface-render:
  null_view_avg_ms: 1.019058
  authored_view_avg_ms: 1.314673
  authored_overhead_ms: 0.295614
  flat_height_avg_ms: 6.188057
  raised_height_avg_ms: 5.098443
  occluded_decal_avg_ms: 5.143577
  pass_budget_ms: 6.000
  deterministic: true
  result: fail
```

This is `FAIL-PRODUCT`: `flat_height_avg_ms` exceeded the 6 ms budget by 0.188057 ms.
The aggregate correctly stopped, so later headless benchmark workloads are unrun.
`make stability-headless` is also unrun because B0 requires the narrower performance gate
to pass first. Do not raise the budget, average away the result, or optimize code before a
focused performance requirements/reproduction increment compares current methodology,
repeatability, workload checksums, prior environment, and the affected render stages.

### Final-newline inventory

A bounded scan of regular `.c`/`.h` files directly under `src/`, `tests/`,
`vendor/src/smc/include/`, and `vendor/src/smc/src/c/` found:

```text
scanned_text_sources=254
missing_final_newline=192
```

The affected set included first-party production/test files and pinned SMC inputs. Local
Clang 22 compiled the application despite this state; Ubuntu Clang 18 rejected it with
`-Wnewline-eof`. C1 preserved an exact pre-edit path/hash manifest, appended only missing
first-party final newline bytes in bounded batches, and proved all preceding bytes unchanged.
The four SMC corrections are deferred to V1-20.

### Asset-byte inventory

A recursive scan of regular files below `assets/` found one file with bytes at or above
`0x80`: `assets/README.md` (58 such bytes). It is documentation and not runtime-authored
input. No runtime asset file contained a non-ASCII byte. No encoding or code-page meaning
was assigned.

### Unrun or unavailable B0 evidence

- `make benchmark-headless`: incomplete after the surface-render `FAIL-PRODUCT`.
- `make stability-headless`: unrun behind the benchmark blocker.
- `make matrix`: unrun in this baseline increment; the preserved 2026-09-12 matrix pass
  remains historical rather than current evidence.
- `make standards`: unrun because `cppcheck` is unavailable; `standards-core` passed.
- current ASan, UBSan, canonical leak, and coverage: unrun; historical evidence remains.
- Linux Docker profiles and native Windows profile: not rerun at this B0 checkpoint.
- native Linux display/input host: not run and not passed. Windows display/input is out of scope.

### B0 disposition

B0 has established a trustworthy functional baseline but is not complete enough to unlock
feature work. Two independent next increments are safe:

1. **P1 performance reproduction:** isolate the surface-render budget failure before any
   optimization or budget change.
2. **C1 final-newline remediation:** create the exact path/hash manifest, append only
   missing first-party newline bytes, then run local strict builds and Ubuntu Clang diagnostics.

**P1 result (2026-09-15):** Complete. Five sequential benchmark trials and three
1,000-iteration stability trials reproduced the surface-render failure on the physical host.
Flat medians were 7.526312 ms and 7.693334 ms respectively against the unchanged 6 ms budget;
all checksums were stable and deterministic. The failing stage is the current opaque prepared-
heightfield path. No renderer, benchmark, flag, or budget change was made. See
[`reviews/2026-09-15-v1-0-p1-surface-performance-reproduction.md`](reviews/2026-09-15-v1-0-p1-surface-performance-reproduction.md).

W1 through W5 are complete under the scoped native Windows contract.
No mirror source work is authorized without distinct evidence.

## Dependency and increment plan

```text
B0 current-host baseline and byte inventory
 |\
 | +--> C1 first-party Clang newline remediation
 |
 +----> W1 platform capability/caller inventory
          -> W2 filesystem/durability adapter
          -> W3 directory/catalog adapter
          -> W4 locale-independent numeric parser adapter
          -> W5 native Windows strict application/test checks

B0 -> L1 native Linux display/input smoke
B0 -> M1 mirror report classification (activate only with distinct evidence)

required GCC profiles + W5 + L1 + M1 disposition + byte inventory + performance disposition -> V1-0 closeout
```

### B0 — Current-host baseline and inventories

1. Record `verification-environment`.
2. Run a strict default application build with default SMC stream mode.
3. Run `make test`, `make test-ui-standards`, and `make standards-core`.
4. Run focused mirror tests before broader headless benchmark/stability gates.
5. Run `make benchmark-headless` and `make stability-headless` only after narrower gates pass.
6. Record missing tools and native-platform/display limitations without converting them to pass.
7. Scan bounded runtime asset files for non-ASCII bytes and classify documentation separately.

**Rollback:** Evidence and documentation only; generated `build/` artifacts are disposable.

### C1 — First-party Clang remediation and diagnostic continuation

1. Produce an exact list and hashes of files lacking final newlines.
2. Add only missing final newlines in a standalone increment.
3. Prove all other bytes are unchanged.
4. Run focused local strict Clang builds.
5. Keep Ubuntu Clang informational and run a strict diagnostic continuation after failure.
6. Defer the owned-upstream SMC newline/pin update to V1-20.

**Rollback:** Revert only final-newline byte additions. No warning configuration changes.

**C1 result (2026-09-14):** 188 tracked first-party/test files now contain exactly
their original bytes plus one LF and pass strict GCC/Clang, focused SMC, standards-core,
and complete functional gates. Four affected SMC files are supplied to profiles from a
pinned upstream archive, so ignored local-vendor edits were restored. After Docker was
restored and the pinned image rebuilt, Ubuntu Clang 18 reached strict compilation: every
first-party newline diagnostic was gone and only the same four pinned SMC files failed. See
[`reviews/2026-09-14-v1-0-c1-final-newline-remediation.md`](reviews/2026-09-14-v1-0-c1-final-newline-remediation.md).

**Platform evidence continuation (2026-09-14):** The complete platform harness passed;
the Ubuntu Clang image built and isolated the four pinned SMC files;
and native Windows reproduced the complete W1 portability families while restoring the
VM to `shut off`. No external timeout wrapped a platform command. See
[`reviews/2026-09-14-v1-0-platform-profile-evidence.md`](reviews/2026-09-14-v1-0-platform-profile-evidence.md).

### W1 — Windows platform-boundary decision record

Before source edits, list every incompatible call site, its owning contract, existing tests,
and Windows equivalent. Resolve:

- exact durability guarantees available on UCRT/Win32 and how post-commit durability warnings
  are surfaced;
- symlink/reparse-point and regular-direct-child policy;
- project-owned directory creation permissions semantics;
- locale-independent decimal parsing without global locale mutation;
- UTF-8/project-path conversion boundary needed by later v1 work;
- adapter initialization/cleanup and fault-injection strategy.

**Rollback:** Decision record only.

**W1 result (2026-09-14):** Complete. Capability ownership, commit state, native
scene/asset/sprite transaction boundaries, metadata, safe directory creation, reparse-safe
catalog discovery, C-locale number conversion, strict UTF-8/native paths, fault injection,
implementation increments, and acceptance are locked in
[`V1_0_W1_WINDOWS_PLATFORM_CAPABILITY_DECISION_2026-09-14.md`](V1_0_W1_WINDOWS_PLATFORM_CAPABILITY_DECISION_2026-09-14.md).

### W2-W4 — Narrow portability adapters

Land one capability family at a time. Domain APIs keep their current typed outcomes and
state guarantees. Add normal, boundary, injected failure, and preserved POSIX behavior tests
before replacing each call site.

**Rollback:** Each adapter and caller migration is independently revertible. Do not mix file
durability, catalogs, and numeric parsing in one code increment.

**W2-A result (2026-09-14):** Isolated path/file primitives and the 63rd focused runner are
implemented and locally verified. Native Windows accepts the 63-runner inventory and
strict-compiles the new platform modules without diagnostics, then stops in known unmigrated
callers before runner execution. See
[`reviews/2026-09-14-v1-0-w2a-platform-foundation.md`](reviews/2026-09-14-v1-0-w2a-platform-foundation.md).

**W2-B1 result (2026-09-14):** The native Windows profile now builds, inspects, and runs the
isolated platform runner before the full application. Native PE/import checks pass; 9/9
tests prove Unicode same-directory absent/existing replacement, hidden-attribute
preservation, read-only/sharing pre-commit failure preservation, and reparse classification.
Cross-volume evidence is unavailable on the single-volume VM and is outside native scene's
same-directory transaction. See
[`reviews/2026-09-14-v1-0-w2b1-native-replacement-prototype.md`](reviews/2026-09-14-v1-0-w2b1-native-replacement-prototype.md).

**W2-B2 result (2026-09-14):** Native scene save now uses platform metadata, file sync,
replacement, and commit state while retaining every existing scene result, diagnostic,
recovery, identity, and dirty-state contract. Local strict/sanitizer owner tests pass 48/48;
native strict compilation reports zero scene-document diagnostics. See
[`reviews/2026-09-14-v1-0-w2b2-native-scene-save-migration.md`](reviews/2026-09-14-v1-0-w2b2-native-scene-save-migration.md).

**W2-C1 result (2026-09-14):** UI preferences now use platform file sync/replacement.
Injected pre-commit failures preserve destination bytes and remain active-not-saved;
committed durability warning has a distinct appended saved outcome and user feedback. Local
strict/sanitizer tests pass 6/6 and native strict compilation reports zero owner diagnostics.
See
[`reviews/2026-09-14-v1-0-w2c1-ui-preferences-migration.md`](reviews/2026-09-14-v1-0-w2c1-ui-preferences-migration.md).

**W2-C2 result (2026-09-14):** Material save/Save As now use platform sync/replacement.
Pre-commit failure preserves destination/snapshot/path/dirty state; warning commit updates
path/snapshot/clean state. Asset refresh recognizes both committed outcomes. Strict and
sanitizer owner tests pass 8/8; native strict compilation has zero owner diagnostics. See
[`reviews/2026-09-14-v1-0-w2c2-material-document-migration.md`](reviews/2026-09-14-v1-0-w2c2-material-document-migration.md).

**W2-C3 result (2026-09-14):** Object atomic creation now uses platform sync/replacement.
Uncommitted failures keep `out_id` zero and preserve destination bytes; warning commit sets
the selected lowest-free ID. Registry/generation remain untouched. Strict/sanitizer owner
tests pass 4/4; native strict compilation reports zero owner diagnostics. See
[`reviews/2026-09-14-v1-0-w2c3-object-document-migration.md`](reviews/2026-09-14-v1-0-w2c3-object-document-migration.md).

**W2-C4 result (2026-09-14):** Flow saves now use platform sync, destination inspection, and
replacement. Uncommitted failures preserve destination/document identity; warning commits
update path and clean state. Workspace history/saved ownership and editor feedback distinguish
committed warning from failure. Strict GCC/Clang document tests pass 9/9, workspace tests pass
10/10, sanitizer checks pass, and native strict compilation reports zero owner diagnostics.
See [`reviews/2026-09-14-v1-0-w2c4-flow-document-migration.md`](reviews/2026-09-14-v1-0-w2c4-flow-document-migration.md).

**W2-C5 result (2026-09-14):** Authored UI saves now use platform sync, destination
inspection, and replacement. Uncommitted failures preserve destination/document/workspace
identity; warning commits update clean state and heap-owned history snapshots. Warning
Save-and-close refreshes and closes without becoming a failed save. Strict GCC/Clang document
tests pass 14/14, workspace tests pass 13/13, sanitizer checks pass, and native strict
compilation reports zero `ui_document.c` diagnostics. See
[`reviews/2026-09-14-v1-0-w2c5-ui-document-migration.md`](reviews/2026-09-14-v1-0-w2c5-ui-document-migration.md).

**W2-D1 result (2026-09-14):** `platform_fs_ensure_directory` now provides one-level,
no-follow UTF-8 directory creation and is used by UI menu and unified-editor sprite-directory
owners. Existing directories succeed; files, links/reparse points, missing parents, and
injected failures are typed without caller-state mutation. Local strict/sanitizer platform,
workspace, and editor tests pass. Native platform preflight passes 10/10 and both owner
`mkdir` diagnostics are gone. See
[`reviews/2026-09-14-v1-0-w2d1-managed-directory-creation.md`](reviews/2026-09-14-v1-0-w2d1-managed-directory-creation.md).

**W2-D2 result (2026-09-14):** Sprite folder save now uses platform file sync, no-follow
inspection, no-overwrite directory moves, restoration, and validated flat cleanup. Committed
warnings update path/clean state and editor registry/runtime; incomplete cleanup/restoration has
a distinct result. Strict/sanitizer platform, sprite, and editor tests pass. Native preflight
passes 11/11 and `sprite_document.c` has zero diagnostics. See
[`reviews/2026-09-14-v1-0-w2d2-sprite-folder-publication.md`](reviews/2026-09-14-v1-0-w2d2-sprite-folder-publication.md).

**W2-C6 result (2026-09-14):** Decal document Save and Save As now use platform file
sync, no-follow destination inspection, and replacement. Uncommitted failures preserve
destination, path, saved snapshot, and dirty state; committed durability warnings publish
path/snapshot/clean state. Asset refresh recognizes every committed decal result. Strict
GCC/Clang owner tests pass 7/7, focused ASan/LeakSanitizer and UBSan pass, asset refresh
passes 7/7, unified editor passes 99/99, and all 63 registered runners pass. Native Windows
strict compilation reports no decal-owner diagnostic and now stops only in W3/W4 owners.
See
[`reviews/2026-09-14-v1-0-w2c6-decal-document-migration.md`](reviews/2026-09-14-v1-0-w2c6-decal-document-migration.md).

**W3 result (2026-09-14):** `platform_catalog` now streams borrowed strict-UTF-8
direct-child names with typed no-follow regular-file/directory/link-or-reparse/other
metadata. `MapCatalog` retains filtering, dynamic ownership, sorting, and transactional
snapshot replacement. Local strict GCC/Clang owner tests pass 8/8, platform tests pass
10/10, focused ASan/LeakSanitizer and UBSan pass, and all 63 runners pass. Native Windows
platform tests pass 13/13, native `MapCatalog` passes 7/7, real reparse and multilingual
enumeration are proven, and strict application compilation now stops only in W4's
`scene_format` locale calls. See
[`reviews/2026-09-14-v1-0-w3-direct-child-catalog.md`](reviews/2026-09-14-v1-0-w3-direct-child-catalog.md).

**W4 result (2026-09-14):** `platform_number` now parses complete finite ASCII decimals and
formats canonical `%.17g` tokens through per-call POSIX/UCRT locale objects without
process-global locale mutation or failed-output changes. `scene_format` delegates conversion
while retaining grammar, diagnostics, exact bytes, and transactional ownership. Local strict
GCC/Clang number tests pass 5/5, scene tests pass 24/24, focused sanitizers pass, and all 64
runners pass. Native PE/import checks pass; UCRT number tests pass 5/5, native scene format
passes 24/24, and the strict application builds. See
[`reviews/2026-09-14-v1-0-w4-locale-independent-number.md`](reviews/2026-09-14-v1-0-w4-locale-independent-number.md).

### W5 — Native Windows functional profile

Run the unchanged strict policy in the existing native Windows provider through the
single `platform-test-windows` target:

1. dependency and source setup;
2. strict application build;
3. complete `make test` aggregate.

Any first failure remains nonzero and preserves later phases as unrun.

**W5 result (2026-09-15):** Native Windows passes strict application compilation and
strict compilation/execution of all 64 runners through the single scoped target. The VM
was already running and remained running. Earlier standards, binary-inspection, display,
stability, and performance evidence is outside the revised Windows acceptance scope. See
[`reviews/2026-09-15-v1-0-w5-native-windows-functional.md`](reviews/2026-09-15-v1-0-w5-native-windows-functional.md).

### L1 — Native Linux display and input host

For the native Linux display host, record OS/build, architecture, video backend, GPU/driver where
relevant, logical and window dimensions, scale factor, input devices, session type, SDL
identity, command, duration, and result. Exercise startup, presentation, resize,
keyboard/mouse input, application transitions, and clean teardown. Pointer activation,
clipboard, and IME receive their complete semantic gates in later owning phases.

**Rollback:** Harnesses remain adapters over normal application behavior; no display-only
logic enters domains.

**L1 result (2026-09-16):** `make display-acceptance-linux` passed on native X11 with
the SDL X11/OpenGL path. Presentation, EWMH resize, keyboard, synthetic-host XTest
pointer motion/down/up, application-state transition, and bounded teardown passed over
12.042 seconds and 1,459 frames. The captured presentation was 1366x768. XTest is not
claimed as physical-device input.

### M1 — Distinct mirror report only

If new evidence exists, first add a failing deterministic fixture and identify the earliest
divergent stage. Otherwise record that the known report is covered and leave mirror source
untouched. Complete reflected entities/sprites remain V1-11 work.

## Acceptance matrix

| Track | Normal | Boundary | Failure | Preserved regression | Platform/manual |
|---|---|---|---|---|---|
| B0 | Strict build and complete aggregate | Full registered inventory | Missing/broken tools typed | Default SMC and mirror tests | Current-host limits recorded |
| Clang | Strict app build plus diagnostic sweep | First-party plus pinned SMC inputs | Compiler failure remains nonzero | No warning suppression; exact four-file deferral | Informational Ubuntu Clang profile |
| File durability | Atomic replacement | Existing destination, metadata, directory sync | Pre/post-commit failures distinguished | Dirty/saved state and destination rules | Native Windows filesystem |
| Catalog | Sorted regular direct children | symlink/reparse, long names, missing root | Refresh preserves old catalog | Existing POSIX catalog behavior | Native Windows paths |
| Numeric parsing | C-locale finite values | range, decimal separator, overflow | Output unchanged | Existing strict full consumption | Linux/Windows locale variation |
| Display/input | Startup/present/input/teardown | resize, scale, repeated transitions | backend/device/init failure | Headless controllers unchanged | Native Linux |
| Mirror | Existing planar fixture | camera height and bilateral bounds | opening/second mirror fallback | one bounce and cache rules | Distinct report only |
| Byte inventory | Runtime assets scan clean/known | every regular asset file | unreadable file is reported | no inferred code page | Rescan before V1-4 |

## Verification commands

Commands are run from `/bigdisk/programming/C/terminal-style-game` and reported with exact
outcomes rather than presumed success:

```text
make verification-environment
make all
make test
make test-ui-standards
make standards-core
./build/test-mirror-trace
./build/test-optical-render
make benchmark-headless
make stability-headless
make matrix
make platform-survey
make platform-check
```

`make standards`, sanitizer/leak/coverage gates, and native providers run when their tools
and bounded environments are available. Missing or broken tools follow the verification
policy and do not become successful skips.

## V1-0 closeout gate

V1-0 may become Verified only when:

1. the current baseline and final post-remediation baseline are recorded;
2. all required Linux profiles and native Windows W5 pass;
3. native Linux display/input smoke evidence passes;
4. SMC runners, default shipping mode, matrix roles, performance, and stability remain intact;
5. the mirror report is documented as covered or a distinct defect is corrected through a
   display-level failing fixture;
6. the non-ASCII runtime-asset inventory has an explicit result and no byte was assigned a
   host-code-page meaning;
7. documentation and implementation agree and all residual gates are explicit.

## Current handoff

V1-0 is Verified. The distinct mirror report was reproduced, corrected, manually
confirmed, and regression-guarded; complete headless benchmark/stability aggregates and
native Linux display/input acceptance pass. Proceed with V1-1 Q1 through
`V1_1_UI_RULES_AND_TOKENS_Q1_PLAN_2026-09-16.md`. Do not begin V1-3 pointer/motion
implementation until V1-1 accepts measurable motion, reduced-motion, focus, scale, and
viewport rules. The four-file owned-upstream SMC correction remains deferred to V1-20
and guarded by the strict informational Clang diagnostic sweep.
