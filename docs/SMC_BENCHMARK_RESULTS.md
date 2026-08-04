# SMC State Tracking Benchmark Results

> **New-device summary (authoritative for yotsubox2):**  
> [`docs/benchmarks/2026-07-26-yotsubox2-reverify.md`](benchmarks/2026-07-26-yotsubox2-reverify.md)  
> Handoff: [`docs/handoffs/2026-07-26-benchmark-reverify-yotsubox2.md`](handoffs/2026-07-26-benchmark-reverify-yotsubox2.md)  
> Main six modes below are complete (BUILD/PARSE/RUN pass, DECISION_PASS).  
> Dynamic scenario matrix section is **incomplete** (stopped after X11 failure).

**Date**: 2026-07-26T08:50:34-04:00  
**Host**: yotsubox2 (AMD Ryzen 9 7950X, Linux 6.15.4-gentoo)  
**Git commit**: a60c7e7  
**Git branch**: dev  
**SMC submodule commit**: 3783ae9  
**Grid**: 160x260 (41600 cells)

## Mode: baseline

### Build
```
mkdir -p build
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror     -DPROFILE_FRAME=1  -I"/nas/contents/Projects/Programming Projects/C/terminal-style-game/terminal-style-game/vendor/dist/include" -I"/nas/contents/Projects/Programming Projects/C/terminal-style-game/terminal-style-game/vendor/src/SDL/include"  src/app.c src/asset_designer.c src/asset_loader.c src/assets.c src/camera.c src/command_system.c src/config.c src/decal_io.c src/editor_selection.c src/glyph_atlas.c src/glyph_block_cache.c src/grid.c src/input.c src/lighting.c src/lighting_cache.c src/live_editor.c src/main.c src/map.c src/map_loader.c src/material_designer.c src/math.c src/menu_state.c src/raycast.c src/renderer.c src/scale.c src/scene_document.c src/smc_indexed_state_tracker.c src/smc_render_opt.c src/smc_state_tracker.c src/timing.c src/ui_ele.c src/unified_editor.c src/world.c  -o build/ascii-fps -L"/nas/contents/Projects/Programming Projects/C/terminal-style-game/terminal-style-game/vendor/dist/lib64" -lSDL3 -lSDL3_mixer -lenet -lm  -Wl,-rpath,'$ORIGIN/../vendor/dist/lib64'
```
### Warmup (discarded)
```
error: XDG_RUNTIME_DIR is invalid or not set in the environment.
Renderer stats: cells_total=41600 cells_rasterized=41600 cells_skipped=0 skip_rate=0.0% framebuffer_checksum=2989410282

Frame profile (baseline):
  frames:              249
  grid/raycast:        0.837 ms/frame
  state packing:       0.000 ms/frame
  smc batch diff:      0.000 ms/frame
  smc stream diff:     0.000 ms/frame
  dirty decision:      1.765 ms/frame
  dirty iteration:     1.765 ms/frame
  rasterization:       1.765 ms/frame
  SDL/update/present:  0.755 ms/frame
  other/unaccounted:   0.000 ms/frame
  total profiled:      2.520 ms/frame
{
  "grid_width": 260,
  "grid_height": 160,
  "target_fps": 120,
  "avg_render_ms": 2.52,
```

### Measured Runs
#### Run 1
```
error: XDG_RUNTIME_DIR is invalid or not set in the environment.
Renderer stats: cells_total=41600 cells_rasterized=41600 cells_skipped=0 skip_rate=0.0% framebuffer_checksum=320224107

Frame profile (baseline):
  frames:              627
  grid/raycast:        0.852 ms/frame
  state packing:       0.000 ms/frame
  smc batch diff:      0.000 ms/frame
  smc stream diff:     0.000 ms/frame
  dirty decision:      1.798 ms/frame
  dirty iteration:     1.798 ms/frame
  rasterization:       1.798 ms/frame
  SDL/update/present:  0.766 ms/frame
  other/unaccounted:   0.000 ms/frame
  total profiled:      2.564 ms/frame
{
  "grid_width": 260,
  "grid_height": 160,
  "target_fps": 120,
  "avg_render_ms": 2.56,
  "worst_render_ms": 4.09,
  "effective_worst_ms": 4.09,
  "outlier_trimmed": false,
  "min_spare_ms": 3.12,
  "frames": 627,
  "result": "ideal"
}
```

#### Run 2
```
error: XDG_RUNTIME_DIR is invalid or not set in the environment.
Renderer stats: cells_total=41600 cells_rasterized=41600 cells_skipped=0 skip_rate=0.0% framebuffer_checksum=3578627115

Frame profile (baseline):
  frames:              626
  grid/raycast:        0.848 ms/frame
  state packing:       0.000 ms/frame
  smc batch diff:      0.000 ms/frame
  smc stream diff:     0.000 ms/frame
  dirty decision:      1.837 ms/frame
  dirty iteration:     1.837 ms/frame
  rasterization:       1.837 ms/frame
  SDL/update/present:  0.767 ms/frame
  other/unaccounted:   0.000 ms/frame
  total profiled:      2.604 ms/frame
{
  "grid_width": 260,
  "grid_height": 160,
  "target_fps": 120,
  "avg_render_ms": 2.60,
  "worst_render_ms": 5.19,
  "effective_worst_ms": 5.19,
  "outlier_trimmed": false,
  "min_spare_ms": 2.04,
  "frames": 626,
  "result": "ideal"
}
```

#### Run 3
```
error: XDG_RUNTIME_DIR is invalid or not set in the environment.
Renderer stats: cells_total=41600 cells_rasterized=41600 cells_skipped=0 skip_rate=0.0% framebuffer_checksum=2060829812

Frame profile (baseline):
  frames:              626
  grid/raycast:        0.841 ms/frame
  state packing:       0.000 ms/frame
  smc batch diff:      0.000 ms/frame
  smc stream diff:     0.000 ms/frame
  dirty decision:      1.772 ms/frame
  dirty iteration:     1.772 ms/frame
  rasterization:       1.772 ms/frame
  SDL/update/present:  0.765 ms/frame
  other/unaccounted:   0.000 ms/frame
  total profiled:      2.537 ms/frame
{
  "grid_width": 260,
  "grid_height": 160,
  "target_fps": 120,
  "avg_render_ms": 2.54,
  "worst_render_ms": 4.92,
  "effective_worst_ms": 4.92,
  "outlier_trimmed": false,
  "min_spare_ms": 2.18,
  "frames": 626,
  "result": "ideal"
}
```

## Mode: dirty_cells

### Build
```
mkdir -p build
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror    -DUSE_DIRTY_CELLS=1 -DPROFILE_FRAME=1  -I"/nas/contents/Projects/Programming Projects/C/terminal-style-game/terminal-style-game/vendor/dist/include" -I"/nas/contents/Projects/Programming Projects/C/terminal-style-game/terminal-style-game/vendor/src/SDL/include"  src/app.c src/asset_designer.c src/asset_loader.c src/assets.c src/camera.c src/command_system.c src/config.c src/decal_io.c src/editor_selection.c src/glyph_atlas.c src/glyph_block_cache.c src/grid.c src/input.c src/lighting.c src/lighting_cache.c src/live_editor.c src/main.c src/map.c src/map_loader.c src/material_designer.c src/math.c src/menu_state.c src/raycast.c src/renderer.c src/scale.c src/scene_document.c src/smc_indexed_state_tracker.c src/smc_render_opt.c src/smc_state_tracker.c src/timing.c src/ui_ele.c src/unified_editor.c src/world.c  -o build/ascii-fps -L"/nas/contents/Projects/Programming Projects/C/terminal-style-game/terminal-style-game/vendor/dist/lib64" -lSDL3 -lSDL3_mixer -lenet -lm  -Wl,-rpath,'$ORIGIN/../vendor/dist/lib64'
```
### Warmup (discarded)
```
error: XDG_RUNTIME_DIR is invalid or not set in the environment.
Renderer stats: cells_total=41600 cells_rasterized=2 cells_skipped=41598 skip_rate=100.0% framebuffer_checksum=3564017551

Frame profile (custom dirty cells):
  frames:              251
  grid/raycast:        0.875 ms/frame
  state packing:       0.000 ms/frame
  smc batch diff:      0.000 ms/frame
  smc stream diff:     0.000 ms/frame
  dirty decision:      0.245 ms/frame
  dirty iteration:     0.245 ms/frame
  rasterization:       0.245 ms/frame
  SDL/update/present:  0.852 ms/frame
  other/unaccounted:   0.000 ms/frame
  total profiled:      1.097 ms/frame
{
  "grid_width": 260,
  "grid_height": 160,
  "target_fps": 120,
  "avg_render_ms": 1.10,
```

### Measured Runs
#### Run 1
```
error: XDG_RUNTIME_DIR is invalid or not set in the environment.
Renderer stats: cells_total=41600 cells_rasterized=1 cells_skipped=41599 skip_rate=100.0% framebuffer_checksum=2832938243

Frame profile (custom dirty cells):
  frames:              627
  grid/raycast:        0.884 ms/frame
  state packing:       0.000 ms/frame
  smc batch diff:      0.000 ms/frame
  smc stream diff:     0.000 ms/frame
  dirty decision:      0.238 ms/frame
  dirty iteration:     0.238 ms/frame
  rasterization:       0.238 ms/frame
  SDL/update/present:  0.819 ms/frame
  other/unaccounted:   0.000 ms/frame
  total profiled:      1.057 ms/frame
{
  "grid_width": 260,
  "grid_height": 160,
  "target_fps": 120,
  "avg_render_ms": 1.06,
  "worst_render_ms": 1.83,
  "effective_worst_ms": 1.83,
  "outlier_trimmed": false,
  "min_spare_ms": 5.48,
  "frames": 627,
  "result": "ideal"
}
```

#### Run 2
```
error: XDG_RUNTIME_DIR is invalid or not set in the environment.
Renderer stats: cells_total=41600 cells_rasterized=1 cells_skipped=41599 skip_rate=100.0% framebuffer_checksum=763336309

Frame profile (custom dirty cells):
  frames:              627
  grid/raycast:        0.859 ms/frame
  state packing:       0.000 ms/frame
  smc batch diff:      0.000 ms/frame
  smc stream diff:     0.000 ms/frame
  dirty decision:      0.234 ms/frame
  dirty iteration:     0.234 ms/frame
  rasterization:       0.234 ms/frame
  SDL/update/present:  0.838 ms/frame
  other/unaccounted:   0.000 ms/frame
  total profiled:      1.072 ms/frame
{
  "grid_width": 260,
  "grid_height": 160,
  "target_fps": 120,
  "avg_render_ms": 1.07,
  "worst_render_ms": 1.70,
  "effective_worst_ms": 1.70,
  "outlier_trimmed": false,
  "min_spare_ms": 5.60,
  "frames": 627,
  "result": "ideal"
}
```

#### Run 3
```
error: XDG_RUNTIME_DIR is invalid or not set in the environment.
Renderer stats: cells_total=41600 cells_rasterized=1 cells_skipped=41599 skip_rate=100.0% framebuffer_checksum=788438976

Frame profile (custom dirty cells):
  frames:              628
  grid/raycast:        0.859 ms/frame
  state packing:       0.000 ms/frame
  smc batch diff:      0.000 ms/frame
  smc stream diff:     0.000 ms/frame
  dirty decision:      0.235 ms/frame
  dirty iteration:     0.235 ms/frame
  rasterization:       0.235 ms/frame
  SDL/update/present:  0.824 ms/frame
  other/unaccounted:   0.000 ms/frame
  total profiled:      1.059 ms/frame
{
  "grid_width": 260,
  "grid_height": 160,
  "target_fps": 120,
  "avg_render_ms": 1.06,
  "worst_render_ms": 1.33,
  "effective_worst_ms": 1.33,
  "outlier_trimmed": false,
  "min_spare_ms": 5.88,
  "frames": 628,
  "result": "ideal"
}
```

## Mode: smc_indexed

### Build
```
mkdir -p build
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror -DUSE_SMC_INDEXED_STATE_TRACKER=1    -DPROFILE_FRAME=1  -I"/nas/contents/Projects/Programming Projects/C/terminal-style-game/terminal-style-game/vendor/dist/include" -I"/nas/contents/Projects/Programming Projects/C/terminal-style-game/terminal-style-game/vendor/src/SDL/include" -I"vendor/src/smc/include" -I"vendor/src/smc/src/c" src/app.c src/asset_designer.c src/asset_loader.c src/assets.c src/camera.c src/command_system.c src/config.c src/decal_io.c src/editor_selection.c src/glyph_atlas.c src/glyph_block_cache.c src/grid.c src/input.c src/lighting.c src/lighting_cache.c src/live_editor.c src/main.c src/map.c src/map_loader.c src/material_designer.c src/math.c src/menu_state.c src/raycast.c src/renderer.c src/scale.c src/scene_document.c src/smc_indexed_state_tracker.c src/smc_render_opt.c src/smc_state_tracker.c src/timing.c src/ui_ele.c src/unified_editor.c src/world.c vendor/src/smc/src/c/smc_runtime_stub.c vendor/src/smc/src/c/smc_artifact.c vendor/src/smc/src/c/smc_state.c -o build/ascii-fps -L"/nas/contents/Projects/Programming Projects/C/terminal-style-game/terminal-style-game/vendor/dist/lib64" -lSDL3 -lSDL3_mixer -lenet -lm -lm -Wl,-rpath,'$ORIGIN/../vendor/dist/lib64'
```
### Warmup (discarded)
```
error: XDG_RUNTIME_DIR is invalid or not set in the environment.
SMC indexed stats: checks=10483200 changed=42088 unchanged=10441112 stores=42088 bytes_compared=83865600 out_of_range=0 clears=0
Renderer stats: cells_total=41600 cells_rasterized=1 cells_skipped=41599 skip_rate=100.0% framebuffer_checksum=3574760718

Frame profile (SMC indexed):
  frames:              252
  grid/raycast:        0.870 ms/frame
  state packing:       0.000 ms/frame
  smc batch diff:      0.000 ms/frame
  smc stream diff:     0.000 ms/frame
  dirty decision:      0.617 ms/frame
  dirty iteration:     0.617 ms/frame
  rasterization:       0.617 ms/frame
  SDL/update/present:  0.897 ms/frame
  other/unaccounted:   0.000 ms/frame
  total profiled:      1.514 ms/frame
{
  "grid_width": 260,
  "grid_height": 160,
  "target_fps": 120,
```

### Measured Runs
#### Run 1
```
error: XDG_RUNTIME_DIR is invalid or not set in the environment.
SMC indexed stats: checks=25875200 changed=42724 unchanged=25832476 stores=42724 bytes_compared=207001600 out_of_range=0 clears=0
Renderer stats: cells_total=41600 cells_rasterized=1 cells_skipped=41599 skip_rate=100.0% framebuffer_checksum=3268480090

Frame profile (SMC indexed):
  frames:              622
  grid/raycast:        0.837 ms/frame
  state packing:       0.000 ms/frame
  smc batch diff:      0.000 ms/frame
  smc stream diff:     0.000 ms/frame
  dirty decision:      0.602 ms/frame
  dirty iteration:     0.602 ms/frame
  rasterization:       0.602 ms/frame
  SDL/update/present:  0.820 ms/frame
  other/unaccounted:   0.000 ms/frame
  total profiled:      1.423 ms/frame
{
  "grid_width": 260,
  "grid_height": 160,
  "target_fps": 120,
  "avg_render_ms": 1.42,
  "worst_render_ms": 2.11,
  "effective_worst_ms": 2.11,
  "outlier_trimmed": false,
  "min_spare_ms": 5.21,
  "frames": 622,
  "result": "ideal"
}
```

#### Run 2
```
error: XDG_RUNTIME_DIR is invalid or not set in the environment.
SMC indexed stats: checks=25875200 changed=42722 unchanged=25832478 stores=42722 bytes_compared=207001600 out_of_range=0 clears=0
Renderer stats: cells_total=41600 cells_rasterized=1 cells_skipped=41599 skip_rate=100.0% framebuffer_checksum=3663052485

Frame profile (SMC indexed):
  frames:              622
  grid/raycast:        0.828 ms/frame
  state packing:       0.000 ms/frame
  smc batch diff:      0.000 ms/frame
  smc stream diff:     0.000 ms/frame
  dirty decision:      0.600 ms/frame
  dirty iteration:     0.600 ms/frame
  rasterization:       0.600 ms/frame
  SDL/update/present:  0.838 ms/frame
  other/unaccounted:   0.000 ms/frame
  total profiled:      1.438 ms/frame
{
  "grid_width": 260,
  "grid_height": 160,
  "target_fps": 120,
  "avg_render_ms": 1.44,
  "worst_render_ms": 1.90,
  "effective_worst_ms": 1.90,
  "outlier_trimmed": false,
  "min_spare_ms": 5.32,
  "frames": 622,
  "result": "ideal"
}
```

#### Run 3
```
error: XDG_RUNTIME_DIR is invalid or not set in the environment.
SMC indexed stats: checks=25792000 changed=42720 unchanged=25749280 stores=42720 bytes_compared=206336000 out_of_range=0 clears=0
Renderer stats: cells_total=41600 cells_rasterized=1 cells_skipped=41599 skip_rate=100.0% framebuffer_checksum=3268364053

Frame profile (SMC indexed):
  frames:              620
  grid/raycast:        0.816 ms/frame
  state packing:       0.000 ms/frame
  smc batch diff:      0.000 ms/frame
  smc stream diff:     0.000 ms/frame
  dirty decision:      0.602 ms/frame
  dirty iteration:     0.602 ms/frame
  rasterization:       0.602 ms/frame
  SDL/update/present:  0.842 ms/frame
  other/unaccounted:   0.000 ms/frame
  total profiled:      1.444 ms/frame
{
  "grid_width": 260,
  "grid_height": 160,
  "target_fps": 120,
  "avg_render_ms": 1.44,
  "worst_render_ms": 2.27,
  "effective_worst_ms": 2.27,
  "outlier_trimmed": false,
  "min_spare_ms": 5.04,
  "frames": 620,
  "result": "ideal"
}
```

## Mode: smc_batch

### Build
```
mkdir -p build
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror -DUSE_SMC_BATCH_STATE_TRACKER=1    -DPROFILE_FRAME=1  -I"/nas/contents/Projects/Programming Projects/C/terminal-style-game/terminal-style-game/vendor/dist/include" -I"/nas/contents/Projects/Programming Projects/C/terminal-style-game/terminal-style-game/vendor/src/SDL/include" -I"vendor/src/smc/include" -I"vendor/src/smc/src/c" src/app.c src/asset_designer.c src/asset_loader.c src/assets.c src/camera.c src/command_system.c src/config.c src/decal_io.c src/editor_selection.c src/glyph_atlas.c src/glyph_block_cache.c src/grid.c src/input.c src/lighting.c src/lighting_cache.c src/live_editor.c src/main.c src/map.c src/map_loader.c src/material_designer.c src/math.c src/menu_state.c src/raycast.c src/renderer.c src/scale.c src/scene_document.c src/smc_indexed_state_tracker.c src/smc_render_opt.c src/smc_state_tracker.c src/timing.c src/ui_ele.c src/unified_editor.c src/world.c vendor/src/smc/src/c/smc_runtime_stub.c vendor/src/smc/src/c/smc_artifact.c vendor/src/smc/src/c/smc_state.c -o build/ascii-fps -L"/nas/contents/Projects/Programming Projects/C/terminal-style-game/terminal-style-game/vendor/dist/lib64" -lSDL3 -lSDL3_mixer -lenet -lm -lm -Wl,-rpath,'$ORIGIN/../vendor/dist/lib64'
```
### Warmup (discarded)
```
error: XDG_RUNTIME_DIR is invalid or not set in the environment.
SMC batch stats: checks=10483200 changed=42088 unchanged=10441112 stores=42088 bytes_compared=83865600 out_of_range=0 clears=0
Renderer stats: cells_total=41600 cells_rasterized=42088 cells_skipped=41599 skip_rate=100.0% framebuffer_checksum=48208362

Frame profile (SMC batch):
  frames:              252
  grid/raycast:        0.839 ms/frame
  state packing:       0.174 ms/frame
  smc batch diff:      0.087 ms/frame
  smc stream diff:     0.000 ms/frame
  dirty decision:      0.000 ms/frame
  dirty iteration:     0.105 ms/frame
  rasterization:       0.105 ms/frame
  SDL/update/present:  0.867 ms/frame
  other/unaccounted:   0.000 ms/frame
  total profiled:      1.146 ms/frame
{
  "grid_width": 260,
  "grid_height": 160,
  "target_fps": 120,
```

### Measured Runs
#### Run 1
```
error: XDG_RUNTIME_DIR is invalid or not set in the environment.
SMC batch stats: checks=26166400 changed=42728 unchanged=26123672 stores=42728 bytes_compared=209331200 out_of_range=0 clears=0
Renderer stats: cells_total=41600 cells_rasterized=42728 cells_skipped=41599 skip_rate=100.0% framebuffer_checksum=642481619

Frame profile (SMC batch):
  frames:              629
  grid/raycast:        0.837 ms/frame
  state packing:       0.175 ms/frame
  smc batch diff:      0.086 ms/frame
  smc stream diff:     0.000 ms/frame
  dirty decision:      0.000 ms/frame
  dirty iteration:     0.093 ms/frame
  rasterization:       0.093 ms/frame
  SDL/update/present:  0.847 ms/frame
  other/unaccounted:   0.000 ms/frame
  total profiled:      1.115 ms/frame
{
  "grid_width": 260,
  "grid_height": 160,
  "target_fps": 120,
  "avg_render_ms": 1.12,
  "worst_render_ms": 1.64,
  "effective_worst_ms": 1.64,
  "outlier_trimmed": false,
  "min_spare_ms": 5.44,
  "frames": 629,
  "result": "ideal"
}
```

#### Run 2
```
error: XDG_RUNTIME_DIR is invalid or not set in the environment.
SMC batch stats: checks=26166400 changed=42727 unchanged=26123673 stores=42727 bytes_compared=209331200 out_of_range=0 clears=0
Renderer stats: cells_total=41600 cells_rasterized=42727 cells_skipped=41599 skip_rate=100.0% framebuffer_checksum=1528212921

Frame profile (SMC batch):
  frames:              629
  grid/raycast:        0.845 ms/frame
  state packing:       0.182 ms/frame
  smc batch diff:      0.086 ms/frame
  smc stream diff:     0.000 ms/frame
  dirty decision:      0.000 ms/frame
  dirty iteration:     0.094 ms/frame
  rasterization:       0.094 ms/frame
  SDL/update/present:  0.849 ms/frame
  other/unaccounted:   0.000 ms/frame
  total profiled:      1.125 ms/frame
{
  "grid_width": 260,
  "grid_height": 160,
  "target_fps": 120,
  "avg_render_ms": 1.12,
  "worst_render_ms": 2.29,
  "effective_worst_ms": 2.29,
  "outlier_trimmed": false,
  "min_spare_ms": 5.01,
  "frames": 629,
  "result": "ideal"
}
```

#### Run 3
```
error: XDG_RUNTIME_DIR is invalid or not set in the environment.
SMC batch stats: checks=26166400 changed=42729 unchanged=26123671 stores=42729 bytes_compared=209331200 out_of_range=0 clears=0
Renderer stats: cells_total=41600 cells_rasterized=42729 cells_skipped=41599 skip_rate=100.0% framebuffer_checksum=14539093

Frame profile (SMC batch):
  frames:              629
  grid/raycast:        0.834 ms/frame
  state packing:       0.173 ms/frame
  smc batch diff:      0.086 ms/frame
  smc stream diff:     0.000 ms/frame
  dirty decision:      0.000 ms/frame
  dirty iteration:     0.094 ms/frame
  rasterization:       0.094 ms/frame
  SDL/update/present:  0.847 ms/frame
  other/unaccounted:   0.000 ms/frame
  total profiled:      1.114 ms/frame
{
  "grid_width": 260,
  "grid_height": 160,
  "target_fps": 120,
  "avg_render_ms": 1.11,
  "worst_render_ms": 1.52,
  "effective_worst_ms": 1.52,
  "outlier_trimmed": false,
  "min_spare_ms": 5.15,
  "frames": 629,
  "result": "ideal"
}
```

## Mode: smc_stream_opt

### Build
```
mkdir -p build
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror -DUSE_SMC_STREAM_STATE_TRACKER=1    -DPROFILE_FRAME=1  -I"/nas/contents/Projects/Programming Projects/C/terminal-style-game/terminal-style-game/vendor/dist/include" -I"/nas/contents/Projects/Programming Projects/C/terminal-style-game/terminal-style-game/vendor/src/SDL/include" -I"vendor/src/smc/include" -I"vendor/src/smc/src/c" src/app.c src/asset_designer.c src/asset_loader.c src/assets.c src/camera.c src/command_system.c src/config.c src/decal_io.c src/editor_selection.c src/glyph_atlas.c src/glyph_block_cache.c src/grid.c src/input.c src/lighting.c src/lighting_cache.c src/live_editor.c src/main.c src/map.c src/map_loader.c src/material_designer.c src/math.c src/menu_state.c src/raycast.c src/renderer.c src/scale.c src/scene_document.c src/smc_indexed_state_tracker.c src/smc_render_opt.c src/smc_state_tracker.c src/timing.c src/ui_ele.c src/unified_editor.c src/world.c vendor/src/smc/src/c/smc_runtime_stub.c vendor/src/smc/src/c/smc_artifact.c vendor/src/smc/src/c/smc_state.c -o build/ascii-fps -L"/nas/contents/Projects/Programming Projects/C/terminal-style-game/terminal-style-game/vendor/dist/lib64" -lSDL3 -lSDL3_mixer -lenet -lm -lm -Wl,-rpath,'$ORIGIN/../vendor/dist/lib64'
```
### Warmup (discarded)
```
error: XDG_RUNTIME_DIR is invalid or not set in the environment.
SMC stream stats: checks=10566400 changed=42090 unchanged=10524310 stores=42090 bytes_compared=73964800 out_of_range=0 clears=0 fallback_count=0
Renderer stats: cells_total=41600 cells_rasterized=42090 cells_skipped=41599 skip_rate=100.0% framebuffer_checksum=2316658952

Frame profile (SMC stream):
  frames:              254
  grid/raycast:        0.812 ms/frame
  state packing:       0.000 ms/frame
  smc batch diff:      0.000 ms/frame
  smc stream diff:     0.000 ms/frame
  dirty decision:      0.000 ms/frame
  dirty iteration:     0.016 ms/frame
  rasterization:       0.016 ms/frame
  SDL/update/present:  0.837 ms/frame
  other/unaccounted:   0.000 ms/frame
  total profiled:      1.039 ms/frame
{
  "grid_width": 260,
  "grid_height": 160,
  "target_fps": 120,
```

### Measured Runs
#### Run 1
```
error: XDG_RUNTIME_DIR is invalid or not set in the environment.
SMC stream stats: checks=26499200 changed=42736 unchanged=26456464 stores=42736 bytes_compared=185494400 out_of_range=0 clears=0 fallback_count=0
Renderer stats: cells_total=41600 cells_rasterized=42736 cells_skipped=41599 skip_rate=100.0% framebuffer_checksum=90377835

Frame profile (SMC stream):
  frames:              637
  grid/raycast:        0.806 ms/frame
  state packing:       0.000 ms/frame
  smc batch diff:      0.000 ms/frame
  smc stream diff:     0.000 ms/frame
  dirty decision:      0.000 ms/frame
  dirty iteration:     0.007 ms/frame
  rasterization:       0.007 ms/frame
  SDL/update/present:  0.806 ms/frame
  other/unaccounted:   0.000 ms/frame
  total profiled:      1.001 ms/frame
{
  "grid_width": 260,
  "grid_height": 160,
  "target_fps": 120,
  "avg_render_ms": 1.00,
  "worst_render_ms": 1.56,
  "effective_worst_ms": 1.56,
  "outlier_trimmed": false,
  "min_spare_ms": 5.91,
  "frames": 637,
  "result": "ideal"
}
```

#### Run 2
```
error: XDG_RUNTIME_DIR is invalid or not set in the environment.
SMC stream stats: checks=26499200 changed=42739 unchanged=26456461 stores=42739 bytes_compared=185494400 out_of_range=0 clears=0 fallback_count=0
Renderer stats: cells_total=41600 cells_rasterized=42739 cells_skipped=41599 skip_rate=100.0% framebuffer_checksum=1276528686

Frame profile (SMC stream):
  frames:              637
  grid/raycast:        0.818 ms/frame
  state packing:       0.000 ms/frame
  smc batch diff:      0.000 ms/frame
  smc stream diff:     0.000 ms/frame
  dirty decision:      0.000 ms/frame
  dirty iteration:     0.008 ms/frame
  rasterization:       0.008 ms/frame
  SDL/update/present:  0.800 ms/frame
  other/unaccounted:   0.000 ms/frame
  total profiled:      0.995 ms/frame
{
  "grid_width": 260,
  "grid_height": 160,
  "target_fps": 120,
  "avg_render_ms": 0.99,
  "worst_render_ms": 1.58,
  "effective_worst_ms": 1.58,
  "outlier_trimmed": false,
  "min_spare_ms": 5.78,
  "frames": 637,
  "result": "ideal"
}
```

#### Run 3
```
error: XDG_RUNTIME_DIR is invalid or not set in the environment.
SMC stream stats: checks=26457600 changed=42733 unchanged=26414867 stores=42733 bytes_compared=185203200 out_of_range=0 clears=0 fallback_count=0
Renderer stats: cells_total=41600 cells_rasterized=42733 cells_skipped=41599 skip_rate=100.0% framebuffer_checksum=4182145967

Frame profile (SMC stream):
  frames:              636
  grid/raycast:        0.806 ms/frame
  state packing:       0.000 ms/frame
  smc batch diff:      0.000 ms/frame
  smc stream diff:     0.000 ms/frame
  dirty decision:      0.000 ms/frame
  dirty iteration:     0.008 ms/frame
  rasterization:       0.008 ms/frame
  SDL/update/present:  0.822 ms/frame
  other/unaccounted:   0.000 ms/frame
  total profiled:      1.016 ms/frame
{
  "grid_width": 260,
  "grid_height": 160,
  "target_fps": 120,
  "avg_render_ms": 1.02,
  "worst_render_ms": 1.41,
  "effective_worst_ms": 1.41,
  "outlier_trimmed": false,
  "min_spare_ms": 5.87,
  "frames": 636,
  "result": "ideal"
}
```

## Mode: smc_stream_base

### Build
```
mkdir -p build
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror -DUSE_SMC_STREAM_STATE_TRACKER=1    -DPROFILE_FRAME=1 -DSMC_DISABLE_OPTIMIZED_STREAM_KERNELS -I"/nas/contents/Projects/Programming Projects/C/terminal-style-game/terminal-style-game/vendor/dist/include" -I"/nas/contents/Projects/Programming Projects/C/terminal-style-game/terminal-style-game/vendor/src/SDL/include" -I"vendor/src/smc/include" -I"vendor/src/smc/src/c" src/app.c src/asset_designer.c src/asset_loader.c src/assets.c src/camera.c src/command_system.c src/config.c src/decal_io.c src/editor_selection.c src/glyph_atlas.c src/glyph_block_cache.c src/grid.c src/input.c src/lighting.c src/lighting_cache.c src/live_editor.c src/main.c src/map.c src/map_loader.c src/material_designer.c src/math.c src/menu_state.c src/raycast.c src/renderer.c src/scale.c src/scene_document.c src/smc_indexed_state_tracker.c src/smc_render_opt.c src/smc_state_tracker.c src/timing.c src/ui_ele.c src/unified_editor.c src/world.c vendor/src/smc/src/c/smc_runtime_stub.c vendor/src/smc/src/c/smc_artifact.c vendor/src/smc/src/c/smc_state.c -o build/ascii-fps -L"/nas/contents/Projects/Programming Projects/C/terminal-style-game/terminal-style-game/vendor/dist/lib64" -lSDL3 -lSDL3_mixer -lenet -lm -lm -Wl,-rpath,'$ORIGIN/../vendor/dist/lib64'
```
### Warmup (discarded)
```
error: XDG_RUNTIME_DIR is invalid or not set in the environment.
SMC stream stats: checks=10358400 changed=42084 unchanged=10316316 stores=42084 bytes_compared=72508800 out_of_range=0 clears=0 fallback_count=0
Renderer stats: cells_total=41600 cells_rasterized=42084 cells_skipped=41599 skip_rate=100.0% framebuffer_checksum=4246989889

Frame profile (SMC stream):
  frames:              249
  grid/raycast:        0.815 ms/frame
  state packing:       0.000 ms/frame
  smc batch diff:      0.000 ms/frame
  smc stream diff:     0.000 ms/frame
  dirty decision:      0.000 ms/frame
  dirty iteration:     0.020 ms/frame
  rasterization:       0.020 ms/frame
  SDL/update/present:  0.852 ms/frame
  other/unaccounted:   0.000 ms/frame
  total profiled:      1.291 ms/frame
{
  "grid_width": 260,
  "grid_height": 160,
  "target_fps": 120,
```

### Measured Runs
#### Run 1
```
error: XDG_RUNTIME_DIR is invalid or not set in the environment.
SMC stream stats: checks=25875200 changed=42721 unchanged=25832479 stores=42721 bytes_compared=181126400 out_of_range=0 clears=0 fallback_count=0
Renderer stats: cells_total=41600 cells_rasterized=42721 cells_skipped=41599 skip_rate=100.0% framebuffer_checksum=1050363530

Frame profile (SMC stream):
  frames:              622
  grid/raycast:        0.832 ms/frame
  state packing:       0.000 ms/frame
  smc batch diff:      0.000 ms/frame
  smc stream diff:     0.000 ms/frame
  dirty decision:      0.000 ms/frame
  dirty iteration:     0.009 ms/frame
  rasterization:       0.009 ms/frame
  SDL/update/present:  0.838 ms/frame
  other/unaccounted:   0.000 ms/frame
  total profiled:      1.265 ms/frame
{
  "grid_width": 260,
  "grid_height": 160,
  "target_fps": 120,
  "avg_render_ms": 1.27,
  "worst_render_ms": 1.75,
  "effective_worst_ms": 1.75,
  "outlier_trimmed": false,
  "min_spare_ms": 5.45,
  "frames": 622,
  "result": "ideal"
}
```

#### Run 2
```
error: XDG_RUNTIME_DIR is invalid or not set in the environment.
SMC stream stats: checks=25958400 changed=42723 unchanged=25915677 stores=42723 bytes_compared=181708800 out_of_range=0 clears=0 fallback_count=0
Renderer stats: cells_total=41600 cells_rasterized=42723 cells_skipped=41599 skip_rate=100.0% framebuffer_checksum=3329546834

Frame profile (SMC stream):
  frames:              624
  grid/raycast:        0.820 ms/frame
  state packing:       0.000 ms/frame
  smc batch diff:      0.000 ms/frame
  smc stream diff:     0.000 ms/frame
  dirty decision:      0.000 ms/frame
  dirty iteration:     0.008 ms/frame
  rasterization:       0.008 ms/frame
  SDL/update/present:  0.828 ms/frame
  other/unaccounted:   0.000 ms/frame
  total profiled:      1.252 ms/frame
{
  "grid_width": 260,
  "grid_height": 160,
  "target_fps": 120,
  "avg_render_ms": 1.25,
  "worst_render_ms": 2.16,
  "effective_worst_ms": 2.16,
  "outlier_trimmed": false,
  "min_spare_ms": 5.10,
  "frames": 624,
  "result": "ideal"
}
```

#### Run 3
```
error: XDG_RUNTIME_DIR is invalid or not set in the environment.
SMC stream stats: checks=25916800 changed=42723 unchanged=25874077 stores=42723 bytes_compared=181417600 out_of_range=0 clears=0 fallback_count=0
Renderer stats: cells_total=41600 cells_rasterized=42723 cells_skipped=41599 skip_rate=100.0% framebuffer_checksum=3677084494

Frame profile (SMC stream):
  frames:              623
  grid/raycast:        0.826 ms/frame
  state packing:       0.000 ms/frame
  smc batch diff:      0.000 ms/frame
  smc stream diff:     0.000 ms/frame
  dirty decision:      0.000 ms/frame
  dirty iteration:     0.008 ms/frame
  rasterization:       0.008 ms/frame
  SDL/update/present:  0.831 ms/frame
  other/unaccounted:   0.000 ms/frame
  total profiled:      1.254 ms/frame
{
  "grid_width": 260,
  "grid_height": 160,
  "target_fps": 120,
  "avg_render_ms": 1.25,
  "worst_render_ms": 2.37,
  "effective_worst_ms": 2.37,
  "outlier_trimmed": false,
  "min_spare_ms": 5.14,
  "frames": 623,
  "result": "ideal"
}
```

## Summary (Median Values)

| mode | avg_render_ms | raycast_grid_ms | state_pack_ms | smc_batch_diff_ms | smc_stream_diff_ms | dirty_check_ms | dirty_iter_ms | raster_ms | sdl_update_ms | checks | changed | unchanged | stores | bytes_compared | out_of_range | fallback_count | cells_processed | cells_skipped | parse_status | build_status | run_status |
|------|-------------|-----------------|---------------|-------------|-------------------|----------------|-------------|---------|--------------|--------|---------|-----------|--------|---------------|-------------|---------------|----------------|---------------|----------------|--------------|------------|
| baseline | 2.56 | 0.848 | 0.000 | 0.000 | 0.000 | 1.798 | 1.798 | 1.798 | 0.766 | missing | missing | missing | missing | missing | missing | missing | 41600 | 0 | PARSE_PASS | BUILD_PASS | RUN_PASS |
| dirty_cells | 1.06 | 0.859 | 0.000 | 0.000 | 0.000 | 0.235 | 0.235 | 0.235 | 0.824 | missing | missing | missing | missing | missing | missing | missing | 1 | 41599 | PARSE_PASS | BUILD_PASS | RUN_PASS |
| smc_indexed | 1.44 | 0.828 | 0.000 | 0.000 | 0.000 | 0.602 | 0.602 | 0.602 | 0.838 | 25875200 | 42722 | 25832476 | 42722 | 207001600 | 0 | missing | 1 | 41599 | PARSE_PASS | BUILD_PASS | RUN_PASS |
| smc_batch | 1.12 | 0.837 | 0.175 | 0.086 | 0.000 | 0.000 | 0.094 | 0.094 | 0.847 | 26166400 | 42728 | 26123672 | 42728 | 209331200 | 0 | missing | 42728 | 41599 | PARSE_PASS | BUILD_PASS | RUN_PASS |
| smc_stream_opt | 1.00 | 0.806 | 0.000 | 0.000 | 0.000 | 0.000 | 0.008 | 0.008 | 0.806 | 26499200 | 42736 | 26456461 | 42736 | 185494400 | 0 | 0 | 42736 | 41599 | PARSE_PASS | BUILD_PASS | RUN_PASS |
| smc_stream_base | 1.25 | 0.826 | 0.000 | 0.000 | 0.000 | 0.000 | 0.008 | 0.008 | 0.831 | 25916800 | 42723 | 25874077 | 42723 | 181417600 | 0 | 0 | 42723 | 41599 | PARSE_PASS | BUILD_PASS | RUN_PASS |

## Correctness Verification (smc_stream_opt)

- out_of_range: 0
- fallback_count: 0
- bytes_compared: 185494400
- checks * 7: 185494400

**Result: CORRECTNESS_PASS**

## Decision

- Stream optimized median: 1.00 ms
- Packed batch median: 1.12 ms

**Final Verdict: DECISION_PASS (1.00ms <= 1.12ms)**

## Dynamic Scene Benchmark Matrix

### Scenario: idle

error: XDG_RUNTIME_DIR is invalid or not set in the environment.
Renderer stats: cells_total=41600 cells_rasterized=1 cells_skipped=41599 skip_rate=100.0% framebuffer_checksum=2510471264

Frame profile (custom dirty cells):
  frames:              600
  grid/raycast:        0.775 ms/frame
  state packing:       0.000 ms/frame
  smc batch diff:      0.000 ms/frame
  smc stream diff:     0.000 ms/frame
  dirty decision:      0.230 ms/frame
  dirty iteration:     0.230 ms/frame
  rasterization:       0.230 ms/frame
  SDL/update/present:  0.677 ms/frame
  other/unaccounted:   0.000 ms/frame
  total profiled:      0.907 ms/frame
{
  "grid_width": 260,
  "grid_height": 160,
  "target_fps": 120,
  "avg_render_ms": 0.91,
  "worst_render_ms": 1.24,
  "effective_worst_ms": 1.24,
  "outlier_trimmed": false,
  "min_spare_ms": 5.92,
  "frames": 600,
  "result": "ideal"
}

error: XDG_RUNTIME_DIR is invalid or not set in the environment.
SMC batch stats: checks=24960000 changed=42694 unchanged=24917306 stores=42694 bytes_compared=199680000 out_of_range=0 clears=0
Renderer stats: cells_total=41600 cells_rasterized=42694 cells_skipped=41599 skip_rate=100.0% framebuffer_checksum=2231826293

Frame profile (SMC batch):
  frames:              600
  grid/raycast:        0.776 ms/frame
  state packing:       0.163 ms/frame
  smc batch diff:      0.082 ms/frame
  smc stream diff:     0.000 ms/frame
  dirty decision:      0.000 ms/frame
  dirty iteration:     0.089 ms/frame
  rasterization:       0.089 ms/frame
  SDL/update/present:  0.700 ms/frame
  other/unaccounted:   0.000 ms/frame
  total profiled:      0.952 ms/frame
{
  "grid_width": 260,
  "grid_height": 160,
  "target_fps": 120,
  "avg_render_ms": 0.95,
  "worst_render_ms": 1.41,
  "effective_worst_ms": 1.41,
  "outlier_trimmed": false,
  "min_spare_ms": 5.60,
  "frames": 600,
  "result": "ideal"
}

error: XDG_RUNTIME_DIR is invalid or not set in the environment.
SMC stream stats: checks=24960000 changed=42692 unchanged=24917308 stores=42692 bytes_compared=174720000 out_of_range=0 clears=0 fallback_count=0
Renderer stats: cells_total=41600 cells_rasterized=42692 cells_skipped=41599 skip_rate=100.0% framebuffer_checksum=172008354

Frame profile (SMC stream):
  frames:              600
  grid/raycast:        0.755 ms/frame
  state packing:       0.000 ms/frame
  smc batch diff:      0.000 ms/frame
  smc stream diff:     0.000 ms/frame
  dirty decision:      0.000 ms/frame
  dirty iteration:     0.007 ms/frame
  rasterization:       0.007 ms/frame
  SDL/update/present:  0.700 ms/frame
  other/unaccounted:   0.000 ms/frame
  total profiled:      0.889 ms/frame
{
  "grid_width": 260,
  "grid_height": 160,
  "target_fps": 120,
  "avg_render_ms": 0.89,
  "worst_render_ms": 1.25,
  "effective_worst_ms": 1.25,
  "outlier_trimmed": false,
  "min_spare_ms": 6.04,
  "frames": 600,
  "result": "ideal"
}

### Scenario: camera

error: XDG_RUNTIME_DIR is invalid or not set in the environment.
Renderer stats: cells_total=41600 cells_rasterized=1698 cells_skipped=39902 skip_rate=95.9% framebuffer_checksum=2703286640

Frame profile (custom dirty cells):
  frames:              600
  grid/raycast:        0.622 ms/frame
  state packing:       0.000 ms/frame
  smc batch diff:      0.000 ms/frame
  smc stream diff:     0.000 ms/frame
  dirty decision:      0.293 ms/frame
  dirty iteration:     0.293 ms/frame
  rasterization:       0.293 ms/frame
  SDL/update/present:  0.684 ms/frame
  other/unaccounted:   0.000 ms/frame
  total profiled:      0.977 ms/frame
{
  "grid_width": 260,
  "grid_height": 160,
  "target_fps": 120,
  "avg_render_ms": 0.98,
  "worst_render_ms": 1.38,
  "effective_worst_ms": 1.38,
  "outlier_trimmed": false,
  "min_spare_ms": 5.88,
  "frames": 600,
  "result": "ideal"
}

error: XDG_RUNTIME_DIR is invalid or not set in the environment.
SMC batch stats: checks=24960000 changed=425264 unchanged=24534736 stores=425264 bytes_compared=199680000 out_of_range=0 clears=0
Renderer stats: cells_total=41600 cells_rasterized=425264 cells_skipped=39902 skip_rate=95.9% framebuffer_checksum=4098833307

Frame profile (SMC batch):
  frames:              600
  grid/raycast:        0.614 ms/frame
  state packing:       0.174 ms/frame
  smc batch diff:      0.090 ms/frame
  smc stream diff:     0.000 ms/frame
  dirty decision:      0.000 ms/frame
  dirty iteration:     0.153 ms/frame
  rasterization:       0.153 ms/frame
  SDL/update/present:  0.663 ms/frame
  other/unaccounted:   0.000 ms/frame
  total profiled:      0.989 ms/frame
{
  "grid_width": 260,
  "grid_height": 160,
  "target_fps": 120,
  "avg_render_ms": 0.99,
  "worst_render_ms": 1.55,
  "effective_worst_ms": 1.55,
  "outlier_trimmed": false,
  "min_spare_ms": 5.54,
  "frames": 600,
  "result": "ideal"
}

**RUN_FAIL: smc_stream_opt**
### Scenario: rotate

