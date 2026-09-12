# R8 Increment I3 Implementation Record — Vertical Movement Core — 2026-08-19

## Status

**Complete and verified.** R8 is Active. I3 adds deterministic heightfield
gravity, fall/landing, step-up, head-clearance, and cell-edge traversal behind a
headless runtime boundary.

> Historical record from 2026-08-19; the current physics boundary additionally
> rejects cells missing a finite surface, and R8 is now **Verified (2026-08-21)**.

## Implemented scope

1. **Pure vertical-physics boundary**
   - Added `vertical_physics.{h,c}` with explicit state, inputs, result enum, and
     deterministic transitions.
   - Consumes borrowed `SceneHeightView`; mutates only runtime `Camera` and
     `VerticalPhysicsState`; owns no authored state, allocation, UI, renderer,
     storage, global time, or input polling.
   - Uses bounded 1/120-second internal substeps and constant-acceleration
     integration, so the same elapsed interval produces equivalent state.

2. **Grounding, step-up, drops, and clearance**
   - Runtime body base is `camera.z - eye_height`.
   - Grounded Z is `floor_h + eye_height`.
   - Floor deltas up to `step_height` snap deterministically; larger deltas roll
     X/Y back and return `VERTICAL_PHYSICS_BLOCKED_STEP`.
   - Lower target floors begin an airborne fall only when the body fits under the
     target ceiling at the shared edge; otherwise movement rolls back.
   - Intervals below runtime `head_clearance` reject entry without weakening the
     fixed schema clearance invariant.

3. **Authored gravity**
   - Effective gravity uses the map-wide magnitude/orientation plus per-cell
     fixed-point scale and orientation override.
   - Down gravity falls/lands on floor; up gravity accelerates toward ceiling;
     north/south/east/west gravity accelerates airborne X/Y.
   - Camera orientation is unchanged and no second grounded interval exists.

4. **Unified-editor integration**
   - `UnifiedEditorState` owns the explicit runtime physics state.
   - Successful native load/import/new/reload replacements reset state/velocity.
   - Physics initializes/snaps on any document update, including Edit mode, so a
     newly opened scene cannot render using stale Z.
   - Walk mode uses existing camera input as a movement proposal, then physics
     accepts, snaps, falls, or rolls it back. WASD is suppressed while airborne;
     air control remains I4.
   - Blocked step/clearance surfaces as existing `EDITOR_STATUS_PLAYER_BLOCKED`.

5. **Build/test integration**
   - Added `test-vertical-physics` to the Make aggregate test target.
   - Unified-editor test linkage includes the new narrow module.

## Tests added/updated

`test_vertical_physics` (9 deterministic tests):

- flat reset/grounding;
- accepted step-up and blocked large step with exact rollback;
- lower-floor fall and landing;
- first-frame movement validates from the previous cell;
- drop blocked when target ceiling cannot fit the current body;
- runtime head-clearance block;
- local gravity scale changes fall rate;
- up and lateral gravity affect airborne motion;
- large elapsed step equals equivalent explicit substeps; invalid input rejects.

`test_unified_editor` (74/74): adds a controller regression for accepted step,
blocked step/status, and physics reset on document replacement; every existing
editor workflow remains green.

## Verification evidence

- Strict focused runners (`-std=c11 -Wall -Wextra -Wpedantic -Werror`):
  - vertical physics: **9/9 passed**;
  - unified editor: **74/74 passed**.
- `make check`: passed (application + full aggregate suite, including the new
  physics runner).
- Focused AddressSanitizer + leak detection: physics **9/9**, editor **74/74**.
- Focused UndefinedBehaviorSanitizer: physics **9/9**, editor **74/74**.
- `git diff --check`: passed.

## Boundaries and limitations

- Jump impulse, user air control, and ladder climbing remain I4.
- Side/up gravity applies while airborne but does not redefine visual up or create
  wall/ceiling grounded states; the one-floor interval contract remains intact.
- Runtime integration is currently the unified editor's native-scene walk mode.
  The Playing state still consumes a legacy `Map` without a `SceneDocument` or
  `SceneHeightView`; I3 does not introduce a duplicate height owner to force that
  path. The pure module can be reused when Playing consumes native scenes.
- There is no per-frame allocation, random input, implicit time source, or
  renderer dependency in the physics module.

## Exit assessment

I3's deterministic gravity, fall/landing, step-up, clearance, rollback, local
override, integration, strict full suite, and sanitizer gates pass. I4 (jump,
full X/Y air control, and ladders) is next.