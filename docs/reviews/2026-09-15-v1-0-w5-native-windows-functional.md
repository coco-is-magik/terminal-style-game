# V1-0 W5 Native Windows Functional Profile — 2026-09-15

## Outcome

**W5 is complete. The native Windows 10 x64 UCRT64 profile passes dependency/source identity,
platform preflight, strict application compilation, strict compilation and execution of all 64
registered test runners, `standards-core`, and PE/import inspection of the application plus all
64 test binaries. No inspected binary imports `msys-2.0.dll` or `cygwin1.dll`.**

The final configured command was:

```text
PROFILE_VM_NAME=win10-survey make platform-test-windows
```

`build/platform-profiles/windows-10-x64-gcc/result.env` records `PRIMARY_OUTCOME=PASS`,
`PRIMARY_PHASE=native-binary-check`, and status 0. The VM began `shut off`, was started by the
harness, and was restored through QGA to `shut off`.

## Requirements and preserved behavior

- Windows dependency libraries come from normal Make policy, not a provider-only override.
- Static-library ordering remains explicit: ENet precedes its Windows system dependencies.
- Linux link commands contain no Windows system libraries.
- All 64 registered runners remain in the canonical aggregate and execute without skips.
- Existing typed committed-warning outcomes remain distinct from failures.
- Atomic-save failure preserves destination and dirty/saved identity; warning commits remain
  committed and visible to callers.
- Test fixtures remain unique, isolated, explicitly cleaned, and byte-deterministic.
- Native binaries remain PE x86-64 without an accidental POSIX runtime dependency.
- C1/C2 newline, P1 performance, sprite-animation locale formatting, and L1/W6 display/input
  acceptance were not broadened into W5.

## Implementation

### Make link policy

`Makefile` now defines empty `PLATFORM_LIBS` by default and selects
`-lwinmm -lws2_32` only when the standard Windows environment reports `OS=Windows_NT`.
`LIBS` and `TEST_LIBS` append that value after `-lenet`, matching the pinned ENet Windows
consumer contract. `tools/platform-profiles/test-make-platform-libs.sh` proves the Windows order
for application and tests and proves those libraries do not leak into Linux policy. The test is
part of `make test-platform-harness`.

### Portable test fixtures

Test-owned unique files and directories now live beneath the Make-owned `build/` workspace
instead of assuming a writable POSIX `/tmp` drive root. UCRT/POSIX `mkdir` signature differences
are isolated through test-local helpers. The decal integration fixtures no longer invoke shell
`mkdir -p` or `rm -rf`; they create and remove only known paths through bounded C operations.
Their map/decal files are written in binary mode so explicit LF bytes remain exact across CRTs.

### Persistence and commit state

- Deprecated legacy `scene_document_save` now uses `platform_fs_inspect_nofollow` and
  `platform_fs_replace`, so replacing an existing legacy destination works on Windows without
  weakening failure preservation.
- `decal_document` reopens its completed temporary file as `r+b`, granting the Windows
  `FlushFileBuffers` path the write-capable handle it requires. The platform capability owner
  proves read-only sync rejection and update-stream sync success natively.
- `decal_save_to_file` writes binary-mode canonical LF bytes.
- Sprite restoration now treats either committed move state as completed restoration. A Windows
  durability warning on `backup -> target` no longer becomes false transaction-incomplete;
  actual non-commit still does.
- Ordinary tests for scene, decal, sprite, flow, UI document/workspace, and UI preference saves
  accept either documented committed result while preserving exact explicit warning and failure
  tests, byte assertions, identity, dirty state, history, and cleanup checks.

### Native stack bounds

Several tests placed two very large editor/workspace snapshots on the stack. Windows exited those
tests with shell status 127 before cmocka could report an assertion. Reopened
`UnifiedEditorState`, full editor snapshots, and retained `FlowWorkspace` snapshots now use
explicit heap ownership in only the affected tests. Each allocation is checked, destroyed where
required, and freed. Product stack policy was not enlarged globally.

## Regression coverage

- Make-policy regression: Linux exclusion plus ordered Windows application/test ENet libraries.
- Native platform capability: read-only file sync is access denied; `r+b` sync succeeds.
- Scene and command owners: replacement, exact bytes, dirty/saved state, and failure preservation.
- Decal I/O/document/refresh: canonical bytes, write-capable sync, warning commits, and registry
  refresh.
- Sprite document/editor: warning commits, rollback restoration, incomplete recovery, canonical
  manifest/frame bytes, and registry/runtime publication.
- Flow/UI/preferences: warning commits retain path, saved snapshots, history, byte content, and
  truthful results.
- Unified editor: 99 workflows, including all large-state round trips and state-preservation
  snapshots.

## Verification

| Check | Result |
|---|---|
| Native platform preflight | `PASS`; platform 13/13, number 5/5, map catalog 7/7, scene format 24/24 |
| Native strict application build | `PASS`, `-Wall -Wextra -Wpedantic -Werror` |
| Native strict test build | `PASS`, all 64 registered runners |
| Native `make test` | `PASS`, 64 runner invocations |
| Native `standards-core` | `PASS` |
| Native binary inspection | `PASS`, application + 64 tests; PE x86-64; no MSYS/Cygwin runtime |
| Native VM lifecycle | `PASS`, `shut off` -> `shut off` |
| Local strict GCC aggregate | `PASS`, all 64 runners |
| Local strict GCC application | `PASS`, warnings clean under `-Werror` |
| Local `standards-core` | `PASS` |
| Local platform harness | `PASS`, including Make link policy regression |
| Local smoke | `PASS`, `{"smoke":"ok","map_width":10,"map_height":6}` |
| Unified-editor ASan + LeakSanitizer | `PASS`, 99/99, no report |
| Unified-editor UBSan | `PASS`, 99/99, no report |

Strict Clang compiled the changed decal I/O and decal-document owners, then stopped on the
pre-existing missing final newline in `src/sprite_document_internal.h`. Full Clang and full
64-runner sanitizer rebuilds were not claimed for W5; the former belongs to C1/C2 and the latter
exceeds the bounded command window. The complete GCC aggregate and focused broad sanitizer owner
passed.

## Failures and corrections

1. The initial native failure was unresolved ENet Winsock symbols. Upstream's pinned ENet build
   also requires `winmm`; both were added through normal ordered Make policy.
2. Strict test compilation then exposed POSIX two-argument `mkdir` calls. Test-local wrappers and
   `build/` fixture roots replaced platform assumptions without changing product paths.
3. A broad patch to `test_unified_editor.c` applied with high fuzz and changed three cleanup or
   creation calls incorrectly. Immediate source review and strict execution found each mistake;
   function-anchored corrections restored the intended setup/cleanup sequence.
4. Decal integration tests used POSIX shell commands. Bounded C setup/cleanup replaced them.
5. Their map loader initially returned `NULL` because UCRT text translation made `ftell` physical
   size differ from text-mode `fread` length. Exact fixture bytes are now written in binary mode;
   production newline remediation was not broadened.
6. Native fixture setup failed under `/tmp`; test-owned templates were moved to `build/`.
7. Legacy C `rename` could not replace existing Windows destinations; the established platform
   replacement boundary now owns that operation.
8. Native save tests rejected truthful durability warnings. Ordinary committed-success
   assertions now accept both committed outcomes; explicit warning/failure tests remain exact.
9. Several status-127 exits were traced to oversized test stack frames. Moving only secondary
   full-state snapshots to checked heap ownership resolved each native crash.
10. Decal save/refresh failed because `FlushFileBuffers` received a read-only handle. `r+b` and a
    native capability regression corrected and proved the access contract.
11. Sprite publish-failure rollback misclassified a committed-warning restoration as incomplete.
    The condition now follows the accepted three-state commit model.
12. Decal exact bytes exposed UCRT CRLF output. Binary serialization restored canonical LF bytes.

## Remaining risks and next safe action

1. Proceed to **L1/W6 native Linux and Windows display/input acceptance**, recording backend,
   dimensions, scaling, devices, session context, presentation, resize, input, transitions, and
   clean teardown. Do not infer display/input success from this headless functional profile.
2. C1/C2 still require strict-compatible final newlines, including the pinned SMC revision/hash
   decision; W5 does not upgrade required Ubuntu Clang evidence.
3. P1 performance/stability and final baseline reconciliation remain open.
4. Sprite-animation locale-sensitive formatting remains an independently tracked owner.
5. Mirror disposition and final non-ASCII inventory remain required V1-0 closeout inputs.
