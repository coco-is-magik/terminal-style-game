# V1-3 I1 Pause-Context Motion Implementation — 2026-09-16

## Outcome

The first accepted D6 vocabulary consumer is implemented in one real application context:
the gameplay pause overlay. Automated verification is complete. Native visual acceptance is
pending, so no second context is authorized.

## Preserved interaction contract

- Escape opens and closes pause through the existing immediate `MenuStack` operations.
- Resume activates immediately; exit decoration does not retain or resurrect the menu.
- The pause menu canvas, text, focus markers, controls, action order, and geometry do not
  move.
- Settings and confirmation remain immediate nested subcontexts.
- Motion does not affect pointer behavior, mouse-lock decisions, application state,
  documents, authored Menu state, or persistence.

## Implementation boundary

`ui_pause_motion` is pure and headless. It observes explicit pause visibility, absolute
milliseconds, and reduced-motion state, then returns presentation progress and decorative
visibility. `app.c` owns the SDL clock and renders ten accent/focus punctuation glyphs in a
separate fixed 80x40 canvas. The canvas is composed at z=19 beneath the unchanged menu at
z=20. No generic compositor offset or framebuffer RGB path was added.

Reduced motion is session-only in I1. Settings exposes one dynamic status row and one toggle
button. It starts OFF, resolves active motion immediately when enabled, emits no displaced
decoration, and never reads or writes preference files. Persistence is deliberately deferred
because preference v1 is strict and requires a separate migration decision.

## Verification

- strict default application build: pass;
- strict no-state-tracker application build: pass;
- `test-ui-pause-motion`: 5/5 pass;
- `test-app-modules`: 5/5 pass;
- `test-menu-state`: 13/13 pass;
- `test-ui-ele`: 17/17 pass;
- focused ASan/LeakSanitizer: 5/5 pass, no diagnostics;
- focused UBSan: 5/5 pass, no diagnostics;
- `make test-ui-standards`: pass across fifteen focused owners;
- complete `make test`: pass after bounded resume;
- `make standards`: pass with cppcheck and policy/structure/inventory guards.

## Findings retained

- Byte-level replay initially failed because new struct padding was uninitialized; output
  commits now have deterministic object representations.
- A broad fuzzy application patch initially misplaced decoration lifecycle code; direct
  review corrected clear order, visibility source, and exit scope before final testing.
- A shadowed frame sample would have suppressed decoration despite a clean strict build;
  direct source review found and corrected it.
- Concurrent broad checks timed out during analysis/compilation; sequential/resumed checks
  passed and no product test failed.

## Remaining manual gate

On a native display, test 100/125/150/200% scale, full/reduced motion, rapid Escape/Resume,
opening Settings and confirmation during/after pause entry, and returning to main menu.
Confirm the menu and focus never move, exit decoration never captures input, reduced motion
is immediate, and runtime allocation/texture counters remain quiet.

Stop after this review. A second context requires explicit I1 acceptance and its own bounded
plan.