# The tide, drawn in and back out: the characters themselves

Reference of record: UI_LOOK_AND_FEEL_REFERENCE_OF_RECORD.md §1.1 ("fluid motion expressed through
discrete cells", "the marks should be simple; the behavior should carry the richness"), §1.2 (a change
of state is one continuous motion), §3.3 (motion roles; `UI_THEME_MOTION_TRANSITION`), §4.4 (motion
budget; nothing obscures a control), and the **change of direction recorded 2026-10-12**. The identity
these transitions serve — **black, white, red, green, blue and flowy, smooth motion** — is locked in
that document ("the visual identity, locked"). This supersedes the between-surface tide's geometry in
`2026-10-09-state-transitions.md`; the menu→world settle (Stage C of that record) is unchanged apart
from its window.

## Summary

The owner reported two things about the state transition: it still read as a blur — barely one frame —
and, more importantly, it did not "draw in the characters". The material flooded the surface instead
of the characters themselves moving. Three things changed:

- **The tide now carries the surface's own characters.** `render_tide` (`src/ui_animation.c`) reads the
  surface, turns the ground to **opaque black**, and re-draws each cell at `centre + (cell - centre) *
  scale`, with `scale` running 1 -> 0 as the characters draw in and 0 -> 1 as they flow back out. Text
  keeps its face and colour, the fabric keeps its weave, and the ground the characters leave is black.
  The outermost ring of the gathered mass carries the same red/green/blue chromatic fringe the backdrop
  draws where the material meets empty space.
- **The window is its own role, and a long one.** A new `UI_THEME_MOTION_TRANSITION`
  (`src/ui_theme.{h,c}`) gives a change of state **1600 ms** each way — a 3.2 s round trip, timed like
  a deep breath in and a deep breath out. It is held apart from the control-level
  `MAJOR_ENTER`/`MAJOR_EXIT` roles (unchanged at 160/120 ms), so retuning the transition never slows a
  button's reassembly or the pause registration with it.
- **The backdrop is laid down before the transition.** `src/app.c` draws the `while_visible` field
  before the tide on both the incoming and the outgoing surface — drawn after, the field would fill the
  black the characters leave — and on the ambient clock, so the fabric is continuous across the change
  of state. The last frame of the exit window is held one instant inside it, so the overlay never
  flashes the un-gathered outgoing surface at the handover.

Owner direction, recorded verbatim in the reference of record:

> ... The characters all draw into the center and then flow out again, like a tide coming in and going
> out. ... It also doesnt draw in the characters. We expect all the characters on the screen to appear
> to draw in, leaving black behind, and then flow back out. Dont worry about it being fast, its
> stylized. Time it like a deep breath in and a deep breath out. The base level motion is good though
> ... just work on the execution. ... keep the same styling thats working elsewhere ... White, Black,
> Red, Green, Blue only.

## What changed

- **`src/ui_animation.c`, `render_tide`.** A radial contraction and expansion of the surface's own
  cells. The window's linear fraction is recovered from the shared ease and laid over the tide's own
  curve, so the characters leave gently, cross at a steady pace, and settle home slowly. A snapshot of
  the region is taken first — the characters cross over one another as they draw in, so the surface has
  to be read before the ground it leaves is written — the ground is turned to an *opaque* space (the
  empty glyph 0 would be transparent and let the frame underneath show through), and each non-empty
  cell is re-drawn at `centre + (cell - centre) * scale`. The boundary ring then takes the backdrop's
  chromatic fringe where a gathered cell meets the black.
- **`src/ui_animation.c`, `animation_progress`.** A context unit's role is now chosen with the
  primitive's registry spec: a `covers_content` primitive on a context trigger runs `TRANSITION`, while
  every other context unit keeps `MAJOR_ENTER`/`MAJOR_EXIT`. `render_settle` takes its window from the
  same role, so the menu→world handover shares it.
- **`src/ui_theme.{h,c}`.** `UI_THEME_MOTION_TRANSITION = 1600 ms`, appended after the existing roles so
  no index moved. `MAJOR_ENTER`/`MAJOR_EXIT` are untouched.
- **No asset changed.** The tide is one primitive authored once per menu; `assets/ui_elements/*_tide_*.txt`
  and `assets/ui_effects/tide_*.txt` are untouched, as is `master_map.txt`. All four menus move
  together.

The reveal retraces the cover exactly (the surface is a pure function of one measure — how far in the
characters sit — not of a clock), the exit ends where the enter begins, reduced motion still draws
nothing, and the `covers_content` sanction is unchanged.

## Evidence

Deterministic text probe of the composited cells (a scratch tool built from the runner's sources and not
committed — a capture of the surface is session-local and version control is out of scope for this
change). All figures are cells of a 260x160 surface, 41,600 in total, rendered through
`ui_animation_render_layout` exactly as the application does: the layout over its fabric, then the tide.

**The tide between surfaces.** `drawn` is the number of cells carrying a glyph; `reach` is how far the
drawn content extends from the surface's centre, in cells (the corner is 152.0). The window is 1600 ms
each way.

| t (ms) | exit: drawn | exit: reach | enter: drawn | enter: reach |
|--------|-------------|-------------|--------------|--------------|
| 0      | 41,600 (100.0 %) | 152.0 | 1 (0.0 %) | 0.7 |
| 200    | 38,192 (91.8 %)  | 145.3 | 96 (0.2 %) | 6.5 |
| 400    | 29,920 (71.9 %)  | 128.6 | 1,092 (2.6 %) | 24.0 |
| 600    | 19,580 (47.1 %)  | 103.9 | 4,264 (10.2 %) | 47.9 |
| 800    | 10,400 (25.0 %)  | 75.6  | 10,400 (25.0 %) | 75.6 |
| 1000   | 4,264 (10.2 %)   | 47.9  | 19,580 (47.1 %) | 103.9 |
| 1200   | 1,092 (2.6 %)    | 24.0  | 29,920 (71.9 %) | 128.6 |
| 1400   | 96 (0.2 %)       | 6.5   | 38,192 (91.8 %) | 145.3 |
| 1600   | window ended     | –     | 41,600 (100.0 %) | 152.0 |

The exit and enter columns are the same figures in reverse: the reveal flows out exactly what the
draw-in gathered. The characters really draw in — the content's reach falls from the corner (152.0) to
the centre (0.7) — and the ground they leave is black, so the drawn fraction is a smooth S from 100 %
to a 96-cell clump at the centre and back. At exactly the window's end the unit reports itself finished
and draws nothing (that is why the exit row reads "window ended"), so the application holds the last
frame one millisecond inside the window; the enter at its own end is the surface at rest, having
completed the flow-out.

**The handover.** The last exit frame against the first enter frame differ in **3 of 41,600 cells
(0.01 %)**, so nothing jumps when the surface is swapped.

The properties are held by the suite, not only by this probe. `test_tide_draws_the_characters_in_then_restores_them`
requires the exit to open on the surface exactly as it stood, the characters to draw toward the centre
at halfway, the ground to be black and the centre to be all that is left (with the chromatic fringe) at
the close, and the control to be restored untouched on the way out.
`test_tide_draws_in_with_black_behind_and_retraces` requires the rest surface to be full to its
corners, the corners to be empty and the middle covered at halfway, the close to be nearly all black,
the two directions to compose the same surface at equal draw-in, the handover frames to agree within
1 %, and the tide to be unmoved by retuning the ambient loop.

## Validation

- `make test` exit 0 (full suite).
- `test-ui-animation` 16/16 (its two tide tests rewritten for the characters-drawn-in contract, and the
  app path test now lays the fabric down first), `test-ui-theme` 9/9 (the new role's duration),
  `test-ui-motion` 5/5, `test-ui-animation-playback` 4/4, `test-ui-pause-motion` 5/5,
  `test-ui-workbench-chrome` 24/24 (the lifecycle rule now allows an authored cell to be blacked
  inside the transition window; the recorded pixel fixtures did **not** move, because the tray's
  transition cards sample the whole-menu evaluator and the tide is not among them).
- `make standards` exit 0; `make check-ui-workbench-frame` PASS.

## Not done here

- **No live capture was reviewed at the active scale.** What the numbers establish is the drawn fraction
  and the reach through both windows, the black the characters leave, the handover continuity, and the
  reversal at equal draw-in; what they cannot establish is how it *reads* on the display, which no
  capture in this change covers.
- **The menu→world settle keeps its own motion.** A surface handing over to the live frame still
  condenses onto its lattice, cycles through the fabric's glyphs, and thins away (the 2026-10-09
  direction, Stage C), now on the same `TRANSITION` window. If the owner wants that handover to draw
  the characters in as well, it is a separate change.

