# R12 I13 Menu Element Authoring Plan — 2026-09-11

## Status

**Implemented; automated verification passed on 2026-09-11.** I13 extends the
`Ctrl+U` authored-game Menu workspace with typed element construction, staged Text/Button
field editing, and confirmed subtree removal while preserving the I12 preview, lifecycle,
and application-UI boundary.

## Locked document mutations

`UiDocument` adds three narrow failure-atomic APIs without changing persisted v3 data:

- `ui_document_set_content` edits printable one-line content on Text/Button elements;
- `ui_document_set_flow_port` edits a Button identifier and revalidates global uniqueness;
- `ui_document_remove_subtree` removes one non-root element and all descendants while
  preserving retained document order, stable IDs, and monotonic next-ID allocation.

Root removal, Container content, non-Button ports, empty/invalid ports, duplicate ports,
missing elements, malformed source documents, and state-ID exhaustion reject without
changing the caller's document.

## Workspace actions and construction

`E` opens a context action list for the selected hierarchy element. Availability is typed:

- every non-root element exposes Properties and Remove;
- Containers expose Add Container, Add Text, and Add Button;
- Text and Button expose Edit content;
- Button additionally exposes Edit flow port.

New children use deterministic collision-free identities (`container_N`, `text_N`, or
`button_N`). New Buttons initially export the matching unique `button_N` port. Workspace
defaults are Container 20×8, Text 12×1 with `TEXT`, and Button 12×3 with `BUTTON`, so every
new element is immediately visible and remains editable through I12 properties.

Backspace from hierarchy/actions opens explicit removal confirmation. Removing a Container
removes its complete subtree as one command and selects the retained parent. Undo restores
the exact subtree and removed selection; redo selects the parent again.

## Text and port editing

Content and port changes are staged in a workspace buffer. Enter validates and commits one
command; Escape cancels without document/history mutation. Backspace edits the staged
buffer. Content accepts the existing bounded printable ASCII contract; ports accept only
the existing bounded identifier vocabulary. Invalid/empty/duplicate ports remain staged
with a visible mutation error so the user can correct or cancel them.

Text modes own routing before letter-based scene/editor actions. `Ctrl+U` and Escape still
cancel the active field. SDL text input is active for Menu names, content, and ports.

Changing a Button port does not silently rewrite `game.flow`: that would require a separate
cross-document transaction and dirty-state design. The next authoritative project-catalog
composition exposes the changed port and existing reference validation reports any stale
flow edge.

## History and persistence

I12's compact layout-only delta cannot represent arbitrary create/remove/text commands.
I13 therefore stores heap-owned exact before/after `UiDocument` snapshots only for commands
that exist, still capped at 32. Ownership is released on redo truncation, document open,
Discard, workspace close, and editor destroy. Each add, field commit, layout adjustment, or
subtree removal is one command. Save updates saved identity/path in every retained snapshot,
preserving Save-relative dirty behavior across undo/redo.

## Preserved boundaries

I13 does not add element rename, reparent, reorder, duplicate, anchors, colors, borders,
alignment, sprite assignment, pointer selection, drag/resize, runtime interaction preview,
target loading, HUD authoring, or application-menu replacement. It never scans or writes
application-owned `ui_layouts` or `ui_elements`. Authored normal colors and centralized
transient preview-state colors remain unchanged; selection retains non-color indicators.

## Implementation refinements and failed checks

- Strict compilation rejected unbounded staged-content formatting into the overlay line;
  rendering is now explicitly truncated while the full bounded field remains editable.
- A preview regression initially expected all five content characters from a 1×1 default
  Text. Workspace-created elements now receive usable typed geometry rather than changing
  the lower-level `UiDocument` default contract.
- The first preview assertion also conflicted with the selected-element `>` marker replacing
  the first glyph. The test now changes hierarchy selection before checking content,
  preserving the established non-color focus marker.
- Heap snapshots were chosen over embedding 64 full documents in every editor state; this
  keeps idle controller storage bounded and small while supporting exact subtree history.

## Verification evidence

Final verification on 2026-09-11:

- strict `UiDocument`: **11/11 passed**;
- strict `UiMenuWorkspace`: **6/6 passed**;
- strict unified editor: **91/91 passed**;
- strict input: **15/15 passed**;
- affected project catalog and I3–I6 document/layout/render/interaction suites: **passed**;
- clean optimized full `make test`: **passed**, status 0;
- full `make asan`: **passed**, status 0, with no AddressSanitizer or leak report;
- full `make ubsan`: **passed**, status 0, with no undefined-behavior report;
- production `make all`: **passed** under C11 `-Wall -Wextra -Wpedantic -Werror`;
- `make smoke`: **passed** with
  `{"smoke":"ok","map_width":10,"map_height":6}`;
- deprecated legacy-symbol and current-renderer guards: **passed**;
- scoped trailing-whitespace and forbidden application-UI coupling guards: **passed**;
- `make style`: **skipped**, because `cppcheck` is unavailable in the environment.

No display-backed manual editor session was run; the visual/input workflow is covered at
deterministic headless boundaries.

## Next increment boundary

I14 should add element rename plus parent/order authoring: validated stable-name editing,
reparenting under Containers with cycle prevention, and deterministic painter-order moves,
all as transactional hierarchy commands. Pointer hit selection and drag/resize should
follow only after keyboard hierarchy semantics are complete and verified.