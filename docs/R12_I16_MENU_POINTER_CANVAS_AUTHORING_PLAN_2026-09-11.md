# R12 I16 Menu Pointer Canvas Authoring Plan — 2026-09-11

## Status

**Implemented; automated verification passed on 2026-09-11.** I16 adds deterministic
pointer selection, drag move, and bounded resize to the `Ctrl+U` authored-Menu preview while
preserving keyboard-equivalent layout semantics and exact history.

## Input and coordinate boundary

The SDL input adapter retains absolute window pointer pixels plus held left-button state and
one-frame left press/release edges. `input_begin_frame` clears only the edges and logical-grid
validity; held state and absolute position remain. `app.c` uses SDL's configured logical
letterbox conversion, then converts render pixels to logical grid cells exactly once at the
application edge. Headless workspace/controller code receives only preview-local integer cells.

The unified editor now receives viewport columns as well as rows. The Menu preview keeps its
existing grid origin `(40,3)` and bounded 80×25 maximum; pointer routing and rendering derive
the same preview-local coordinates from those values.

## Pointer workflow

- Left press in hierarchy/property mode selects the topmost visible non-root element under the
  pointer using resolved clip geometry and document paint order.
- A press on the already-selected element's visible bottom-right `+` handle begins resize.
  Any other element hit begins move and also selects that stable element ID.
- Motion rebuilds from one exact before snapshot and updates the staged document for live
  preview. This prevents cumulative drift across many motion events.
- Release records at most one exact snapshot-history command. A click without motion changes
  selection but consumes no command.
- Escape or `Ctrl+U` during manipulation restores the exact before document and selection
  without history mutation.
- Invalid/overflowing motion preserves the last valid staged document; the user may correct,
  release the valid state, or cancel. Workspace clear/destruction frees an active snapshot.

The pointer changes the same authored x/y or width/height fields as keyboard properties.
Start/Center/End/Stretch retain their existing I4 meaning; I16 does not apply hidden
anchor-dependent rewrites. Keyboard properties remain the exact accessible equivalent.

## Rendering and ownership

The preview overlays a non-color `+` resize handle at the selected non-root element's resolved,
clipped bottom-right cell. During manipulation the footer shows Drag/Release/Escape guidance.
The Menu workspace consumes pointer activity before camera or scene editing. Application code
clears consumed deltas and button edges before downstream use.

Only an active manipulation owns one additional heap `UiDocument` snapshot. Idle workspace
storage is unchanged. The existing history still owns exact before/after snapshots only for
committed commands and remains capped at 32.

## Preserved boundaries

I16 does not add drag reparenting, drag ordering, multi-handle constraint editing, element
duplication, runtime interaction preview, multi-resolution controls, target loading, HUD
binding, new element types, schema changes, or application-menu replacement. It never scans or
writes application-owned `ui_layouts` or `ui_elements`.

## Implementation refinements and failed checks

- The first strict compile caught pointer functions placed before private `select_id` and
  `record_change` declarations. Explicit private forward declarations restored C11 ordering.
- Extending positional `InputEvent` initializers with relative fields triggered `-Werror` across
  input tests. The design was simplified: `InputEvent.x/y` retain their established relative
  motion meaning, while `input_process` queries absolute pointer position once per frame.
- A combined correction patch had one context line from the wrong file and was rejected
  atomically. It was reapplied as exact file-local edits.
- Paint-order hit choice explicitly compares resolved `paint_order` rather than assuming the
  resolver output array is ordered after I14 reparents.
- Press and release edges in one frame immediately finalize selection so a fast click cannot
  leave a stuck manipulation.

## Verification evidence

- strict input: **15/15 passed**;
- strict `UiMenuWorkspace`: **9/9 passed**;
- strict unified editor: **94/94 passed**;
- strict `UiDocument`: **12/12 passed**;
- affected layout/render/interaction/runtime suites: **3/3, 6/6, 6/6, and 6/6 passed**;
- clean optimized full `make test`: **passed**, status 0;
- full `make asan`: **passed**, status 0, with no AddressSanitizer or leak report;
- full `make ubsan`: **passed**, status 0, with no undefined-behavior report;
- production `make all`: **passed** under C11 `-Wall -Wextra -Wpedantic -Werror`;
- smoke: **passed** with `{"smoke":"ok","map_width":10,"map_height":6}`;
- deprecated legacy-symbol and current-renderer guards: **passed**;
- scoped trailing-whitespace check: **passed**;
- `make style`: **skipped**, because `cppcheck` is unavailable in the environment.

No display-backed manual editor session was run; absolute/logical coordinate routing, selection,
move/resize preview, commit/cancel, handle rendering, history, and isolation are covered at
deterministic headless boundaries.

## Next increment boundary

I17 should add supported logical-resolution/UI-scale preview choices, runtime-like Menu test
mode using the existing I9 host, current project action/reference diagnostics, and typed target
request reporting. It must not load targets, rewrite `game.flow`, or replace application menus.