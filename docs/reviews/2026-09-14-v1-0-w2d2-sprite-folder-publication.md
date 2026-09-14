# V1-0 W2-D2 Sprite Folder Publication Transaction — 2026-09-14

## Outcome

**W2-D2 complete. `sprite_document` now writes and syncs a complete candidate folder, moves
an existing target to backup, publishes the candidate through native directory moves, restores
on publication failure where possible, and reports committed warnings or incomplete recovery
truthfully.**

This increment implements D7 from
[`../V1_0_W1_WINDOWS_PLATFORM_CAPABILITY_DECISION_2026-09-14.md`](../V1_0_W1_WINDOWS_PLATFORM_CAPABILITY_DECISION_2026-09-14.md).
It does not model a directory tree as a single-file replacement. Decal persistence, catalog
enumeration, and locale conversion remain unchanged.

## Result contract

Two values were appended to `SpriteDocumentResult`, preserving all previous enum numbers:

- `SPRITE_DOCUMENT_OK_DURABILITY_WARNING`: the new target is committed and the document is
  clean, but move durability or backup cleanup could not be fully confirmed;
- `SPRITE_DOCUMENT_TRANSACTION_INCOMPLETE`: no new target was committed, but candidate cleanup
  or restoration of the old target did not complete cleanly.

`sprite_document_result_is_committed` identifies ordinary and warning commits. Existing
`SPRITE_DOCUMENT_IO_ERROR` remains the result for pre-commit failure with complete candidate
cleanup and, when required, successful restoration.

## Platform primitives

### `platform_fs_move`

- moves one path to an absent destination without overwrite;
- rejects an existing destination before mutation;
- uses strict UTF-8/native paths and `MoveFileExW(..., MOVEFILE_WRITE_THROUGH)` on Windows;
- uses `rename` plus parent-directory sync on POSIX;
- reports `NOT_COMMITTED`, `COMMITTED`, or `COMMITTED_DURABILITY_WARNING`;
- clears expected destination-not-found native context before a successful move.

Windows directory moves remain conservatively durability warnings because directory-entry
durability cannot be represented as the same guarantee as the POSIX parent-directory sync.

### `platform_fs_remove_flat_directory`

- accepts only an ordinary directory containing ordinary direct files;
- validates the complete flat shape before deleting any child;
- rejects nested directories and links/reparse points without partially cleaning the folder;
- deletes direct files and then the directory;
- is intentionally non-recursive and does not enumerate names to callers.

Private platform faults were appended for move and flat-directory cleanup.

## Sprite transaction

`sprite_document_internal_save` now performs:

1. complete validation of frames, dimensions, materials, and playback metadata;
2. path allocation before any filesystem mutation for pathless documents;
3. same-parent candidate directory creation;
4. manifest and every frame write, flush, platform file sync, and close;
5. no-follow target and backup classification;
6. old-target move to explicit `.backup` when present;
7. candidate move to the numeric target;
8. old-target restoration if publication fails;
9. backup cleanup only after successful publication;
10. path publication and dirty clearing for every committed outcome.

The registry is not mutated by save. `sprite_document_commit_to_registry` remains the explicit
deep-copy publication step.

`sprite_document_internal.h` provides per-call faults for sync, backup move, publish move,
restore move, candidate cleanup, backup cleanup, and durability. No global mutable fault state
was introduced.

## Editor handling

Unified-editor existing-save and new-canvas workflows now use
`sprite_document_result_is_committed`. A durability-warning save still commits the staged
sprite to the registry and refreshes runtime, then reports the existing
`EDITOR_STATUS_DURABILITY_WARNING` instead of `EDITOR_STATUS_SAVE_FAILED`.

The explicit cleanup after a later new-canvas placement failure remains unchanged.

## Tests

### Platform capability

The focused platform runner now has eight Linux tests and eleven native Windows tests. New
coverage proves:

- no-overwrite directory move;
- injected pre-commit move failure preserving source/destination;
- committed durability-warning move state;
- ordinary successful move with cleared native error context;
- flat-folder cleanup;
- injected cleanup failure preserving files;
- complete-shape validation preserving a folder containing a nested directory.

### Sprite owner

The focused sprite runner now has seven tests covering:

- existing static and animated exact serialization and registry commit behavior;
- sync failure with clean candidate cleanup;
- backup-move failure preserving old target;
- publish failure with successful restoration;
- candidate cleanup failure retaining a complete candidate and returning incomplete;
- publish plus restore failure retaining old bytes in backup and returning incomplete;
- backup-cleanup failure after publication returning committed warning;
- move durability warning returning committed warning;
- path/dirty identity for committed and uncommitted results;
- registry data and generation remaining unchanged until explicit commit;
- committed-result classification.

### Unified editor

The editor runner now verifies that a warning save clears sprite dirty state, deep-copies the
new sprite into the registry, refreshes runtime, and reports durability warning rather than
save failure.

## Verification

| Check | Result |
|---|---|
| Strict GCC platform runner | `PASS`, 8/8 on Linux |
| Strict Clang platform runner | `PASS`, 8/8 on Linux |
| Platform ASan + leak detection | `PASS`, 8/8, no report |
| Platform UBSan | `PASS`, 8/8, no report |
| Strict GCC sprite-document runner | `PASS`, 7/7 |
| Strict Clang sprite-document runner | `PASS`, 7/7 |
| Sprite ASan + leak detection | `PASS`, 7/7, no report |
| Sprite UBSan | `PASS`, 7/7, no report |
| Strict GCC unified-editor runner | `PASS`, 99/99 |
| Strict Clang unified-editor runner | `PASS`, 99/99 |
| Unified-editor ASan + leak detection | `PASS`, 99/99, no report |
| Unified-editor UBSan | `PASS`, 99/99, no report |
| Strict GCC/default-SMC application build | `PASS`, explicit status 0 |
| Strict Clang/default-SMC application build | `PASS`, explicit status 0 |
| Complete runner build | `PASS`, explicit status 0 |
| Complete `make test` | `PASS`, explicit status 0 and 63 pass markers |
| `make standards-core` | `PASS`, explicit status 0 |
| `make test-platform-harness` | `PASS`, explicit status 0 |
| Direct sprite OS-call scan | `PASS`, zero sync/move/stat/enumeration calls |
| Passing-test recovery artifact scan | `PASS`, zero temp/backup folders |

### Failures and corrections during verification

1. The first flat-cleanup implementation could delete regular children before discovering a
   later nested directory. It was replaced with validation and deletion passes; tests prove
   malformed folders remain intact.
2. The first Windows two-pass correction contained an invalid temporary placeholder. Immediate
   source review caught it before compilation; the search pattern is now safely retained across
   both passes.
3. The first platform fault assertion expected access-denied on POSIX, but injected `EIO`
   correctly maps to `PLATFORM_FS_IO_ERROR`. The platform-specific expectation was corrected.
4. Strict compilers rejected preprocessor directives inside a cmocka macro argument. The
   expected value is now selected in a local variable.
5. The first restore-failure test incorrectly required retaining the unpublished candidate.
   The owner correctly cleaned it while retaining old data in backup; the test now protects
   that accepted recovery policy.
6. Final review found stale destination-not-found native context on successful moves. The
   context is now cleared and directly regression-tested.
7. Two recovery folders left by the early failed assertion were inspected and removed. Passing
   focused and sanitizer suites leave no recovery artifacts.

### Native Windows profile

Executed after the final correction without an external timeout:

```sh
PROFILE_VM_NAME=win10-survey make platform-test-windows
```

Observed:

- isolated platform runner remains native PE x86-64 without MSYS/Cygwin imports;
- native preflight increased from 10/10 to 11/11 and passed;
- `W2D_SPRITE_DIRECTORY_PRIMITIVES=PROVEN` was emitted;
- `sprite_document.c` strict-compiles with zero diagnostics;
- full product remains `FAIL-PRODUCT` only in `decal_document`, `map_catalog`, and
  `scene_format`;
- VM cleanup passed and final state is `shut off`;
- the tracked Python cache modified by execution was restored from `HEAD`.

## Preserved invariants

- Exact manifest and frame bytes remain unchanged.
- Every candidate file is flushed, synced, and closed before publication.
- Existing target bytes remain available at target or backup on every tested failure path.
- Registry contents and generation are untouched by save.
- Committed data is never reported as unsaved or left dirty.
- Incomplete cleanup/restoration is never mislabeled as a clean rollback.
- Existing enum numbers remain stable.
- Directory cleanup is flat and non-recursive.
- No catalog, locale, decal, SMC, mirror, or benchmark behavior changed.
- Runner inventory remains 63.

## Limitations

1. The focused sprite owner fixture remains POSIX-shaped (`mkdtemp`, `opendir`, `unlink`,
   `rmdir`) and was not executed natively. Native evidence is the 11/11 platform primitive
   preflight plus diagnostic-free strict owner compilation.
2. Candidate directory creation still uses the current CRT `mkdtemp` path. Native UCRT64
   strict compilation accepts it; broader Unicode temporary-directory ownership is later v1
   work.
3. Windows directory move commits are conservatively durability warnings. The editor displays
   that warning while accepting committed data.
4. Flat cleanup intentionally rejects nested or linked content. It does not attempt recursive
   repair of tampered sprite folders.
5. Animation `%.17g` formatting remains locale-sensitive until W4.

## Next safe action

Proceed to **W2-C6 — `decal_document` persistence migration**. Preserve exact decal bytes,
ID allocation, registry/snapshot ownership, path and dirty state, and add committed-warning
handling before replacing direct sync/replacement calls.

W3 catalog enumeration, W4 locale conversion, pinned-SMC remediation, and P1 performance
remain independent. Do not externally timeout platform profiles.