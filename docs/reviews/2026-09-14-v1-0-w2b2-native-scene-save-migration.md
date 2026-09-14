# V1-0 W2-B2 Native Scene Save Migration — 2026-09-14

## Outcome

**W2-B2 complete. Native scene save now uses the platform filesystem boundary for
destination metadata, temporary-file sync, replacement, and post-commit durability state.
All existing scene save results, diagnostics, recovery, identity, and dirty-state contracts
remain intact under local owner tests.**

Native Windows strict compilation now reports zero diagnostics in `scene_document.c`.
End-to-end native Windows `SceneDocument` execution remains unavailable because the real
scene runner also links `scene_format.c`, whose POSIX locale APIs are independently scheduled
for W4.

This increment follows the W2-B1 evidence in
[`2026-09-14-v1-0-w2b1-native-replacement-prototype.md`](2026-09-14-v1-0-w2b1-native-replacement-prototype.md).

## Implementation

### `src/scene_document.c`

Only native `.tscene` save platform operations changed:

- destination metadata uses `platform_fs_inspect_nofollow`;
- existing-destination metadata application uses `platform_fs_apply_metadata`;
- caller-flushed temporary streams use `platform_fs_sync_file`;
- destination publication and parent durability use `platform_fs_replace` and its explicit
  commit state;
- native error codes are copied into the existing bounded integer diagnostic field.

The scene owner still controls:

- path/name validation;
- repair blocking and document validation;
- deterministic serialization;
- same-directory temporary naming/creation;
- stream creation, writing, flushing, and close;
- completed-temporary cleanup or retention;
- document path/name, migration flags, saved state, and dirty state;
- scene result and diagnostic mapping.

No scene public result or diagnostic enum changed.

### Fault/result mapping

Existing scene fault seams remain authoritative:

| Scene fault | Preserved result/consequence |
|---|---|
| temp create | `SCENE_SAVE_TEMP_CREATE_FAILED`; destination/identity unchanged |
| write | `SCENE_SAVE_WRITE_FAILED`; temporary removed |
| flush | `SCENE_SAVE_FLUSH_FAILED`; temporary removed |
| file sync | `SCENE_SAVE_FILE_SYNC_FAILED`; temporary removed |
| close | `SCENE_SAVE_CLOSE_FAILED`; temporary removed |
| metadata/mode | `SCENE_SAVE_MODE_FAILED`; temporary removed |
| replacement | `SCENE_SAVE_REPLACE_FAILED`; completed temporary retained and diagnosed |
| directory durability | `SCENE_SAVE_OK_DURABILITY_WARNING`; identity committed and clean |

An actual platform `NOT_COMMITTED` result maps to replacement failure. A platform
`COMMITTED_DURABILITY_WARNING` updates scene identity/saved state and maps to the existing
scene durability warning. On Windows, successful existing-destination replacement therefore
remains conservative rather than being mislabeled fully durable.

### `Makefile`

`SRC_PLATFORM_PATH` and `SRC_PLATFORM_FS` are defined before and included in
`SRC_SCENE_DOCUMENT`. Every focused runner that links scene document code now receives the
platform implementations through the existing source group. Runner count remains 63.

## Preserved behavior

- Native scene format bytes and serializer order are unchanged.
- Same-directory temporary creation is unchanged.
- Existing POSIX permission preservation and new-file `0600` behavior pass unchanged.
- Every pre-commit failure preserves destination, path identity, and dirty state.
- Replacement failure retains the completed recovery temporary and reports its path.
- Post-commit durability warning updates path/name/saved state and clears dirty state.
- Unified editor continues to map the warning to `EDITOR_STATUS_DURABILITY_WARNING`.
- Legacy digit-map save is not migrated and retains its existing implementation.
- Asset/UI saves, sprite folders, directory creation, catalog enumeration, and locale
  conversion are unchanged.
- SMC, mirror behavior, checked-in assets, and benchmark budgets are unchanged.

## Verification

| Check | Result |
|---|---|
| Strict GCC scene-document runner | `PASS`, 48/48 |
| Strict Clang scene-document runner | `PASS`, 48/48 |
| Scene-document ASan + leak detection | `PASS`, 48/48, no report |
| Scene-document UBSan | `PASS`, 48/48, no report |
| Platform-capability runner | `PASS`, local 6/6 |
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
- strict application compilation contains `scene_document.c` but reports zero diagnostics
  for it;
- previous scene `fchmod` and `fsync` errors are absent;
- full product remains `FAIL-PRODUCT` at strict application compilation in the expected
  unmigrated owners:
  `decal_document`, `flow_document`, `map_catalog`, `material_document`,
  `object_document`, `scene_format`, `sprite_document`, `ui_document`,
  `ui_menu_workspace`, `ui_preferences`, and `unified_editor`;
- lifecycle cleanup passed and final VM state is `shut off`.

## Limitations and unresolved policy

1. Native end-to-end scene save was not executed. The real owner runner requires
   `scene_format.c`, which cannot compile on UCRT until W4. Current evidence combines native
   9/9 platform execution, local 48/48 owner execution, and diagnostic-free native strict
   compilation of the migrated owner.
2. `platform_fs_inspect_nofollow` exposes symlink/reparse identity, while historical POSIX
   `stat` followed a destination link. W2-B2 does not define a new scene destination-link
   policy. Ordinary-file behavior is verified; link-target save policy requires a separate
   security/compatibility decision before any deliberate change.
3. Existing-destination Windows success remains a committed durability warning. No
   crash-consistent parent-directory guarantee is claimed.
4. Same-directory temp creation and stream opening still use the current CRT/POSIX-shaped
   calls. Broad Unicode path ownership remains later v1 work; W2-B2 does not claim complete
   Unicode scene persistence.

## Next safe action

Proceed to **W2-C1 — `ui_preferences` single-file persistence migration**. Before source
changes, add/retain owner tests proving active-value preservation, destination preservation
before commit, and correct state after committed replacement. Migrate only its file sync and
replacement operations through `platform_fs`; do not combine material/object/flow/UI/decal
owners into the same increment.

W3 catalog, W4 locale conversion, pinned-SMC remediation, and P1 performance remain
independent. Do not externally timeout platform profiles.
