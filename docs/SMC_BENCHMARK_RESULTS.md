# SMC State Tracking Benchmark Results

**Date**: 2026-07-17T11:58:56-04:00
**Git commit**: 5bde8dc
**Git branch**: dev
**SMC submodule commit**: 3783ae9
**Grid**: 160x260 (41600 cells)

## Mode: baseline

### Build
```
mkdir -p build
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror     -DPROFILE_FRAME=1  -I"/bigdisk/programming/C/terminal-style-game/vendor/dist/include" -I"/bigdisk/programming/C/terminal-style-game/vendor/src/SDL/include"  src/app.c src/asset_designer.c src/asset_loader.c src/assets.c src/camera.c src/config.c src/decal_io.c src/glyph_atlas.c src/glyph_block_cache.c src/grid.c src/input.c src/lighting.c src/lighting_cache.c src/live_editor.c src/main.c src/map.c src/map_loader.c src/material_designer.c src/math.c src/menu_state.c src/raycast.c src/renderer.c src/scale.c src/smc_indexed_state_tracker.c src/smc_render_opt.c src/smc_state_tracker.c src/timing.c src/ui_ele.c src/world.c  -o build/ascii-fps -L"/bigdisk/programming/C/terminal-style-game/vendor/dist/lib64" -lSDL3 -lSDL3_mixer -lenet -lm  -Wl,-rpath,'$ORIGIN/../vendor/dist/lib64'
```
### Warmup (discarded)
```
Renderer stats: cells_total=41600 cells_rasterized=41600 cells_skipped=0 skip_rate=0.0% framebuffer_checksum=3779999112

Frame profile (baseline):
  frames:              159
  grid/raycast:        2.490 ms/frame
  state packing:       0.000 ms/frame
  smc batch diff:      0.000 ms/frame
  smc stream diff:     0.000 ms/frame
  dirty decision:      5.140 ms/frame
  dirty iteration:     5.140 ms/frame
  rasterization:       5.140 ms/frame
  SDL/update/present:  4.937 ms/frame
  other/unaccounted:   0.000 ms/frame
  total profiled:      10.077 ms/frame
{
  "grid_width": 260,
  "grid_height": 160,
  "target_fps": 120,
  "avg_render_ms": 10.08,
  "worst_render_ms": 13.85,
```

### Measured Runs
#### Run 1
```
Renderer stats: cells_total=41600 cells_rasterized=41600 cells_skipped=0 skip_rate=0.0% framebuffer_checksum=4107194411

Frame profile (baseline):
  frames:              450
  grid/raycast:        2.234 ms/frame
  state packing:       0.000 ms/frame
  smc batch diff:      0.000 ms/frame
  smc stream diff:     0.000 ms/frame
  dirty decision:      4.575 ms/frame
  dirty iteration:     4.575 ms/frame
  rasterization:       4.575 ms/frame
  SDL/update/present:  4.285 ms/frame
  other/unaccounted:   0.000 ms/frame
  total profiled:      8.860 ms/frame
{
  "grid_width": 260,
  "grid_height": 160,
  "target_fps": 120,
  "avg_render_ms": 8.86,
  "worst_render_ms": 22.23,
  "effective_worst_ms": 22.23,
  "outlier_trimmed": false,
  "min_spare_ms": -17.61,
  "frames": 450,
  "result": "fail_performance"
}
```

#### Run 2
```
Renderer stats: cells_total=41600 cells_rasterized=41600 cells_skipped=0 skip_rate=0.0% framebuffer_checksum=1106167798

Frame profile (baseline):
  frames:              468
  grid/raycast:        2.096 ms/frame
  state packing:       0.000 ms/frame
  smc batch diff:      0.000 ms/frame
  smc stream diff:     0.000 ms/frame
  dirty decision:      4.298 ms/frame
  dirty iteration:     4.298 ms/frame
  rasterization:       4.298 ms/frame
  SDL/update/present:  4.287 ms/frame
  other/unaccounted:   0.000 ms/frame
  total profiled:      8.585 ms/frame
{
  "grid_width": 260,
  "grid_height": 160,
  "target_fps": 120,
  "avg_render_ms": 8.59,
  "worst_render_ms": 13.56,
  "effective_worst_ms": 13.56,
  "outlier_trimmed": false,
  "min_spare_ms": -7.94,
  "frames": 468,
  "result": "fail_performance"
}
```

#### Run 3
```
Renderer stats: cells_total=41600 cells_rasterized=41600 cells_skipped=0 skip_rate=0.0% framebuffer_checksum=2428918089

Frame profile (baseline):
  frames:              464
  grid/raycast:        2.193 ms/frame
  state packing:       0.000 ms/frame
  smc batch diff:      0.000 ms/frame
  smc stream diff:     0.000 ms/frame
  dirty decision:      4.340 ms/frame
  dirty iteration:     4.340 ms/frame
  rasterization:       4.340 ms/frame
  SDL/update/present:  4.225 ms/frame
  other/unaccounted:   0.000 ms/frame
  total profiled:      8.564 ms/frame
{
  "grid_width": 260,
  "grid_height": 160,
  "target_fps": 120,
  "avg_render_ms": 8.57,
  "worst_render_ms": 14.39,
  "effective_worst_ms": 14.39,
  "outlier_trimmed": false,
  "min_spare_ms": -7.69,
  "frames": 464,
  "result": "fail_performance"
}
```

## Mode: dirty_cells

### Build
```
mkdir -p build
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror    -DUSE_DIRTY_CELLS=1 -DPROFILE_FRAME=1  -I"/bigdisk/programming/C/terminal-style-game/vendor/dist/include" -I"/bigdisk/programming/C/terminal-style-game/vendor/src/SDL/include"  src/app.c src/asset_designer.c src/asset_loader.c src/assets.c src/camera.c src/config.c src/decal_io.c src/glyph_atlas.c src/glyph_block_cache.c src/grid.c src/input.c src/lighting.c src/lighting_cache.c src/live_editor.c src/main.c src/map.c src/map_loader.c src/material_designer.c src/math.c src/menu_state.c src/raycast.c src/renderer.c src/scale.c src/smc_indexed_state_tracker.c src/smc_render_opt.c src/smc_state_tracker.c src/timing.c src/ui_ele.c src/world.c  -o build/ascii-fps -L"/bigdisk/programming/C/terminal-style-game/vendor/dist/lib64" -lSDL3 -lSDL3_mixer -lenet -lm  -Wl,-rpath,'$ORIGIN/../vendor/dist/lib64'
```
### Warmup (discarded)
```
Renderer stats: cells_total=41600 cells_rasterized=2 cells_skipped=41598 skip_rate=100.0% framebuffer_checksum=647457468

Frame profile (custom dirty cells):
  frames:              221
  grid/raycast:        2.383 ms/frame
  state packing:       0.000 ms/frame
  smc batch diff:      0.000 ms/frame
  smc stream diff:     0.000 ms/frame
  dirty decision:      0.595 ms/frame
  dirty iteration:     0.595 ms/frame
  rasterization:       0.595 ms/frame
  SDL/update/present:  5.321 ms/frame
  other/unaccounted:   0.000 ms/frame
  total profiled:      5.916 ms/frame
{
  "grid_width": 260,
  "grid_height": 160,
  "target_fps": 120,
  "avg_render_ms": 5.92,
  "worst_render_ms": 10.91,
```

### Measured Runs
#### Run 1
```
Renderer stats: cells_total=41600 cells_rasterized=1 cells_skipped=41599 skip_rate=100.0% framebuffer_checksum=576007933

Frame profile (custom dirty cells):
  frames:              596
  grid/raycast:        2.275 ms/frame
  state packing:       0.000 ms/frame
  smc batch diff:      0.000 ms/frame
  smc stream diff:     0.000 ms/frame
  dirty decision:      0.529 ms/frame
  dirty iteration:     0.529 ms/frame
  rasterization:       0.529 ms/frame
  SDL/update/present:  4.606 ms/frame
  other/unaccounted:   0.000 ms/frame
  total profiled:      5.135 ms/frame
{
  "grid_width": 260,
  "grid_height": 160,
  "target_fps": 120,
  "avg_render_ms": 5.14,
  "worst_render_ms": 10.72,
  "effective_worst_ms": 10.72,
  "outlier_trimmed": false,
  "min_spare_ms": -5.20,
  "frames": 596,
  "result": "fail_performance"
}
```

#### Run 2
```
Renderer stats: cells_total=41600 cells_rasterized=1 cells_skipped=41599 skip_rate=100.0% framebuffer_checksum=169005392

Frame profile (custom dirty cells):
  frames:              600
  grid/raycast:        2.210 ms/frame
  state packing:       0.000 ms/frame
  smc batch diff:      0.000 ms/frame
  smc stream diff:     0.000 ms/frame
  dirty decision:      0.509 ms/frame
  dirty iteration:     0.509 ms/frame
  rasterization:       0.509 ms/frame
  SDL/update/present:  4.536 ms/frame
  other/unaccounted:   0.000 ms/frame
  total profiled:      5.045 ms/frame
{
  "grid_width": 260,
  "grid_height": 160,
  "target_fps": 120,
  "avg_render_ms": 5.05,
  "worst_render_ms": 16.75,
  "effective_worst_ms": 16.75,
  "outlier_trimmed": false,
  "min_spare_ms": -10.27,
  "frames": 600,
  "result": "fail_performance"
}
```

#### Run 3
```
Renderer stats: cells_total=41600 cells_rasterized=1 cells_skipped=41599 skip_rate=100.0% framebuffer_checksum=1586750823

Frame profile (custom dirty cells):
  frames:              599
  grid/raycast:        2.195 ms/frame
  state packing:       0.000 ms/frame
  smc batch diff:      0.000 ms/frame
  smc stream diff:     0.000 ms/frame
  dirty decision:      0.515 ms/frame
  dirty iteration:     0.515 ms/frame
  rasterization:       0.515 ms/frame
  SDL/update/present:  4.420 ms/frame
  other/unaccounted:   0.000 ms/frame
  total profiled:      4.935 ms/frame
{
  "grid_width": 260,
  "grid_height": 160,
  "target_fps": 120,
  "avg_render_ms": 4.94,
  "worst_render_ms": 8.99,
  "effective_worst_ms": 8.99,
  "outlier_trimmed": false,
  "min_spare_ms": -4.86,
  "frames": 599,
  "result": "fail_performance"
}
```

## Mode: smc_indexed

### Build
```
mkdir -p build
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror -DUSE_SMC_INDEXED_STATE_TRACKER=1    -DPROFILE_FRAME=1  -I"/bigdisk/programming/C/terminal-style-game/vendor/dist/include" -I"/bigdisk/programming/C/terminal-style-game/vendor/src/SDL/include" -I"vendor/src/smc/include" -I"vendor/src/smc/src/c" src/app.c src/asset_designer.c src/asset_loader.c src/assets.c src/camera.c src/config.c src/decal_io.c src/glyph_atlas.c src/glyph_block_cache.c src/grid.c src/input.c src/lighting.c src/lighting_cache.c src/live_editor.c src/main.c src/map.c src/map_loader.c src/material_designer.c src/math.c src/menu_state.c src/raycast.c src/renderer.c src/scale.c src/smc_indexed_state_tracker.c src/smc_render_opt.c src/smc_state_tracker.c src/timing.c src/ui_ele.c src/world.c vendor/src/smc/src/c/smc_runtime_stub.c vendor/src/smc/src/c/smc_artifact.c vendor/src/smc/src/c/smc_state.c -o build/ascii-fps -L"/bigdisk/programming/C/terminal-style-game/vendor/dist/lib64" -lSDL3 -lSDL3_mixer -lenet -lm -lm -Wl,-rpath,'$ORIGIN/../vendor/dist/lib64'
```
### Warmup (discarded)
```
SMC indexed stats: checks=8777600 changed=42043 unchanged=8735557 stores=42043 bytes_compared=70220800 out_of_range=0 clears=0
Renderer stats: cells_total=41600 cells_rasterized=2 cells_skipped=41598 skip_rate=100.0% framebuffer_checksum=2368223234

Frame profile (SMC indexed):
  frames:              211
  grid/raycast:        2.425 ms/frame
  state packing:       0.000 ms/frame
  smc batch diff:      0.000 ms/frame
  smc stream diff:     0.000 ms/frame
  dirty decision:      1.842 ms/frame
  dirty iteration:     1.842 ms/frame
  rasterization:       1.842 ms/frame
  SDL/update/present:  5.046 ms/frame
  other/unaccounted:   0.000 ms/frame
  total profiled:      6.888 ms/frame
{
  "grid_width": 260,
  "grid_height": 160,
  "target_fps": 120,
  "avg_render_ms": 6.89,
```

### Measured Runs
#### Run 1
```
SMC indexed stats: checks=24502400 changed=42516 unchanged=24459884 stores=42516 bytes_compared=196019200 out_of_range=0 clears=0
Renderer stats: cells_total=41600 cells_rasterized=1 cells_skipped=41599 skip_rate=100.0% framebuffer_checksum=3249187400

Frame profile (SMC indexed):
  frames:              589
  grid/raycast:        2.200 ms/frame
  state packing:       0.000 ms/frame
  smc batch diff:      0.000 ms/frame
  smc stream diff:     0.000 ms/frame
  dirty decision:      1.692 ms/frame
  dirty iteration:     1.692 ms/frame
  rasterization:       1.692 ms/frame
  SDL/update/present:  4.416 ms/frame
  other/unaccounted:   0.000 ms/frame
  total profiled:      6.109 ms/frame
{
  "grid_width": 260,
  "grid_height": 160,
  "target_fps": 120,
  "avg_render_ms": 6.11,
  "worst_render_ms": 11.64,
  "effective_worst_ms": 11.64,
  "outlier_trimmed": false,
  "min_spare_ms": -5.16,
  "frames": 589,
  "result": "fail_performance"
}
```

#### Run 2
```
SMC indexed stats: checks=24211200 changed=42507 unchanged=24168693 stores=42507 bytes_compared=193689600 out_of_range=0 clears=0
Renderer stats: cells_total=41600 cells_rasterized=1 cells_skipped=41599 skip_rate=100.0% framebuffer_checksum=1126818478

Frame profile (SMC indexed):
  frames:              582
  grid/raycast:        2.172 ms/frame
  state packing:       0.000 ms/frame
  smc batch diff:      0.000 ms/frame
  smc stream diff:     0.000 ms/frame
  dirty decision:      1.698 ms/frame
  dirty iteration:     1.698 ms/frame
  rasterization:       1.698 ms/frame
  SDL/update/present:  4.555 ms/frame
  other/unaccounted:   0.000 ms/frame
  total profiled:      6.253 ms/frame
{
  "grid_width": 260,
  "grid_height": 160,
  "target_fps": 120,
  "avg_render_ms": 6.25,
  "worst_render_ms": 16.02,
  "effective_worst_ms": 16.02,
  "outlier_trimmed": false,
  "min_spare_ms": -9.56,
  "frames": 582,
  "result": "fail_performance"
}
```

#### Run 3
```
SMC indexed stats: checks=24044800 changed=42495 unchanged=24002305 stores=42495 bytes_compared=192358400 out_of_range=0 clears=0
Renderer stats: cells_total=41600 cells_rasterized=1 cells_skipped=41599 skip_rate=100.0% framebuffer_checksum=2232842943

Frame profile (SMC indexed):
  frames:              578
  grid/raycast:        2.217 ms/frame
  state packing:       0.000 ms/frame
  smc batch diff:      0.000 ms/frame
  smc stream diff:     0.000 ms/frame
  dirty decision:      1.745 ms/frame
  dirty iteration:     1.745 ms/frame
  rasterization:       1.745 ms/frame
  SDL/update/present:  4.531 ms/frame
  other/unaccounted:   0.000 ms/frame
  total profiled:      6.277 ms/frame
{
  "grid_width": 260,
  "grid_height": 160,
  "target_fps": 120,
  "avg_render_ms": 6.28,
  "worst_render_ms": 10.67,
  "effective_worst_ms": 10.67,
  "outlier_trimmed": false,
  "min_spare_ms": -4.18,
  "frames": 578,
  "result": "fail_performance"
}
```

## Mode: smc_batch

### Build
```
mkdir -p build
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror -DUSE_SMC_BATCH_STATE_TRACKER=1    -DPROFILE_FRAME=1  -I"/bigdisk/programming/C/terminal-style-game/vendor/dist/include" -I"/bigdisk/programming/C/terminal-style-game/vendor/src/SDL/include" -I"vendor/src/smc/include" -I"vendor/src/smc/src/c" src/app.c src/asset_designer.c src/asset_loader.c src/assets.c src/camera.c src/config.c src/decal_io.c src/glyph_atlas.c src/glyph_block_cache.c src/grid.c src/input.c src/lighting.c src/lighting_cache.c src/live_editor.c src/main.c src/map.c src/map_loader.c src/material_designer.c src/math.c src/menu_state.c src/raycast.c src/renderer.c src/scale.c src/smc_indexed_state_tracker.c src/smc_render_opt.c src/smc_state_tracker.c src/timing.c src/ui_ele.c src/world.c vendor/src/smc/src/c/smc_runtime_stub.c vendor/src/smc/src/c/smc_artifact.c vendor/src/smc/src/c/smc_state.c -o build/ascii-fps -L"/bigdisk/programming/C/terminal-style-game/vendor/dist/lib64" -lSDL3 -lSDL3_mixer -lenet -lm -lm -Wl,-rpath,'$ORIGIN/../vendor/dist/lib64'
```
### Warmup (discarded)
```
SMC batch stats: checks=8944000 changed=42047 unchanged=8901953 stores=42047 bytes_compared=71552000 out_of_range=0 clears=0
Renderer stats: cells_total=41600 cells_rasterized=42047 cells_skipped=41599 skip_rate=100.0% framebuffer_checksum=2972072771

Frame profile (SMC batch):
  frames:              215
  grid/raycast:        2.533 ms/frame
  state packing:       0.515 ms/frame
  smc batch diff:      0.238 ms/frame
  smc stream diff:     0.000 ms/frame
  dirty decision:      0.000 ms/frame
  dirty iteration:     0.346 ms/frame
  rasterization:       0.346 ms/frame
  SDL/update/present:  5.185 ms/frame
  other/unaccounted:   0.000 ms/frame
  total profiled:      6.046 ms/frame
{
  "grid_width": 260,
  "grid_height": 160,
  "target_fps": 120,
  "avg_render_ms": 6.05,
```

### Measured Runs
#### Run 1
```
SMC batch stats: checks=24876800 changed=42713 unchanged=24834087 stores=42713 bytes_compared=199014400 out_of_range=0 clears=0
Renderer stats: cells_total=41600 cells_rasterized=42713 cells_skipped=41599 skip_rate=100.0% framebuffer_checksum=2462060619

Frame profile (SMC batch):
  frames:              598
  grid/raycast:        2.129 ms/frame
  state packing:       0.474 ms/frame
  smc batch diff:      0.205 ms/frame
  smc stream diff:     0.000 ms/frame
  dirty decision:      0.000 ms/frame
  dirty iteration:     0.241 ms/frame
  rasterization:       0.241 ms/frame
  SDL/update/present:  4.430 ms/frame
  other/unaccounted:   0.000 ms/frame
  total profiled:      5.144 ms/frame
{
  "grid_width": 260,
  "grid_height": 160,
  "target_fps": 120,
  "avg_render_ms": 5.15,
  "worst_render_ms": 15.60,
  "effective_worst_ms": 15.60,
  "outlier_trimmed": false,
  "min_spare_ms": -9.11,
  "frames": 598,
  "result": "fail_performance"
}
```

#### Run 2
```
SMC batch stats: checks=24752000 changed=42522 unchanged=24709478 stores=42522 bytes_compared=198016000 out_of_range=0 clears=0
Renderer stats: cells_total=41600 cells_rasterized=42522 cells_skipped=41599 skip_rate=100.0% framebuffer_checksum=346203325

Frame profile (SMC batch):
  frames:              595
  grid/raycast:        2.217 ms/frame
  state packing:       0.481 ms/frame
  smc batch diff:      0.209 ms/frame
  smc stream diff:     0.000 ms/frame
  dirty decision:      0.000 ms/frame
  dirty iteration:     0.235 ms/frame
  rasterization:       0.235 ms/frame
  SDL/update/present:  4.556 ms/frame
  other/unaccounted:   0.000 ms/frame
  total profiled:      5.272 ms/frame
{
  "grid_width": 260,
  "grid_height": 160,
  "target_fps": 120,
  "avg_render_ms": 5.27,
  "worst_render_ms": 10.43,
  "effective_worst_ms": 10.43,
  "outlier_trimmed": false,
  "min_spare_ms": -4.09,
  "frames": 595,
  "result": "fail_performance"
}
```

#### Run 3
```
SMC batch stats: checks=24793600 changed=42508 unchanged=24751092 stores=42508 bytes_compared=198348800 out_of_range=0 clears=0
Renderer stats: cells_total=41600 cells_rasterized=42508 cells_skipped=41599 skip_rate=100.0% framebuffer_checksum=3656065602

Frame profile (SMC batch):
  frames:              596
  grid/raycast:        2.145 ms/frame
  state packing:       0.473 ms/frame
  smc batch diff:      0.206 ms/frame
  smc stream diff:     0.000 ms/frame
  dirty decision:      0.000 ms/frame
  dirty iteration:     0.235 ms/frame
  rasterization:       0.235 ms/frame
  SDL/update/present:  4.489 ms/frame
  other/unaccounted:   0.000 ms/frame
  total profiled:      5.197 ms/frame
{
  "grid_width": 260,
  "grid_height": 160,
  "target_fps": 120,
  "avg_render_ms": 5.20,
  "worst_render_ms": 12.67,
  "effective_worst_ms": 12.67,
  "outlier_trimmed": false,
  "min_spare_ms": -6.26,
  "frames": 596,
  "result": "fail_performance"
}
```

## Mode: smc_stream_opt

### Build
```
mkdir -p build
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror -DUSE_SMC_STREAM_STATE_TRACKER=1    -DPROFILE_FRAME=1  -I"/bigdisk/programming/C/terminal-style-game/vendor/dist/include" -I"/bigdisk/programming/C/terminal-style-game/vendor/src/SDL/include" -I"vendor/src/smc/include" -I"vendor/src/smc/src/c" src/app.c src/asset_designer.c src/asset_loader.c src/assets.c src/camera.c src/config.c src/decal_io.c src/glyph_atlas.c src/glyph_block_cache.c src/grid.c src/input.c src/lighting.c src/lighting_cache.c src/live_editor.c src/main.c src/map.c src/map_loader.c src/material_designer.c src/math.c src/menu_state.c src/raycast.c src/renderer.c src/scale.c src/smc_indexed_state_tracker.c src/smc_render_opt.c src/smc_state_tracker.c src/timing.c src/ui_ele.c src/world.c vendor/src/smc/src/c/smc_runtime_stub.c vendor/src/smc/src/c/smc_artifact.c vendor/src/smc/src/c/smc_state.c -o build/ascii-fps -L"/bigdisk/programming/C/terminal-style-game/vendor/dist/lib64" -lSDL3 -lSDL3_mixer -lenet -lm -lm -Wl,-rpath,'$ORIGIN/../vendor/dist/lib64'
```
### Warmup (discarded)
```
SMC stream stats: checks=9152000 changed=42051 unchanged=9109949 stores=42051 bytes_compared=64064000 out_of_range=0 clears=0 fallback_count=0
Renderer stats: cells_total=41600 cells_rasterized=42051 cells_skipped=41599 skip_rate=100.0% framebuffer_checksum=2812453702

Frame profile (SMC stream):
  frames:              220
  grid/raycast:        2.370 ms/frame
  state packing:       0.000 ms/frame
  smc batch diff:      0.000 ms/frame
  smc stream diff:     0.000 ms/frame
  dirty decision:      0.000 ms/frame
  dirty iteration:     0.070 ms/frame
  rasterization:       0.070 ms/frame
  SDL/update/present:  5.257 ms/frame
  other/unaccounted:   0.000 ms/frame
  total profiled:      5.916 ms/frame
{
  "grid_width": 260,
  "grid_height": 160,
  "target_fps": 120,
  "avg_render_ms": 5.92,
```

### Measured Runs
#### Run 1
```
SMC stream stats: checks=24835200 changed=42517 unchanged=24792683 stores=42517 bytes_compared=173846400 out_of_range=0 clears=0 fallback_count=0
Renderer stats: cells_total=41600 cells_rasterized=42517 cells_skipped=41599 skip_rate=100.0% framebuffer_checksum=1255455474

Frame profile (SMC stream):
  frames:              597
  grid/raycast:        2.185 ms/frame
  state packing:       0.000 ms/frame
  smc batch diff:      0.000 ms/frame
  smc stream diff:     0.000 ms/frame
  dirty decision:      0.000 ms/frame
  dirty iteration:     0.028 ms/frame
  rasterization:       0.028 ms/frame
  SDL/update/present:  4.591 ms/frame
  other/unaccounted:   0.000 ms/frame
  total profiled:      5.189 ms/frame
{
  "grid_width": 260,
  "grid_height": 160,
  "target_fps": 120,
  "avg_render_ms": 5.19,
  "worst_render_ms": 9.32,
  "effective_worst_ms": 9.32,
  "outlier_trimmed": false,
  "min_spare_ms": -3.05,
  "frames": 597,
  "result": "fail_performance"
}
```

#### Run 2
```
SMC stream stats: checks=24752000 changed=42510 unchanged=24709490 stores=42510 bytes_compared=173264000 out_of_range=0 clears=0 fallback_count=0
Renderer stats: cells_total=41600 cells_rasterized=42510 cells_skipped=41599 skip_rate=100.0% framebuffer_checksum=70684743

Frame profile (SMC stream):
  frames:              595
  grid/raycast:        2.158 ms/frame
  state packing:       0.000 ms/frame
  smc batch diff:      0.000 ms/frame
  smc stream diff:     0.000 ms/frame
  dirty decision:      0.000 ms/frame
  dirty iteration:     0.031 ms/frame
  rasterization:       0.031 ms/frame
  SDL/update/present:  4.582 ms/frame
  other/unaccounted:   0.000 ms/frame
  total profiled:      5.185 ms/frame
{
  "grid_width": 260,
  "grid_height": 160,
  "target_fps": 120,
  "avg_render_ms": 5.19,
  "worst_render_ms": 11.57,
  "effective_worst_ms": 11.57,
  "outlier_trimmed": false,
  "min_spare_ms": -5.03,
  "frames": 595,
  "result": "fail_performance"
}
```

#### Run 3
```
SMC stream stats: checks=24752000 changed=42516 unchanged=24709484 stores=42516 bytes_compared=173264000 out_of_range=0 clears=0 fallback_count=0
Renderer stats: cells_total=41600 cells_rasterized=42516 cells_skipped=41599 skip_rate=100.0% framebuffer_checksum=121008746

Frame profile (SMC stream):
  frames:              595
  grid/raycast:        2.238 ms/frame
  state packing:       0.000 ms/frame
  smc batch diff:      0.000 ms/frame
  smc stream diff:     0.000 ms/frame
  dirty decision:      0.000 ms/frame
  dirty iteration:     0.034 ms/frame
  rasterization:       0.034 ms/frame
  SDL/update/present:  4.611 ms/frame
  other/unaccounted:   0.000 ms/frame
  total profiled:      5.231 ms/frame
{
  "grid_width": 260,
  "grid_height": 160,
  "target_fps": 120,
  "avg_render_ms": 5.23,
  "worst_render_ms": 9.81,
  "effective_worst_ms": 9.81,
  "outlier_trimmed": false,
  "min_spare_ms": -5.06,
  "frames": 595,
  "result": "fail_performance"
}
```

## Mode: smc_stream_base

### Build
```
mkdir -p build
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror -DUSE_SMC_STREAM_STATE_TRACKER=1    -DPROFILE_FRAME=1 -DSMC_DISABLE_OPTIMIZED_STREAM_KERNELS -I"/bigdisk/programming/C/terminal-style-game/vendor/dist/include" -I"/bigdisk/programming/C/terminal-style-game/vendor/src/SDL/include" -I"vendor/src/smc/include" -I"vendor/src/smc/src/c" src/app.c src/asset_designer.c src/asset_loader.c src/assets.c src/camera.c src/config.c src/decal_io.c src/glyph_atlas.c src/glyph_block_cache.c src/grid.c src/input.c src/lighting.c src/lighting_cache.c src/live_editor.c src/main.c src/map.c src/map_loader.c src/material_designer.c src/math.c src/menu_state.c src/raycast.c src/renderer.c src/scale.c src/smc_indexed_state_tracker.c src/smc_render_opt.c src/smc_state_tracker.c src/timing.c src/ui_ele.c src/world.c vendor/src/smc/src/c/smc_runtime_stub.c vendor/src/smc/src/c/smc_artifact.c vendor/src/smc/src/c/smc_state.c -o build/ascii-fps -L"/bigdisk/programming/C/terminal-style-game/vendor/dist/lib64" -lSDL3 -lSDL3_mixer -lenet -lm -lm -Wl,-rpath,'$ORIGIN/../vendor/dist/lib64'
```
### Warmup (discarded)
```
SMC stream stats: checks=8985600 changed=42048 unchanged=8943552 stores=42048 bytes_compared=62899200 out_of_range=0 clears=0 fallback_count=0
Renderer stats: cells_total=41600 cells_rasterized=42048 cells_skipped=41599 skip_rate=100.0% framebuffer_checksum=3456285543

Frame profile (SMC stream):
  frames:              216
  grid/raycast:        2.322 ms/frame
  state packing:       0.000 ms/frame
  smc batch diff:      0.000 ms/frame
  smc stream diff:     0.000 ms/frame
  dirty decision:      0.000 ms/frame
  dirty iteration:     0.077 ms/frame
  rasterization:       0.077 ms/frame
  SDL/update/present:  5.133 ms/frame
  other/unaccounted:   0.000 ms/frame
  total profiled:      6.584 ms/frame
{
  "grid_width": 260,
  "grid_height": 160,
  "target_fps": 120,
  "avg_render_ms": 6.59,
```

### Measured Runs
#### Run 1
```
SMC stream stats: checks=24752000 changed=42514 unchanged=24709486 stores=42514 bytes_compared=173264000 out_of_range=0 clears=0 fallback_count=0
Renderer stats: cells_total=41600 cells_rasterized=42514 cells_skipped=41599 skip_rate=100.0% framebuffer_checksum=2985696352

Frame profile (SMC stream):
  frames:              595
  grid/raycast:        2.097 ms/frame
  state packing:       0.000 ms/frame
  smc batch diff:      0.000 ms/frame
  smc stream diff:     0.000 ms/frame
  dirty decision:      0.000 ms/frame
  dirty iteration:     0.029 ms/frame
  rasterization:       0.029 ms/frame
  SDL/update/present:  4.487 ms/frame
  other/unaccounted:   0.000 ms/frame
  total profiled:      5.747 ms/frame
{
  "grid_width": 260,
  "grid_height": 160,
  "target_fps": 120,
  "avg_render_ms": 5.75,
  "worst_render_ms": 9.72,
  "effective_worst_ms": 9.72,
  "outlier_trimmed": false,
  "min_spare_ms": -3.93,
  "frames": 595,
  "result": "fail_performance"
}
```

#### Run 2
```
SMC stream stats: checks=23878400 changed=42489 unchanged=23835911 stores=42489 bytes_compared=167148800 out_of_range=0 clears=0 fallback_count=0
Renderer stats: cells_total=41600 cells_rasterized=42489 cells_skipped=41599 skip_rate=100.0% framebuffer_checksum=1670078965

Frame profile (SMC stream):
  frames:              574
  grid/raycast:        2.177 ms/frame
  state packing:       0.000 ms/frame
  smc batch diff:      0.000 ms/frame
  smc stream diff:     0.000 ms/frame
  dirty decision:      0.000 ms/frame
  dirty iteration:     0.035 ms/frame
  rasterization:       0.035 ms/frame
  SDL/update/present:  4.515 ms/frame
  other/unaccounted:   0.000 ms/frame
  total profiled:      5.882 ms/frame
{
  "grid_width": 260,
  "grid_height": 160,
  "target_fps": 120,
  "avg_render_ms": 5.88,
  "worst_render_ms": 13.44,
  "effective_worst_ms": 13.44,
  "outlier_trimmed": false,
  "min_spare_ms": -10.77,
  "frames": 574,
  "result": "fail_performance"
}
```

#### Run 3
```
SMC stream stats: checks=24336000 changed=42504 unchanged=24293496 stores=42504 bytes_compared=170352000 out_of_range=0 clears=0 fallback_count=0
Renderer stats: cells_total=41600 cells_rasterized=42504 cells_skipped=41599 skip_rate=100.0% framebuffer_checksum=959095170

Frame profile (SMC stream):
  frames:              585
  grid/raycast:        2.195 ms/frame
  state packing:       0.000 ms/frame
  smc batch diff:      0.000 ms/frame
  smc stream diff:     0.000 ms/frame
  dirty decision:      0.000 ms/frame
  dirty iteration:     0.060 ms/frame
  rasterization:       0.060 ms/frame
  SDL/update/present:  4.462 ms/frame
  other/unaccounted:   0.000 ms/frame
  total profiled:      5.834 ms/frame
{
  "grid_width": 260,
  "grid_height": 160,
  "target_fps": 120,
  "avg_render_ms": 5.84,
  "worst_render_ms": 14.14,
  "effective_worst_ms": 14.14,
  "outlier_trimmed": false,
  "min_spare_ms": -7.57,
  "frames": 585,
  "result": "fail_performance"
}
```

## Summary (Median Values)

| mode | avg_render_ms | raycast_grid_ms | state_pack_ms | smc_batch_diff_ms | smc_stream_diff_ms | dirty_check_ms | dirty_iter_ms | raster_ms | sdl_update_ms | checks | changed | unchanged | stores | bytes_compared | out_of_range | fallback_count | cells_processed | cells_skipped | parse_status | build_status | run_status |
|------|-------------|-----------------|---------------|-------------|-------------------|----------------|-------------|---------|--------------|--------|---------|-----------|--------|---------------|-------------|---------------|----------------|---------------|----------------|--------------|------------|
| baseline | 8.59 | 2.193 | 0.000 | 0.000 | 0.000 | 4.340 | 4.340 | 4.340 | 4.285 | missing | missing | missing | missing | missing | missing | missing | 41600 | 0 | PARSE_PASS | BUILD_PASS | RUN_FAIL |
| dirty_cells | 5.05 | 2.210 | 0.000 | 0.000 | 0.000 | 0.515 | 0.515 | 0.515 | 4.536 | missing | missing | missing | missing | missing | missing | missing | 1 | 41599 | PARSE_PASS | BUILD_PASS | RUN_FAIL |
| smc_indexed | 6.25 | 2.200 | 0.000 | 0.000 | 0.000 | 1.698 | 1.698 | 1.698 | 4.531 | 24211200 | 42507 | 24168693 | 42507 | 193689600 | 0 | missing | 1 | 41599 | PARSE_PASS | BUILD_PASS | RUN_FAIL |
| smc_batch | 5.20 | 2.145 | 0.474 | 0.206 | 0.000 | 0.000 | 0.235 | 0.235 | 4.489 | 24793600 | 42522 | 24751092 | 42522 | 198348800 | 0 | missing | 42522 | 41599 | PARSE_PASS | BUILD_PASS | RUN_FAIL |
| smc_stream_opt | 5.19 | 2.185 | 0.000 | 0.000 | 0.000 | 0.000 | 0.031 | 0.031 | 4.591 | 24752000 | 42516 | 24709490 | 42516 | 173264000 | 0 | 0 | 42516 | 41599 | PARSE_PASS | BUILD_PASS | RUN_FAIL |
| smc_stream_base | 5.84 | 2.177 | 0.000 | 0.000 | 0.000 | 0.000 | 0.035 | 0.035 | 4.487 | 24336000 | 42504 | 24293496 | 42504 | 170352000 | 0 | 0 | 42504 | 41599 | PARSE_PASS | BUILD_PASS | RUN_FAIL |

## Correctness Verification (smc_stream_opt)

- out_of_range: 0
- fallback_count: 0
- bytes_compared: 173846400
- checks * 7: 173846400

**Result: CORRECTNESS_PASS**

## Decision

- Stream optimized median: 5.19 ms
- Packed batch median: 5.20 ms

**Final Verdict: DECISION_FAIL (run failure)**

## Dynamic Scene Benchmark Matrix

### Scenario: idle

**RUN_FAIL: dirty_cells**
**RUN_FAIL: smc_batch**
**RUN_FAIL: smc_stream_opt**
### Scenario: camera

**RUN_FAIL: dirty_cells**
**RUN_FAIL: smc_batch**
**RUN_FAIL: smc_stream_opt**
### Scenario: rotate

**RUN_FAIL: dirty_cells**
**RUN_FAIL: smc_batch**
**RUN_FAIL: smc_stream_opt**
### Scenario: flicker

**RUN_FAIL: dirty_cells**
**RUN_FAIL: smc_batch**
**RUN_FAIL: smc_stream_opt**
### Scenario: ui

**RUN_FAIL: dirty_cells**
**RUN_FAIL: smc_batch**
**RUN_FAIL: smc_stream_opt**
### Scenario: fullchange

**RUN_FAIL: dirty_cells**
**RUN_FAIL: smc_batch**
**RUN_FAIL: smc_stream_opt**
---
