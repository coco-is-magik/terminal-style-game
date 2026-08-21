# R8 Increment I5 Implementation Record — 2026-08-20

## Status

**Complete and verified.** I5 adds undoable vertical-world authoring and live
per-map movement tuning to the unified editor. R8 was fully accepted in the
real-video manual inspection on 2026-08-21 and is now **Verified**.

## Implemented

1. **Typed command boundary**
   - `EDITOR_MUTATION_SET_CELL_VERTICAL` snapshots the complete coupled vertical
     value for one cell: floor/ceiling steps, ladder flag, gravity scale, and
     gravity orientation.
   - `EDITOR_MUTATION_SET_MOVEMENT_PARAMETERS` snapshots the complete per-map
     movement block.
   - Invalid values do not mutate the document or history. Undo/redo restores
     exact before/after values and resets runtime vertical physics.
   - Height painting submits up to eight cell requests as one atomic undo step.

2. **Validation and resize safety**
   - Command validation matches canonical v5 ranges, minimum clearance,
     effective-gravity cap, and ladder-cell constraints.
   - Editing the current player cell cannot reduce its interval below configured
     head clearance.
   - Shrink-ring equality now compares the complete fixed 16-byte
     `SceneAuthoredCell`, preventing silent loss of heights, ladders, or gravity
     overrides. Existing growth copies complete cells and new growth defaults
     remain covered by scene-document tests.

3. **Unified-editor workflow**
   - Floor/ceiling inspectors add Height, Ladder, Gravity direction, Gravity
     scale, and Movement rows without creating a parallel modal system.
   - Left/Right changes selected height by 0.25 units and atomically paints an
     existing surface multiselect. Enter toggles ladders.
   - The nested Movement menu tunes gravity magnitude/orientation, step height,
     jump impulse, air control, eye height, head clearance, and ladder speed.
   - Changes affect the running native scene immediately and persist in v5.
   - HUD feedback shows authored values and classifies selected-cell traversal
     as flat/down, ramp/step, jump, or blocked by height/clearance.
   - Existing I2 height-aware hit projection and selection/highlight handles are
     reused, so handles track live geometry without a second coordinate path.

4. **Scope preserved**
   - No angular pitch, arbitrary planes, crouch, slide, stacked traversable
     spaces, renderer rewrite, or second authored-data owner was introduced.

## Focused verification

- `test-command-system`: **38/38**
- `test-editor-domain`: **10/10**
- `test-unified-editor`: **76/76**
- Focused ASan with leak detection: all three runners passed.
- Focused UBSan: all three runners passed.
- Strict C11 compilation used `-Wall -Wextra -Wpedantic -Werror`.

The added tests cover exact command snapshots, invalid coupled values, player
clearance safety, complete-cell shrink blocking, pure request metadata, live
input dispatch, atomic height painting, ladder/gravity edits, movement tuning,
HUD feedback, undo/redo, and native save/reopen.

## Broad and performance verification

- Full `make check` log reached completion with no failure markers.
- Deprecated legacy-call guard passed.
- `git diff --check` passed.
- Surface benchmark: flat **1.808047 ms**, raised **2.292283 ms**.
- Surface stability: flat **1.795895 ms**, raised **2.302642 ms**.
- Flat authored and flat-height checksums remain exactly
  `5602340901454607159`; all scenarios were deterministic and below 6 ms.
- Editor-highlight benchmark/stability: **0.329375 / 0.329981 ms**, deterministic
  and below 1 ms.

Aggregate sanitizer targets retain the pre-existing ENet dependency limitation;
all changed runners passed direct ASan/UBSan execution.

## Acceptance mapping

The deterministic suites cover live height/highlight projection, step vs.
blocked/jump traversal, low-gravity jump mechanics, clearance blocking, ladder
climbing, save/reopen, undo/redo, growth defaults, and migration/flat parity.
The only unperformed evidence is a human-operated real-video ergonomics pass.