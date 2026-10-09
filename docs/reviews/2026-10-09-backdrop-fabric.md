# The backdrop is a solid fabric with a travelling wave

Reference of record: UI_LOOK_AND_FEEL_REFERENCE_OF_RECORD.md §1.1 ("fluid motion expressed through
discrete cells", "coordinated patterns rather than independent flicker"), §1.3 ("ripples should travel
and settle", "the marks should be simple; the behavior should carry the richness"), §3.4 (decorative
chromatic material), §4.4 (motion budget; nothing obscures a control), and the **change of direction
recorded 2026-10-09** in that document. Target document: UI_DISPLAY_SURFACE_TARGET_2026-10-07.md §3.1,
§6, §7.

## Summary

The `living_field` backdrop is no longer a weave of migrating rectangular patches. It is a **solid
fabric** whose glyph density is carried by **one travelling wave**, and the RGB chromatic aberration
now rides the **wake** of that wave as well as the fabric's edges where it meets a control.

Owner direction, recorded verbatim in the reference of record:

> For the motion of the backdrop, lets try a solid field that is rippling gently. A smooth wave that
> goes from the bottom right corner up to the top left as the main centerpiece motion on a loop. Our
> rgb abberation follows in the wake of the wave and when it hits the menu elements. The 'wave' should
> be an effect on the 'fabric' of the backdrop. We can take inspiration from the way a flag or gentle
> water surface might move.

This is Stage A of three: the backdrop. The state-change transitions (a tide between menus, a settle
into the world frame) are Stage B and Stage C and are **not** built here.

## What changed

- **`src/ui_animation.c`, `render_living_field` and its helpers.** One evaluator, one preset name
  (`living_field`), so all four authored menus and the editor's scene browser inherit the change with no
  asset edit.
  - **Solid.** Every blank cell in the region takes a glyph from the density ramp. The material is
    continuous; there are no gaps between shapes, so it can never read as tiles.
  - **One travelling wave.** `living_field_wave()` sums a main wave (`WAVE_CELLS` 48,
    `WAVE_CYCLES` 3) and a finer ripple (`RIPPLE_CELLS` 17, `RIPPLE_CYCLES` 1) along the diagonal
    distance from the surface's bottom-right corner. Their beat is the flag-like lift and settle.
  - **Bottom-heavy density.** `living_field_band()` curves the density through `BAND_GAMMA` 3.0, so the
    sheet is dim and quiet between crests and the wave reads as *light moving through the material*
    rather than as an evenly lit field.
  - **Weave, not stripes.** A static per-cell grain (`living_field_weave`) spreads the glyph choice one
    band either way, so the sheet reads as woven light cells with visible gaps. It is a pure function of
    position: no block structure (no grid artefact), no flicker.
  - **Fold.** A slow anti-diagonal fold (`FOLD_CELLS` 43, `FOLD_CYCLES` 2, ±0.5 band) breathes across
    the direction of travel.
  - **The wake.** `living_field_behind()` measures how far a cell sits behind the *nearest* main crest;
    within `WAKE` (5 cells) a `WAKE_SPECKLE` (18%) fraction of cells takes one of the three additive
    primaries, chosen by how far behind the crest it sits. The trail therefore follows every crest, and
    it is a sparse prismatic speckle rather than a tinted band.
  - **Edges unchanged in kind.** Fabric cells whose neighbour is a drawn glyph or the region border
    still fringe by direction (red left, blue right, green horizontal) — "when it hits the menu
    elements".
  - Sampling stays table-based (the wave, its band and the fold are sampled once per coordinate), with
    three lookups and one hash per cell and no per-cell trigonometry.
- **`docs/UI_LOOK_AND_FEEL_REFERENCE_OF_RECORD.md`.** Change of direction recorded 2026-10-09, citing
  the owner's words; §3.4's "Application (the `living_field`)" rewritten to describe the solid fabric,
  the travelling wave and the wake.
- **`docs/UI_DISPLAY_SURFACE_TARGET_2026-10-07.md` §3.1.** "Material behaviour" and "Chromatic
  treatment" rewritten to match.

## Tuning record — what the captures rejected

Three intermediate versions were built, captured and rejected on evidence, not on opinion:

1. **One swell across the whole diagonal, wide solid rainbow wake.** The wake's colour selection was a
   function of distance along the trail only, so each third painted a *contiguous* band: a wide rainbow
   stripe, not an aberration. Rejected: it read as a colour fill (§3.4), and the fabric was bright
   enough to fight the labels.
2. **Block-based weave.** A 3-cell block hash gave the grain vertical and horizontal coherence: a
   visible grid, and flat horizontal runs of one glyph. Rejected: grid artefacts and stripes are the
   opposite of a fabric.
3. **Gamma 1.6, then 4.0.** At 1.6 the sheet sat around band 4 and the labels lost contrast; at 4.0 the
   body collapsed to near-black. 3.0 is the value kept, with the crest as the only bright line.

## Evidence

- `make test-ui-animation` — 11/11. `test_living_field_is_deterministic_bounded_and_reduced` passes
  **unchanged**: the accepted properties (neutral structure mixed with coloured traces, primaries only,
  identical cells at the loop boundary, region bounds, reduced motion drawing nothing) all still hold,
  so no test was weakened or rewritten to accommodate the new look.
  `test_surface_extent_fills_the_frame_unless_bounded` still passes.
- `make benchmark-ui-field` — `living_field ... surface 3.143 ms over 200 iterations (budget 6.0 ms)`.
  The fabric costs the same order as the weave it replaced and stays inside the §7 6 ms surface budget.
- **Reviewed fixture refresh.** The composed-frame oracle
  (`tests/fixtures/ui_workbench_frame_fixtures.h`, asserted by `test-ui-workbench-frame` and
  `check-ui-workbench-frame`) records the byte-level look of the workbench frame for the four menu
  contexts, and all four preview the `living_field` backdrop, so all four checksums changed. They were
  re-recorded from the verified oracle run (`build/ui-workbench-frame --context <c> --scale <s>
  --elapsed-ms 0 --checksum-only`) and both the fixture header and the `Makefile` expectations were
  updated in the same change. This is the refresh path the fixture file prescribes ("Refresh them only
  with an explicit review, never silently"), and no other rendering path changed.
- Live capture at the active UI scale, 12 frames at 150 ms across one `AMBIENT` loop:
  `/tmp/field_loop.gif` (the loop, for review), `/tmp/field_strip.png` (three phases side by side),
  `/tmp/field_zoom2.png` (close-up: the weave, the crest and the prismatic wake running through the
  labels). Reproduce with `python3 /tmp/field_capture.py 12 150` against a running `build/ascii-fps`.
- `make test` — see the validation section below.

## Validation

- `make test` exit 0 (full suite).
- `make test-ui-standards` exit 0 (all UI standards runners).
- `make check-ui-workbench-frame` PASS (recorded frame checksums match).
- `make check-project-structure check-test-inventory check-static-analysis-policy check-unsafe-calls`
  all PASS.
- `make benchmark-ui-field` inside budget (above).
- Live capture reviewed at the active scale. Captures are session-local in `/tmp` and are not committed
  (version control is out of scope for this change).

## Not done here

- **Stage B — the tide between menus.** Directed behaviour: the material surges in and covers the
  surface, then flows back out revealing the new elements. This needs a decision first: the shared
  evaluator protects authored cells from every animation unit
  (`test_lifecycle_protects_authored_spaces_not_backdrop`), and §4.4 says nothing in motion may obscure
  a control. A cover that hides labels is therefore a *sanctioned exception*, not an implementation
  detail, and it is recorded only once that scoping is agreed.
- **Stage C — the settle into the world frame.** Directed behaviour: the material flows out to grid
  positions, cycles through glyphs, and resolves onto the live frame. This is a menu→world transition
  and needs its own timing state in `src/app.c` and its own frozen-frame transform.

*(Both were built later the same day: see `docs/reviews/2026-10-09-state-transitions.md`. Stage B's
tide is authored as elements in the four menu layouts; Stage C's settle is asked for by name by the
runner, because which transition plays is a fact about the destination.)*

## Follow-up (same day): the rate is a setting

The owner found the material a little fast and asked for its speed to be a value they could try rather
than a constant. Nothing about the material changed. The ambient loop the wave advances over is now
`backdrop_period_ms` in `config.ini` (default **2600 ms**; the theme's token value remains 1800 ms),
applied once at startup through `ui_theme_set_ambient_period_ms`. Because the wave advances a whole
number of wavelengths per loop, one loop still lands on exactly the same surface and phase 0 is the
same surface at *any* period — which is what makes the value safe to try, and why the recorded frame
fixtures (rendered at phase 0) do not move. `test_ui_animation` proves both properties,
`test_config` bounds the value to 400..60000 ms and refuses a whole file outside it, and every other
motion role and duration is untouched.

