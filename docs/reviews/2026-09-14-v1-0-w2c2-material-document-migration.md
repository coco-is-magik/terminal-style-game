# V1-0 W2-C2 Material Document Persistence Migration — 2026-09-14

## Outcome

**W2-C2 complete. Material file sync and replacement now use `platform_fs`; committed
durability warning updates Save/Save As identity and saved state instead of being
misclassified as an uncommitted I/O failure.**

This is the second owner-only W2-C increment selected by
[`../V1_0_W1_WINDOWS_PLATFORM_CAPABILITY_DECISION_2026-09-14.md`](../V1_0_W1_WINDOWS_PLATFORM_CAPABILITY_DECISION_2026-09-14.md).
Object, flow, authored UI, decal, sprite, directory, catalog, and locale owners remain
unchanged.

## Contract extension

`MATERIAL_DOCUMENT_OK_DURABILITY_WARNING` was appended to `MaterialDocumentResult`,
preserving every previous enum value. A public
`material_document_result_is_committed` predicate classifies ordinary success and
durability-warning success as committed while rejecting I/O and validation failures.

This predicate is used by both the material owner and `asset_refresh`, preventing downstream
code from treating a replaced file as uncommitted.

## Implementation

### `src/material_document.c`

- Direct descriptor `fsync` is replaced by `platform_fs_sync_file`.
- `platform_fs_inspect_nofollow` determines destination existence.
- Direct `rename` is replaced by `platform_fs_replace`.
- Not-committed replacement removes the temporary and returns the existing
  `MATERIAL_DOCUMENT_IO_ERROR`.
- Committed durability warning returns the appended warning result.
- `material_document_save` updates `saved_value` and marks clean for either committed result.
- `material_document_save_as` updates owned path, `saved_value`, and clean state for either
  committed result.

Serialization, validation, temporary creation, stream writing/flushing/closing, history,
and explicit registry commit remain owned by `MaterialDocument`.

### Fault injection

Added `src/material_document_internal.h` with explicit per-call sync, replacement, and
durability fault values. No global failure state was introduced.

### `src/asset_refresh.c`

Material save-and-refresh proceeds after either committed material result. It still rejects
validation or I/O failure before refreshing. Registry loading, generation, scene diagnostic
refresh, and rollback remain unchanged.

### `Makefile`

The standalone material runner links `platform_path` and `platform_fs` explicitly. Combined
asset-refresh/unified-editor runners already receive those sources through their existing
scene source group, avoiding duplicate definitions. Runner count remains 63.

## Test coverage

`tests/test_material_document.c` now has eight tests. New assertions prove:

- exact material bytes remain `id`, `palette`, and four glyphs with LF endings;
- sync and replacement failures preserve previous destination bytes;
- pre-commit failure preserves dirty state, null Save As path, and the previous saved
  snapshot;
- durability-warning commit publishes exact new bytes;
- committed warning updates Save As path and saved snapshot and marks the document clean;
- registry generation remains unchanged until explicit registry commit;
- the committed-result predicate accepts only ordinary/warning success.

Existing create/edit/undo/redo/discard, validation, open, replacement-ID, preview, Save As,
and registry commit coverage remains intact.

## Verification

| Check | Result |
|---|---|
| Strict GCC material runner | `PASS`, 8/8 |
| Strict Clang material runner | `PASS`, 8/8 |
| Material ASan + leak detection | `PASS`, 8/8, no report |
| Material UBSan | `PASS`, 8/8, no report |
| Asset-refresh integration | `PASS`, 7/7 |
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
- strict application compilation includes `material_document.c` with zero diagnostics;
- its previous direct `fsync` error is absent;
- full product remains `FAIL-PRODUCT` in later owners:
  `decal_document`, `flow_document`, `map_catalog`, `object_document`, `scene_format`,
  `sprite_document`, `ui_document`, `ui_menu_workspace`, and `unified_editor`;
- VM cleanup passed and final state is `shut off`.

## Preserved invariants

- Material bytes and parser compatibility are unchanged.
- Save/Save As do not mutate the registry.
- Registry mutation and generation bump remain explicit in
  `material_document_commit_to_registry` or asset refresh.
- Pre-commit failure preserves destination, saved snapshot, dirty state, and Save As path.
- Committed warning is not reported as uncommitted.
- Existing enum numeric values remain stable.
- No unrelated persistence owner changed.
- SMC, mirror behavior, checked-in assets, and benchmark budgets are unchanged.

## Limitations

1. The focused material fixture remains POSIX-shaped (`mkdtemp`, `unlink`, `rmdir`), so the
   owner runner was not executed natively on Windows. Native evidence is diagnostic-free
   strict owner compilation plus the 9/9 native platform preflight.
2. Temporary creation and stream opening retain current CRT narrow-path calls. Broad Unicode
   material-path ownership is later v1 work.
3. Existing-destination Windows replacement normally returns the committed durability
   warning; stronger crash-durability is not claimed.
4. `asset_refresh` has no material-warning output channel. It correctly continues after a
   committed save, but does not expose that durability warning to the editor. Broader warning
   propagation requires a separately reviewed UI/result contract.

## Next safe action

Proceed to **W2-C3 — `object_document` atomic-creation migration**. Preserve lowest-free ID
selection, duplicate/name/sprite validation, `out_id == 0` on every uncommitted failure,
exact bytes, and absence of registry mutation. Add explicit sync/replacement/durability fault
tests before replacing direct calls.

W2-D directories/sprite folders, W3 catalogs, W4 locale conversion, pinned-SMC remediation,
and P1 performance remain independent. Do not externally timeout platform profiles.
