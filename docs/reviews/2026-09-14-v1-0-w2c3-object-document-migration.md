# V1-0 W2-C3 Object Document Atomic-Creation Migration — 2026-09-14

## Outcome

**W2-C3 complete. Object-asset creation now uses `platform_fs` for file sync and
publication. Uncommitted failure keeps `out_id` zero; committed durability warning publishes
the selected ID and exact object bytes without mutating the registry.**

This is the third owner-only W2-C increment selected by
[`../V1_0_W1_WINDOWS_PLATFORM_CAPABILITY_DECISION_2026-09-14.md`](../V1_0_W1_WINDOWS_PLATFORM_CAPABILITY_DECISION_2026-09-14.md).
Flow, authored UI, decal, sprite, directory, catalog, and locale owners remain unchanged.

## Contract extension

`OBJECT_DOCUMENT_OK_DURABILITY_WARNING` was appended to `ObjectDocumentResult`, preserving
all existing enum numbers. `object_document_result_is_committed` identifies ordinary and
warning success.

The output-ID contract is explicit:

- `*out_id` is reset to zero before validation;
- validation, write, sync, metadata, and not-committed replacement failures leave it zero;
- either committed result sets it to the selected lowest-free ID.

No production caller currently invokes `object_document_create_atomic`, so no downstream
result mapping changed in this increment.

## Implementation

### `src/object_document.c`

- Direct descriptor `fsync` is replaced by `platform_fs_sync_file`.
- `platform_fs_inspect_nofollow` determines whether the selected numeric destination exists.
- Direct `rename` is replaced by `platform_fs_replace`.
- Not-committed replacement removes the candidate and returns the existing I/O error.
- Committed durability warning returns the appended warning result after assigning `out_id`.

Name/duplicate/sprite/direction/full-capacity validation, lowest-free allocation, exact
serialization, same-directory temporary creation, stream write/flush/close, and registry
nonmutation remain owned by the object module.

### Fault injection

Added `src/object_document_internal.h` with explicit per-call sync, replacement, and
durability fault values. No global mutable failure state was introduced.

### `Makefile`

The standalone object runner now links `platform_path` and `platform_fs` explicitly. Combined
runners already receive these sources through existing scene groups. Runner count remains 63.

## Test coverage

`tests/test_object_document.c` now has four tests covering:

- all existing validation and exact output behavior;
- lowest-free selection (`2` when `1` is loaded);
- exact bytes including `front_direction=1.25` and LF endings;
- validation resetting `out_id` to zero;
- sync/replacement failure preserving stale numeric destination bytes;
- zero `out_id` on every injected uncommitted failure;
- committed-warning publication with `out_id == 2` and exact bytes;
- no object slot or registry generation mutation in success, warning, or failure paths;
- committed-result predicate classification.

## Verification

| Check | Result |
|---|---|
| Strict GCC object runner | `PASS`, 4/4 |
| Strict Clang object runner | `PASS`, 4/4 |
| Object ASan + leak detection | `PASS`, 4/4, no report |
| Object UBSan | `PASS`, 4/4, no report |
| Asset-refresh integration | `PASS`, 7/7 |
| Unified-editor runner | reached `PASS`, 97/97; terminal wrapper status after completion was unreliable, so aggregate evidence is authoritative |
| Strict GCC/default-SMC application build | `PASS` |
| Strict Clang/default-SMC application build | `PASS` |
| Complete `make test` | `PASS`, explicit status 0 and 63 pass markers |
| `make standards-core` | `PASS` |
| `make test-platform-harness` | `PASS` |
| `git diff --check` before documentation | `PASS` |

### Native Windows profile

Executed without an external timeout:

```sh
PROFILE_VM_NAME=win10-survey make platform-test-windows
```

Observed:

- isolated native platform preflight remains PE x86-64, import-clean, and 9/9 passing;
- strict application compilation includes `object_document.c` with zero diagnostics;
- its previous direct `fsync` error is absent;
- full product remains `FAIL-PRODUCT` in later owners:
  `decal_document`, `flow_document`, `map_catalog`, `scene_format`, `sprite_document`,
  `ui_document`, `ui_menu_workspace`, and `unified_editor`;
- VM cleanup passed and final state is `shut off`.

## Preserved invariants

- Validation ordering and result values are unchanged.
- Lowest-free ID derives from the borrowed registry and is not reserved by the operation.
- Exact object bytes remain unchanged.
- The registry and generation are never mutated by object creation.
- `out_id` identifies only a committed file.
- Existing enum numbers remain stable.
- No unrelated persistence owner changed.
- SMC, mirror behavior, checked-in assets, and benchmark budgets are unchanged.

## Limitations

1. The focused object fixture remains POSIX-shaped (`mkdtemp`, `unlink`, `rmdir`), so the
   owner runner was not executed natively on Windows. Native evidence is diagnostic-free
   strict owner compilation plus the 9/9 platform preflight.
2. Temporary creation and stream opening retain current CRT narrow-path calls. Broad Unicode
   object-path ownership is later v1 work.
3. `front_direction` still uses the existing `%.17g` formatting. Locale-independent
   formatting is W4; W2-C3 does not claim locale independence for object bytes.
4. Existing stale numeric destinations are replaced according to platform semantics because
   the registry remains the ID allocator authority. Reconciliation of disk-only stale object
   files is outside this portability increment.

## Next safe action

Proceed to **W2-C4 — `flow_document` persistence migration**. Preserve graph validation,
exact bytes, path identity, saved/dirty state, and transactional load. Add explicit sync,
replacement, and durability-warning tests before replacing direct calls, and update flow
workspace result handling for committed warning if required.

W2-D directories/sprite folders, W3 catalogs, W4 locale conversion, pinned-SMC remediation,
and P1 performance remain independent. Do not externally timeout platform profiles.
