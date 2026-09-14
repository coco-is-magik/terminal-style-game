# V1-0 W2-C1 UI Preferences Persistence Migration — 2026-09-14

## Outcome

**W2-C1 complete. UI preference file sync and replacement now use `platform_fs`.
Pre-commit failure remains active-but-not-saved; committed durability warning is now a
distinct saved outcome rather than being misreported as a failure.**

This is the first remaining single-file owner increment ordered by
[`../V1_0_W1_WINDOWS_PLATFORM_CAPABILITY_DECISION_2026-09-14.md`](../V1_0_W1_WINDOWS_PLATFORM_CAPABILITY_DECISION_2026-09-14.md).
It does not migrate any asset document, authored UI document, sprite folder, directory
creation, catalog, or locale operation.

## Contract decision

The pre-W2 API could represent only `UI_PREFERENCES_IO_OK` or
`UI_PREFERENCES_IO_FAILED`. The platform adapter can truthfully report a third state:
replacement committed but durability could not be fully established.

Two enum values were appended without renumbering existing values:

- `UI_PREFERENCES_IO_OK_DURABILITY_WARNING`;
- `UI_PREFERENCES_CHANGE_SAVED_DURABILITY_WARNING`.

Collapsing this state to `FAILED` would incorrectly report `ACTIVE_NOT_SAVED` after the
destination changed. Collapsing it silently to `OK` would hide the durability warning. The
application feedback now distinguishes:

- `active; preference not saved` for pre-commit failure;
- `saved; durability warning` for committed replacement with incomplete durability proof.

## Implementation

### `src/ui_preferences.c` and headers

- `platform_fs_sync_file` replaces direct descriptor `fsync`.
- `platform_fs_inspect_nofollow` determines whether the destination exists.
- `platform_fs_replace` replaces direct `rename` and exposes commit state.
- Not-committed replacement removes the temporary and returns the existing failure result.
- Committed-with-warning returns the new warning result.
- The active scale remains selected for every persistence outcome.
- The version-1 serialized bytes remain unchanged.

Added `src/ui_preferences_internal.h` with explicit per-call sync, replacement, and
durability fault seams. It is private test infrastructure; no global mutable fault state was
introduced.

### `src/app.c`

UI scale feedback recognizes the new committed-warning result and no longer describes a
committed preference as unsaved.

### `Makefile`

`SRC_UI_PREFERENCES` now includes `platform_path` and `platform_fs`, so every dependent
focused runner receives the implementations through the existing source group. Runner count
remains 63.

## Tests

`tests/test_ui_preferences.c` now has six tests. New coverage proves:

- injected file-sync failure preserves previous destination bytes;
- injected replacement failure preserves previous destination bytes;
- both pre-commit failures retain the selected active scale and return
  `UI_PREFERENCES_CHANGE_ACTIVE_NOT_SAVED`;
- committed durability warning publishes the new bytes;
- committed warning retains the selected active scale and returns
  `UI_PREFERENCES_CHANGE_SAVED_DURABILITY_WARNING`;
- `last_save_result` records the matching typed I/O result.

Existing precedence, invalid-file transactionality, endpoint-no-write, transition/reset,
format, and blocked-destination behavior remain covered.

## Verification

| Check | Result |
|---|---|
| Strict GCC UI-preferences runner | `PASS`, 6/6 |
| Strict Clang UI-preferences runner | `PASS`, 6/6 |
| UI-preferences ASan + leak detection | `PASS`, 6/6, no report |
| UI-preferences UBSan | `PASS`, 6/6, no report |
| Dependent UI-compositor runner | `PASS`, 7/7 |
| `make test-ui-standards` | `PASS` |
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

- native platform preflight remains PE x86-64, import-clean, and 9/9 passing;
- strict application compilation includes `ui_preferences.c` with zero diagnostics;
- its prior direct `fsync` error is absent;
- full product remains `FAIL-PRODUCT` in later unmigrated owners:
  `decal_document`, `flow_document`, `map_catalog`, `material_document`,
  `object_document`, `scene_format`, `sprite_document`, `ui_document`,
  `ui_menu_workspace`, and `unified_editor`;
- VM cleanup passed and final state is `shut off`.

## Preserved invariants

- Preference format remains exactly:
  `version = 1` and `ui_scale_percent = <preset>` with LF line endings.
- Immutable-default/user precedence and invalid-file fallback are unchanged.
- Endpoint no-change performs no write.
- Failed persistence never rolls back the active in-memory scale.
- Pre-commit failure does not replace the destination.
- No unrelated public enum values changed.
- Material, object, flow, authored UI, decal, sprite, catalog, directory, and locale owners
  are untouched.
- SMC, mirror behavior, checked-in assets, and benchmark budgets are unchanged.

## Limitations

1. The focused preferences fixture remains POSIX-shaped (`mkdtemp`, mode-bearing `mkdir`,
   and POSIX timestamp fields), so the full owner runner was not executed natively on
   Windows. Current native evidence is diagnostic-free strict owner compilation plus the
   passing native platform preflight.
2. Temporary file creation and stream opening retain their existing CRT narrow-path form.
   Broad Unicode preference-path ownership is later v1 work.
3. Existing-destination Windows replacement normally produces the new committed durability
   warning because stronger crash-durability evidence is not claimed.
4. Destination symlink/reparse policy remains an explicit later compatibility/security
   decision, consistent with W2-B2.

## Next safe action

Proceed to **W2-C2 — `material_document` persistence migration** as a separate owner
increment. Before migration, add deterministic pre-commit and committed-warning tests and
decide the smallest compatible result extension needed to avoid treating committed data as
unsaved. Preserve saved snapshot, dirty state, Save As identity, registry ownership, and
exact material bytes.

W2-D directories/sprite folders, W3 catalogs, W4 locale conversion, pinned-SMC remediation,
and P1 performance remain independent. Do not externally timeout platform profiles.
