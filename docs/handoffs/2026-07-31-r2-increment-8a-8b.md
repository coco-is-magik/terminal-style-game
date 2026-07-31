# R2 Increment 8A/8B Handoff — 2026-07-31

## Task objective and status

Implement the R2 **editor workflows** increment (plan increment 8): New/Open/Import/Save/
Save As/Reload/overwrite/repair/dirty-close/window-close and input-consumption tests.

**Status: Partially complete — Increment 8 is NOT yet Verified.** Increments 8A and 8B as
scoped this session are implemented with passing focused tests, but several user-visible
plan contracts for increment 8 are not wired into the running editor yet. R2 remains
**Active** in the roadmap. A read-only review on 2026-07-31 found gaps G1–G5 below that
must close before Q3 manual checks and Review C.

Authoritative current status: `FEATURE_ROADMAP.md` (R2 = Active), the implementation
record, and this handoff. Do not rely on
`EDITOR_UNIFICATION_STATUS_REPORT_2026-07-27.md` for current R2 state — it is a
historical R0-era report.

## Accepted requirements (plan increment 8, `R2_REQUIREMENTS_AND_IMPLEMENTATION_PLAN_2026-07-31.md` L115-128)

- Native root: `assets/scenes`; extension: lowercase `.tscene`.
- Ctrl+N: New; Ctrl+O: Open/Import chooser; Ctrl+S: Save; Ctrl+Shift+S: Save As; F5: Reload.
- Chooser lists native scenes first and exposes legacy `.txt` files through a visibly
  separate **Import legacy map…** action.
- Save As accepts only `[A-Za-z0-9_-]{1,64}`, derives the scene display name, writes
  `assets/scenes/<name>.tscene`, and confirms replacement of an existing file.
- Dirty Open, New, Reload, editor exit, and window close all use Save/Discard/Cancel.
  A failed operation cannot discard or switch the document.
- Repair mode displays a persistent banner and unresolved references, uses visible
  fallback patterns, disables Save, and permits explicit replacement with a loaded
  decal asset ID. R2 does not add decal placement editing.

## Work completed (verified, strict + sanitizers)

### Increment 8A — typed transactional editor operations

- `scene_document_create_new`: exact 10×6 map, config default-material border, interior 0,
  name `untitled`, config ambient, spawn `1.5,1.5,0`, `next_instance_id = 1`, no native
  path, `imported_unsaved = true`, state 1/0 dirty. Transactional via candidate + commit.
- `UnifiedEditorState` now owns a derived `WorldState runtime_world`; init/destroy and all
  load/import/open/new paths stage and commit document + runtime together (transactional).
- Public operations: `unified_editor_open_native`, `unified_editor_import_legacy`,
  `unified_editor_new_scene`, `unified_editor_save_as`.
- `unified_editor_has_document` now detects valid map ownership, not path, so pathless
  New/import documents are active.
- Save routes to native durable save for `.tscene` paths, legacy otherwise; pathless Save
  returns `SCENE_SAVE_NO_PATH`. Save As uses the durable native API and maps
  repair/durability statuses.
- Tests: `test_scene_document.c` 32/32; `test_unified_editor.c` 33/33.

### Increment 8B — chooser seams, dirty-close, Save As plumbing

- `map_catalog_refresh_native` / `map_catalog_refresh_extension` bounded, regular-file only,
  lowercased, sorted. Legacy `map_catalog_refresh` (`.txt`) preserved. Tests 5/5.
- Input: added `editor_save_as_pressed` (Ctrl+Shift+S), `editor_import_pressed` (Ctrl+I),
  `editor_new_pressed` (Ctrl+N). Tests 7/7.
- Editor state: `EditorChooserKind`, `EditorPendingAction`, dirty-prompt continuation for
  New/chooser-load/exit/close, bounded `save_as_name[65]` + `save_as_path[1024]` (no heap),
  `EDITOR_MODAL_SAVE_AS` + `EDITOR_MODAL_OVERWRITE_PROMPT`, `request_window_close`.
- `unified_editor_request_new` / `unified_editor_request_window_close` route through the
  same dirty Save/Discard/Cancel path; pathless dirty Save opens Save As before continuing
  the destructive action.
- Save As name validation `[A-Za-z0-9_-]{1,64}`, path `assets/scenes/<name>.tscene`,
  `stat`-based overwrite confirm, then durable save.
- `app.c`: SDL_QUIT diverted to `unified_editor_request_window_close` while editor active;
  editor return-to-menu and window-close honored; editor launch still uses `assets/maps`
  (config has no map_root field).

### Base R2 foundation already Verified earlier (increments 1–7)

Domain/types/diagnostics/IDs, strict native v1 parser + canonical serializer, transactional
load/import, reusable decal repair/fallback, derived runtime adapter, durable native
Save/Save As with fault injection. See the implementation record ledger for each row.

## Verification evidence

- Strict application build (`make`) passes with `-Wall -Wextra -Wpedantic -Werror`.
- Focused strict runners pass: scene-document 32/32, unified-editor 33/33, scene-format
  11/11, map-catalog 5/5, input 7/7, command-system 17/17.
- `make check` passes all non-interactive suites.
- Combined ASan + UBSan + leak detectors pass on scene-document 32/32 and
  unified-editor 33/33.

## Review findings 2026-07-31 (read-only, code vs plan) — must be resolved

| ID | Plan contract | Plan ref | Observed | Severity |
|---|---|---|---|---|
| G1 | Ctrl+O = Open/Import chooser listing native first with separate **Import legacy map…** action | L118-121 | Ctrl+O calls `begin_map_open` (legacy); Ctrl+I calls `begin_legacy_import`. `begin_native_open` is defined but never called from any input path | Blocker for Q3/exit |
| G2 | Repair mode displays unresolved references | L126-127 | Overlay shows only `Status: Repair required before Save`; does not list missing decal refs | Gap |
| G3 | Repair permits explicit replacement with a loaded decal asset ID | L126-128 | `scene_document_internal_replace_decal_asset` exists but no public editor API/modal/input invokes it | Blocker for Q3 |
| G4 | Save As / Overwrite modals are rendered | L122-124 | Modal logic exists but overlay only draws EXIT/RELOAD/MAP_CHOOSER/DIRTY; no on-screen Save As/Overwrite prompt | Gap |
| G5 | Ctrl+O should open Open/Import chooser | L118 | Reverted to legacy Ctrl+O; record defers native chooser to "future menu action" | Deviation |

### Documentation note

The implementation-record failure note (R2_IMPLEMENTATION_RECORD_2026-07-31.md:238-244)
admits G1/G5, yet the ledger marks **8B Pass**. The plan contract for increment 8 is
broader than what this session's 8A/8B delivered. Treat this handoff's gap table as the
authoritative list of remaining increment-8 work, and correct the ledger row when G1–G4
are closed.

## Rejected approaches

- Moving Ctrl+O to native-only (broke the legacy map-open contract; see record failure
  note). The combined Open/Import chooser must preserve legacy access via a separate
  Import action rather than replacing Ctrl+O outright.
- Direct-destination writes, force-save on repair, in-place legacy conversion: remain
  forbidden by the plan.

## Next action (one visible gap)

1. Wire the combined Open/Import chooser: make Ctrl+O open a chooser rooted at
   `assets/scenes` listing native scenes first, with a separate **Import legacy map…**
   action that swaps to the legacy catalog.
2. Add a focused regression test asserting Ctrl+O shows native entries and the Import
   action is reachable, and that dirty-close still routes through Save/Discard/Cancel.
3. Then resolve G2–G4 (repair reference list + repair-replace editor API + Save As/
   Overwrite overlay rendering).

## Recovery guidance

- The 8A/8B machinery and tests are sound; do not tear them out. Only the missing editor
  wiring (G1–G4) stands between here and a complete increment-8 implementation.
- Keep `begin_native_open` and `begin_legacy_import`; both are needed for the combined
  chooser.
- Preserve legacy `.txt` access through the open workflow — do not regress the
  `test_ctrl_o_and_catalog_failure_preserve_document` contract.
- Rerun the focused strict runners and the two sanitizer runners after wiring changes,
  then update the implementation-record ledger row for 8B to reflect the closed gaps.

## Stable-reference guidance

- Plan: `R2_REQUIREMENTS_AND_IMPLEMENTATION_PLAN_2026-07-31.md`
- Format: `R2_NATIVE_SCENE_V1_SPEC_2026-07-31.md`
- Evidence: `R2_IMPLEMENTATION_RECORD_2026-07-31.md`
- Roadmap status: `FEATURE_ROADMAP.md` (R2 = Active; not Verified until Q3 + Review C)