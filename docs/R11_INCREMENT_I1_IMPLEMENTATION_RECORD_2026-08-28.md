# R11 Increment I1 Implementation Record — Sprite Runtime — 2026-08-28

## Status

**Implemented; automated verification passed on 2026-08-28.** Manual visual
acceptance is intentionally paired with I2 because I1 provides no ordinary
scene placement/persistence path.

> **Amendment (2026-09-02):** I2 scene v8 authoring code is now present in the
> working tree (commit `277fb3c`); the statement below that "I2 and I3 remain
> unimplemented" is superseded for I2. I2's exit-gate verification, its
> dedicated implementation record, and the bundled I1+I2 manual acceptance are
> still pending. I3 remains unimplemented.

## Delivered behavior

- Added the narrow, headless `sprite_render` module. It consumes explicit Grid,
  Map, Camera, AssetRegistry, WorldState, optional height view, world depth, and
  overlay-depth inputs; it owns no authored state, input, time, or allocation.
- Existing `WorldState.sprites` instances now render as decorative
  camera-facing billboards in every current renderer path through
  `raycast_render_world_overlays()`.
- Each `SpriteAsset` glyph/material grid is normalized to one world unit tall;
  projected width preserves `cols/rows`; the billboard is centered on its XY
  anchor and rests on the local authored floor (or Z=0 compatibility floor).
- Pattern glyphs and materials remain exact authored cells; no distance glyph
  substitution occurs. Whitespace/NUL cells are transparent.
- Sprite color samples the anchor tile's RGB `LightLevel` and applies it through
  `palette_sample()`, including colored and anti-light channel clamping.
- Missing assets, malformed dimensions, unloaded/zero material references,
  invalid IDs, out-of-map anchors, and missing authored floors are safe no-ops.

## Occlusion and ordering boundary

- `Grid` now owns an allocation-once `overlay_depths` workspace. The renderer
  initializes it every overlay pass; decals and sprites share it, preventing a
  farther sprite from overwriting a nearer decal.
- World occlusion remains read-only: bounded height/optical rendering compares
  each sprite cell against `world_depths`; the compatibility path compares
  against `column_depths`. Sprites never write either frontier.
- Multiple sprites resolve by nearest perpendicular depth independent of runtime
  array order. Equal-depth samples retain the earlier existing overlay, giving
  deterministic decal-before-sprite and stable sprite source ordering.
- Sprites remain decorative: no collision, sight-ray blocking, light shadowing,
  mirror reflection, trigger identity, or attachment/parent semantics.

## Files

- Added: `src/sprite_render.h`, `src/sprite_render.c`.
- Updated: `src/raycast.c` (overlay integration), `src/grid.h/c` (reusable depth
  workspace), `src/world.h`, `src/assets.h`, `src/asset_loader.c` comments.
- Added tests: `tests/test_sprite_render.c` (5 deterministic cases).
- Added benchmark: `tests/benchmark_sprite_render.c` and
  `make benchmark-sprite-render`.
- Updated Make source/test groups, `README.md`, `assets/README.md`, roadmap,
  R11 plan, and active handoff.

## Tests and automated evidence

- `test-sprite-render`: 5/5 — exact per-channel lighting, visible projection,
  bounded and column world occlusion, nearest-sprite ordering in both array
  orders, transparent/missing/invalid no-ops, read-only world depth, and shared
  decal/sprite frontier.
- Existing optical-render suite: 17/17; core: 61/61.
- Complete strict `make -j2 check`: passed under
  `-Wall -Wextra -Wpedantic -Werror`, including the new focused runner.
- Full ASan/LeakSanitizer suite: passed.
- Full UBSan suite: passed.
- Clean optimized build, smoke, and current-renderer caller guard: passed.
  Smoke result: `{"smoke":"ok","map_width":10,"map_height":6}`.

## Paired benchmark evidence

`make benchmark-sprite-render` uses a 160x90 current heightfield renderer,
20x12 map, 200 measured frames per path, and 128 active 3x5 sprites. Both paths
must remain at or below 6 ms and reproduce their initial checksum each frame.

- No-sprite baseline: **2.315268 ms**.
- 128 sprites: **2.676926 ms**.
- Measured sprite overhead: **0.361658 ms**.
- Baseline checksum: `1846712605617511097`.
- Sprite checksum: `5403392855886966484`.
- Deterministic: true; result: **PASS**.

Earlier same-session measurement before sanitizer clean/rebuild also passed
(5.014952 ms baseline / 5.023430 ms sprites); the optimized clean rerun above is
the binding recorded result. A final post-documentation rerun also passed
(5.461039 ms baseline / 4.211868 ms sprites) with the same checksums; scheduler
noise can invert a single baseline/sprite subtraction, so the binding acceptance
claim is that both paired paths remain below 6 ms, not that one subtraction is a
stable standalone cost estimate.

## Recorded failures and corrections

1. Focused test compilation first failed because `<stdio.h>` was missing for
   `snprintf`; include added.
2. Strict definite-initialization analysis rejected cmocka output variables;
   test outputs are now explicitly initialized without weakening assertions.
3. Focused target initially omitted `optical_runtime_view.c`, required by
   `camera.c`; target linkage corrected.
4. `benchmark-surface-render` reproduced the existing tracked flat inherited
   height timing issue: flat **8.331661 ms** exceeded 6 ms while raised
   **5.716133 ms** passed; deterministic checksums remained stable. This is not
   hidden or treated as I1 evidence. The paired sprite benchmark isolates and
   passes I1's measured overhead.

## Plan refinements

- Existing runtime sprite asset loading in `asset_loader_load_registry()` was
  confirmed and required no code change. The earlier discussion caveat that I1
  might lack loaded assets was incorrect; only scene-authored placement is absent.
- I1 intentionally does not introduce the planned entity/trigger session module:
  static decorative rendering has no tick/state transition. The pure explicit
  `sprite_render` boundary is the smallest correct I1 seam; the session begins
  when I3 introduces trigger state.

## Remaining acceptance and stop boundary

Manual visual review is bundled with I2: place a multi-cell sprite, inspect
billboarding while strafing, colored/anti-light shading, wall/decal occlusion,
missing-asset safety, undo/redo, save, and reopen. Do not add a debug-only I1
placement shortcut solely to manufacture manual acceptance.

> **Amendment (2026-09-02):** I2 scene v8/placement/selection is now present in
> the working tree (commit `277fb3c`) and requires the bundled manual acceptance
> above plus the I2 exit-gate verification and a dedicated implementation
> record. I3 triggers still require explicit authorization. Animation,
> objects/components, game-mode spawn expansion, solid/occluding sprites,
> mirrors, and sprite-to-object attachment remain deferred.