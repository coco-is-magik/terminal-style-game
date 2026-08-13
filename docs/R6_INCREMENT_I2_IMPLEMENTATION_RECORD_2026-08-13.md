# R6 Increment I2 Implementation Record — 2026-08-13

## Status

**Complete and verified.** Undoable placed-decal editing is implemented through the
authoritative scene command history, including reusable-pattern placement on
floor/ceiling/wall faces, a nested surface Decals menu, create-empty pattern flow,
typed decal inspector, and confirm-gated removal. I2's deterministic exit gate and
Q2 pass; aggregate tests, sanitizers, the build matrix, native round-trip, and
headless smoke also pass. R6 I1 and I2 are both complete, so R6 is Verified.

## Delivered behavior

1. Surface inspector fields include a **Decals** row that opens a nested submenu.
2. The Decals submenu lists one item per placed decal on that surface (matched by
   surface kind, grid anchor, and rotation), plus an **Add decal…** action.
3. **Add decal…** creates a new empty decal pattern asset (`1 × 1` default with
   editable columns/rows), saves it under `<asset_root>/decals`, refreshes the
   registry, and places a straight-up canvas instance on the selected surface.
4. Placed decal instances are editable through a typed inspector: width, height,
   rotation, depth, glyph step, and surface-local U/V position.
5. The inspector exposes a nonnumeric **Remove** row; selecting it opens a confirm
   prompt. Enter removes the instance atomically; Esc makes no mutation.
6. Placement, property edits, and removal are undoable/redoable through the single
   scene command history. Native Save/Open persists them without a format bump.
7. Wall decal orientation remains compatible with v4: east/west faces use
   `side = 0`, south/north faces use `side = 1`, with west/north anchored on the
   neighboring grid line and rotation selecting the face normal.

## Implementation

- `scene_document_internal` helpers for decal set/insert/remove provide stable-ID
  lookup, validated indices, bounded dynamic storage growth, and order preservation.
- `EDITOR_MUTATION_SET_DECAL`, `EDITOR_MUTATION_INSERT_DECAL`, and
  `EDITOR_MUTATION_REMOVE_DECAL` carry snapshots for undo/redo, validate bounds and
  stable IDs, and merge only same-ID property sets.
- A shared scene-wide ID collision guard checks new decal and light IDs against
  `document->next_instance_id` and existing instances; allocation failure rolls
  back `next_instance_id` and leaves history unchanged.
- `editor_command_commit_runtime` is the single boundary for runtime-edit failures
  (placement, property edits, removal): a failed runtime rebuild rolls the
  document and history back to the pre-commit state.
- Wall-face canonical mapping preserves the legacy v4 `side = 0/1` encoding plus
  authored rotation; opposite faces of the same wall grid line are distinguished
  by rotation, so they are not conflated during surface submenu matching.
- The create-empty pattern flow is deterministic: default `1 × 1`, editable
  columns/rows in the add-decal modal, save to `<asset_root>/decals`, registry
  refresh, then immediate placement. Tests isolate the registry refresh and
  restore the baseline asset registry afterward.
- The decal inspector uses typed numeric fields with bounds and a choice-only
  Remove field; surface-aware Position U/V semantics depend on the decal's surface.
- Selection uses `SELECTION_DECAL` with a dedicated `EditorInspectorMode` and modal
  prompt `EDITOR_MODAL_DECAL_REMOVE_PROMPT`.

## Verification evidence

Final checks run on 2026-08-13:

| Check | Result | Meaning |
|---|---|---|
| `make -B all` | **Pass** | Strict C11 application build passed with `-Wall -Wextra -Wpedantic -Werror`. |
| `test-scene-document` | **Pass: 43/43** | Decal set/insert/remove ordering, stable IDs, and capacity growth included. |
| `test-command-system` | **Pass: 35/35** | Decal set/insert/remove undo/redo, shared-ID collision guard, and ID-allocation rollback included. |
| `test-editor-domain` | **Pass: 9/9** | Decal inspector field presentation, bounds, and Remove choice behavior included. |
| `test-unified-editor` | **Pass: 70/70** | Create-empty flow, placement on floor/ceiling/wall faces, opposite-face matching, confirm/cancel removal, runtime rollback, undo/redo, overlay, and native round-trip included. |
| `test-input` | **Pass: 12/12** | Existing placement-action edges unchanged. |
| `make test` | **Pass: 426/426 across 31 suites** | Aggregate regression suite passes; no unrelated regressions. |
| `make asan` | **Pass** | Complete suite passed under AddressSanitizer. |
| `make ubsan` | **Pass** | Complete suite passed under UndefinedBehaviorSanitizer. |
| `make matrix` | **Pass: 8/8 configurations** | Tracker, lighting-cache, and glyph-cache variants passed. |
| `make smoke` | **Pass** | Headless application smoke returned `{"smoke":"ok","map_width":10,"map_height":6}`. |

## Static audit

Audited on 2026-08-13:

- All `SELECTION_DECAL`, `EDITOR_INSPECTOR_DECAL`, and
  `EDITOR_MODAL_DECAL_REMOVE_PROMPT` switch branches are handled in the editor
  dispatch, input, and overlay paths.
- No production code mutates `document->decals[...]` outside
  `scene_document_internal_*` helpers; the command system is the only caller.
- `EDITOR_SURFACE_FIELD_DECALS` is presented in the surface inspector and opens
  the nested Decals submenu for every surface kind.

## Remaining follow-up

An attended visual playthrough was not performed in this headless agent session.
The production controller paths, overlay text, runtime-world refresh, modal input,
and application startup are covered deterministically/headlessly. A later human
review may additionally confirm visual placement and decal orientation feel; this
is not an unresolved I2 correctness blocker.

Interactive decal-pattern painting UI remains deferred past R6, as decided in
`R6_DECISION_RECORD_2026-08-13.md`.

## Research boundary

This review inspected the R6 decision/plan documents, roadmap status, affected
scene document, command system, editor domain, unified editor, and relevant tests.
It did not review unrelated subsystems.
