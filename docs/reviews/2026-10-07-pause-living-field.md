# Living-field menus: pause and main (first slices of the §1 target)

Reference of record: UI_LOOK_AND_FEEL_REFERENCE_OF_RECORD.md §1.1 (dense coordinated chromatic
display; controls dependable, display alive), §1.3 (behaviour-first material language), §2.1
(deterministic, explicit time, stable controls, decorative chromatic channels use accent/focus or
the §3.4 material tokens),
§4.4 (nothing obscures a control); and §6/§3.3 (the 2026-10-07 change of direction and the ambient
role). Target document: UI_DISPLAY_SURFACE_TARGET_2026-10-07.md §3.1, §5, §7.

## Summary

The first bounded slices of the 2026-10-07 direction. A new shared-evaluator animation preset,
`living_field`, renders a dense, coordinated, chromatic backdrop, bound behind the pause menu
(`animation_living_field`) and then the main menu (`animation_main_field`). These are the first
shipping surfaces to carry the §1 character rather than a diagnostic specimen's sparseness.

## Decision

- **D-1 (chromatic material set): resolved 2026-10-07.** An additive decorative chromatic-material
  set was added (see the D-1 follow-up below); the field now draws multiple saturated hues.
- **D-2 (field duration): resolved.** An additive role `UI_THEME_MOTION_AMBIENT` (1800 ms) was added
  to §3.3, and `while_visible` now maps to it, so the field has an in-vocabulary period instead of a
  hidden constant; the 80/160/120/120 ms transition roles are unchanged.

## Behaviour

- Occupancy is the product of two travelling axis waves modulated by a slower diagonal fold wave
  (`wave_x · wave_y · fold`), so lit cells form coherent **rectangular patches** that migrate, breathe
  and fold as one material, with negative space between them — the coordinated travelling pattern of
  §1.1, not a flat stripe or a ring. `phase = fmod(elapsed, 1800 ms)/1800 ms·2π`, a pure function of
  explicit time.
- Colour is **sparse and part of the material**: the structure stays a neutral tone (`text_secondary`)
  and only a minority of lit cells take a saturated `ui_theme_material_palette()` speck. Speck
  membership is gated by a slow moving chroma wave, so colour gathers into regions that flow through
  the fabric and disperse; hue is a per-cell hash, so neighbouring lit cells seldom share a hue and no
  spatial rainbow ramp appears.
- `orientation` selects the axis the two travelling waves emphasise (`horizontal`, `vertical`, or
  `radial`, which folds them about the region centre).
- The field fills only **empty** cells inside its bounds, so it never overwrites authored text, a
  control, or a focus marker — including in the workbench preview path, which draws without an
  authored mask.
- Reduced Motion draws nothing (the evaluator returns before dispatch).

## Files

- `src/ui_animation.c` — `render_living_field()` + dispatch; `while_visible` → ambient role.
- `src/ui_theme.h` / `src/ui_theme.c` — `UI_THEME_MOTION_AMBIENT` = 1800 ms; the decorative
  chromatic-material set (`ui_theme_material_palette()`).
- `src/ui_ele.c` — `ui_ele_animation_is_valid()` accepts `living_field`.
- `src/ui_workbench.c` — preset chooser lists `living_field`.
- `assets/ui_elements/animation_living_field.txt`, `assets/ui_elements/animation_main_field.txt` —
  reusable templates.
- `assets/ui_layouts/pause_menu.txt`, `assets/ui_layouts/main_menu.txt`,
  `assets/ui_layouts/master_map.txt` — bindings.
- `tests/test_ui_animation.c` — `test_living_field_is_deterministic_bounded_and_reduced`.
- `tests/test_ui_theme.c` — ambient role expectation; decorative material-set checks.
- `tests/test_ui_ele.c` — master-map preload count/entry for `animation_main_field`.
- `tests/benchmark_ui_field.c` (+ Makefile) — `benchmark-ui-field` / `stability-ui-field`.

## Evidence

- `make test-ui-standards` — all owners pass (added coverage above).
- `make benchmark-ui-field` — **~0.6 ms** average over 200 iterations, under the 6 ms budget;
  `--stability` identical over 1000 iterations.
- Refreshed contract fixtures (main and pause), each citing this record:
  - `tests/fixtures/ui_workbench_frame_fixtures.h`: `{MENU_MAIN,100}` → `11098204912662601525`,
    `{MENU_MAIN,150}` → `17577477401565191989`, `{MENU_PAUSE,100}` → `3419473230861947814`.
  - `Makefile` `check-ui-workbench-frame`: main → `11098204912662601525`,
    pause → `3419473230861947814`.
  - `tests/test_ui_workbench_chrome.c` `test_runtime_pixel_fixtures`: MENU_MAIN → `2816664593535307868`,
    MENU_PAUSE → `9750079444698591704`.
- `tests/test_ui_workbench_frame.c` `test_preview_matches_normal_run_rendering` now skips focus-marker
  cells on both sides (the test's stated intent), because the preview's deliberate `-1` selection and
  the composed frame's selection differ only by markers.

## D-1 follow-up (2026-10-07): chromatic material set

D-1 was resolved the same day. An additive, **decorative-only** six-colour material set
(`ui_theme_material_palette()`; Reference of Record §3.4) supplies saturated primaries and
secondaries. `render_living_field` draws its chromatic cells from this set, intermingled over the
neutral structure (see "Field rework" below). The set is never semantic and never a literal;
`test-ui-theme` proves opacity, distinctness, and a ≥ 3.0 contrast ratio against `canvas`.

## Field rework (2026-10-07): intermingled selective colour

The first cut of the field used an 8-cell-wide hue ramp over all six material colours — a smooth
spatial sweep. Rendered, this read as literal rainbow stripes (main, `horizontal`) and rainbow rings
(pause, `radial`): a spatial *ordering* of hue that does **not** match the inspiration. Re-measuring
the foundational footage (`docs/inspiration_and_notes/light_and_motion`, the §1.1 source) at cell scale
showed the opposite organisation — ~50% neutral / 50% saturated cells, hues spread evenly across the
whole wheel, and neighbouring cells seldom sharing a hue (agreement ≈ chance beyond one cell). That is
*intermingled* colour over a neutral structure, never a gradient across space.

The field now matches that organisation: neutral-dominant occupancy with intermingled, region-clustered
chromatic traces and a slow temporal drift. Guards: `test-ui-animation` still asserts determinism,
boundedness, seamless loop, and the reduced-motion no-op, and now also asserts that **both** neutral and
chromatic lit cells are present (the old ramp produced only chromatic cells, so this fails if the ramp
returns). `docs/UI_DISPLAY_SURFACE_TARGET_2026-10-07.md` and the workbench asset reference were updated
to describe the intermingled model.

## Fabric rework (2026-10-07): white-dominant flowing fabric

The intermingled model above still read wrong on screen. It filled the surface with a rigid plane wave
of colour noise: flat travelling bands (main) and concentric rings (pause), roughly half-coloured, with
the colour statically hashed per cell. That is a colour show, not the reference. Re-measuring
`docs/inspiration_and_notes/light_and_motion` at cell scale showed a **white-dominant weave** — a neutral
fabric of crisp rectangular forms whose density folds and travels as one material, carrying only
**sparse** saturated RGB specks — and `more_motion` confirmed the intended motion is coherent
multi-scale travel, not per-cell flicker or a single sliding stripe.

The field was rebuilt to match:

- **Occupation is a coherent material, not a stripe.** `field = wave_x · wave_y · fold(x+y)`: a product
  of two travelling axis waves (periods 14 and 11 cells) modulated by a slower diagonal fold wave
  (period 16). Lit cells therefore bunch into **rectangular patches** that migrate, breathe and fold
  together, with negative space between them; the old single-axis plane wave, the radial rings, and the
  spatial hue ramp are gone.
- **White-dominant with sparse colour.** The structure is the neutral `text_secondary` weave; only a
  minority of lit cells take a saturated `ui_theme_material_palette()` speck. Speck membership is gated
  by a slow moving chroma wave, so colour gathers into regions that flow through the fabric and disperse
  instead of sitting as a fixed scatter. Hue is a per-cell hash.
- **RGB chromatic.** The material set was made crisp additive primaries/secondaries (`#FF2B2B`,
  `#FFE02B`, `#2BFF4F`, `#2BE8FF`, `#2B55FF`, `#FF2BD6`) instead of the previous pastel values.
- Periods and thresholds live in named constants (`UI_LIVING_FIELD_PERIOD_X/Y/D/C`,
  `UI_LIVING_FIELD_LIT_LEVEL`, `UI_LIVING_FIELD_CHROMA_BASE/GAIN`); `orientation` selects the axis or
  centre the waves travel about. All invariants are unchanged: deterministic, explicit time, seamless
  over the AMBIENT loop, reduced-motion no-op, empty-cell-only.

`test-ui-animation` still asserts determinism, boundedness, seamless loop, reduced-motion no-op, and
that both neutral and chromatic lit cells are present. The five recorded fixtures were refreshed with
this change as their only cause (see Evidence).

## Limitations and next

- The material set is used only by the main and pause fields; it is not yet a general surface material.
- The field is bound to `main_menu` and `pause_menu`. `settings`, `confirm_quit`, the HUD, the world,
  and the visualizations are unchanged.