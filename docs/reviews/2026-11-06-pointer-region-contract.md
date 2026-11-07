# Pointer region contract and graph hover — 2026-11-06

Reference: `UI_POINTER_INTERACTION_MODEL_2026-11-06.md` §3, §4, §6 and §8 P1;
requirement 12 of `EDITOR_REQUIREMENTS_AND_REGRESSION_TESTS.md` (input priority and
consumption); `UI_DESIGN_AND_TEST_STANDARDS.md` (keyboard/pointer equivalence).

## Summary

- Every editor pointer surface now takes its rectangle from the function that paints
  it. `UiEditorSurfaceRegion` and `ui_editor_presentation_preview_region` own the
  preview's origin **and** its size clamps, and `ui_editor_surface_region_translate`
  is the one rule turning a frame cell into a surface-local one.
- This fixes a real defect rather than only duplicating a number. The authored-Menu
  test surface subtracted the literal origin `40, 3`; the row happened to match
  `UI_EDITOR_PREVIEW_TOP`, but the column origin is `columns - 2 - preview_width`, so
  the substitution was correct only when the preview happened to be `columns - 42`
  wide. On any other frame or document size a test-mode click resolved to a cell that
  was never painted.
- Flow-graph node boxes now take the bright role while the pointer is over them. Hover
  is recomputed each frame from the same hit test the click uses, selects nothing, and
  never dirties the flow.
- Keyboard behaviour is unchanged: `Up/Down` selection, inspector navigation, and
  every other shortcut behave exactly as before.

## What changed

- **`src/ui_editor_presentation.h/.c`** — the region type, the preview region provider
  (origin plus the clamps that used to be duplicated in the renderer and the host), and
  the bounded translation. The renderer's inline clamp block is replaced by the
  provider, so the geometry exists once.
- **`src/ui_editor_host.h/.c`** — `UiEditorHostViewport` is now just the viewport size;
  the host derives the preview region itself, translates a press through it (outside
  the region reaches no control and reports no action), and keeps the unbounded offset
  for motion because a drag may leave the preview and the workspace clamps it.
- **`src/unified_editor.c`** — the authored-Menu workspace no longer supplies a
  literal origin; the test surface translates through the preview region
  (`editor_ui_menu_test_pointer`), and a press off the surface is consumed and hits
  nothing; `editor_flow_graph_region` is shared by the painter and the hit test;
  `flow_hover_*` state is recomputed per frame and cleared when the workspace closes or
  the editor session resets.
- **`src/ui_editor_runtime.c`** — the standalone host's viewport literal is gone.
- **Tests** — `test_preview_region_matches_the_painted_frame` asserts the painted `+`
  corners are exactly the cells outside the region and that a press on the frame
  reaches nothing; `test_pointer_hosts_produce_identical_history` now presses where the
  painter draws instead of at coordinates derived from the literal; the flow-graph
  pointer test asserts hover is presentation only and brightens the box.

## Evidence

- `make test` — **PASS**, 81 suites / **926 tests**, 0 failures, exit status 0.
- `test-ui-editor-host` **4/4** (new painted-frame test included);
  `test-unified-editor` **104/104**; `test-ui-editor-presentation` **3/3**;
  `test-flow-workspace` **13/13**.
- `make standards` — **PASS**, exit status 0 (cppcheck, suppression policy,
  banned unsafe calls, project structure, test inventory, legacy symbols, current
  renderer API, workbench boundaries).
- Application link clean under `-Wall -Wextra -Wpedantic -Werror`.

## Not done here

- Authored-preview hover, drag with ownership, rotation preview, display-backed Button
  activation through a real pointer, and wire gestures: P2 and P3 of the model, which
  wait on decisions D1–D4.
- No whole-suite sanitizer run and no canonical `make leak` container run for this
  increment; both remain unverified here rather than assumed.
- Native display acceptance at 150% and 200% UI scale with Reduced Motion stays the
  owner's. The value of this change is that the pointer's geometry and the painter's
  are now provably the same rectangle; it is not a claim that the editor was exercised
  on a display.
