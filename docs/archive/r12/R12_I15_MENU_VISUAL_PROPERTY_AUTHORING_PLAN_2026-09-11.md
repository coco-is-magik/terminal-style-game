# R12 I15 Menu Visual Property Authoring Plan — 2026-09-11

## Status

**Implemented; automated verification passed on 2026-09-11.** I15 exposes every
existing v3 authored layout/visual field through the `Ctrl+U` Menu property workspace without
changing the persisted schema or the I5 render semantics.

## Locked property surface

The existing x, y, width, height, and local scale rows are joined by:

- horizontal and vertical Start/Center/End/Stretch anchors;
- Native/Sprite visual mode and Sprite ID 1–255;
- Text/Button Left/Center/Right alignment;
- independent foreground/background red, green, blue, and alpha channels;
- fill enablement and printable fill glyph;
- border enablement and printable border glyph; and
- default visibility.

Up/Down skips unavailable rows. Left/Right performs one bounded typed adjustment and records
one exact snapshot-history command. Semantic labels such as `Center`, `Sprite`, and `On` are
rendered instead of raw enum values. The property panel scrolls around the selected row.

## Typed availability and mutation rules

- The root exposes visual properties but not its locked layout fields.
- Non-root elements expose layout and anchor fields.
- Alignment is available only for Text and Button elements.
- Sprite mode exposes Visual, Sprite ID, and Visible; native-only authored fields are hidden.
- Native mode exposes authored colors, typed alignment where applicable, fill/border policy,
  glyphs when enabled, and visibility; Sprite ID is hidden.
- Switching Native to Sprite atomically assigns Sprite ID 1. Switching back atomically clears
  Sprite ID to 0, satisfying existing v3 validation at every observable boundary.
- Sprite ID wraps in the existing valid 1–255 range. Color channels clamp at 0/255 and reject
  no-op boundary changes without consuming history. Glyphs wrap through printable ASCII 32–126.
- Entering Stretch clamps a negative axis offset/extent to zero. Leaving Stretch raises a zero
  extent to one. Other authored coordinates remain unchanged.
- Boolean rows toggle for either Left or Right. Every successful operation routes through the
  existing failure-atomic `ui_document_set_layout` or `ui_document_set_visual` API.

Missing Sprite assets remain valid authored references but make the existing preview report
its established missing-visual diagnostic. I15 does not silently choose from loaded assets;
asset browsing belongs to a separately scoped picker only if later evidence requires it.

## History, persistence, and boundaries

Every adjustment participates in the same heap-owned exact-document history. It retains the
newest 32 commands and evicts the oldest at capacity instead of disabling further editing.
Selected stable element identity is preserved across undo/redo. Save/reload preserves all edited
v3 fields. Edit preview shows authored normal colors while retaining non-color selection markers;
Test mode continues to use transient focused/pressed/disabled theme colors.

Manual acceptance initially exposed two issues not covered by the I15 suite: 255 one-step color
adjustments exhausted history at 223, and selected/root authored colors were masked by focus-theme
colors in Edit preview. The closeout correction fixes both and verifies a full 255-step channel
edit, newest-32 undo/redo retention, and exact root authored RGBA preview cells.

I15 does not add pointer selection, drag/resize, runtime interaction preview, multi-resolution
preview controls, target loading, HUD binding, application-menu replacement, new element types,
or a schema migration. It never scans or writes application-owned `ui_layouts` or `ui_elements`.

## Implementation refinements and failed checks

- Making root Properties available changed an I13 test's fixed Down-key assumption. That test
  now selects its intended Add Text action explicitly instead of depending on action-list shape.
- The first context-light test patch matched an unrelated R4 Down-navigation sequence and
  replaced it with a Menu action assignment. The strict unified-editor suite exposed the error;
  the R4 input sequence was restored before I15 verification continued.
- A typed formatter replaced the previous five-element integer array immediately when the enum
  expanded, preventing an out-of-bounds read and providing semantic values for every new row.

## Verification evidence

- strict `UiMenuWorkspace`: **8/8 passed**;
- strict unified editor: **93/93 passed**;
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

No display-backed manual editor session was run; routing, scrolling overlay presentation,
preview output, history, and persistence are covered at deterministic headless boundaries.

## Next increment boundary

I16 should add preview-canvas pointer selection, drag move, and bounded resize handles. Pointer
manipulations should preview continuously but commit as one exact history command on release;
Escape should cancel to the before snapshot. Keyboard hierarchy/property workflows remain the
accessible equivalent and drag reparent/order remain out of scope.