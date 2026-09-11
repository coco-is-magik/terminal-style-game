# R12 I14 Menu Hierarchy Structure Authoring Plan — 2026-09-11

## Status

**Implemented; automated verification passed on 2026-09-11.** I14 completes the
keyboard-first hierarchy structure boundary approved after I13: validated element rename,
cycle-safe reparenting under Containers, and adjacent sibling subtree moves in painter order.

## Locked scope

- Rename is available for every non-root element and uses staged identifier entry.
- Reparent lists only valid Containers: it excludes the selected element, its current parent,
  non-Containers, and descendants that would create a cycle.
- Move Earlier and Move Later exchange the selected element's complete subtree with the
  immediately adjacent sibling subtree. Boundary actions are unavailable.
- Reparent changes only the selected root element's `parent_id`; it preserves existing
  document paint order and all subtree IDs, content, ports, layout, and visuals.
- Rename, reparent, and each adjacent move are one exact snapshot-history command. Selection
  remains on the same stable element ID across apply, undo, and redo.

## Document mutations

`UiDocument` adds four failure-atomic APIs without changing persisted v3 data:

- `ui_document_rename_element` enforces the existing bounded identifier vocabulary and global
  element-name uniqueness;
- `ui_document_reparent_subtree` requires a Container target and rejects root changes and
  ancestry cycles;
- `ui_document_move_subtree_earlier` and `ui_document_move_subtree_later` swap whole adjacent
  sibling groups while preserving each subtree's internal document order.

Missing elements, malformed source documents, invalid/duplicate names, root operations,
non-Container targets, cycles, boundary moves, and state-ID exhaustion reject without changing
the caller's document.

## Workspace and input

`E` actions add Rename, Reparent, Move Earlier, and Move Later. Rename uses the existing staged
text buffer with Enter-to-commit and Escape-to-cancel. Invalid or duplicate names remain staged
for correction without document/history mutation. Reparent uses an Up/Down target picker with
Enter-to-commit and Escape-to-actions. Rename text mode owns input before letter-based scene
actions, and SDL text input is active for the mode.

## Preserved boundaries

I14 does not add visual-property editing, pointer selection, drag/resize, runtime interaction
preview, target loading, HUD authoring, or application-menu replacement. It does not change
Button ports or `game.flow`, add element types, or scan/write application-owned `ui_layouts`
or `ui_elements`.

## Implementation refinements and failed checks

- The first strict workspace compile rejected use of `ui_document.c`'s private ancestry helper.
  A workspace-local read-only helper now preserves the module boundary.
- The first workspace regression selected the root because root is a valid alternative parent;
  the test now navigates explicitly to the intended second Container.
- The next regression exposed that a successful array-order move left `element_index` pointing
  at the sibling moved into the old row. The controller now restores selection by stable ID
  immediately after recording the command, matching undo/redo semantics.
- Reparent was initially described as moving a subtree to the end of its new parent's children.
  The implemented narrower rule changes only `parent_id`; painter order changes only through
  explicit Move Earlier/Move Later commands.

## Verification evidence

- strict `UiDocument`: **12/12 passed**;
- strict `UiMenuWorkspace`: **7/7 passed**;
- strict unified editor: **92/92 passed**;
- affected layout/render/interaction/runtime suites: **3/3, 6/6, 6/6, and 6/6 passed**;
- clean optimized full `make test`: **passed**, status 0;
- full `make asan`: **passed**, status 0, with no AddressSanitizer or leak report;
- full `make ubsan`: **passed**, status 0, with no undefined-behavior report;
- production `make all`: **passed** under C11 `-Wall -Wextra -Wpedantic -Werror`;
- smoke: **passed** with `{"smoke":"ok","map_width":10,"map_height":6}`;
- deprecated legacy-symbol and current-renderer guards: **passed**;
- scoped trailing-whitespace check: **passed**;
- `make style`: **skipped**, because `cppcheck` is unavailable in the environment.

No display-backed manual editor session was run; keyboard routing, overlay presentation, and
workspace behavior are covered at deterministic headless boundaries.

## Next increment boundary

I15 should expose existing v3 visual/layout fields through the Menu property workspace:
horizontal/vertical anchors, alignment, native/sprite mode and sprite ID, authored colors,
fill/border toggles and glyphs, and default visibility. It should not expand the persisted
schema unless an existing field proves insufficient.