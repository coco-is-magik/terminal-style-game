# The menu surface is the display

Reference of record: UI_LOOK_AND_FEEL_REFERENCE_OF_RECORD.md §1.1 (keep the controls dependable; the
display carries the life), §2.1 (deterministic, explicit time, colours from tokens, stable controls),
§4.1 (faithful preview), §4.4 (nothing displaces or obscures a control). Target document:
UI_DISPLAY_SURFACE_TARGET_2026-10-07.md §3.1, §6, §9.

## Summary

Authored menus no longer sit in an 80x40 box in the middle of a black screen. A menu is composed into
the **whole grid**, exactly like the game and the editor, so it fills the same viewport at the same
cell size, and its backdrop field covers that whole surface.

This closes the last artefact of the menu-surface review: the game viewport and the editor viewport
filled the window while every menu was a small rectangle surrounded by black.

## What changed

- **`src/app.c`.** `APP_UI_MENU_WIDTH`/`APP_UI_MENU_HEIGHT` are gone. The menu canvases are created at
  the grid size, the staging surface is cleared over the whole frame, and
  `ui_canvas_copy_grid_region(..., 0, 0)` copies the surface instead of a centred crop. Layers keep
  `UI_SCALE_INHERIT_GLOBAL`; the anchor stays `UI_ANCHOR_CENTER`, which at 100% is identical to
  top-left (canvas == surface) and above 100% keeps the centred menu content on screen while the
  enlarged canvas still covers it.
- **`src/ui_ele.c/.h`.** New element field `extent=box|surface` (`UiExtent`, zero = `box`) with a
  strict parser; unknown values fail the load.
- **`src/ui_animation.c`.** `render_unit` gives a `UI_EXTENT_SURFACE` unit the whole render surface as
  its bounds, so extent is resolved against the surface it is drawn into rather than a literal.
- **Assets.** The four backdrop fields (`animation_main_field`, `animation_living_field`,
  `animation_settings_field`, `animation_confirm_field`) declare `extent=surface`; their geometry
  fields are zeroed with an explanatory `#` comment because `extent=surface` does not use them.

## Why an element field and not a bigger number

An asset must not carry a grid size: `260x160` is `config.ini`, not a constant. `extent=surface`
resolves against the surface at render time, which is also what keeps the workbench preview honest —
the preview renders into its own 260x160 authored surface, so the same field fills it for the same
reason. This is the legacy element model's counterpart of the v4 document model's
`UI_DOCUMENT_ANCHOR_STRETCH` (`src/ui_layout_resolver.c` already implements it), so the two models
agree on the concept.

## Owner rule recorded 2026-10-08 — one glyph size per frame

Within the game frame — authored menus, the world, and the HUD — every cell is the same size; the
accessibility scale scales the frame as a whole. Explicit carve-outs: the centre crosshair layer and
the workbench's own editor chrome (different instruments, and the workbench's independent scale is a
reviewed decision of its own, `reviews/2026-09-19-application-ui-editor-a3-independent-scale.md`).

This is why the box was **not** fixed by scaling a backdrop: a `FIXED_100` backdrop inside a scaled
menu would have been two glyph sizes in one surface, and a coverage/scale policy enum would have
encoded that mistake. It is a recorded rule guarded by review, not by a test; no test can see
`src/app.c`'s layer construction.

## Evidence

- `test-ui-ele` (24 tests, was 23): `test_ui_element_extent_parsing_and_production_fields` covers the
  strict parser and asserts every production backdrop declares `extent=surface`.
- `test-ui-animation` (11 tests, was 10): `test_surface_extent_fills_the_frame_unless_bounded` renders
  the same unit both ways on a 260x160 grid across six explicit times — bounded (the default) paints
  nothing outside its target, surface paints outside it — and asserts reduced motion still draws no
  field.
- **Fixtures re-recorded with review** (all four contexts changed, nothing else re-tuned):
  `tests/fixtures/ui_workbench_frame_fixtures.h` and the `check-ui-workbench-frame` expectations in the
  `Makefile` (main 12788386773026097164, main@150 8822537883753028476, pause 10582059475855600612,
  settings 12207819633615769261, confirm 5199418223914077754), and the compositor pixel hashes in
  `tests/test_ui_workbench_chrome.c` (4594411236743001354, 7128938155756202966, 9440602223055106050,
  11330631336630635389). `test-ui-workbench-frame` (10), `test-ui-workbench-chrome` (24) and
  `test-ui-workbench` (12) pass.
- **Live native X11 capture** (`--display-acceptance`, Return to start, Escape to pause): the main menu
  and the pause menu both fill the window edge to edge, and the pause menu shows the paused game
  recessed behind it. Artifacts (session-local): `/tmp/surface_main.png`, `/tmp/surface_playing.png`,
  `/tmp/surface_paused.png`. A second capture at the 200% accessibility preset
  (`/tmp/cmp100.png` vs `/tmp/cmp200.png`) confirms the menu content is still centred and fully on
  screen with the enlarged canvas, and that the scale is applied.
- `make` (application) builds clean under `-Werror`.
- **Benchmark** (`benchmark-ui-field`, target §7): the runner now measures both authorable shapes.
  `living_field` bounded 0.737 ms, **surface (shipped) 1.591 ms** over 200 iterations against the
  6.0 ms surface budget; `stability-ui-field` over 1000 iterations: bounded 0.829 ms, surface
  1.935 ms. Both are inside budget with headroom, and the surface case is ~2.2x the bounded case for
  13x the area (most of the surface is negative space that the field skips).

## Scope statements

- **Not changed by this slice:** the accessibility scale still enlarges UI layers relative to the
  unscaled world (pre-existing for every UI layer, including the HUD), and at raised scales a menu's
  *authored* control text can wrap inside its authored button width (`START GAME` does at 200%). Both
  are recorded here as pre-existing, not as consequences of this change.
- **Not implemented:** the workbench has no control for `extent` (or `underlay`). It rewrites the
  `elements=` line and preserves other lines, so hand-authored values survive a save.
- **Now authorable, not shipped:** a bounded (floating-panel) menu is `extent=box` on its field
  together with `underlay=dim`. No layout uses that shape today.

## Consequences for the earlier record

`reviews/2026-10-08-menu-underlay.md` recorded the visible box as deferred work ("slice 2"). Its
premise — that a box comes from the 80x40 canvas crop rather than from authored content — is now
resolved: the box is gone from every shipped layout, and a box is an authored choice again.

