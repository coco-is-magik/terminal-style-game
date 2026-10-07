# Pause-context living field (first slice of the §1 target)

Reference of record: UI_LOOK_AND_FEEL_REFERENCE_OF_RECORD.md §1.1 (dense coordinated chromatic
display; controls dependable, display alive), §1.3 (behaviour-first material language), §2.1
(deterministic, explicit time, stable controls, decorative chromatic channels use accent/focus),
§4.4 (nothing obscures a control); and §6/§3.3 (the 2026-10-07 change of direction and the ambient
role). Target document: UI_DISPLAY_SURFACE_TARGET_2026-10-07.md §3.1, §5, §7.

## Summary

The first bounded slice of the 2026-10-07 direction. A new shared-evaluator animation preset,
`living_field`, renders a dense, coordinated, chromatic backdrop and is bound behind the pause menu
as `animation_living_field`. It is the first shipping surface to carry the §1 character rather than a
diagnostic specimen's sparseness.

## Decision

- **D-1 (chromatic material set): deferred.** The field uses only the two §2.1-sanctioned decorative
  channels, palette `accent` and `focus` (a mint/pale-mint pair). It is dense and alive, but not yet
  multi-hue; true "saturated primary and secondary" material still requires an additive `ui_theme`
  chromatic-material set.
- **D-2 (field duration): resolved.** An additive role `UI_THEME_MOTION_AMBIENT` (1800 ms) was added
  to §3.3, and `while_visible` now maps to it, so the field has an in-vocabulary period instead of a
  hidden constant; the 80/160/120/120 ms transition roles are unchanged.

## Behaviour

- A wave `sin(coord·2π/10 − phase)` drives glyph occupancy; a second wave `sin(coord·2π/17 − phase)`
  selects `accent` vs `focus`, so the two traces separate and recombine as the pattern travels
  (§1.1). `phase = fmod(elapsed, 1800 ms)/1800 ms·2π`, a pure function of explicit time.
- `orientation` selects the axis; `radial` ripples from the region centre (§1.3 ripple, travel and
  settle).
- The field fills only **empty** cells inside its bounds, so it never overwrites authored text, a
  control, or a focus marker — including in the workbench preview path, which draws without an
  authored mask.
- Reduced Motion draws nothing (the evaluator returns before dispatch).

## Files

- `src/ui_animation.c` — `render_living_field()` + dispatch; `while_visible` → ambient role.
- `src/ui_theme.h` / `src/ui_theme.c` — `UI_THEME_MOTION_AMBIENT` = 1800 ms.
- `src/ui_ele.c` — `ui_ele_animation_is_valid()` accepts `living_field`.
- `src/ui_workbench.c` — preset chooser lists `living_field`.
- `assets/ui_elements/animation_living_field.txt` — reusable template.
- `assets/ui_layouts/pause_menu.txt`, `assets/ui_layouts/master_map.txt` — binding.
- `tests/test_ui_animation.c` — `test_living_field_is_deterministic_bounded_and_reduced`.
- `tests/test_ui_theme.c` — ambient role expectation.
- `tests/benchmark_ui_field.c` (+ Makefile) — `benchmark-ui-field` / `stability-ui-field`.

## Evidence

- `make test-ui-standards` — all owners pass (added coverage above).
- `make benchmark-ui-field` — **0.616 ms** average over 200 iterations, under the 6 ms budget;
  `--stability` identical over 1000 iterations.
- Refreshed contract fixtures (pause only), each with this record cited:
  - `tests/fixtures/ui_workbench_frame_fixtures.h` `{MENU_PAUSE,100}` → `13042746718342089901`.
  - `Makefile` `check-ui-workbench-frame` pause value → `13042746718342089901`.
  - `tests/test_ui_workbench_chrome.c` `test_runtime_pixel_fixtures` MENU_PAUSE → `8763026618871911208`.
- `tests/test_ui_workbench_frame.c` `test_preview_matches_normal_run_rendering` now skips focus-marker
  cells on both sides (the test's stated intent), because the preview's deliberate `-1` selection and
  the composed frame's selection differ only by markers.

## Limitations and next

- Colour is two-toned (accent/focus only) pending D-1.
- The field is bound only to the pause context; the main menu, HUD, world, and visualizations are
  unchanged. Promoting the field to the main menu is a near-trivial re-bind once D-1 is decided.