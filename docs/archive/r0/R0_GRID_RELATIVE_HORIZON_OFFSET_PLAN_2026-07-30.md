# R0 Grid-Relative Horizon Offset Plan — 2026-07-30

## Status

**Verified 2026-07-31.** Implementation, automated verification, and user-confirmed
gameplay/editor acceptance are complete. This is the detailed plan for R0 outcome
5 in `FEATURE_ROADMAP.md`. Verification evidence is recorded in
`R0_GRID_RELATIVE_HORIZON_OFFSET_IMPLEMENTATION_RECORD_2026-07-30.md`.

## Goal

Replace the fixed `Camera.pitch` clamp of `[-100, 100]` with a bound derived from
the active logical viewport height. Preserve the existing 2.5D projection model:
the value is a horizon displacement measured in logical grid rows, not an angle.

The approved legal range is:

```text
[-viewport_rows, +viewport_rows]
```

Positive values shift the horizon downward; negative values shift it upward. A
full-range value may move the horizon just beyond the corresponding viewport edge.

## Required behavior

1. The application passes the active logical grid height explicitly to camera
   update logic. The camera module does not read global configuration or map size.
2. `viewport_rows > 0` clamps the horizon offset exactly to
   `±viewport_rows`.
3. A non-positive viewport height returns the safe deterministic level value
   `0.0`.
4. A non-finite incoming or mouse-derived offset returns `0.0`; NaN and infinity
   must not reach projection consumers.
5. Existing mouse direction and sensitivity remain unchanged: subtract
   `mouse_dy * 0.5` rows.
6. Normal gameplay and editor walk mode use the same policy.
7. Wall, floor, ceiling, decal/light, and editor-highlight rendering remain
   finite, clipped, and memory-safe at zero and both exact extremes.
8. Tests cover multiple viewport heights, no-change/interior values, both clamp
   directions, repeated overshoot, invalid heights, non-finite values, and
   representative extreme rendering.

## Forbidden behavior

- Do not describe this as true angular pitch or unrestricted vertical viewing.
- Do not add camera Z, vertical collision, slopes, stacked geometry, or vertical
  world authoring.
- Do not derive the clamp from window pixels, map dimensions, or startup config.
- Do not change yaw, movement, collision, FOV, UI scale, crosshair position, or
  editor mode transitions.
- Do not allocate per frame or redesign the renderer.
- Do not refactor projection formulas unless a focused test demonstrates a safety
  or correctness defect at the new accepted range.

## Ownership and API boundary

`camera.c` owns validation and clamping through one pure helper:

```c
double camera_clamp_horizon_offset(double offset, int viewport_rows);
```

`camera_update()` accepts `viewport_rows` explicitly. `app.c` and
`unified_editor.c` pass the active `Grid.height`; no compatibility wrapper hides a
fixed fallback.

## Projection audit

The implementation audit covers:

- wall slice bounds and floor/ceiling denominators in `raycast.c`;
- decal and light projection;
- selected/hovered wall-face projection in `editor_highlight.c`;
- all floating-to-integer conversions of the horizon offset.

Existing clipping/epsilon guards remain unless tests prove them insufficient.

## Verification and exit gate

1. Focused camera policy tests pass under strict C11 warnings.
2. Extreme world/decal/highlight projection tests pass at
   `-viewport_rows`, `0`, and `+viewport_rows`.
3. Strict application build and aggregate tests pass.
4. ASan, UBSan, and applicable build matrix pass.
5. A bounded runtime path reports no allocation, texture, or tracker guard
   failure.
6. Manual gameplay and editor acceptance confirms smooth travel to both bounds,
   stable rendering, and normal return to level view.
7. Implementation evidence and stable documentation use accurate 2.5D horizon-
   offset terminology.

True angular pitch remains deferred to R8 after the world-model decision.
