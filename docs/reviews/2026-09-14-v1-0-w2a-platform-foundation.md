# V1-0 W2-A Platform Path/File Foundation — 2026-09-14

## Outcome

**W2-A implemented and locally verified. Native Windows compilation reached the known
unmigrated callers; W2-B remains blocked on the focused native replacement-semantics
prototype and runner execution.**

This increment implements the isolated foundation selected by
[`../V1_0_W1_WINDOWS_PLATFORM_CAPABILITY_DECISION_2026-09-14.md`](../V1_0_W1_WINDOWS_PLATFORM_CAPABILITY_DECISION_2026-09-14.md).
It does not migrate any document, catalog, directory-creation, or numeric-format owner.

## Implemented scope

### `platform_path`

Added:

- `src/platform_path.h`;
- `src/platform_path_internal.h`;
- `src/platform_path.c`.

The portable public API provides:

- length-aware strict UTF-8 validation;
- rejection of embedded NUL, overlong forms, surrogate encodings, invalid continuation
  bytes, and code points above U+10FFFF;
- owned opaque native paths with explicit initialize/destroy operations;
- checked UTF-8-to-native and native-to-UTF-8 conversion;
- output preservation on invalid input, allocation failure, or conversion failure.

POSIX paths retain validated bytes. `_WIN32` implementation uses strict
`MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, ...)` and
`WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, ...)` sizing/conversion passes.

### `platform_fs`

Added:

- `src/platform_fs.h`;
- `src/platform_fs_internal.h`;
- `src/platform_fs.c`.

The portable public API provides:

- typed filesystem results;
- bounded native error domain/code without mutable global state;
- explicit not-committed, committed, and committed-with-durability-warning states;
- no-follow metadata inspection;
- file metadata application;
- synchronization of an already-flushed caller-owned `FILE *`;
- a same-directory replacement prototype.

POSIX uses `lstat`, `fchmod`, `fsync`, `rename`, and parent-directory `fsync`. A successful
rename followed by a failed durability step reports committed-with-warning.

The Windows branch uses wide paths, `GetFileAttributesW`, `_get_osfhandle`,
`FlushFileBuffers`, `SetFileInformationByHandle`, `ReplaceFileW` for an existing
destination, and `MoveFileExW(..., MOVEFILE_WRITE_THROUGH)` for an absent destination.
Existing-destination Windows replacement is conservatively reported as committed with a
durability warning pending the W2-B native prototype. This is an implemented prototype,
not a claim that all target filesystems provide atomic/durable replacement.

### Fault injection

Internal headers expose explicit per-call fault values for path allocation/conversion and
filesystem inspect, metadata, sync, replace, and durability operations. No global failure
state or production logging was added.

### Focused runner and inventory

Added `tests/test_platform_capabilities.c` with six deterministic tests covering:

- multilingual UTF-8/native round trip;
- malformed UTF-8 and embedded-NUL rejection;
- output preservation under injected path failures;
- ordinary file, directory, and POSIX symbolic-link no-follow metadata;
- sync and permission application plus typed injected failures;
- existing and absent destination replacement;
- pre-commit destination/temp preservation;
- injected post-commit durability warning;
- invalid-argument output preservation.

Registered `build/test-platform-capabilities` in `Makefile`. The canonical inventory is now
63 runners and Windows native output count is 64 including `ascii-fps.exe`. Updated:

- `tools/platform-profiles/windows_product.py`;
- `tools/platform-profiles/test-windows-product.py`;
- `tools/platform-profiles/test-windows-guest.py`.

Historical records that accurately describe earlier 62-runner evidence were not rewritten.

## Preserved invariants

- No document, editor, catalog, numeric format, or authored file behavior changed.
- No production owner calls the new modules yet.
- Serialization, path identity, dirty/saved state, histories, diagnostics, and cleanup remain
  with their current owners.
- No process-global locale or platform last-error state was introduced.
- Windows-specific headers and operations occur only in platform implementations.
- ANSI/OEM Win32 path APIs are not used.
- Sprite directory publication is not represented as a single-file transaction.
- Pinned SMC and mirror sources were not changed.

## Verification evidence

| Check | Result |
|---|---|
| Strict GCC focused runner | `PASS`, 6/6 |
| Strict Clang focused runner | `PASS`, 6/6 |
| Focused ASan + leak detection | `PASS`, 6/6, no report |
| Focused UBSan | `PASS`, 6/6, no report |
| Strict GCC/default-SMC application build | `PASS`, `-Werror` clean |
| Local Clang 22/default-SMC application build | `PASS`, `-Werror` clean |
| Complete `make test` | `PASS`, explicit status 0 and 63 pass markers |
| `make standards-core` | `PASS` |
| `make test-platform-harness` | `PASS`, including Windows guest/product fixtures |
| `git diff --check` before documentation | `PASS` |

### Native Windows profile

The first invocation omitted the required machine-local selector and correctly returned:

```text
FAIL-MISSING-TOOL: profile=windows-10-x64-gcc phase=vm-locate
reason=windows-vm-not-configured
```

Read-only `qemu:///session` enumeration showed the sole local domain `win10-survey`, matching
prior evidence. The configured invocation was then run without an external timeout:

```sh
PROFILE_VM_NAME=win10-survey make platform-test-windows
```

Observed result:

- exact 63-runner inventory accepted, including `test-platform-capabilities`;
- strict application compilation included `src/platform_fs.c` and `src/platform_path.c`;
- no diagnostic names either new platform module;
- profile remains `FAIL-PRODUCT` at strict application build only in known unmigrated
  `fsync`/`fchmod`, catalog, POSIX locale, and two-argument `mkdir` callers;
- cleanup passed through QGA and final VM state is `shut off`.

Because the unchanged profile stops at the first strict application failure, the new focused
Windows executable was not built or run. Native replacement, metadata, sharing, same-volume,
reparse, and runtime UTF-8 behavior remain unverified.

Evidence is retained under
`build/platform-profiles/windows-10-x64-gcc/`, especially `test-inventory.log`,
`strict-app-build.log`, `result.env`, and `vm-lifecycle.log`.

## Failures and corrections

1. The first focused test asserted that `fputs` returns the number of bytes. GCC and Clang
   both built cleanly, then the same assertion failed. C guarantees only a nonnegative
   success result; the assertion was corrected without weakening the tested write/sync
   behavior.
2. A verification batch incorrectly ran multiple writers against `build/` concurrently and
   the GCC runner hit `Text file busy`. Remaining build-directory operations were serialized.
3. One long foreground aggregate reached all 63 passing runners but the terminal wrapper
   closed with an unreliable status. It was not counted. A background run wrote its own
   status file and completed with status 0 and 63 pass markers.
4. The first Windows command omitted `PROFILE_VM_NAME`. The configured rerun reached product
   compilation and produced the evidence above; tracked profile configuration was not
   modified.

## Remaining risks and stop condition

W2-A proves local contracts and strict native compilation of the platform modules. It does
not prove the Windows replacement guarantee. Do not migrate `scene_document` or describe
Windows replacement as atomic/durable until the W2-B prototype executes on the native VM
and records:

- absent and existing destination publication;
- destination attributes and read-only behavior;
- sharing/access failures;
- same-volume and cross-volume behavior;
- temporary/destination state for every failure;
- the strongest supportable durability claim;
- native reparse and multilingual path cases.

## Next safe action

Create a focused native Windows replacement-prototype execution path that can build and run
the isolated 63rd runner before the still-failing full application build, or migrate only
the minimum W2-B native-scene caller slice after its failure tests are written. The preferred
next action is the isolated native prototype first, so semantics are evidenced before
document code changes.

Pinned-SMC remediation and P1 performance remain independent. Do not externally timeout
platform profiles.
