# R0 Grid-Relative Horizon Offset Implementation Record — 2026-07-30

## Status

**Implementation and automated verification complete. Manual acceptance remains.**

## Delivered behavior

- `camera_clamp_horizon_offset()` is the single validation/clamp policy.
- Positive viewport rows clamp to exactly `[-viewport_rows, +viewport_rows]`.
- Invalid viewport rows and non-finite values return deterministic level view
  (`0.0`).
- `camera_update()` receives the active logical grid height explicitly.
- Normal gameplay and editor walk mode pass the same active `Grid.height`.
- Mouse direction and sensitivity remain `pitch -= mouse_dy * 0.5` rows.
- The implementation remains a 2.5D horizon displacement; no angular pitch,
  camera Z, vertical collision, world-model, UI-scale, or crosshair behavior changed.

## Projection audit

The wall/floor/ceiling path in `raycast.c`, wall/floor/ceiling decal projection,
light/sprite screen-space projection, and editor wall-face highlighting were
reviewed at the expanded accepted range. Existing clipping and epsilon guards
remained finite and memory-safe in focused tests at exact `-viewport_rows`, `0`,
and `+viewport_rows`. No focused test demonstrated a projection-formula defect,
so no speculative formula refactor was made.

## Regression coverage

- `tests/test_camera.c`: multiple viewport heights; interior/no-change values;
  positive and negative clamps; repeated overshoot; invalid heights; NaN and
  infinities; unchanged sensitivity/direction; exact active-height updates.
- `tests/test_core.c`: world rendering at exact negative, zero, and positive
  viewport-row extremes.
- `tests/test_decals.c`: wall decal rendering at both exact extremes.
- `tests/test_editor_highlight.c`: selected/hovered wall-face projection at both
  exact extremes.

## Verification evidence

Completed successfully:

- clean strict `make test`: 26 passing test groups, no failure markers;
- `make ubsan`;
- `make asan`;
- `make matrix`: default plus indexed, batch, stream, lighting-cache, and
  glyph-cache configurations passed;
- strict application build;
- `--smoke-test`, reporting `{"smoke":"ok","map_width":10,"map_height":6}`;
- bounded benchmark and dummy-video stability paths completed without crash,
  sanitizer finding, allocation guard, texture guard, or tracker guard output.

The bounded performance paths returned their existing `fail_performance`
classification (`camera`, 120 frames; dummy-video stability, one second). These
runs are environment/performance gates rather than correctness gates; they
completed normally and did not report a horizon-offset safety failure.

During verification, the new decal test initially compared the tri-state
`WorldInsertResult` as a boolean. `WORLD_INSERT_OK` is zero, so the assertion was
corrected to compare the enum explicitly. The production implementation was not
changed by that test correction.

## Remaining acceptance

Manual interactive acceptance is still required in both normal gameplay and
editor walk mode:

1. Move smoothly to both vertical bounds.
2. Confirm stable walls, floor, ceiling, decals/lights, and editor highlights.
3. Return to level view and confirm unchanged yaw, movement, crosshair, menus,
   and UI zoom behavior.

True angular pitch remains deferred to R8.