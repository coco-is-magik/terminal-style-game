# R12 I12 Visual Authored-Menu Workspace Plan — 2026-09-10

## Status

**Implemented; automated verification passed on 2026-09-10.** I12 introduces the first
visual authored-game Menu workspace inside the unified editor. It reuses `UiDocument`,
the responsive layout resolver, and the headless render adapter rather than creating a
parallel UI schema or renderer.

## Locked workspace boundary

`UiMenuWorkspace` is a separate headless staged controller owned by
`UnifiedEditorState`. It owns:

- a direct-child `<asset-root>/menus/*.tui` chooser using the established regular-file,
  sorted, non-recursive `MapCatalog` seam;
- one staged `UiDocument` and exact saved snapshot;
- create-name, hierarchy, property, and dirty-close modes;
- stable element and property selection;
- 32 bounded layout-property history commands.

Non-repeating `Ctrl+U` opens the workspace while `APP_STATE_EDITOR` remains active.
Ctrl is required so ordinary `u` remains available in Menu-name text input and existing
single-letter editor actions stay unambiguous. The workspace owns keyboard/pointer input
before scene editing, suppresses camera movement/look, unlocks relative mouse mode, and
hides the crosshair.

## Menu lifecycle

- The chooser lists only validated-path candidates under `menus/*.tui` plus
  `Create new Menu`.
- Existing documents load transactionally through `ui_document_load`; malformed files
  preserve chooser/workspace state and display a typed error.
- New names accept only bounded `UiDocument` identifiers and save to
  `<asset-root>/menus/<name>.tui`.
- Existing destinations are rejected rather than silently overwritten.
- Ctrl+S uses `UiDocument` same-directory temporary-file replacement.
- Escape from a dirty document presents Save, Discard, or Cancel.
- Catalog refresh and save failures preserve the usable staged document.

## Visual preview and hierarchy

The unified-editor overlay presents a stable-ID indented hierarchy beside a live preview.
The preview calls `ui_render_document` with the editor's borrowed project assets and one
centralized transient editor-preview theme. Authored normal colors remain document data.
The selected element uses both hierarchy `>` selection and the I5 non-color `>/<` focus
markers.

The available preview viewport is responsive and bounded to 80×25 logical cells. Smaller
grids resolve the same authored anchors, Stretch behavior, local scaling, clipping, and
painter order through the existing I4/I5 implementation. Missing sprite/material assets
produce a visible preview failure without mutating the document or destination.

The checked-in `assets/menus/main_menu.tui` is upgraded to canonical v3 with an explicit
centered 20×3 Start Button so first entry demonstrates useful responsive geometry and
native border/text rendering.

## Property and history slice

I12 deliberately edits only non-root layout properties:

- `x` and `y` offsets in one-cell steps;
- `width` and `height` in one-cell steps with positive non-Stretch size preserved;
- local scale in 25-point steps within the existing `[25,400]` range.

All edits route through `ui_document_set_layout`. History stores stable element ID,
before/after layouts, and logical state IDs rather than whole-document copies. Undo/redo
preserve monotonic state identity, redo truncation, and Save-relative dirty identity.

## Intentionally deferred

I12 does not add element create/remove/duplicate/reparent/reorder, text or Button-port
editing, anchors, colors, borders, alignment, sprite assignment, pointer hit selection,
drag/resize handles, runtime interaction preview, target loading, app-state transitions,
HUD authoring, or application-menu replacement. It never scans or writes application-owned
`ui_layouts` or `ui_elements`.

## Verification evidence

Final verification on 2026-09-10:

- focused strict input: **15/15 passed**;
- focused strict `UiMenuWorkspace`: **4/4 passed**;
- focused strict unified editor: **90/90 passed**;
- affected I3–I5 document/layout/render and I11 catalog suites: **passed**;
- clean optimized full `make test`: **passed**, status 0;
- full `make asan`: **passed**, status 0, with no AddressSanitizer or leak report;
- full `make ubsan`: **passed**, status 0, with no undefined-behavior report;
- production `make all`: **passed** under C11 `-Wall -Wextra -Wpedantic -Werror`;
- `make smoke`: **passed** with
  `{"smoke":"ok","map_width":10,"map_height":6}`;
- deprecated legacy-symbol and current-renderer guards: **passed**;
- scoped trailing-whitespace and forbidden application-UI coupling guards: **passed**;
- `make style`: **skipped**, because `cppcheck` is unavailable in the environment.

No display-backed manual editor session was run. Input priority, hierarchy, non-color
selection markers, responsive preview output, property editing, history, persistence,
dirty close, camera/crosshair suppression, and Scene-state isolation are covered through
deterministic headless tests.

## Next increment boundary

I13 should extend the visual Menu authoring loop with catalog-safe element construction
and removal plus content/flow-port editing: create Container/Text/Button children under a
selected Container, edit bounded text and unique Button flow ports, and remove elements
transactionally with hierarchy/history updates. Pointer manipulation, broad visual/style
editing, and runtime preview should remain later slices.