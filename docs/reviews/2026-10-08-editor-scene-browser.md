# Editor scene browser: the first editor-interface surface rebuilt

> **Superseded 2026-10-09.** This document records the 2026-10-08 framed-panel design and its decision
> to draw the browser's focused row with the focus perimeter in its *shape-only* form. The browser is now
> a **display surface** (full-display living-field material, one centred block of title, count, rows and
> hint, and the perimeter with its chromatic chase, as an authored focused button draws it), so the
> panel geometry and the shape-only rule below no longer describe the implementation. The colour-token
> work recorded here — retiring the editor overlay's literals for
> `ui_app_theme_workbench_palette()` roles — stands. See
> [`2026-10-09-editor-scene-browser-display-surface.md`](2026-10-09-editor-scene-browser-display-surface.md).

Reference of record: UI_LOOK_AND_FEEL_REFERENCE_OF_RECORD.md §2 and §3.1 (all colour from tokens),
§4.2 (white-dominant, selectively coloured editor interface; accent and focus are state, not
decoration), §4.4 (nothing in motion displaces or obscures a control), §5.1 (the sanctioned
editor-literal-to-token work), §5.2 (auto-rejections: a new hardcoded colour, and a pane renderer
created beside the existing ones). Target document: UI_DISPLAY_SURFACE_TARGET_2026-10-07.md §3.5.

## Summary

The editor's **OPEN SCENE / IMPORT LEGACY MAP** browser is the first editor-interface surface rebuilt
with what the menu work established. It was ragged text from column 1 with a `>` marker and hardcoded
colours; it is now a framed panel with a title, the hint, the entry count and a list whose selected
row takes the shared focus perimeter.

Behaviour is untouched: the same modal, the same keys (Up/Down, Enter, Ctrl+I, Ctrl+O, Esc), the same
catalog, the same selection and scroll semantics, the same strings for the title and the empty case.

## What changed

- **`src/unified_editor.c`.** The editor overlay's colour literals (`{220,220,220}`, `{0,0,0}`,
  `{160,160,160}`, `{220,180,80}`, `{120,220,160}`) are replaced by `ui_app_theme_workbench_palette()`
  roles: `primary_text`, `canvas`, `secondary_text`, `warning`, and `focus` for the selected row. The
  selection is a state, so it takes `focus` rather than a hue of its own — the green selection is gone
  with the literal. This retires the literals for the *whole* editor overlay, not just the browser.
- **`editor_render_scene_browser()`** (new, in the same file, extending the existing overlay renderer
  per §5.2 rejection 9): a bordered panel that fits its content, a title row with a right-aligned
  `n / total` count, the hint row, the rows at a two-row step so the perimeter lands on the blank rows,
  the empty-catalog message, and the focused row drawn in `focus` inside the shared perimeter.
- **`src/ui_ele.h/.c`.** `ui_ele_focus_perimeter_draw()` exposes the shared focus perimeter in its
  shape-only form (the frame without the travelling chromatic fringe). The element path is unchanged
  and still chases; the editor calls the shape-only entry, because the chromatic chase stays a
  display-surface treatment (§4.2).
- **`src/unified_editor.h`.** `UNIFIED_EDITOR_INTERFACE_COLUMNS/ROWS` (100x40), so the editor owns the
  size of the surface it draws into and can lay panels out inside it. `src/app.c` now uses those
  instead of its own private `APP_UI_EDITOR_WIDTH/HEIGHT` — one definition instead of two.
- **`Makefile`.** The editor test links `ui_ele.c`, `ui_effect.c` and the palette adapter, listed
  directly rather than via `SRC_UI_ELE` because `SRC_UI_ELE` also carries `number_parse.c`, which
  `SRC_DECAL_IO` already provides (a duplicate source file on one link line is a multiple-definition
  error).
- **`tests/test_unified_editor.c`.** Two assertions that encoded the old `hi` literal (`fg.g == 220`)
  now assert the `focus` role instead; the test files that run after them were failing only because
  cmocka aborts a test on a failed assertion, which skipped that test's directory cleanup.

## Evidence

- `test-unified-editor` (100 tests, was 99): the new
  `test_scene_browser_panel_is_palette_driven` asserts the frame corners and their `border`/`panel`
  roles, the title, the hint, the focused row in the `focus` role, the perimeter around that row — and
  then scans **every cell inside the panel** to assert each foreground and background is one of the six
  palette roles the panel uses. That scan is the standing guard for §5.2 rejection 2.
- **Composed-frame artifact** (the §5.2 rejection 7 requirement), session-local:
  `/tmp/browser_open.png` and `/tmp/browser_moved.png`, captured from the running application on the
  native X11 session (main menu → EDITOR). The panel shows `OPEN SCENE`, the hint, `1 / 4`, the four
  scenes, and the perimeter around the selected row.
- `make` (application) builds clean under `-Werror`.

## Scope statements

- **Direction unchanged.** §4.2 still governs the editor interface: it stays white-dominant and
  restrained, and no chromatic material was added to it. What changed is that its colours now come
  from roles instead of literals, and one pane-shaped list became a real panel. The chromatic chase,
  the field material and the fringe remain display-surface treatments.
- **Not changed:** every key, mode, catalog rule, selection semantic and status string. The browser's
  visible row count is 9 rather than 10, which is presentation only: the update path wraps the index
  over the catalog and never referenced the visible count.
- **Not yet done:** the rest of the editor interface (status block, flow workspace, save and prompt
  modals, pickers) still draws as column-1 text. It now shares one palette, so the next surface is a
  smaller change than this one was.
