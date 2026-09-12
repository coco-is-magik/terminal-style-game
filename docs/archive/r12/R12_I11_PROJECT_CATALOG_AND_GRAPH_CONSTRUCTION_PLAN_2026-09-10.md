# R12 I11 Project Catalog and Graph Construction Plan — 2026-09-10

## Status

**Implemented; automated verification passed on 2026-09-10.** I11 composes an owning
project flow catalog from authoritative authored Scene/Menu documents and completes the
minimum text-based graph construction loop inside the I10 workspace.

## Locked project catalog boundary

`FlowProjectCatalog` scans only direct regular files in these conventional project
locations:

- `<asset-root>/scenes/*.tscene` for authored Scene documents;
- `<asset-root>/menus/*.tui` for authored game Menu documents.

Each candidate is loaded by its authoritative document parser. Scene ports are derived
through `scene_flow_reference_view_build`; Menu Button ports are derived through
`ui_document_build_flow_reference`. The owning catalog copies names and ports, sorts by
typed asset/name identity, rejects duplicate or invalid entries, and replaces the live
catalog only after complete validation. Missing Scene/Menu directories are valid empty
sources. Application-owned `ui_layouts` and `ui_elements` are never scanned.

The checked-in `assets/menus/main_menu.tui` establishes the Menu directory/extension
convention and exports `start_game`. It remains an authored game asset; it does not
replace the application's current main menu.

## Graph construction behavior

When `G` opens project `game.flow`, the workspace receives the refreshed catalog and:

- presents every validated port for the selected graph node, including unconnected ports;
- presents existing graph nodes plus absent catalog assets as target choices;
- connects an unconnected port to an existing target;
- adds an absent Scene/Menu node and connects it in one validated transaction;
- rewires existing connections as before;
- removes a selected connection only if all retained nodes remain reachable;
- removes a selected non-Start node together with incident connections only if the
  retained graph remains valid.

No mutation accepts free-form asset or port names. Failed mutations preserve document,
history, selection ownership, and unrelated Scene editor state. A catalog refresh failure
opens the retained valid graph in browse-only mode and displays a typed error.

## History and persistence

Workspace history is bounded to 32 commands and stores exact before/after `FlowDocument`
snapshots. This makes compound add+connect and node+incident-edge removal one undo step.
Undo/redo preserve monotonic state identity, redo truncation, and Save-relative dirty
identity. Save, Save As, dirty-close Save/Discard/Cancel, and malformed-flow recovery
retain the I10 transactional behavior.

## User controls

- `Up` / `Down`: select nodes, validated ports, targets, or close choices;
- `Enter`: descend or connect/rewire the selected port and target;
- `Backspace`: remove the selected existing connection or non-Start node;
- `Escape` / `G`: move back or enter the dirty-close workflow;
- `Ctrl+Z` / `Ctrl+Y`: undo/redo graph commands;
- `Ctrl+S`: atomically save the existing `game.flow` path.

## Intentionally deferred

I11 does not create, rename, or edit Scene/Menu assets or their ports. It does not add a
free-positioned node canvas, target loading, Start Game replacement, app-state
transitions, HUD binding, or application-menu conversion.

## Verification evidence

Final verification on 2026-09-10:

- focused strict `FlowDocument`: **7/7 passed**;
- focused strict `FlowWorkspace`: **7/7 passed**;
- focused strict `FlowProjectCatalog`: **2/2 passed**;
- focused strict unified editor: **89/89 passed**;
- clean optimized full `make test`: **passed**, status 0;
- full `make asan`: **passed**, status 0, with no AddressSanitizer or leak report;
- full `make ubsan`: **passed**, status 0, with no undefined-behavior report;
- exact-final-source focused ASan and UBSan unified editor: **89/89 passed** each;
- production `make all`: **passed** under C11 `-Wall -Wextra -Wpedantic -Werror`;
- `make smoke`: **passed** with
  `{"smoke":"ok","map_width":10,"map_height":6}`;
- deprecated legacy-symbol and current-renderer guards: **passed**;
- trailing-whitespace and forbidden `ui_layouts` coupling guards: **passed**.
- `make style`: **skipped**, because `cppcheck` is unavailable in the environment.

No display-backed manual editor session was run. Catalog composition, staged construction,
overlay text, input routing, failure recovery, Scene-state isolation, persistence, and
history behavior are covered at deterministic headless boundaries.

## Next increment boundary

I12 should add the first visual authored Menu workspace over `UiDocument`: project Menu
selection/open/create lifecycle, responsive canvas preview, hierarchy selection, and the
smallest property-editing command slice with undo/redo and Save/Discard. It must continue
to keep application-owned menus outside the authored-game asset namespace. Target asset
loading and application-state transitions remain separate runtime integration work.