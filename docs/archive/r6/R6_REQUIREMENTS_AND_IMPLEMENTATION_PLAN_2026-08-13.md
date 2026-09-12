# R6 Requirements and Implementation Plan — 2026-08-13

## Status

**Verified; I1 and I2 complete.** Phase R6 (Decal placement and point-light
authoring) is complete and its product/architecture decisions remain locked in
`R6_DECISION_RECORD_2026-08-13.md`. R5 is Verified (I1–I4, Q1–Q3, Review E) and
R6 builds directly on its reusable asset-document and eager-refresh foundation.
I1's and I2's deterministic exit gates and Q2 pass; aggregate tests, sanitizers,
the build matrix, native round-trip, and headless smoke also pass. Evidence is
recorded in `R6_INCREMENT_I1_IMPLEMENTATION_RECORD_2026-08-13.md` and
`R6_INCREMENT_I2_IMPLEMENTATION_RECORD_2026-08-13.md`. R7 is next.

## Scope

Author placed visual/environment content: point-light creation/deletion with the
existing inspector edit model, and decal placement/orientation on surfaces.
Pattern content stays authored through the existing headless `decal_painter` and
`assets/decals/*` files (interactive painting UI is explicitly deferred past R6).

## Locked decisions

See `R6_DECISION_RECORD_2026-08-13.md` for:

- Two increments: lights first (I1), decals second (I2).
- Lights placed centered in the hovered cell (wall face → adjacent cell center).
- Light deletion reuses the wall-removal confirm flow; no dedicated placement mode.
- Decals placed as a straight-up canvas via the surface submenu; dimensions/orientation/position editable afterward.
- Decal selection via surface → Decals submenu (one item per decal on that surface).
- Interactive decal painting UI deferred; no scene version bump (v4 already covers everything).

## Increments

### I1 — Point-light creation and deletion

**Status:** Complete and verified. See
`R6_INCREMENT_I1_IMPLEMENTATION_RECORD_2026-08-13.md` for implementation and
verification evidence.

**Goal:** Users can place a new point light at the hovered cell, then delete a
selected light through the inspector, both as atomic undoable commands.

**Tasks:**

1. Add insert-light and remove-light internal mutations:
   - `scene_document_internal_insert_light(doc, index, const SceneLight *)`
   - `scene_document_internal_remove_light(doc, index, SceneInstanceId expected_id)`
   (Mirror the existing `insert_decal`/`remove_decal` helpers in
   `scene_document_internal.h`; only `SET_LIGHT` exists for lights today.)
2. Add `EDITOR_MUTATION_INSERT_LIGHT` and `EDITOR_MUTATION_REMOVE_LIGHT` to
   `command_system.h`, with validation (non-zero stable ID, in-bounds position,
   capacity `SCENE_MAX_LIGHTS = 64`), undo/redo snapshots, and mutation
   collision/merge rules parallel to existing light handling.
3. Editor place flow (no dedicated mode):
   - Hover a cell → press `L` to place centered in the hovered cell (wall-face
     hover → adjacent cell center). Enter remains inspector/modal confirmation.
   - New light enters a fresh inspector for immediate refinement.
   - Insertion at full capacity rejects and reports; never silently drops.
4. Editor delete flow: reuse the wall-removal confirm flow. Select the light → a
   **"remove" menu item** asks to confirm → one atomic remove command.
5. Update `editor_domain`/`unified_editor` overlay for the new inspector action,
   and add the remove-light confirmation prompt.

**Exit gate:** Deterministic tests cover place (cell + wall-face adjacent),
capacity-full rejection, delete confirm/cancel, undo/redo of insert/remove,
stable-ID preservation, and reload round-trip. Strict build and Q2 pass.

**Estimated effort:** 0.5–1 day.

### I2 — Decal placement and orientation (placement-first)

**Status:** Complete and verified. See
`R6_INCREMENT_I2_IMPLEMENTATION_RECORD_2026-08-13.md` for implementation and
verification evidence.

**Goal:** Users can associate a reusable decal pattern asset with a surface
(create-empty flow) and place/orient the canvas on a wall/floor/ceiling; select
and delete placed instances. Pattern content editing stays headless.

**Tasks:**

1. Decal create-empty flow: new decal pattern asset before placement (reuse
   `DecalDocument.create` + asset refresh + decal shortlist from R5 I3).
2. Placement flow via the surface submenu:
   - Select a surface (wall/floor/ceiling) → **Decals** submenu lists one item per
     decal on that surface, plus an **Add decal…** action.
   - Add flow sets initial dimensions; the canvas defaults to a straight-up
     orientation on the surface.
   - Dimensions/orientation/position/size are editable afterward with the
     existing inspector tools.
3. Selection: surface → Decals submenu → pick the placed instance by stable ID.
4. Deletion: reuse the confirm-gated removal pattern (delete decal instance).
5. Wire the placed-instance mutation commands (`insert_decal`/`remove_decal`
   already exist at `scene_document_internal`; add the editor command-layer
   mutations and inspector handling).

**Note on forbidden shortcuts (roadmap):** no camera-facing decal art, no embedded
per-instance pattern copy without an explicit reason, no coordinate-only identity
for movable instances. Painting UI remains deferred (only create-empty +
placement here).

**Exit gate:** Deterministic tests cover create-empty, place on each surface kind,
default straight-up orientation, select through the Decals submenu, edit
dimensions/orientation afterward, delete confirm/cancel, undo/redo, and reload
round-trip. Strict build and Q2 pass.

**Estimated effort:** 1.5–2 days.

## Test impacts

- `test_scene_document.c`: insert/remove light and decal-instance history tests;
  capacity and stable-ID tests.
- `test_command_system.c`: insert/remove light and decal mutations, undo/redo,
  validation, collision/merge.
- `test_unified_editor.c`: place flow (cell + wall-face adjacent), delete confirm,
  surface → Decals submenu selection, add-decal flow, status/overlay text.
- `test_editor_domain.c`: inspector handling for light remove action and decal
  field editing.
- `test_input.c`: non-repeating `L` placement edge and per-frame reset.
- No new format/migration tests (v4 already covers all fields).

## Affected files checklist

| File | I1 | I2 |
|---|---|---|
| `src/scene_document_internal.h` | ✓ | ✓ |
| `src/scene_document.c` | ✓ | ✓ |
| `src/command_system.h` | ✓ | ✓ |
| `src/command_system.c` | ✓ | ✓ |
| `src/editor_domain.h` / `.c` | ✓ | ✓ |
| `src/unified_editor.h` / `.c` | ✓ | ✓ |
| `src/input.h` / `.c` | ✓ |  |
| `tests/test_scene_document.c` | ✓ | ✓ |
| `tests/test_command_system.c` | ✓ | ✓ |
| `tests/test_unified_editor.c` | ✓ | ✓ |
| `tests/test_editor_domain.c` | ✓ | ✓ |
| `tests/test_input.c` | ✓ |  |

