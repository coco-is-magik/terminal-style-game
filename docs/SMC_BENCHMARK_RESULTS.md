# SMC State Tracking Benchmark Results

**Date**: 2026-07-15T15:48:38-04:00
**Git commit**: 0bbf0d6
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
Renderer stats: cells_total=41600 cells_rasterized=41600 cells_skipped=0 skip_rate=0.0% framebuffer_checksum=2010957825

Frame profile (baseline):
  frames:              171
  grid/raycast:        2.354 ms/frame
  state packing:       0.000 ms/frame
  smc diff:            0.000 ms/frame
  dirty decision:      4.562 ms/frame
  dirty iteration:     4.562 ms/frame
  rasterization:       4.562 ms/frame
  SDL/update/present:  4.785 ms/frame
  other/unaccounted:   0.000 ms/frame
  total profiled:      9.347 ms/frame
{
  "grid_width": 260,
  "grid_height": 160,
  "target_fps": 120,
  "avg_render_ms": 9.35,
  "worst_render_ms": 11.65,
  "effective_worst_ms": 11.65,
```

### Measured Runs
#### Run 1
```
Renderer stats: cells_total=41600 cells_rasterized=41600 cells_skipped=0 skip_rate=0.0% framebuffer_checksum=198599348

Frame profile (baseline):
  frames:              461
  grid/raycast:        2.355 ms/frame
  state packing:       0.000 ms/frame
  smc diff:            0.000 ms/frame
  dirty decision:      4.371 ms/frame
  dirty iteration:     4.371 ms/frame
  rasterization:       4.371 ms/frame
  SDL/update/present:  4.107 ms/frame
  other/unaccounted:   0.000 ms/frame
  total profiled:      8.478 ms/frame
{
  "grid_width": 260,
  "grid_height": 160,
  "target_fps": 120,
  "avg_render_ms": 8.48,
  "worst_render_ms": 20.19,
  "effective_worst_ms": 20.19,
  "outlier_trimmed": false,
  "min_spare_ms": -14.21,
  "frames": 461,
  "result": "fail_performance"
}
```

#### Run 2
```
Renderer stats: cells_total=41600 cells_rasterized=41600 cells_skipped=0 skip_rate=0.0% framebuffer_checksum=3874438535

Frame profile (baseline):
  frames:              461
  grid/raycast:        2.293 ms/frame
  state packing:       0.000 ms/frame
  smc diff:            0.000 ms/frame
  dirty decision:      4.295 ms/frame
  dirty iteration:     4.295 ms/frame
  rasterization:       4.295 ms/frame
  SDL/update/present:  4.243 ms/frame
  other/unaccounted:   0.000 ms/frame
  total profiled:      8.539 ms/frame
{
  "grid_width": 260,
  "grid_height": 160,
  "target_fps": 120,
  "avg_render_ms": 8.54,
  "worst_render_ms": 12.43,
  "effective_worst_ms": 12.43,
  "outlier_trimmed": false,
  "min_spare_ms": -6.03,
  "frames": 461,
  "result": "fail_performance"
}
```

#### Run 3
```
Renderer stats: cells_total=41600 cells_rasterized=41600 cells_skipped=0 skip_rate=0.0% framebuffer_checksum=308075351

Frame profile (baseline):
  frames:              470
  grid/raycast:        2.182 ms/frame
  state packing:       0.000 ms/frame
  smc diff:            0.000 ms/frame
  dirty decision:      4.295 ms/frame
  dirty iteration:     4.295 ms/frame
  rasterization:       4.295 ms/frame
  SDL/update/present:  4.133 ms/frame
  other/unaccounted:   0.000 ms/frame
  total profiled:      8.429 ms/frame
{
  "grid_width": 260,
  "grid_height": 160,
  "target_fps": 120,
  "avg_render_ms": 8.43,
  "worst_render_ms": 11.74,
  "effective_worst_ms": 11.74,
  "outlier_trimmed": false,
  "min_spare_ms": -5.72,
  "frames": 470,
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
Renderer stats: cells_total=41600 cells_rasterized=1 cells_skipped=41599 skip_rate=100.0% framebuffer_checksum=1163285031

Frame profile (custom dirty cells):
  frames:              235
  grid/raycast:        2.152 ms/frame
  state packing:       0.000 ms/frame
  smc diff:            0.000 ms/frame
  dirty decision:      0.550 ms/frame
  dirty iteration:     0.550 ms/frame
  rasterization:       0.550 ms/frame
  SDL/update/present:  4.738 ms/frame
  other/unaccounted:   0.000 ms/frame
  total profiled:      5.288 ms/frame
{
  "grid_width": 260,
  "grid_height": 160,
  "target_fps": 120,
  "avg_render_ms": 5.29,
  "worst_render_ms": 12.78,
  "effective_worst_ms": 12.78,
```

### Measured Runs
#### Run 1
```
Renderer stats: cells_total=41600 cells_rasterized=1 cells_skipped=41599 skip_rate=100.0% framebuffer_checksum=3342754873

Frame profile (custom dirty cells):
  frames:              602
  grid/raycast:        2.115 ms/frame
  state packing:       0.000 ms/frame
  smc diff:            0.000 ms/frame
  dirty decision:      0.517 ms/frame
  dirty iteration:     0.517 ms/frame
  rasterization:       0.517 ms/frame
  SDL/update/present:  4.622 ms/frame
  other/unaccounted:   0.000 ms/frame
  total profiled:      5.139 ms/frame
{
  "grid_width": 260,
  "grid_height": 160,
  "target_fps": 120,
  "avg_render_ms": 5.14,
  "worst_render_ms": 15.33,
  "effective_worst_ms": 15.33,
  "outlier_trimmed": false,
  "min_spare_ms": -10.03,
  "frames": 602,
  "result": "fail_performance"
}
```

#### Run 2
```
Renderer stats: cells_total=41600 cells_rasterized=1 cells_skipped=41599 skip_rate=100.0% framebuffer_checksum=2150998380

Frame profile (custom dirty cells):
  frames:              616
  grid/raycast:        2.236 ms/frame
  state packing:       0.000 ms/frame
  smc diff:            0.000 ms/frame
  dirty decision:      0.502 ms/frame
  dirty iteration:     0.502 ms/frame
  rasterization:       0.502 ms/frame
  SDL/update/present:  4.741 ms/frame
  other/unaccounted:   0.000 ms/frame
  total profiled:      5.243 ms/frame
{
  "grid_width": 260,
  "grid_height": 160,
  "target_fps": 120,
  "avg_render_ms": 5.24,
  "worst_render_ms": 13.45,
  "effective_worst_ms": 13.45,
  "outlier_trimmed": false,
  "min_spare_ms": -7.00,
  "frames": 616,
  "result": "fail_performance"
}
```

#### Run 3
```
Renderer stats: cells_total=41600 cells_rasterized=1 cells_skipped=41599 skip_rate=100.0% framebuffer_checksum=107244035

Frame profile (custom dirty cells):
  frames:              614
  grid/raycast:        2.188 ms/frame
  state packing:       0.000 ms/frame
  smc diff:            0.000 ms/frame
  dirty decision:      0.498 ms/frame
  dirty iteration:     0.498 ms/frame
  rasterization:       0.498 ms/frame
  SDL/update/present:  4.450 ms/frame
  other/unaccounted:   0.000 ms/frame
  total profiled:      4.948 ms/frame
{
  "grid_width": 260,
  "grid_height": 160,
  "target_fps": 120,
  "avg_render_ms": 4.95,
  "worst_render_ms": 10.63,
  "effective_worst_ms": 10.63,
  "outlier_trimmed": false,
  "min_spare_ms": -4.29,
  "frames": 614,
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
SMC indexed stats: checks=8736000 changed=42041 unchanged=8693959 stores=42041 bytes_compared=69888000 out_of_range=0 clears=0
Renderer stats: cells_total=41600 cells_rasterized=1 cells_skipped=41599 skip_rate=100.0% framebuffer_checksum=2420840273

Frame profile (SMC indexed):
  frames:              210
  grid/raycast:        2.372 ms/frame
  state packing:       0.000 ms/frame
  smc diff:            0.000 ms/frame
  dirty decision:      1.829 ms/frame
  dirty iteration:     1.829 ms/frame
  rasterization:       1.829 ms/frame
  SDL/update/present:  5.221 ms/frame
  other/unaccounted:   0.000 ms/frame
  total profiled:      7.050 ms/frame
{
  "grid_width": 260,
  "grid_height": 160,
  "target_fps": 120,
  "avg_render_ms": 7.05,
  "worst_render_ms": 12.52,
```

### Measured Runs
#### Run 1
```
SMC indexed stats: checks=24544000 changed=42502 unchanged=24501498 stores=42502 bytes_compared=196352000 out_of_range=0 clears=0
Renderer stats: cells_total=41600 cells_rasterized=1 cells_skipped=41599 skip_rate=100.0% framebuffer_checksum=928585814

Frame profile (SMC indexed):
  frames:              590
  grid/raycast:        2.146 ms/frame
  state packing:       0.000 ms/frame
  smc diff:            0.000 ms/frame
  dirty decision:      1.681 ms/frame
  dirty iteration:     1.681 ms/frame
  rasterization:       1.681 ms/frame
  SDL/update/present:  4.344 ms/frame
  other/unaccounted:   0.000 ms/frame
  total profiled:      6.026 ms/frame
{
  "grid_width": 260,
  "grid_height": 160,
  "target_fps": 120,
  "avg_render_ms": 6.03,
  "worst_render_ms": 12.90,
  "effective_worst_ms": 12.90,
  "outlier_trimmed": false,
  "min_spare_ms": -6.50,
  "frames": 590,
  "result": "fail_performance"
}
```

#### Run 2
```
SMC indexed stats: checks=24252800 changed=42498 unchanged=24210302 stores=42498 bytes_compared=194022400 out_of_range=0 clears=0
Renderer stats: cells_total=41600 cells_rasterized=1 cells_skipped=41599 skip_rate=100.0% framebuffer_checksum=3367827701

Frame profile (SMC indexed):
  frames:              583
  grid/raycast:        2.203 ms/frame
  state packing:       0.000 ms/frame
  smc diff:            0.000 ms/frame
  dirty decision:      1.596 ms/frame
  dirty iteration:     1.596 ms/frame
  rasterization:       1.596 ms/frame
  SDL/update/present:  4.547 ms/frame
  other/unaccounted:   0.000 ms/frame
  total profiled:      6.142 ms/frame
{
  "grid_width": 260,
  "grid_height": 160,
  "target_fps": 120,
  "avg_render_ms": 6.14,
  "worst_render_ms": 12.96,
  "effective_worst_ms": 12.96,
  "outlier_trimmed": false,
  "min_spare_ms": -6.70,
  "frames": 583,
  "result": "fail_performance"
}
```

#### Run 3
```
SMC indexed stats: checks=24710400 changed=42500 unchanged=24667900 stores=42500 bytes_compared=197683200 out_of_range=0 clears=0
Renderer stats: cells_total=41600 cells_rasterized=1 cells_skipped=41599 skip_rate=100.0% framebuffer_checksum=4230629300

Frame profile (SMC indexed):
  frames:              594
  grid/raycast:        2.064 ms/frame
  state packing:       0.000 ms/frame
  smc diff:            0.000 ms/frame
  dirty decision:      1.563 ms/frame
  dirty iteration:     1.563 ms/frame
  rasterization:       1.563 ms/frame
  SDL/update/present:  4.508 ms/frame
  other/unaccounted:   0.000 ms/frame
  total profiled:      6.071 ms/frame
{
  "grid_width": 260,
  "grid_height": 160,
  "target_fps": 120,
  "avg_render_ms": 6.07,
  "worst_render_ms": 13.69,
  "effective_worst_ms": 13.69,
  "outlier_trimmed": false,
  "min_spare_ms": -7.27,
  "frames": 594,
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
SMC batch stats: checks=9318400 changed=42058 unchanged=9276342 stores=42058 bytes_compared=74547200 out_of_range=0 clears=0
Renderer stats: cells_total=41600 cells_rasterized=42058 cells_skipped=41599 skip_rate=100.0% framebuffer_checksum=4039072498

Frame profile (SMC batch):
  frames:              224
  grid/raycast:        2.235 ms/frame
  state packing:       0.576 ms/frame
  smc diff:            0.232 ms/frame
  dirty decision:      0.000 ms/frame
  dirty iteration:     0.059 ms/frame
  rasterization:       0.059 ms/frame
  SDL/update/present:  4.949 ms/frame
  other/unaccounted:   0.000 ms/frame
  total profiled:      5.816 ms/frame
{
  "grid_width": 260,
  "grid_height": 160,
  "target_fps": 120,
  "avg_render_ms": 5.82,
  "worst_render_ms": 12.38,
```

### Measured Runs
#### Run 1
```
SMC batch stats: checks=25209600 changed=42525 unchanged=25167075 stores=42525 bytes_compared=201676800 out_of_range=0 clears=0
Renderer stats: cells_total=41600 cells_rasterized=42525 cells_skipped=41599 skip_rate=100.0% framebuffer_checksum=943333285

Frame profile (SMC batch):
  frames:              606
  grid/raycast:        2.213 ms/frame
  state packing:       0.490 ms/frame
  smc diff:            0.227 ms/frame
  dirty decision:      0.000 ms/frame
  dirty iteration:     0.024 ms/frame
  rasterization:       0.024 ms/frame
  SDL/update/present:  4.512 ms/frame
  other/unaccounted:   0.000 ms/frame
  total profiled:      5.252 ms/frame
{
  "grid_width": 260,
  "grid_height": 160,
  "target_fps": 120,
  "avg_render_ms": 5.25,
  "worst_render_ms": 12.01,
  "effective_worst_ms": 12.01,
  "outlier_trimmed": false,
  "min_spare_ms": -5.64,
  "frames": 606,
  "result": "fail_performance"
}
```

#### Run 2
```
SMC batch stats: checks=25542400 changed=42531 unchanged=25499869 stores=42531 bytes_compared=204339200 out_of_range=0 clears=0
Renderer stats: cells_total=41600 cells_rasterized=42531 cells_skipped=41599 skip_rate=100.0% framebuffer_checksum=3023068005

Frame profile (SMC batch):
  frames:              614
  grid/raycast:        2.268 ms/frame
  state packing:       0.480 ms/frame
  smc diff:            0.225 ms/frame
  dirty decision:      0.000 ms/frame
  dirty iteration:     0.024 ms/frame
  rasterization:       0.024 ms/frame
  SDL/update/present:  4.797 ms/frame
  other/unaccounted:   0.000 ms/frame
  total profiled:      5.527 ms/frame
{
  "grid_width": 260,
  "grid_height": 160,
  "target_fps": 120,
  "avg_render_ms": 5.53,
  "worst_render_ms": 13.12,
  "effective_worst_ms": 13.12,
  "outlier_trimmed": false,
  "min_spare_ms": -6.79,
  "frames": 614,
  "result": "fail_performance"
}
```

#### Run 3
```
SMC batch stats: checks=24710400 changed=42502 unchanged=24667898 stores=42502 bytes_compared=197683200 out_of_range=0 clears=0
Renderer stats: cells_total=41600 cells_rasterized=42502 cells_skipped=41599 skip_rate=100.0% framebuffer_checksum=3173872474

Frame profile (SMC batch):
  frames:              594
  grid/raycast:        2.265 ms/frame
  state packing:       0.498 ms/frame
  smc diff:            0.222 ms/frame
  dirty decision:      0.000 ms/frame
  dirty iteration:     0.024 ms/frame
  rasterization:       0.024 ms/frame
  SDL/update/present:  4.724 ms/frame
  other/unaccounted:   0.000 ms/frame
  total profiled:      5.468 ms/frame
{
  "grid_width": 260,
  "grid_height": 160,
  "target_fps": 120,
  "avg_render_ms": 5.47,
  "worst_render_ms": 12.55,
  "effective_worst_ms": 12.55,
  "outlier_trimmed": false,
  "min_spare_ms": -6.04,
  "frames": 594,
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
SMC stream stats: checks=9526400 changed=42063 unchanged=9484337 stores=42063 bytes_compared=66684800 out_of_range=0 clears=0 fallback_count=0
Renderer stats: cells_total=41600 cells_rasterized=42063 cells_skipped=41599 skip_rate=100.0% framebuffer_checksum=1782350669

Frame profile (SMC stream):
  frames:              229
  grid/raycast:        2.315 ms/frame
  state packing:       0.000 ms/frame
  smc diff:            0.000 ms/frame
  dirty decision:      0.000 ms/frame
  dirty iteration:     0.147 ms/frame
  rasterization:       0.147 ms/frame
  SDL/update/present:  4.991 ms/frame
  other/unaccounted:   0.000 ms/frame
  total profiled:      5.753 ms/frame
{
  "grid_width": 260,
  "grid_height": 160,
  "target_fps": 120,
  "avg_render_ms": 5.75,
  "worst_render_ms": 10.18,
```

### Measured Runs
#### Run 1
```
SMC stream stats: checks=24876800 changed=42508 unchanged=24834292 stores=42508 bytes_compared=174137600 out_of_range=0 clears=0 fallback_count=0
Renderer stats: cells_total=41600 cells_rasterized=42508 cells_skipped=41599 skip_rate=100.0% framebuffer_checksum=244725594

Frame profile (SMC stream):
  frames:              598
  grid/raycast:        2.287 ms/frame
  state packing:       0.000 ms/frame
  smc diff:            0.000 ms/frame
  dirty decision:      0.000 ms/frame
  dirty iteration:     0.024 ms/frame
  rasterization:       0.024 ms/frame
  SDL/update/present:  4.705 ms/frame
  other/unaccounted:   0.000 ms/frame
  total profiled:      5.313 ms/frame
{
  "grid_width": 260,
  "grid_height": 160,
  "target_fps": 120,
  "avg_render_ms": 5.31,
  "worst_render_ms": 12.02,
  "effective_worst_ms": 12.02,
  "outlier_trimmed": false,
  "min_spare_ms": -6.61,
  "frames": 598,
  "result": "fail_performance"
}
```

#### Run 2
```
SMC stream stats: checks=25043200 changed=42514 unchanged=25000686 stores=42514 bytes_compared=175302400 out_of_range=0 clears=0 fallback_count=0
Renderer stats: cells_total=41600 cells_rasterized=42514 cells_skipped=41599 skip_rate=100.0% framebuffer_checksum=459807797

Frame profile (SMC stream):
  frames:              602
  grid/raycast:        2.260 ms/frame
  state packing:       0.000 ms/frame
  smc diff:            0.000 ms/frame
  dirty decision:      0.000 ms/frame
  dirty iteration:     0.015 ms/frame
  rasterization:       0.015 ms/frame
  SDL/update/present:  4.445 ms/frame
  other/unaccounted:   0.000 ms/frame
  total profiled:      5.034 ms/frame
{
  "grid_width": 260,
  "grid_height": 160,
  "target_fps": 120,
  "avg_render_ms": 5.04,
  "worst_render_ms": 12.25,
  "effective_worst_ms": 12.25,
  "outlier_trimmed": false,
  "min_spare_ms": -5.97,
  "frames": 602,
  "result": "fail_performance"
}
```

#### Run 3
```
SMC stream stats: checks=25209600 changed=42513 unchanged=25167087 stores=42513 bytes_compared=176467200 out_of_range=0 clears=0 fallback_count=0
Renderer stats: cells_total=41600 cells_rasterized=42513 cells_skipped=41599 skip_rate=100.0% framebuffer_checksum=712096435

Frame profile (SMC stream):
  frames:              606
  grid/raycast:        2.247 ms/frame
  state packing:       0.000 ms/frame
  smc diff:            0.000 ms/frame
  dirty decision:      0.000 ms/frame
  dirty iteration:     0.019 ms/frame
  rasterization:       0.019 ms/frame
  SDL/update/present:  4.585 ms/frame
  other/unaccounted:   0.000 ms/frame
  total profiled:      5.173 ms/frame
{
  "grid_width": 260,
  "grid_height": 160,
  "target_fps": 120,
  "avg_render_ms": 5.17,
  "worst_render_ms": 12.08,
  "effective_worst_ms": 12.08,
  "outlier_trimmed": false,
  "min_spare_ms": -5.76,
  "frames": 606,
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
SMC stream stats: checks=9568000 changed=42064 unchanged=9525936 stores=42064 bytes_compared=66976000 out_of_range=0 clears=0 fallback_count=0
Renderer stats: cells_total=41600 cells_rasterized=42064 cells_skipped=41599 skip_rate=100.0% framebuffer_checksum=2359925525

Frame profile (SMC stream):
  frames:              230
  grid/raycast:        2.258 ms/frame
  state packing:       0.000 ms/frame
  smc diff:            0.000 ms/frame
  dirty decision:      0.000 ms/frame
  dirty iteration:     0.056 ms/frame
  rasterization:       0.056 ms/frame
  SDL/update/present:  4.918 ms/frame
  other/unaccounted:   0.000 ms/frame
  total profiled:      6.192 ms/frame
{
  "grid_width": 260,
  "grid_height": 160,
  "target_fps": 120,
  "avg_render_ms": 6.19,
  "worst_render_ms": 12.60,
```

### Measured Runs
#### Run 1
```
SMC stream stats: checks=25084800 changed=42514 unchanged=25042286 stores=42514 bytes_compared=175593600 out_of_range=0 clears=0 fallback_count=0
Renderer stats: cells_total=41600 cells_rasterized=42514 cells_skipped=41599 skip_rate=100.0% framebuffer_checksum=3544407408

Frame profile (SMC stream):
  frames:              603
  grid/raycast:        2.157 ms/frame
  state packing:       0.000 ms/frame
  smc diff:            0.000 ms/frame
  dirty decision:      0.000 ms/frame
  dirty iteration:     0.020 ms/frame
  rasterization:       0.020 ms/frame
  SDL/update/present:  4.512 ms/frame
  other/unaccounted:   0.000 ms/frame
  total profiled:      5.747 ms/frame
{
  "grid_width": 260,
  "grid_height": 160,
  "target_fps": 120,
  "avg_render_ms": 5.75,
  "worst_render_ms": 13.69,
  "effective_worst_ms": 13.69,
  "outlier_trimmed": false,
  "min_spare_ms": -7.32,
  "frames": 603,
  "result": "fail_performance"
}
```

#### Run 2
```
SMC stream stats: checks=25084800 changed=42515 unchanged=25042285 stores=42515 bytes_compared=175593600 out_of_range=0 clears=0 fallback_count=0
Renderer stats: cells_total=41600 cells_rasterized=42515 cells_skipped=41599 skip_rate=100.0% framebuffer_checksum=2824370914

Frame profile (SMC stream):
  frames:              603
  grid/raycast:        2.285 ms/frame
  state packing:       0.000 ms/frame
  smc diff:            0.000 ms/frame
  dirty decision:      0.000 ms/frame
  dirty iteration:     0.019 ms/frame
  rasterization:       0.019 ms/frame
  SDL/update/present:  4.641 ms/frame
  other/unaccounted:   0.000 ms/frame
  total profiled:      5.866 ms/frame
{
  "grid_width": 260,
  "grid_height": 160,
  "target_fps": 120,
  "avg_render_ms": 5.87,
  "worst_render_ms": 17.22,
  "effective_worst_ms": 17.22,
  "outlier_trimmed": false,
  "min_spare_ms": -10.84,
  "frames": 603,
  "result": "fail_performance"
}
```

#### Run 3
```
SMC stream stats: checks=25001600 changed=42510 unchanged=24959090 stores=42510 bytes_compared=175011200 out_of_range=0 clears=0 fallback_count=0
Renderer stats: cells_total=41600 cells_rasterized=42510 cells_skipped=41597 skip_rate=100.0% framebuffer_checksum=3788748016

Frame profile (SMC stream):
  frames:              601
  grid/raycast:        2.170 ms/frame
  state packing:       0.000 ms/frame
  smc diff:            0.000 ms/frame
  dirty decision:      0.000 ms/frame
  dirty iteration:     0.022 ms/frame
  rasterization:       0.022 ms/frame
  SDL/update/present:  4.506 ms/frame
  other/unaccounted:   0.000 ms/frame
  total profiled:      5.785 ms/frame
{
  "grid_width": 260,
  "grid_height": 160,
  "target_fps": 120,
  "avg_render_ms": 5.79,
  "worst_render_ms": 12.37,
  "effective_worst_ms": 12.37,
  "outlier_trimmed": false,
  "min_spare_ms": -5.83,
  "frames": 601,
  "result": "fail_performance"
}
```

## Summary (Median Values)

| mode | avg_render_ms | raycast_grid_ms | state_pack_ms | smc_diff_ms | smc_stream_diff_ms | dirty_check_ms | dirty_iter_ms | raster_ms | sdl_update_ms | checks | changed | unchanged | stores | bytes_compared | out_of_range | fallback_count | cells_processed | cells_skipped | parse_status | build_status | run_status |
|------|-------------|-----------------|---------------|-------------|-------------------|----------------|-------------|---------|--------------|--------|---------|-----------|--------|---------------|-------------|---------------|----------------|---------------|----------------|--------------|------------|
| baseline | 8.48 | 2.293 | 0.000 | 0.000 | missing | 4.295 | 4.295 | 4.295 | 4.133 | missing | missing | missing | missing | missing | missing | missing | 41600 | 0 | PARSE_PASS | BUILD_PASS | RUN_PASS |
| dirty_cells | 5.14 | 2.188 | 0.000 | 0.000 | missing | 0.502 | 0.502 | 0.502 | 4.622 | missing | missing | missing | missing | missing | missing | missing | 1 | 41599 | PARSE_PASS | BUILD_PASS | RUN_PASS |
| smc_indexed | 6.07 | 2.146 | 0.000 | 0.000 | missing | 1.596 | 1.596 | 1.596 | 4.508 | 24544000 | 42500 | 24501498 | 42500 | 196352000 | 0 | missing | 1 | 41599 | PARSE_PASS | BUILD_PASS | RUN_PASS |
| smc_batch | 5.47 | 2.265 | 0.490 | 0.225 | missing | 0.000 | 0.024 | 0.024 | 4.724 | 25209600 | 42525 | 25167075 | 42525 | 201676800 | 0 | missing | 42525 | 41599 | PARSE_PASS | BUILD_PASS | RUN_PASS |
| smc_stream_opt | 5.17 | 2.260 | 0.000 | 0.000 | missing | 0.000 | 0.019 | 0.019 | 4.585 | 25043200 | 42513 | 25000686 | 42513 | 175302400 | 0 | 0 | 42513 | 41599 | PARSE_PASS | BUILD_PASS | RUN_PASS |
| smc_stream_base | 5.79 | 2.170 | 0.000 | 0.000 | missing | 0.000 | 0.020 | 0.020 | 4.512 | 25084800 | 42514 | 25042285 | 42514 | 175593600 | 0 | 0 | 42514 | 41599 | PARSE_PASS | BUILD_PASS | RUN_PASS |

## Correctness Verification (smc_stream_opt)

- out_of_range: 0
- fallback_count: 0
- bytes_compared: 174137600
- checks * 7: 174137600

**Result: CORRECTNESS_PASS**

## Decision

- Stream optimized median: 5.17 ms
- Packed batch median: 5.47 ms

**Final Verdict: DECISION_PASS (5.17ms <= 5.47ms)**

---
*Raw logs in docs/smc_benchmark_logs/*
