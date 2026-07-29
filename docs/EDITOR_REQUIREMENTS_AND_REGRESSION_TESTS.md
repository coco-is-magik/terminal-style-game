# Unified Editor Requirements and Regression Contract

## Status and authority

The wall-material vertical slice in `docs/EDITOR_UNIFICATION_PLAN.md` is
complete as of 2026-07-27. This document is the stable maintenance contract for
future editor changes. Historical plans, handoffs, and status reports remain
useful chronology but must not override these accepted requirements.

## Required behavior

1. The main menu exposes one **Editor** entry; removed legacy editor application
   states must not return.
2. Editor walk and edit modes share one application-owned camera and one
   authoritative `SceneDocument.map` allocation.
3. Wall selection uses the center-camera ray and records the cardinal face, but
   material assignment changes the entire wall cell because the current map
   stores one material per cell.
4. Only occupied wall cells (`material_id > 0`) are valid command targets.
5. Existing loaded materials may be assigned. Loaded IDs above `9` are allowed
   for live preview but must be visibly unsaveable.
6. All authored mutations flow through `CommandHistory`; execute, undo, and redo
   preserve immutable document-state identities and correct dirty state.
7. The renderer, collision, camera, and selection paths read the same map. A
   material edit must be visible on the next render without map copying or world
   reconstruction.
8. Material edits must not reset camera, lights, decals, assets, inspector, or
   unrelated simulation state.
9. Save writes only material IDs `0..9`, one digit per cell plus row newlines.
   It is atomic, excludes `light_map`, and preserves the previous destination on
   failure.
10. Ragged source rows are valid and are padded on the right with material `0`
    to the longest row.
11. An unloaded current wall material is shown numerically with `(missing)` and
    may be replaced with a loaded material.
12. Input priority is modal, inspector, editor shortcuts, world selection,
    camera, then application shortcuts. Consumed keyboard/pointer input must not
    activate a later layer.
13. Dirty reload and editor exit require explicit confirmation. Failed save
    blocks Save-and-Exit and preserves edits/history.
14. Reusable decal persistence and headless pattern editing remain available as
    `decal_io` and `decal_painter`; future UI integration must reuse rather than
    duplicate these boundaries.
15. Editor world visualization draws a persistent solid selected-wall-face
    outline, a distinct dashed hover outline, and an adaptive center crosshair.
    It is an editor-only post-pass: normal nearest-wall occlusion applies,
    selection wins overlap, the face interior remains visible, and no authored
    map/material state is mutated.
16. Escape dismissing the wall inspector also clears its persistent selection
    and solid world highlight. A later ordinary frame may still show the dashed
    hover target under the center crosshair. The next Escape opens the editor
    exit prompt as before.

## Forbidden regressions

- No second editable map, synthetic editor preview map, or synchronization copy.
- No direct UI mutation of authored map cells.
- No full-world rebuild for apply, undo, or redo.
- No command that turns an empty cell into a wall in this vertical slice.
- No silent truncation or serialization of material IDs outside `0..9`.
- No checked-in asset writes from automated tests.
- No restoration of removed Asset Designer, Live Editor, or Material Designer
  application states as separate products.
- No material mutation, authored-state copy, heap allocation, or through-wall
  rendering to visualize editor selection.

## Regression suite map

| Contract | Primary runner / coverage |
|---|---|
| Scene ownership, transactional load, atomic save, `0..9`, ragged rows, derived data exclusion | `test-scene-document` |
| Occupied-wall target, command transaction, state IDs, branch truncation, OOM, undo/redo dirty semantics | `test-command-system` |
| Center ray, four face directions, bounds, empty-cell rejection | `test-editor-selection` |
| Selected/hover wall-face outlines, cardinal faces, occlusion, contrast, overlap priority, map/interior preservation, crosshair | `test-editor-highlight` |
| Mode/camera preservation, input consumption, selection/dismissal, picker, missing marker, apply/undo/redo/save/reload/exit, authoritative map/no-reset state | `test-unified-editor` |
| Painter lifecycle, ownership, bounded paint/erase/fill/clear, transactionality | `test-decal-painter` |
| Decal persistence | `test-decal-io` |
| Current menu/layout/action contracts | `test-menu-state`, `test-ui-ele` |
| Rendering and engine regression under configured tracker | `test-core`, `test-decals` |

The aggregate `make test` includes a dedicated editor-highlight runner. A future
change that intentionally alters a requirement must update this contract, the
relevant tests, and user-facing documentation together; tests must not simply be
removed or weakened to obtain a pass.

## Supported validation matrix

Run strict C11 builds/tests for default SMC stream, explicit stream, custom
dirty cells, and lighting cache. `USE_NO_STATE_TRACKER=1` is an explicit
diagnostic baseline and should at least compile cleanly when its boundary is
changed. Runtime `make benchmark` and `make stability` require a video
environment and use the representative raycast workload.

## Manual acceptance record

On 2026-07-27 the unified editor was exercised interactively and accepted by
the user: entry, walk/edit behavior, visual material editing, and the overall
foundation looked correct. Repeat a focused smoke test when changing SDL input,
application state transitions, renderer integration, or editor overlays.

On 2026-07-29 the user interactively accepted the adaptive center crosshair and
the distinct hovered/selected wall-face outlines after strict, aggregate, and
focused sanitizer verification passed.

Later on 2026-07-29 the user confirmed the post-acceptance correction: pressing
Escape once after wall selection closes the inspector and removes the solid
persistent-selection outline.
