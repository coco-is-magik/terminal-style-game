# R12 I10 Unified-Editor Game-Flow Workspace Plan — 2026-09-09

## Status

**Implemented; automated verification passed on 2026-09-09.** I10 introduces the
first user-facing R12 authoring workspace inside the existing unified editor. It
provides deterministic graph browsing and staged edge-target rewiring without creating
a disconnected application state or treating application-owned UI layouts as game menus.

## Locked workspace boundary

`FlowWorkspace` is a separate headless controller embedded by `UnifiedEditorState`.
It owns:

- one staged `FlowDocument`;
- one exact saved document snapshot for Discard;
- node, outgoing-edge, and target selection indices;
- nested workspace/close-prompt mode;
- a bounded edge-target command history;
- active and dirty workflow state.

It performs no rendering, SDL input processing, catalog scanning, scene/menu loading,
application-state transition, or mutation of the editor's `SceneDocument` and scene
command history.

## User entry and project path

- Non-repeating `G` opens the game-flow workspace while `APP_STATE_EDITOR` remains
  active.
- The existing editor footer advertises `G=game flow` without removing established
  light/sprite/object shortcut labels.
- On first entry, an editor configured with an authored asset root attempts exactly
  `<asset-root>/game.flow`.
- A valid file loads transactionally. A malformed file preserves the retained valid
  graph, opens the recoverable workspace, and shows a load error.
- If no file exists, the valid built-in Start-only graph opens. It requires the public
  Save As seam before ordinary Ctrl+S can succeed.
- The checked-in `assets/game.flow` is a minimal valid project graph from Start to the
  existing `testscene`; it declares no nonexistent Menu or scene-exit ports.

## Presentation and navigation

The text overlay presents a tree-shaped view over graph data:

1. **Nodes:** stable ID and Start/Scene/Menu type/name.
2. **Connections:** named outgoing source ports and typed targets for the selected node.
3. **Targets:** every existing non-Start node as a legal target candidate.
4. **Close prompt:** Save, Discard, or Cancel when staged changes are dirty.

Up/Down wraps deterministically in document order. Enter descends or commits. Escape
returns Targets→Connections→Nodes. Escape from clean Nodes closes; Escape from dirty
Nodes opens the close prompt. `G` acts like Escape while the workspace is open.

The workspace consumes keyboard and pointer ownership before scene edit/walk behavior,
suppresses camera movement/look, unlocks relative mouse mode, and hides the crosshair.

## Mutation and history

I10 adds `flow_document_set_edge_target`, which transactionally rewires one stable edge
to an existing target node and validates the complete graph before commit. A rejected
rewire restores the original target and document identity.

The workspace records at most `FLOW_MAX_EDGES` edge-target commands. Each command owns:

- stable edge ID;
- before/after target node IDs;
- before/after logical document state IDs.

Ctrl+Z/Ctrl+Y replay validated target mutations, restore the recorded logical state,
and preserve the monotonic next-state counter. New mutations truncate redo. Save updates
the saved document identity/snapshot without erasing history, so undo after Save becomes
dirty and redo back to the saved state becomes clean. Discard restores the exact saved
snapshot and clears workspace history.

## Save and failure behavior

- Ctrl+S saves only when the staged flow has an existing path.
- A public unified-editor Save As seam supports later file-picker integration and tests.
- Dirty-close Save commits atomically through `FlowDocument`, updates the snapshot, and
  closes only on success.
- Failed Save keeps the workspace open and displays a bounded error.
- Failed load, open, mutation, Save, undo, or redo preserves unrelated scene editor data.

## Intentionally deferred

I10 does not create/remove/rename graph nodes, create/remove named ports, or create/remove
edges. Those workflows require authoritative Scene and authored Menu catalogs plus
text/picker requirements; guessing names or scanning application UI assets would violate
the established R12 boundaries. I10's first safe mutation is rewiring existing authored
connections to existing typed nodes.

I10 also does not add the visual Menu canvas, target loading, Start Game replacement,
HUD binding, application menu replacement, or a graphical free-positioned node canvas.

## Verification scope

- transactional edge-target mutation and reachability rollback;
- node/connection/target navigation and selection;
- nested Escape and dirty close Save/Discard/Cancel;
- bounded undo/redo, redo truncation, and Save-relative dirty identity;
- transactional load/save and conventional project path;
- malformed project flow recovery;
- non-repeat `G` input and downstream edge consumption;
- unified-editor overlay, movement suppression, crosshair hiding, and scene-state isolation;
- preservation of established editor footer shortcuts;
- strict, ASan/LeakSanitizer, UBSan, aggregate, application-build, and smoke gates.

## Verification evidence

Final verification on 2026-09-09:

- focused strict `FlowDocument`: **6/6 passed**;
- focused strict `FlowWorkspace`: **6/6 passed**;
- focused strict input: **14/14 passed**;
- focused strict unified editor: **87/87 passed**;
- full `make asan`: **passed**, status 0, with no AddressSanitizer or leak report;
- full `make ubsan`: **passed**, status 0, with no undefined-behavior report;
- clean optimized full `make test`: **passed**, status 0;
- production `make all`: **passed** under C11 `-Wall -Wextra -Wpedantic -Werror`;
- `make smoke`: **passed** with
  `{"smoke":"ok","map_width":10,"map_height":6}`;
- deprecated legacy-symbol and current-renderer guards: **passed**;
- trailing-whitespace audit of all I10 code, tests, records, and Make changes:
  **passed**;
- `make style`: **skipped**, because `cppcheck` is unavailable in the environment.

The checked-in `assets/game.flow` is parsed and semantically validated by a focused
regression test. No display-backed manual editor session was run; keyboard routing,
overlay text, camera suppression, crosshair suppression, persistence, and recovery are
covered headlessly at the unified-editor controller boundary.

## Next increment boundary

I11 should add authoritative project asset catalog composition and the missing graph
construction workflows: add existing Scene/Menu nodes, expose their validated ports,
connect currently unconnected ports, and remove/reconnect edges with bounded history.
Only after that data-authoring loop is complete should R12 advance to the visual Menu
canvas and hierarchy/property inspector.