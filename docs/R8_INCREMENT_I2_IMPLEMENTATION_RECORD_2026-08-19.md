# R8 Increment I2 Implementation Record — Height-Aware View and Rendering — 2026-08-19

## Status

**Complete and verified.** R8 remains Active. I1's canonical v5 authored data is
now consumed by the renderer, editor selection, and editor highlights through a
borrowed, read-only `SceneHeightView`.

## Implemented scope

1. **Shared projection seam**
   - Added `height_projection.h`: allocation-free fixed-step conversion, borrowed
     view validation, horizon-row composition, and vertical world-Z projection.
   - Renderer, selection, and highlight code use the same arithmetic; no second
     coordinate model or authored copy exists.

2. **Runtime camera Z**
   - `Camera.z` is runtime-only and initializes to the flat-world eye height 0.5.
   - Existing `Camera.pitch` remains the bounded screen-row horizon offset.
   - I3 owns dynamic Z/grounding; I2 only projects the current runtime value.

3. **Height-aware renderer**
   - Added opt-in `raycast_render_height`; existing `raycast_render` remains a
     legacy-compatible wrapper with a NULL height view.
   - Wall top/bottom rows derive from the hit cell's authored floor/ceiling.
   - Floor/ceiling casting uses one bounded correction from the initially sampled
     cell's authored plane; no recursion, per-frame allocation, or renderer rewrite.
   - Default floor/ceiling + `camera.z == 0.5` take the exact legacy expressions,
     preserving byte-identical output/checksums.
   - Decal world-point projection now uses runtime camera Z instead of a local
     hard-coded 0.5 constant.

4. **Editor agreement**
   - Height-aware horizontal picking uses the same sampled-plane correction.
   - Wall highlights use the renderer's authored wall-span bounds.
   - Horizontal highlights use height-aware selection projection.
   - Unified-editor hover and application render paths borrow one height view
     from `SceneDocument`; existing APIs remain wrappers for non-height callers.

5. **Benchmark expansion**
   - Surface benchmark now measures legacy, authored-material, flat-height,
     raised-height, and occluded-decal-on-raised-height paths.
   - Pass threshold aligned to the repository's 6 ms minimum-acceptable target.

## Tests added/updated

- `test_camera`: runtime eye Z initializes to 0.5.
- `test_core`: default height view is byte-identical to legacy output; authored
  raised wall/floor changes output deterministically; camera Z changes projection;
  invalid height view falls back exactly.
- `test_editor_selection`: authored floor height changes the projected selection
  distance and target cell.
- `test_editor_highlight`: raised wall outline moves with the rendered span.
- `test_unified_editor`: all 73 existing integration tests pass with the new
  borrowed-view hover path.

## Verification evidence

- Strict C11 focused runners (`-Wall -Wextra -Wpedantic -Werror`):
  - camera: **5/5 passed**;
  - core/render: **60/60 passed**;
  - editor selection: **23/23 passed**;
  - editor highlight: **19/19 passed**;
  - unified editor: **73/73 passed**.
- `make check`: passed (application + full aggregate suite).
- Focused AddressSanitizer + leak detection: all five runners above passed.
- Focused UndefinedBehaviorSanitizer: all five runners above passed.
- Benchmark (200 iterations): flat-height **1.801961 ms**, raised-height
  **2.326374 ms**, deterministic, 6 ms gate passed.
- Stability (1,000 iterations): flat-height **1.797452 ms**, raised-height
  **2.316092 ms**, deterministic, 6 ms gate passed.
- Flat parity: authored checksum and flat-height checksum both
  `5602340901454607159`.

## Boundaries and limitations

- I2 does not implement gravity, grounding, step-up, ramps, jump, or ladders;
  those remain I3/I4.
- True angular pitch remains out of scope; camera Z composes with the existing
  horizon-offset projection.
- Horizontal height casting intentionally uses one bounded sampled-cell
  correction, the cheapest deterministic heightfield path. Arbitrary planes,
  recursive sector traversal, and stacked spaces remain forbidden.
- Point lights have no authored Z in the current scene model; their existing
  center-row billboard remains unchanged until a later height-aware light/entity
  schema requires otherwise.

## Exit assessment

I2's flat parity, authored-height rendering, projection agreement, strict full
suite, sanitizer evidence, and benchmark/stability gates pass. I3 (movement core:
gravity, fall, step-up, head-clearance, and cell-aligned ramp traversal) is next.