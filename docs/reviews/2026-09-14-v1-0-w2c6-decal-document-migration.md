# V1-0 W2-C6 Decal Document Persistence Migration — 2026-09-14

## Outcome

**W2-C6 is implemented and locally verified. `decal_document` now uses the established
platform filesystem capabilities for temporary-file synchronization, destination
inspection, and replacement. Committed durability warnings publish path, saved snapshot,
and clean state rather than being misreported as uncommitted failures.**

This increment implements the remaining W2 single-file owner migration from
[`../V1_0_REQUIREMENTS_AND_EVIDENCE_PLAN_2026-09-14.md`](../V1_0_REQUIREMENTS_AND_EVIDENCE_PLAN_2026-09-14.md)
under the W1 capability decisions in
[`../V1_0_W1_WINDOWS_PLATFORM_CAPABILITY_DECISION_2026-09-14.md`](../V1_0_W1_WINDOWS_PLATFORM_CAPABILITY_DECISION_2026-09-14.md).
It does not change the decal format, catalog enumeration, locale conversion, painter
behavior, mirrors, SMC, or performance thresholds.

## Contract extension

`DECAL_DOCUMENT_OK_DURABILITY_WARNING` was appended to `DecalDocumentResult`, preserving
all previous enum values. The public `decal_document_result_is_committed` predicate accepts
ordinary success and durability-warning success while rejecting validation and I/O
failures.

`asset_refresh` now uses that predicate for decal Save and Save As. A committed warning
therefore proceeds through registry reload and scene repair-diagnostic refresh instead of
leaving disk, document, and registry state inconsistent. `AssetRefreshResult` was not
broadened; this matches the accepted material-document precedent.

## Implementation

### `src/decal_document.c` and headers

- `decal_save_to_file` remains the serialization owner and exact file format authority.
- The completed temporary file is reopened as a stream and synchronized through
  `platform_fs_sync_file`.
- `platform_fs_inspect_nofollow` determines whether the destination exists.
- `platform_fs_replace` replaces direct publication and reports explicit commit state.
- Not-committed sync/replacement failures remove the temporary and retain the existing
  `DECAL_DOCUMENT_IO_ERROR` mapping.
- A committed durability warning returns the appended warning result.
- Save and Save As publish the deep-copied saved snapshot and mark clean for every committed
  result; Save As also publishes its owned path only after commit.
- Explicit registry commit remains separate and is not performed by document save.

`src/decal_document_internal.h` provides per-call sync, replacement, and durability fault
seams. No global mutable fault state was introduced.

### Build linkage

The focused decal-document runner now links `platform_path` and `platform_fs`. Combined
application, unified-editor, and asset-refresh runners already receive those modules through
their existing source groups, so no duplicate source definition was added. Runner inventory
remains 63.

## Regression coverage

`tests/test_decal_document.c` now runs seven tests and additionally proves:

- committed-result classification;
- exact canonical structured decal bytes with LF endings;
- ordinary Save As and overwrite Save behavior;
- injected sync and replacement failures preserve destination bytes;
- uncommitted Save As failure preserves dirty state, null path, and saved snapshot;
- durability-warning Save As publishes exact bytes, path, saved snapshot, and clean state;
- durability-warning overwrite Save publishes changed bytes and saved snapshot while
  retaining path identity; and
- document save does not mutate registry generation.

Existing creation, lowest-free ID, validation, paint, resize, undo/redo, preview, open,
discard, and explicit registry-commit coverage remains intact. `test-asset-refresh` and
`test-unified-editor` retain the save/refresh/placement integration coverage.

## Verification evidence

| Check | Result |
|---|---|
| Pre-change strict GCC decal owner | `PASS`, 4/4 |
| Pre-change asset refresh | `PASS`, 7/7 |
| Final strict GCC decal owner | `PASS`, 7/7 |
| Final strict Clang decal owner | `PASS`, 7/7 |
| Asset refresh, strict GCC and Clang builds/runs | `PASS`, 7/7 |
| Unified editor | `PASS`, 99/99 |
| Focused decal ASan + LeakSanitizer | `PASS`, 7/7, no report |
| Focused decal UBSan | `PASS`, 7/7, no report |
| Asset-refresh ASan + LeakSanitizer | `PASS`, 7/7, no report |
| Asset-refresh UBSan | `PASS`, 7/7, no report |
| Strict default GCC/SMC-stream application | `PASS` |
| Complete runner build | `PASS`, 63 runners, after bounded timeout recovery |
| Complete `make test` | `PASS`, all 63 registered runners |
| UI standards aggregate | `PASS`, all nine owners |
| `make standards-core` | `PASS` |
| `make test-platform-harness` | `PASS` |
| `make smoke` | `PASS`, `{"smoke":"ok","map_width":10,"map_height":6}` |

All strict C compilation used `-std=c11 -O2 -Wall -Wextra -Wpedantic -Werror` unless a
named sanitizer profile replaced optimization with the documented `-O1 -g` sanitizer
configuration.

## Native Windows evidence

The native profile was run without an external timeout:

```text
PROFILE_VM_NAME=win10-survey make platform-test-windows
```

Observed result:

- native platform capability preflight passed 11/11 and remained PE x86-64 without an
  accidental MSYS/Cygwin runtime dependency;
- `decal_document.c` produced zero strict native diagnostics;
- the full product stopped at the expected `FAIL-PRODUCT` strict application phase only in
  `map_catalog.c` and `scene_format.c`, which are owned by W3 and W4;
- later native phases remained correctly unrun;
- VM cleanup passed through QGA and restored `win10-survey` from `shut off` to `shut off`.

This is native compile evidence for the migrated owner, not complete native decal-runner or
Windows-product acceptance.

## Failures and corrections

### New private header lacked a final newline

Attempt: compile the focused owner with local Clang after adding the private fault header.

Result: `-Wnewline-eof` failed the strict build for
`src/decal_document_internal.h`.

Failure mode: the newly added header ended immediately after `#endif`.

Correction: add the conventional final newline and descriptive include-guard comment.
Strict Clang owner compilation and 7/7 execution then passed.

### Complete runner build exceeded the bounded command window

Attempt: `make -j2 test-build` under a 120-second local command bound.

Result: `FAIL-TIMEOUT` while compiling late UI runners; no product diagnostic was emitted.

Correction: inspect the dry-run remainder, build the known bounded unfinished target set,
then rerun `make test-build`. The inventory check passed, followed by all 63 functional
runners passing. The unchanged timed-out command was not retried with a larger external
timeout.

### Simultaneous compiler output was not accepted as execution evidence

Attempt: build the same focused output path with GCC and Clang concurrently.

Result: both compiler commands returned success, but either could replace the shared output
before execution.

Correction: rebuild and execute the focused owner sequentially under each compiler. Both
strict runs passed 7/7.

### Full local Clang application remains blocked outside W2-C6

Attempt: strict local Clang application build after owner verification.

Result: `FAIL-PRODUCT` on pre-existing missing final newlines in earlier private headers and
the four pinned SMC files. The changed decal owner was not among the diagnostics.

Disposition: preserve this as open C1/C2 evidence. Do not broaden W2-C6 into unrelated
newline or dependency remediation.

### Nonexistent sanitizer/provider path inspections

Two planned helper-file reads used paths that do not exist. The maintained Makefile and
actual provider runner were used instead. No product source depended on either missing path.

## Preserved invariants

- Exact decal bytes and parser compatibility are unchanged.
- Lowest-free ID selection and material/dimension validation are unchanged.
- Value, saved snapshot, path, history, and dirty-state ownership remain in
  `DecalDocument`.
- Save and Save As do not mutate the registry.
- Registry publication and generation changes remain explicit through commit/refresh.
- Uncommitted failures preserve destination and document identity.
- Committed data is never reported as dirty or uncommitted.
- Existing enum numeric values remain stable.
- Scene command history remains separate from asset save/refresh.
- No catalog, locale, glyph, font, UI, sprite, mirror, SMC, or benchmark behavior changed.

## Limitations and remaining work

1. The focused decal fixture remains POSIX-shaped and was not executed natively on Windows.
   Native evidence is the 11/11 platform preflight plus diagnostic-free strict owner
   compilation.
2. Temporary creation, serialization open, and cleanup retain current CRT narrow-path
   operations. Broad Unicode asset-path ownership belongs to later v1 work.
3. Successful existing-destination replacement on Windows remains conservatively reported
   as a durability warning.
4. `AssetRefreshResult` does not carry the underlying decal durability warning to its
   caller. It correctly continues after commit, matching the accepted material behavior.
5. Full local Clang application compilation remains blocked by pre-existing private-header
   and pinned-SMC newline defects outside this increment.
6. V1-0 remains open for W3, W4, W5, native display/input evidence, pinned-SMC remediation,
   P1 performance, mirror classification closeout, and final acceptance.

## Next safe action

Proceed to **W3 — direct-child catalog adapter**. Add platform enumeration metadata and
tests, then migrate only `MapCatalog` while preserving complete-candidate ownership,
extension filtering, deterministic sorting, no-follow link/reparse rejection, long/UTF-8
name behavior, and exact live-catalog preservation on failure.

W4 locale conversion, pinned-SMC remediation, and P1 performance remain independent. Do
not combine them with W3, and do not externally timeout platform profiles.
