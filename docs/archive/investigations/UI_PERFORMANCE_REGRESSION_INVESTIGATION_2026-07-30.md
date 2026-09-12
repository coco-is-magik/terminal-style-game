# UI Performance Regression Investigation — 2026-07-30

## Status and boundary

Diagnosis completed and remediated on 2026-07-30. The reported symptom was an
interactive HUD average frame time increase from roughly 7.2 ms to 10+ ms after
the R0 UI zoom work. The original evidence below is retained as the root-cause
record; implementation and verification are recorded in
`R0_UI_ZOOM_PERFORMANCE_REMEDIATION_IMPLEMENTATION_RECORD_2026-07-30.md`.

## Metric distinction

The HUD `Avg Frame Time` is not the same measurement as benchmark
`avg_render_ms`.

- The HUD measures almost the complete active loop from frame start through
  presentation. It includes input, simulation, raycast/grid generation, UI text
  updates, compatibility staging, compact-canvas copying, layer construction,
  renderer work, upload, and presentation.
- Benchmark `avg_render_ms` measures only `renderer_draw()` and benchmark mode
  bypasses the normal layered UI path entirely.
- Published HUD values are one-second rolling windows.

Consequently, a healthy benchmark does not rule out an interactive UI-path
regression.

## Confirmed current renderer evidence

At 260x160 cells (2,080x1,280 pixels, approximately 10.16 MiB uploaded per
frame), strict unoptimized `PROFILE_FRAME=1` runs produced:

| Workload | Renderer average | Dirty/raster phase | SDL upload/present |
|---|---:|---:|---:|
| Default stream, raycast | 5.20 ms | 0.135 ms | 4.502 ms |
| Stream, 600-frame idle | 5.42 ms | 0.134 ms | 4.723 ms |
| Stream, camera mutation | 5.60 ms | 0.305 ms | 4.700 ms |
| Stream, faster rotation | 6.18 ms | 0.628 ms | 4.933 ms |
| No tracker/full raster | 8.17 ms | 4.176 ms | 3.997 ms |

The renderer-only stream result remains close to the previously documented
5.00–5.52 ms representative raycast baseline. This rejects a broad raycaster,
horizon-offset, or renderer-upload regression as the explanation for the full
interactive increase.

An `-O2` diagnostic build reduced the same stream benchmark from 5.20 to 4.97 ms,
mostly reducing CPU-side raycast/tracker work while upload/present remained 4.696
ms. The normal Makefile deliberately has no optimization flag, but that is an
existing project condition rather than a newly introduced cause. That diagnostic
also exposed existing optimization-only `stringop-truncation` warnings; they were
demoted only for the temporary measurement and no project build policy changed.

## Confirmed newly introduced hot-path work

### 1. Restore-list merge has quadratic behavior

In default stream mode, `renderer_draw_layers()` scans every logical cell. For
each previously UI-touched cell, it linearly scans the current dirty-index list to
avoid duplicates, then appends missing cells. Its cost is approximately:

```text
O(total_grid_cells + restored_ui_cells * evolving_dirty_count)
```

This is performed before rasterization every interactive frame that follows a UI
frame. At the current 260x160 grid there are 41,600 cells. A standalone exact-loop
microbenchmark compiled at the project's effective `-O0` measured:

| Initial world-dirty cells | Prior UI restore cells | Merge time |
|---:|---:|---:|
| 0 | 800 | 1.318 ms |
| 2,700 | 800 | 5.436 ms |
| 5,000 | 800 | 9.702 ms |
| 2,700 | 1,800 | 13.872 ms |

These are algorithm-isolation measurements, not complete-frame predictions, but
they prove that the merge alone is large enough to explain a several-millisecond
interactive regression under realistic dirty/UI cardinalities.

The current profiler misleadingly labels this merge plus rasterization together
as `dirty iteration`/`rasterization`; it does not time stream diff separately due
to a phase-boundary bug and it does not separate restore merging from raster work.

### 2. The HUD is now independently scaled and recomposed every frame

Normal play keeps the 64x20 HUD canvas active. At the shipped 150% scale, touched
HUD source cells are expanded pixel by pixel. Each destination pixel performs
clipping checks, framebuffer addressing, division by eight for restore-cell
mapping, and a restore-mask write. The compositor also scans all 1,280 HUD canvas
slots for occupancy each frame.

The exact cost depends on current touched text length and presentation backend, so
it is a confirmed contributor but was not isolated as a separate timing in the
existing instrumentation.

### 3. The compatibility staging seam adds unconditional memory traffic

Normal mode clears the full 41,600-cell staging grid every frame: 41,600 cells ×
12 bytes = approximately 499 KiB. It then scans/copies fixed compact regions:

- HUD: 1,280 cells during play;
- menu: 3,200 cells while a menu is open;
- editor: 4,000 cells plus a 320-cell footer in editor modes.

This did not exist before layered UI integration. It is linear and likely smaller
than the quadratic restore merge, but it contributes to the HUD's whole-frame
metric.

### 4. HUD content performs repeated heap churn every frame

`draw_data_ui_overlay()` updates nine dynamic strings each frame. Each call reaches
`ui_ele_set_content()`, which duplicates the new string and frees the previous
one. Thus normal gameplay performs at least nine allocation/free pairs per frame
before rendering the HUD. Renderer allocation counters do not observe these
general allocations. This violates the intended no-per-frame-allocation property
outside the narrowly instrumented renderer boundary and can add allocator cost and
jitter.

## Historical evidence and comparability

- The exact remembered 7.2 ms interactive average is not present in the maintained
  local benchmark records inspected.
- Historical representative stream-raycast results are 5.00–5.52 ms renderer-only.
- Historical no-tracker full-raster results are generally around 8.3–9.3 ms.
- Existing performance documents contain acknowledged contradictory datasets, and
  benchmark methodology still lacks a standardized warm-up/context record.

The user's direct before/after observation remains valid symptom evidence, but it
cannot be equated numerically with benchmark `avg_render_ms` without matching the
metric and workload.

## Rejected explanations

- **Grid-relative horizon clamp:** it changes a scalar clamp/update policy, not the
  normal per-column/per-pixel rendering workload; current representative raycast
  timing remains at its established stream baseline.
- **Loss of the default stream tracker:** build output and runtime statistics
  confirm stream mode, zero fallback, and high unchanged-cell counts.
- **Per-frame texture creation or renderer allocation:** runtime guards and prior
  verification report none. One texture and one framebuffer remain.
- **UI preferences file I/O:** preference loading occurs at startup and saving only
  on a scale action, not during ordinary frames.
- **General SDL upload regression:** upload/present remains the dominant fixed
  4.5–4.9 ms renderer cost but is consistent across current controlled runs and
  does not by itself account for the newly reported increase.

## Conclusion

The strongest supported explanation is the R0 UI integration, specifically the
quadratic stream dirty-list/UI-restore merge. Its isolated measured cost can exceed
the entire observed 2.8+ ms regression. Full staging-grid clearing/copying,
pixel-by-pixel 150% HUD composition, and nine or more per-frame UI string
allocations are additional confirmed contributors.

The renderer-only benchmark remained healthy because benchmark mode calls
`renderer_draw()` with no normal UI layers, while the reported HUD metric includes
all app-side UI preparation and layered composition. The prior verification used
dummy-video safety checks rather than representative native interactive
performance, so it did not expose this regression.

## Measurement gap at diagnosis time

Existing instrumentation cannot separately print normal-mode app staging,
compositor, restore merge, and raster timings, and normal-mode shutdown does not
print `FrameProfileStats`. A production fix should first add or use narrowly scoped
diagnostic timing, then compare the same interactive workload before and after.
The remediation added a deterministic `ui-layered` scenario, excludes 64 warm-up
frames from benchmark statistics, and reports UI scale plus restore-merge,
compositor, tracker-diff, raster, and presentation phases in profiled builds.