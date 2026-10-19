# Editing workspaces — the game-flow node graph and the authored-Menu workspace

Reference of record: `UI_LOOK_AND_FEEL_REFERENCE_OF_RECORD.md` §1.1, §1.3, §4.2, §4.3, §4.4,
§4.7, and the **change of direction recorded 2026-10-19 — editing workspaces**. The earlier
inspector slice explicitly left these two panes out
(`reviews/2026-10-13-editor-surface-inspector-menu.md`, "Not done here").

## Summary

- The game-flow workspace is now a deterministic node graph: Start/Scene/Menu boxes, ports,
  orthogonal labelled edges, and a selection-following viewport. The R12 `FlowWorkspace`
  controller, mutations, history and Save/Discard behaviour are unchanged.
- The authored-Menu workspace is now preview-first on the workbench palette: shared panels for
  hierarchy, properties and actions; a framed preview inside the visible region; no material on
  the preview.
- Both workspaces use the shared `ui_editor_panel` module, so the inspector panel and the two
  workspaces cannot drift apart.
- The old `ui_editor_presentation` chrome literals and the mint/dark-green/yellow preview theme are
  retired.

## What changed

- **`src/ui_editor_panel.h/.c`** — extracted from the inspector path: row collection, measurement,
  the content-fitted panel, the black bed, the `living_shore` material, focus perimeter, list
  windowing, and a plain-panel variant. `unified_editor.c` now aliases its old names to this one
  implementation.
- **`src/flow_graph.h/.c`** — pure deterministic layout: BFS layers from Start, document order
  within a layer, fixed node boxes, port counts, orthogonal edge routes, selection-following
  offsets, and node hit testing. No document format change and no persisted positions.
- **`src/unified_editor.c`** — the flow branch is now `editor_render_flow_graph`: a GAME FLOW panel,
  the node inspector or node list, and the graph canvas. Edges are drawn first, node boxes second;
  the focused inspector row owns the only moving perimeter and the selected node is a still frame.
- **`src/ui_editor_presentation.c/.h`** — rewritten on the workbench palette and shared panels.
  The preview is right-aligned inside the visible viewport, framed, and never receives material.
  The property panel is a single-row windowed list in the properties mode so all available
  properties remain reachable on a short frame. `ui_editor_presentation_preview_origin` is shared
  by the renderer and the pointer host.
- **`src/ui_editor_runtime.c`** — the standalone authored-Menu runtime now derives its render theme
  from the same workbench palette; its old mint/dark-green/yellow literal theme is retired.
- **`src/unified_editor.c`** — the interface viewport is seeded from the update seam when the
  application has not reported one, so tests and the pointer host agree with the renderer. The
  preview theme now derives from the workbench palette.
- **Windowing** — sprite load, surface decal, and decal pattern lists now window around the selected
  entry instead of silently dropping the tail.
- **Tests** — `test-flow-workspace` gains layout determinism/bounds/hit-test coverage and the
  inspector operations (`begin_connection`, `begin_add_node`, free-port count);
  `test-unified-editor` covers the inspector rows, the destination modal, Add, and Remove.

## Evidence

- `test-unified-editor` **102/102**.
- `test-flow-workspace` **13/13** (graph layout and inspector operation tests included).
- `test-ui-editor-presentation` **3/3** (including the 130x80 viewport check); `test-ui-editor-host` **3/3**.
- `make test` **81 suites / 923 tests, 0 failures**.
- `make` clean under `-Werror`; `make standards`, `make test-ui-standards` and
  `make check-ui-workbench-frame` pass.

## Follow-up 2026-10-19 — the interaction pass after owner feedback

The first clarity pass cleared the canvas and added labels, but the owner rejected it: the graph
still behaved as a three-screen keyboard wizard (`NODES → CONNECTIONS → TARGETS`), adding was
hidden inside connecting, and nothing on screen said how to change the graph. Labelling a hidden
depth is not a fix. This pass changes the interaction model.

- **One node inspector instead of three modes.** The workspace opens on the selected node's
  `INSPECTOR` (`Enter` on a node reopens it from `NODES`). It lists the node's real ports
  (`start_game -> Scene: testscene`, `extra_menu (not connected)`) and explicit `ACTIONS` rows
  (`+ Add node…`, `- Remove node`). `Up/Down` moves one cursor through those rows; `Enter` acts on
  the focused row; `Left/Esc` returns to `NODES`. The old `CONNECTIONS` and `TARGETS` screens are
  no longer rendered or reachable.
- **Adding is a named action, not a side effect.** `+ Add node…` and `A` take the selected node's
  first free port and open the destination picker with the first absent catalog entry selected.
  When every port is connected, the action refuses and the header says
  `All ports are connected; disconnect one before adding a node`.
- **The destination is a modal with a title.** Choosing a port opens a centred
  `CONNECT '<port>' TO…` panel over the workspace, with existing nodes and a `NEW FROM CATALOG`
  section. `Esc` cancels back to the inspector; `Enter` connects. It is no longer a mode that
  replaces the screen without saying what it is.
- **Keyboard editing matches the inspector.** `Backspace` disconnects the focused port; `A` is
  Add; `Enter` on `- Remove node` removes the node. The graph highlights the focused port's edge
  and the moving perimeter belongs to the inspector row, so there is still exactly one chase.
- **The renderer follows the model.** The left pane is `NODES` or `INSPECTOR`; the graph stays on
  the right. `flow_workspace_begin_connection`, `flow_workspace_begin_add_node`,
  `flow_workspace_disconnect_port`, and `flow_workspace_free_port_count` expose the inspector
  operations without changing the R12 controller's mutations, history, or Save/Discard rules.
- **Short viewports.** The key row is omitted below 30 rows; the node list collapses to one
  windowed row instead of disappearing, the inspector windows around its focused row and its hint
  keeps `A=add` visible, and dialogs are constrained below the header instead of centring over it.
  At 200% the header, the selected action sentence, the focused inspector row, and the modal stay
  legible.
- **Lifetime fix found by ASan.** The authored-Menu test mode passed a stack-local
  `UiRenderTheme` to `ui_menu_runtime_activate()`, which keeps the pointer for the session;
  rendering later read the returned frame. The preview theme now lives in
  `UnifiedEditorState.ui_menu_test_theme` for the test session. Focused ASan + UBSan runs pass.

Evidence for this pass: `test-flow-workspace` **13/13** (new
`test_inspector_begin_connection_and_add_node`), `test-unified-editor` **102/102** (new
`test_r12_i10_flow_inspector_ports_actions_and_add`), `test-input` **16/16**, the aggregate suite
**81 suites / 923 tests, 0 failures**, `make style` (cppcheck) clean, and focused ASan + UBSan +
leak detection on the three runners. Raw-cell dumps at 160×40 and 80×20 were checked: the dialog
stays below the header, and the inspector's focused row and `A=add` hint remain visible.


## Not done here

- Node positions are derived, not persisted. Free positioning needs a sidecar layout or a
  FlowDocument version bump.
- Graph pointer hit-testing exists in the layout module but is not wired into the flow input path;
  keyboard navigation remains the accepted path, matching the deferred pointer model.
- `+ Add node…` requires a free port, because every node must remain reachable in the current
  document model. There is no "add a disconnected node and connect it later" operation; the action
  refuses with a header reason when every port is connected.
- The UI hierarchy remains a tree panel, not a node graph; the flow graph is the only node graph.
- Native visual acceptance at 150% and 200% remains the owner's to confirm.
