# R0 UI Zoom Performance Remediation — Implementation Record (2026-07-30)

## Outcome

Option B was implemented incrementally without changing UI scale behavior,
layer order, clipping, anchors, framebuffer format, tracker selection, or the
single-framebuffer/single-upload architecture. The critical restore operation is
now linear, steady-state HUD text updates do not allocate, compatibility staging
clears only regions about to be rendered, and compositor restore bookkeeping is
performed per clipped source-cell rectangle rather than per destination pixel.

Compiler optimization remains deliberately deferred. No `-O2` or `-O3` flag was
added.

## Root cause and fixes

1. **Restore merge:** the stream/batch path previously searched the entire dirty
   list for every prior UI cell. `renderer_merge_restore_indices()` now consumes
   overlaps in one dirty-list walk and appends remaining restore bits in one mask
   walk: `O(dirty + grid)` with no allocation.
2. **HUD heap churn:** `UiElement` now tracks content capacity. Nine dynamic HUD
   fields and the settings scale field reserve 128 bytes at startup; bounded HUD
   updates copy in place and reject overflow without changing prior text.
3. **Staging traffic:** the unconditional 41,600-cell staging clear was removed.
   Menu, HUD, editor-top, and editor-footer regions are zeroed immediately before
   their known render/copy operations. Startup validates the minimum 160x42 UI
   staging extent so region clears cannot silently fail.
4. **Compositor bookkeeping:** each touched source cell derives and clips its
   scaled destination rectangle once for restore coverage. Contiguous logical
   restore spans use row `memset`; raster loops retain the original deterministic
   integer edge mapping. A retained test reference compositor verifies byte-for-
   byte framebuffer and restore-mask equivalence at 100/125/150/200%, clipping,
   centered/bottom anchors, equal-z ordering, and fixed/global policies.
5. **Representative measurement:** `--benchmark-scenario ui-layered` now executes
   the real staging-to-canvas-to-layer-to-`renderer_draw_layers()` path at 150%.
   It warms up for 64 frames outside statistics, mutates world cells, and hides
   the UI every sixteenth frame to exercise appear/overlap/disappear restoration.
   It reports the scale/warmup context and produces a deterministic
   framebuffer checksum. Profile output separates tracker diff, restore merge,
   rasterization, compositor, and SDL presentation.

## Regression protection

- Restore helper tests cover overlap, no overlap, bounded capacity, and invalid
  tracker indices.
- Staging/canvas regression proves a removed glyph cannot reappear after a
  region-only clear and copy.
- Reserved-content tests prove pointer stability and overflow preservation.
- Compositor tests compare complete framebuffer bytes and restore masks against
  the previous straightforward algorithm.
- The tracker matrix passes for no tracker, custom dirty cells, generic SMC,
  indexed SMC, batch SMC, stream SMC, lighting cache, and glyph cache modes.
- A final 128-frame disappear/restore run produced the identical checksum
  `243293914` in all six renderer modes: no tracker, custom dirty cells, generic
  SMC, indexed SMC, batch SMC, and stream SMC.

## Verification evidence

All builds used strict C11 with `-Wall -Wextra -Wpedantic -Werror`.

| Check | Result |
|---|---|
| Focused core | 48/48 pass |
| Focused UI element | 15/15 pass |
| Focused compositor | 7/7 pass |
| App-option/session tests | 5/5 and 1/1 pass |
| Aggregate `make test` | pass |
| Eight-mode `make matrix` | pass |
| `make sanitize` | pass |
| Profiled application build | pass |

Native unoptimized measurements on this machine:

- representative renderer-only `camera`, 120 measured frames: 5.67, 5.44,
  5.43 ms; checksum `569796141`; all passed the minimum policy;
- dense `ui-layered` before the final periodic-disappear addition, 120 measured
  frames: 7.57, 7.46, 7.45 ms; checksum `615538178` each time;
- final stream `ui-layered` with periodic disappearance, 128 measured frames:
  7.21 ms unprofiled and 7.46 ms profiled; checksum `243293914`;
- that final profiled run measured tracker diff 0.544 ms, restore merge 0.103 ms,
  raster 0.345 ms, compositor 2.542 ms, and SDL/update/present 3.923 ms.

The dense layered scenario intentionally paints substantially more UI than the
normal HUD. Its existing generic benchmark threshold therefore reports
`fail_performance`; that result is a workload/threshold mismatch, not a
correctness failure. The scenario is retained as a regression measurement path,
not silently weakened to pass an unrelated renderer-only budget.

## Deferred work

Evaluate explicit optimized build profiles (`-O2` and `-O3`) as a separate task:
first resolve optimization-only diagnostics under `-Werror`, then compare
correctness checksums and native performance against the strict unoptimized
baseline. Do not make optimization the explanation for, or substitute for, the
algorithmic fixes above.

## Manual visual acceptance — 2026-07-31

The user confirmed the native visual pass at 100/125/150/200% for the HUD,
Main/Pause/Settings menus, moving and disappearing overlays, editor top/footer UI,
and crosshair order. Automated framebuffer equivalence, deterministic checksums,
and all tracker modes had passed before this acceptance.

Review B then found that keyboard focus was distinguished only by foreground and
background colors, contrary to the approved non-color-only accessibility contract.
Focused buttons now render stable `>` and `<` edge markers in addition to the
existing color treatment. The focused UI-element runner (15/15), strict application
build, aggregate suite (26 passing groups), ASan/UBSan target, and all eight matrix
modes passed after the correction.