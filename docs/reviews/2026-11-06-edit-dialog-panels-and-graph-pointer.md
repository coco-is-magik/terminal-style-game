# Edit dialogs on the shared panel, and clicking the game-flow graph — 2026-11-06

Reference: `UI_LOOK_AND_FEEL_REFERENCE_OF_RECORD.md` §1.1, §4.2, §4.4, §4.7 and the
2026-10-13 edit-mode panel direction; the edit-mode submenu rule in
`UI_DESIGN_AND_TEST_STANDARDS.md`. Builds on `reviews/2026-10-19-editor-workspaces.md`,
which extracted the shared `ui_editor_panel` and `flow_graph` modules and listed both
of these behaviours under "Not done here".

## Summary

- Every editor dialog — exit, dirty-open, reload, material collision and overwrite,
  light/decal/sprite/trigger/object removal, the save menu and its overwrite stage —
  is drawn on the shared `ui_editor_panel`: the same bed, `living_shore` material and
  focus perimeter as every editor menu. The selected choice is **framed**; the literal
  `>` selector is gone. Reduced Motion leaves the perimeter still.
- Prompt text is wrapped to the panel's own column, heading included, and the dialog
  slides up instead of covering the editor's two footer rows, so a save path or a
  scene diagnostic is never cropped.
- In the game-flow workspace's `NODES` step, a click on a node box selects that node,
  exactly as `Up/Down` does. Selection is workspace state; pointing at the flow never
  marks it dirty.

Input keys, choice order, save semantics, persisted data and the R12 controller's
mutations, history and Save/Discard rules are unchanged.

## What changed

- **`src/unified_editor.c` — dialogs.** `editor_prompt_render` collects the prompt as
  `UiEditorPanelRow`s and draws it with `ui_editor_panel_render` instead of painting its
  own bed with `editor_menu_panel_bed`. `ui_editor_panel_collect` supplies the row
  classification, so a selected choice (`palette->focus`, or a leading `>` marker) takes
  the perimeter and the marker is dropped. `editor_prompt_break`/`editor_prompt_push`
  wrap each collected line at the panel's text column and continue it on the next
  entry. Placement keeps the prompt's own row, measures with
  `ui_editor_panel_planned_height`, and moves up only when the block would reach the
  footer.
- **`src/unified_editor.c` — graph pointer.** `editor_flow_note` is now the single
  decision for the workspace's heading note (a panel row, so its presence moves the
  graph down by one); `editor_flow_graph_top` derives the graph's first row from the
  same planner the header rendered with; `editor_flow_graph_hit` builds the layout with
  `flow_graph_layout_build` and resolves the pointer with `flow_graph_node_at`.
  `editor_update_flow_workspace` takes the interface viewport and applies the hit, and
  the painter calls the same `editor_flow_graph_top`, so painting and hit testing
  cannot drift apart.
- **Bounded to the visible region.** The dialog's text column is derived from the
  interface columns (not the raw grid) and capped at the panel row's 80-cell label, so
  a dialog neither crosses the composited region nor clips text inside a row.
- **`tests/test_unified_editor.c`** — new `test_edit_dialogs_share_panel_and_visible_region`
  and `test_flow_graph_pointer_selects_the_node_under_it`. The save-menu overwrite test
  now asserts the framed choice and the absence of `> Overwrite`.

## Evidence

- `make` — application link, clean under `-Wall -Wextra -Wpedantic -Werror`: **PASS**.
- `make test` — **PASS**, 81 suites / 925 tests, 0 failures.
- `test-unified-editor` **104/104** (including the two new tests);
  `test-flow-workspace` **13/13**; `test-ui-editor-presentation` **3/3**.
- `make test-ui-standards` — **PASS**, 24 focused runners, exit status 0.
- `make standards-core check-static-analysis-policy` — **PASS**, exit status 0. A full
  `make standards` transcript additionally reports `PASS: cppcheck static analysis
  passed`, the suppression-policy gate, the banned-unsafe-call guard, project
  structure, test inventory, legacy-symbol and current-API guards, and the workbench
  token/host boundaries. The aggregate command's own exit status was not observed
  because the session was terminated after its last gate; the guards were re-run
  separately for a captured status.
- Focused runtime-safety evidence: `test-unified-editor` built with
  `-fsanitize=address,undefined` and run with LeakSanitizer enabled — **104/104, exit
  status 0**, no sanitizer diagnostics. This is focused evidence on one runner, not a
  whole-suite sanitizer claim.

## Not done here

- Dragging nodes, port/wire gestures, and pan/follow controls for very long chains.
- Node positions remain derived, not persisted.
- Prompt wrapping is line-based; a single diagnostic longer than 79 columns continues
  on the following panel rows rather than inside one row.
- Native visual acceptance at 150% and 200% UI scale and under Reduced Motion remains
  the owner's to confirm. Automated grid tests are not on-screen acceptance.
