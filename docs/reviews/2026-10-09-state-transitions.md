# State changes: the tide between menus, the settle into the world frame

Reference of record: UI_LOOK_AND_FEEL_REFERENCE_OF_RECORD.md §1.1 ("fluid motion expressed through
discrete cells", "coordinated patterns rather than independent flicker"), §1.2 (a change of state is
one continuous motion), §1.3 ("ripples should travel and settle", "the marks should be simple; the
behavior should carry the richness"), §3.4 (decorative chromatic material), §4.4 (motion budget;
nothing obscures a control), and the **change of direction recorded 2026-10-09** in that document,
item 4. The identity these transitions serve — **black, white, red, green, blue and flowy, smooth
motion** — is locked in that same document ("the visual identity, locked"). Targets:
UI_DISPLAY_SURFACE_TARGET_2026-10-07.md §4.2 (transitions), §6 (budget).

## Summary

A change of state is now played by the material that is already on screen, and **which transition
plays is a fact about the destination**:

- **Between two surfaces** the **tide** surges in from the surface's bottom-right corner, covers
  everything, and — on the incoming surface — drains back out revealing the new elements. The exits
  of the four menus author `tide_cover`; the enters author `tide_reveal`.
- **From a surface into the world frame** (the game, the editor) the **settle** condenses the surface
  onto its lattice, cycles the characters through the fabric's glyphs where the backdrop wave crosses
  them, and then thins away, so the live frame underneath is simply uncovered.

Owner direction, recorded verbatim in the reference of record:

> For the transition, all the characters should surge in and cover the menu like a receding tide, then
> flow back out to their original place revealing the new menu elements. If we are going from
> menu -> game instead, they should instead flow out to grid positions and flicker through characters
> before settling on revealing the actual frame.
>
> The UI should feel flowy and fluid, and transitions should feel like a hologram changing smoothly
> between states with our RGB chromatic abberation to accent it.

Stage A (the backdrop fabric and its wave) was recorded in `2026-10-09-backdrop-fabric.md`. This is
Stage B (the tide) and Stage C (the settle).

## What changed — Stage B, the tide

- **`src/ui_effect.{h,c}`.** Two primitives, `tide_cover` (trigger `context_exit`) and `tide_reveal`
  (trigger `context_enter`). Both declare `covers_content`: the tide is the sanctioned exception that
  may pass over authored content, and only for as long as it reports a cover. The table's comment
  records the rule; the evaluator (`ui_animation_render_layout`) only withholds the restore of
  authored cells while a primitive reports a cover, so every other primitive keeps the guarantee that
  motion never overwrites a control.
- **`assets/ui_effects/tide_cover.txt`, `tide_reveal.txt`** and **`assets/ui_elements/animation_<menu>_tide_cover.txt`,
  `..._tide_reveal.txt`** for main, pause, settings and confirm. Surface-extent elements, so x/y/width/
  height are unused and the target only anchors them.
- **`src/ui_animation.c`, `render_tide`.** The flood rides the same diagonal as the backdrop wave and is
  drawn from the same ramp and the same per-cell weave, mapped into the top of the ramp
  (`UI_TIDE_BASE`, `UI_TIDE_SPAN`) so the arriving material is denser and still woven. Its edge rides
  the backdrop's anti-diagonal fold (`UI_TIDE_BEND`, `UI_TIDE_RAGGED`), so it arrives as a travelling
  swell rather than a straight line, and `UI_TIDE_FRONT` cells of the edge carry the chromatic fringe —
  "our rgb abberation ... accents it".
- **The tide's own curve inside the shared window.** The shared motion curve is a fast-start pop, which
  is right for a control acknowledging a press and wrong for water: it put most of the surface under
  the flood in the first frames. `render_tide` now recovers the linear fraction of the window from the
  shared curve (its cube root) and lays the tide's own curve over it — leaving the corner slowly,
  crossing at a steady pace, pressing home gently. The window itself is unchanged.
- **`src/app.c`, the handover.** The exit window is `THEME.MAJOR_EXIT` and the enter window
  `THEME.MAJOR_ENTER`, both from `ui_theme_motion_duration_ms` (the literals that stood in for them are
  gone). The incoming surface's reveal starts on the frame the outgoing cover completes, so the two
  halves are one motion; input to the menu is ignored until the new controls are live.
- **`src/app.c`, the underlay capture rule.** The recessed underlay is captured **only while no surface
  is up**. A frame captured while a menu was displayed contained that menu, so the next menu recessed
  the menu it replaced instead of the world behind it — the ghost recorded in `/tmp/tide_record.mkv`.
  A menu opened over another menu now keeps the captured world frame, and returning to the world
  forgets it so the next open captures its own.

## What changed — Stage C, the settle

- **`src/ui_effect.{h,c}`.** A third primitive, `settle` (trigger `context_exit`), also
  `covers_content`, and **`assets/ui_effects/settle.txt`** so it is bindable like any other effect. It
  is deliberately **not** authored into a layout: a layout cannot know whether the surface it is
  handing over to is another menu or the live frame, and which transition plays is a fact about the
  destination.
- **`src/ui_animation.c`, `render_settle`, and `ui_animation_render_world_settle`.** The runner asks
  for the transition by name; the function synthesises one unit addressed at the whole surface, exactly
  the way the evaluator synthesises an unauthored transition for a control, and renders it through
  `render_unit`. One evaluator, one registry, one render path.
- **What the settle does.** The window's linear fraction is recovered from the shared curve, and the
  material is driven by that:
  - **Flow out to grid positions.** Every cell belongs to a block of `UI_SETTLE_LATTICE` (4) cells and
    slides toward that block's far corner, which is where a lattice position is. The character a cell
    shows is read from where that material has travelled to, and the lattice position is never behind
    the cell in either axis, so a character is never read after it has been written — the material does
    not feed on itself.
  - **Condense.** A cell keeps its character while it sits inside the diamond that reaches from its own
    lattice position; the diamond closes over the first half of the window. At the halfway point the
    surface is *exactly* its lattice and nothing else.
  - **Cycle through characters.** Where the main crest of the backdrop wave crosses a lattice position,
    the wave takes the character over and it cycles through the fabric's own glyphs, accented by the
    same prismatic wake the backdrop carries (the same `living_field_behind` /
    `living_field_wake_edge` rule). The takeover waits until the material is moving
    (`UI_SETTLE_TAKEOVER`), so the window's first frame is the surface exactly as it stood.
  - **Thin and resolve.** From the halfway point the lattice thins in a stable order and the material
    advances until `UI_SETTLE_HOLD` (three quarters) and then holds: the last quarter is the settle,
    where the characters are still before they are gone.
  - **Leaving is transparent, not black.** A cell the material has left is written with the empty glyph
    — the one the compositor's copy skips — so the live frame underneath shows through from that cell
    while the rest is still material. The sheet thins on screen instead of blanking the surface and
    being dropped at the end.
  - The wave advances over the **transition window** rather than over the `AMBIENT` loop: a settle is a
    transient, not a surface at rest, so its material crosses the screen once while the window runs.
- **`src/app.c`.** Leaving a menu for the world (the game or the editor) now sets the same exit window
  and plays the settle over the drawn outgoing surface, composited above the frame it is leaving. The
  tide covers on the way to another menu; the settle condenses on the way to the live frame. Reduced
  motion draws neither, which is the recorded no-motion response.

## Second attempt: making the tide read as a tide (2026-10-09)

The first attempt played, and the owner's report on seeing it was:

> It looks like the wave straightens and freezes for a moment then goes to the next thing. It doesn't
> read like a tide receding and going back out, more like a stutter between things.

Three mechanics produced exactly that, and each was measured on the application's own path
(`build/probe_tide_look.c`, composited cells, exit and reveal sampled at the configured 120 fps):

| what it read as | measurement before | cause | measurement after |
|-----------------|--------------------|-------|-------------------|
| "the wave straightens" | the edge of the flood was a perfect anti-diagonal: the boundary's offset was the same on every diagonal, and the material under it carried **1** glyph (one flat stripe), because the band was a constant lift that clamped | `distance = (last_x - 1 - x) + (last_y - 1 - y)` alone decided the flood, and `band = field_band(wave) + UI_TIDE_LIFT` saturated at 6 | the edge swells over **3** or more cells along the anti-diagonal, with cell-scale raggedness, and the flood carries **3+** glyphs: it is the woven fabric |
| "freezes for a moment" | the material's own pattern barely advanced: the phase came from `AMBIENT` (2600 ms), so a 120 ms window advanced it by **4.6 %** of a loop — about 6 cells of travel | `phase = fmod(elapsed_ms, ambient_period) / ambient_period` | the wave journeys `UI_TIDE_TRAVEL` (0.5) of a period *per transition*, driven by the cover, so it moves on every frame and never returns to its opening pattern (a whole number of wavelengths would be a still picture at both ends) |
| "more like a stutter" | the frame the runner swaps surfaces on differed in **34,244 of 41,600 cells (82 %)** from the frame before it, and at one and the same cover the two directions differed in **4,000 cells** | the exit ended at cover 1 under the ambient phase and the reveal opened at cover 1 under a different one; nothing tied them together | the handover frame differs in **52 cells (0.12 %)**, and at equal cover the two directions compose the **same** surface (0 cells): the reveal retraces the exit exactly — "flow back out to their original place" |

The rule that came out of it, recorded in the reference of record (item 4): **a transition is driven by
its own window and by the cover it is crossing, never by the clock the backdrop moves on.** The material
is the same fabric, the edge is the same fold, and the two halves share one phase because they share one
cover — so the surge and the drain are one motion, and the backdrop's speed is free to be a setting.

`test_tide_reads_as_a_tide_not_a_wipe` holds all four properties: the handover frames agree to within
1 % of the surface, the two directions agree at equal cover (found by bisection) to within 1 %, the
edge's offset spreads over at least 3 cells, the flood carries at least 3 glyphs, and the tide composes
byte-identical frames whatever the ambient period is set to.

### The same work exposed a second artifact: the tray was showing a transition

`test_runtime_pixel_fixtures` moved as soon as the menus gained their tide elements, and those four
frames were re-recorded at the time instead of explained. That was the wrong move and is undone here.
The cause: `src/ui_workbench_chrome.c` builds a **full-surface specimen** of the layout and plays
`CONTEXT_ENTER` on it before copying rows out for the tray, so the first *surface-extent* element to
arrive flooded the whole specimen and every tray row taken from it — the tooltip tray was showing the
material of a transition instead of the layout's furniture. The chrome now skips surface-extent units
in a specimen, because such a unit is drawn for the surface it belongs to and not for a sample of one,
and the four recorded frames are exactly what they were before the tide existed (the main context's
checksum matches the pre-tide capture digit for digit). The note in the test records the episode so the
numbers are not "refreshed" over an artifact again.

## Defect found after the fact: the application never played a transition (2026-10-09)

The two transitions above were verified through the *primitive* and through the workbench, and that
was not enough. The application does not load the element catalogue the way the workbench does:

- the workbench (`src/ui_workbench.c`) loads **every** `assets/ui_elements/*.txt` into its cache;
- the application (`src/app.c`) preloads only the per-layout list in
  `assets/ui_layouts/master_map.txt` (`cache_next=`) and then resolves each layout's `elements=` line
  against that cache with a plain lookup (`ui_layout_load`, `src/ui_ele.c`).

A name the cache did not hold became a **NULL slot with no message**, so the tide cover and reveal —
authored into the four menu layouts, never added to the preload lists — were dropped in every
application run. The menus changed state instantly, which is exactly what the owner reported; every
test passed because every test goes through the workbench, which loads everything.

What changed as a result:

- `assets/ui_layouts/master_map.txt`: the tide elements are in each menu's `cache_next=` list. This is
  the actual defect; the list is what the application preloads.
- `src/ui_ele.c`: a name that does not resolve now says so, the way a missing parent or child already
  did (`ui_ele: element '<name>' not found for layout '<layout>'`). Nothing about the behaviour
  changed — the loader still keeps the slot — but the failure is no longer silent.
- `tests/test_ui_animation.c`, `test_application_menu_layouts_play_their_authored_transition`: the
  guard for that class of defect. It resolves each of the four menus *the way `src/app.c` does*
  (master-map cache, one tick, `ui_layout_load`), requires every authored element and every child in
  the parent chain to be non-NULL, and requires the resolved surface to actually change by more than a
  quarter of itself on the way out (cover) and on the way in (reveal). It fails on the pre-fix asset
  with `main_menu: element 6 is unresolved`, and it would have caught this before a single run.

The lesson recorded: a test that loads assets *differently* from the product proves the primitive, not
the product. Anything the product preloads by hand needs a test that preloads it the same way.

After the fix, on the application's own load path (`build/probe_app_transition.c`, the master-map
cache, one tick per layout, `ui_layout_load`, then the layout rendered at explicit times; all four
menus, identical because the tide is one surface material):

| frame | cover of the outgoing surface | cover of the incoming surface |
|-------|-------------------------------|-------------------------------|
| 0 ms (window opens) | 0.0 % | 100.0 % |
| 30 / 60 / 90 ms | 2.2 % / 44.8 % / 94.0 % | – |
| 80 ms of the enter | – | 44.8 % |
| 120 ms of the exit (unit ends) | 0.0 % (the overlay is dropped; the incoming surface is fully covered at that instant) | 0.0 % at 160 ms |

Every layout reports `0 unresolved` elements, where `main_menu` previously reported element 6. A
headless run of the product itself (`SDL_VIDEODRIVER=offscreen ./build/ascii-fps --benchmark-scenario
ui-layered --frames 3`) now loads all four menus and the HUD with no `ui_ele: element ... not found`
line at all; before the fix it printed one per tide element per menu.

## Evidence

Deterministic text probe of the composited cells (`build/probe_settle.c`, a scratch tool built from the
runner's own sources and not committed — a capture of the surface is session-local and version control
is out of scope for this change). All figures are cells of a 260×160 surface, 41,600 in total.

**The settle into the world frame.** `covered` is the set of cells the compositor would copy onto the
overlay, and so the set of cells the live frame underneath is *not* seen through; `differs from start`
counts cells that are not what they were when the window opened.

| t (window 120 ms) | covered | differs from start |
|-------------------|---------|--------------------|
| 0                 | 41,600  | **0**              |
| 30                | 26,000  | 33,946             |
| 60                | 2,600   | 40,666             |
| 90                | 1,325   | 41,105             |
| 120 and after     | 0       | **0**              |

2,600 is exactly the lattice on this surface (one position per block of 4×4), i.e. at the halfway
point the surface is its lattice and nothing else. The window opens on the surface exactly as it stood
and closes with the frame uncovered; the material is 62% of the surface at a quarter of the window,
6% at half, 3% at three quarters, and gone — it thins away rather than being cut, because a cell the
material has left is transparent to the compositor. Two runs at the same explicit times compose
byte-identical cells.

**The tide between menus.** Same surface, `tide_cover` authored in the main menu's layout:

| t (window 120 ms) | covered by the tide |
|-------------------|---------------------|
| 0                 | 0 (0%)              |
| 30                | 871 (2%)            |
| 60                | 16,030 (38%)        |
| 90                | 31,870 (76%)        |
| 120               | 0 (the cover has ended; the incoming surface's own reveal is the whole surface) |

The flood leaves the corner and crosses the surface instead of arriving in the first frames, which is
what the tide's own curve inside the shared window is for.

## Validation

- `make test` exit 0 (full suite).
- `make test-ui-standards` exit 0 (all UI standards runners).
- `make standards` exit 0 (style, the static-analysis policy, and the structure / banned-call /
  test-inventory checks).
- `make check-ui-workbench-frame` PASS (recorded frame checksums match; a transition is not part of a
  settled frame, so no fixture moved for this change).
- `test-ui-animation` 15/15 (including `test_application_menu_layouts_play_their_authored_transition`
  and `test_backdrop_period_retunes_the_speed_of_the_field`), `test-ui-ele` 24/24 (its master-map cache
  counts are a recorded fixture, refreshed with the reason in place), `test-config` 4/4 (the new
  `backdrop_period_ms` bounds), `test-ui-workbench-chrome` 24/24.
- The product's own startup was run headless (offscreen video driver, `ui-layered` scenario, 3 frames)
  and its log carries no unresolved-element message.

## Not done here

- **No live capture was reviewed at the active scale for these two transitions.** The backdrop's
  captures were reviewed for Stage A; this change is verified by the composited-cell probes (the
  primitive through the workbench loader, and the application's own master-map path after the defect
  above was found), by the regression tests, and by a headless run of the product. What the numbers
  establish is the cover fraction at every point of both windows, the exact lattice, the continuity at
  both ends, the resolution of every authored element and determinism; what they cannot establish is
  how it *reads* on the display, which no capture in this change covers.
- **The material's own decoration re-phases at the handover.** The settle advances the wave over its
  window, while the backdrop advances the same wave over the `AMBIENT` loop, so at the first frame of a
  settle the wake speckle is not in the same place as the fabric's. The authored surface is untouched
  at that frame (probe: `differs from start` 0), so this is decoration only; if it reads as a step on
  the display, the fix is to carry the ambient phase into the settle rather than to change the curtain.
