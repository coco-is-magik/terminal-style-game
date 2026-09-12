# R4 Increment E Implementation Record — 2026-08-11

## Start here

**Increment E is complete. R4 remains Active. Increment F is next.**

### Next action

Begin Increment F workflow integration and closeout. Add a representative checked-in
v2 scene, exercise New/Open/Import/Save/Save As/Reload and repair workflows, then run
the targeted architecture review and interactive acceptance before marking R4 Verified.

## Delivered

1. **Borrowed authored-surface render view**
   - Added `SceneSurfaceView`, a zero-copy borrowed pointer/count/dimension view over
     `SceneDocument.authored_cells`.
   - `scene_document_get_surface_view()` validates exact map/cardinality agreement and
     clears failed outputs.
   - `raycast_render()` accepts the optional view without depending on `SceneDocument`.
2. **Per-cell horizontal material rendering**
   - In-bounds floor and ceiling samples resolve independent authored material IDs.
   - Materials reuse the wall renderer's distance glyph bands and scalar
     `palette_sample()` lighting.
   - Non-contiguous loaded IDs are supported.
3. **Preserved compatibility path**
   - A NULL or malformed view retains the previous constant floor/ceiling rendering.
   - Valid views preserve hard-coded out-of-bounds ceiling `{50,50,50}` and floor
     `{30,30,30}` backgrounds.
   - Negative authored-view coordinates use `floor()` so they cannot alias cell zero.
4. **Unmistakable missing-material fallback**
   - Missing or unloaded floor/ceiling references render black `.` glyphs on full-bright
     purple `{128,0,255,255}` backgrounds.
   - The fallback deliberately ignores light level so darkness cannot hide repair work.
5. **Bounded hot path**
   - Surface access is direct row-major array indexing with separate per-column floor
     and ceiling cell caches.
   - No per-frame allocation, copied authored array, schema change, or speculative SMC
     expression was introduced.

## Tests added or strengthened

- Four exact NULL-view baseline locks: level, negative pitch, positive pitch, and
  out-of-bounds.
- Exact authored floor/ceiling material, distance glyph, scalar-lighting, and checksum
  assertions.
- Missing-material fallback equality under light levels `0.0` and `1.0`.
- Malformed-view fallback and valid-view out-of-bounds preservation.
- `SceneSurfaceView` null/failure/default cardinality and borrowing checks.
- Floor decal ordering over authored materials.
- Horizontal selection highlight ordering over authored materials plus borrowed-input
  preservation.
- Dedicated deterministic 260×160 surface-render benchmark/stability runner.

Focused final counts include:

| Runner | Result |
|---|---:|
| `test-core` | 57/57 passed |
| `test-decals` | 29/29 passed |
| `test-editor-highlight` | 17/17 passed |
| `test-scene-document` | 37/37 passed |
| `test-unified-editor` | 52/52 passed |

## Performance and stability

The dedicated runner measures `raycast_render()` directly; setup, checksum, and cleanup
are outside each timed call. Both paths use the same 260×160 Grid, 20×12 map, camera,
assets, and light map.

| Run | Iterations/path | NULL view | Authored view | Authored overhead | Budget | Result |
|---|---:|---:|---:|---:|---:|---:|
| Benchmark | 200 | 3.476741 ms | 2.797465 ms | -0.679276 ms | 8.000 ms | passed |
| Stability | 1,000 | 2.395380 ms | 2.819356 ms | 0.423976 ms | 8.000 ms | passed |

Both runs reported deterministic and distinct NULL/authored Grid checksums.

The existing SDL renderer-backend benchmark produced 6.45 ms on the real-video run and
reported `fail_performance` against its separate 6.0 ms backend gate. Its timer begins
after `raycast_render()` and measures glyph rasterization plus SDL upload/present; it does
not exercise the editor's authored `SceneSurfaceView` path. This result is retained as a
renderer-backend/environment follow-up and is not claimed as passing Increment E
surface-lookup evidence.

## Verification evidence

The strict flags remain:

```text
CFLAGS := -std=c11 -Wall -Wextra -Wpedantic -Werror
```

| Gate | Result |
|---|---:|
| Final normal `make check` | passed |
| Sequential `make asan` | passed; no sanitizer diagnostics |
| Sequential `make ubsan` | passed; no sanitizer diagnostics |
| Sequential `make matrix` | 8/8 modes passed |
| `make benchmark-editor-highlight` | 0.216115 ms/scenario; deterministic; passed |
| Editor-highlight stability | 100,000 scenarios; 0.285704 ms average; passed |
| Dummy-video smoke | passed |
| Dedicated surface benchmark/stability | passed; deterministic |
| `git diff --check` | passed before documentation closeout |

Sanitizer and matrix targets ran sequentially because they clean and reuse `build/`.
The final check restored the normal strict build state.

## Scope preserved

- Scene v2 remains unchanged; the deferred hexadecimal block schema remains an idea.
- `SceneDocument` remains the only authored owner; `SceneSurfaceView` borrows and owns
  nothing.
- The compatibility `Map` remains the wall/collision/light-map view.
- Existing projection, scalar lighting, decal/light/highlight ordering, fixed heights,
  and out-of-bounds backgrounds remain intact.
- No slopes, variable heights, Z movement, per-face wall material, or optical policy was
  introduced.
- R4 is not Verified until Increment F, interactive acceptance, and the targeted review
  are complete.
