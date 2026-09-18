# UI Scene Phase 3 — reusable editor core

Date: 2026-09-18

## Result

Phase 3 implements the reusable headless UI Scene editor core without migrating application UI or
adding the standalone host. The existing `UiMenuWorkspace` remains the sole workspace; no parallel
editor document/history model was introduced.

Implemented:

- Animation creation through the existing typed action and bounded history path;
- Animation layout editing with valid zero-width/zero-height target inheritance preserved;
- typed effect and Animation property candidate browsing;
- candidate preview documents that remain non-dirty and non-historical until acceptance;
- candidate cancel with no staged-document or history mutation;
- candidate acceptance as exactly one existing workspace history entry;
- explicit playback start/event/stop requests and visible stopped/playing status;
- shared `UiEditorAction` vocabulary, used by the embedded workspace input adapter;
- shared `ui_editor_presentation` module that renders hierarchy, properties, candidate values, status,
  selection state, and stable/playback preview from workspace state without `UnifiedEditorState`;
- a focused presentation owner in both the canonical aggregate and UI standards aggregate.

The legacy embedded presentation function remains active temporarily. The shared presentation
module is complete as an independent reusable-core seam, but exact embedded frame replacement is
deferred to Phase 4 host parity so accepted embedded labels, detailed values, pointer geometry, and
report-only target diagnostics remain unchanged.

## Preserved invariants

- Save/Discard and bounded undo/redo continue to use the existing workspace implementation.
- Animation elements remain root-level and cannot be reparented.
- Candidate browsing cannot save, undo, redo, or mutate the staged document.
- Playback uses caller-provided time; presentation does not read a hidden clock.
- Preview does not load flow targets, rewrite flow, or execute system actions.
- Existing Menu action numeric positions remain stable; `Add Animation` was appended.
- The legacy application UI remains the active product path.

## Failures and corrections

1. Focused make aliases were initially assumed for individual UI owners, but those aliases do not
   exist. Verification switched to the explicit `build/test-*` runners.
2. Adding playback to the workspace owner initially omitted the existing motion/theme link
   dependencies. The runner rule now links those dependencies explicitly.
3. The first embedded action adapter used zero-initialized `PREVIOUS` as an implicit no-action
   sentinel. It was replaced with an explicit `have_action` flag before integration testing.
4. The first attempt to route ordinary embedded drawing through the new shared presentation module
   changed seven accepted unified-editor frame contracts. Tests were not weakened. Embedded drawing
   was restored while the shared module received its own focused owner.
5. `Add Animation` was initially inserted in the middle of `UiMenuAction`, shifting a legacy action
   trace. It was appended instead, preserving existing enum positions.
6. The typed layout mutator applied ordinary positive-extent rules before resolving element class.
   It now applies Animation-specific zero-inheritance rules transactionally.
7. The first UI standards aggregate execution channel closed while the target was still running.
   No failing assertion appeared in the captured log; final aggregate evidence is recorded below
   only after a separately polled run finishes.

## Focused evidence

Strict builds use `-std=c11 -O2 -Wall -Wextra -Wpedantic -Werror`.

- `test-ui-menu-workspace`: **18/18 passed**;
- `test-ui-editor-presentation`: **2/2 passed**;
- `test-unified-editor`: **99/99 passed**;
- complete `make test-ui-standards`: **passed**, including the new presentation owner;
- strict default application build: **passed**;
- cppcheck through `make style`: **passed**;
- `make check-test-inventory`: **passed**;
- `make check-project-structure`: **passed**.

## Remaining Phase 4 handoff

Phase 4 must route standalone and embedded hosts through `ui_editor_presentation` while preserving
the accepted embedded frame details characterized by `test-unified-editor`. It must then establish
full action/document/history/candidate/playback/preview-cell parity between both hosts. Protected
assets, pause migration, application routing, and legacy removal remain out of scope until their
later gates.