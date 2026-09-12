# R8 Increment I4 Implementation Record — Jump, Air Control, and Ladders — 2026-08-19

> **Historical/superseded:** jump and air control remain active, but ladder
> schema, editor, and physics behavior described here were removed by the
> 2026-08-20 heightfield remediation and are deferred from R8.

## Status

**Complete and verified.** R8 is Active. I4 completes the movement behavior
selected in the R8 decision record without changing authored ownership,
projection, or the one-interval topology.

> Historical record from 2026-08-19; the current jump default and physics are
> those described in the review follow-up, ladder behavior is deferred, and R8
> is now **Verified (2026-08-21)**.

## Implemented scope

1. **Jump impulse**
   - Added explicit `vertical_physics_jump()` to the headless physics boundary.
   - Jump applies `jump_impulse` opposite the effective local gravity vector.
   - Supports all six gravity orientations (down/up/north/south/east/west).
   - Jump is accepted only while initialized and grounded; repeated airborne
     attempts return `VERTICAL_PHYSICS_NOT_GROUNDED` without resetting velocity.
   - Low-gravity cells produce higher trajectories from the same authored
     impulse; floor/ceiling bounds clamp the body within the current interval.

2. **Full X/Y air control**
   - Unified-editor walk mode reuses the existing camera WASD/strafe normalization
     and axis-sliding collision proposal while airborne.
   - The accepted displacement is multiplied by authored `air_control_scale`
     (`0..1`) before deterministic gravity integration.
   - Airborne entry into a target cell requires the body's current vertical
     extent to fit inside that cell's single interval; low ceilings/floors block
     the proposal rather than allowing heightfield tunneling.

3. **Ladder traversal**
   - Added explicit `vertical_physics_climb()`.
   - In an authored ladder cell, Forward climbs and Backward descends at
     `ladder_speed`; ladder input consumes X/Y translation for that frame.
   - Movement clamps to `floor + eye_height` and
     `ceiling - head_clearance + eye_height`, zeros velocity, and never exits to
     a second interval (G1).
   - Non-ladder and invalid direction requests return typed results.

4. **Input and adapter wiring**
   - Added edge-triggered `editor_jump_pressed` from Space while preserving the
     asset designer's existing `place` signal.
   - `input_begin_frame` clears the jump edge.
   - Unified editor interprets input and calls pure physics operations; the
     physics module remains independent of input/UI.

## Tests added/updated

- `test_input` (12/12): Space emits both existing designer `place` and the new
  editor jump edge; frame reset clears it.
- `test_vertical_physics` (13/13):
  - jump impulse direction for all six gravity orientations;
  - repeated airborne jump rejection;
  - low-gravity trajectory difference;
  - ceiling cap and velocity response;
  - ladder climb/descent clamped inside one interval;
  - non-ladder/invalid-direction rejection;
  - airborne lateral entry requires full body fit.
- `test_unified_editor` (75/75): Space jump, 0.5 air-control displacement,
  repeated airborne jump behavior, ladder Forward/Backward adapter, no X/Y ladder
  drift, interval clamping, and all prior editor workflows.

## Verification evidence

- Strict C11 focused runners (`-Wall -Wextra -Wpedantic -Werror`):
  - input: **12/12 passed**;
  - vertical physics: **13/13 passed**;
  - unified editor: **75/75 passed**.
- `make check`: passed (application + full aggregate suite).
- Focused AddressSanitizer + leak detection: all three runners passed.
- Focused UndefinedBehaviorSanitizer: all three runners passed.
- Render stability after I4 (1,000 iterations): flat-height **1.809975 ms**,
  raised-height **2.309650 ms**, deterministic, 6 ms gate passed.
- Flat checksum remains `5602340901454607159`; raised checksum remains
  `18200245348862420337`.
- `git diff --check`: passed.

## Boundaries and limitations

- Ladder authoring, height editing, and runtime movement-parameter UI remain I5.
- Ladder traversal has no direction field; it remains a single-cell vertical
  affordance bounded by that cell's one interval.
- Side/up gravity still does not rotate visual up or establish wall/ceiling
  grounded states.
- Runtime adapter integration remains unified-editor native-scene walk mode;
  Playing still lacks a native `SceneDocument`/`SceneHeightView`, and I4 does not
  create a duplicate owner.

## Exit assessment

I4's jump, local-gravity response, full scaled air control, height-aware airborne
collision, ladder bounds, controller/input integration, strict suite, sanitizer,
and render-regression gates pass. I5 (editor authoring workflows and per-map
parameter tuning) is next.