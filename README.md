# ascii-fps

A C-based ASCII-style first-person game prototype.

## Rendering Strategy Update

### The First Renderer
The initial iteration of the renderer successfully proved that we could represent the world via a logical cell grid and map that out to the screen using SDL3 without relying on any external assets (by embedding an 8x8 bitmap font array directly into the source). 
However, it proved computationally unscalable for high-resolution setups. At a target of 120 FPS and a 260x160 character grid, rendering each cell individually resulted in over 80,000 `SDL_RenderFillRect` and `SDL_RenderTexture` calls per frame. This decimated the 8.33ms performance budget via tremendous API overhead.

### The New Scalable Strategy
The renderer backend has been upgraded to a **software pixel buffer** pattern. 
Instead of repeatedly asking the GPU backend to draw geometry, the CPU pre-calculates the final image state directly into a localized RGBA memory buffer (`uint32_t *pixel_buffer`). 

By unrolling the inner lookup loop for our 8x8 font glyphs and performing fast bitwise assignments directly onto the flat memory structure, we're capable of evaluating all 41,600 cells dynamically on the CPU in a fraction of a millisecond. 
After the grid is processed, the system triggers exactly one `SDL_UpdateTexture` and exactly one `SDL_RenderTexture` call to push the finalized screen data up to the hardware. 

This approach features:
- **Zero per-frame allocations**: Pre-allocated structures. `malloc` and `free` counts are tracked mathematically.
- **Zero texture recreation**: The streaming texture is created once during window initialization.

### Performance Targets
- **Target Resolution**: 260x160 Logical Character Grid
- **Target Framerate**: 120 FPS (8.33ms budget)
- **Ideal Threshold**: `avg_render_ms <= 4ms` & `worst_render_ms <= 6ms`
- **Minimum Acceptable Threshold**: `avg_render_ms <= 6ms` & `worst_render_ms <= 8ms`

## Build

make

### Build Flags

The following build flags control renderer and engine behavior:

| Flag | Description |
|------|-------------|
| `PROFILE_FRAME=1` | Enable per-frame phase timing instrumentation. Prints timing breakdown at benchmark end. |
| `USE_SMC=1` | Build with generated SMC dispatch code. |
| `USE_LIGHTING_CACHE=1` | Enable lighting shadow ray cache. |
| `USE_GLYPH_CACHE=1` | Enable glyph block caching. |
| `USE_DIRTY_CELLS=1` | Renderer-specific custom dirty-cell tracking (reference path). Baseline for correctness. |
| `USE_SMC_STATE_TRACKER=1` | SMC hash-based API. General-purpose/diagnostic use. Not recommended for dense grids. |
| `USE_SMC_INDEXED_STATE_TRACKER=1` | SMC per-cell indexed API. Diagnostic mode for debugging. |
| `USE_SMC_BATCH_STATE_TRACKER=1` | SMC batch indexed mode. Fallback/comparator. |
| `USE_SMC_STREAM_STATE_TRACKER=1` | **Preferred SMC renderer mode**. Uses stream-based diffing to eliminate state packing overhead. |

Note: Only one dirty-tracking mode may be enabled at a time.

### Dirty-Tracking Modes

For optimizing cell rasterization, these modes control how the renderer detects changed cells. The preferred mode is:

| Mode | Description |
|------|-------------|
| `USE_DIRTY_CELLS=1` | Renderer-specific custom dirty-cell tracking (reference path). Baseline for correctness. |
| `USE_SMC_INDEXED_STATE_TRACKER=1` | SMC per-cell indexed API. Diagnostic mode for debugging. |
| `USE_SMC_STATE_TRACKER=1` | SMC hash-based API. General-purpose/diagnostic use. Not recommended for dense grids. |
| `USE_SMC_BATCH_STATE_TRACKER=1` | SMC batch indexed mode. Fallback/comparator. |
| `USE_SMC_STREAM_STATE_TRACKER=1` | **Preferred SMC renderer mode**. Uses stream-based diffing to eliminate state packing overhead. |

Example:
```bash
make clean && make USE_SMC_STREAM_STATE_TRACKER=1 PROFILE_FRAME=1
```

## Run

make run
make run-normal
make run-stress

The `run` target starts the raycast world.  `run-normal` and `run-stress`
start the software-renderer test patterns used for benchmarking.

## Test

make test
make benchmark
make stability

## Clean

make clean

## Dependencies

Run `vendor.sh` once to clone and build the vendored libraries (SDL3,
SDL3_mixer, enet, cmocka) into `vendor/dist/`.  Requires `cmake`, `git`,
and a C compiler.

## Configuration

`config.ini` overrides hard-coded defaults for window size, grid size,
lighting, raycasting, and the debug HUD.  See the comments in that file
for the available keys.

## Asset system

Game data is loaded from `assets/`:

  - `assets/maps/` — numeric digit grids
  - `assets/palettes/` — near/mid/far colour stops
  - `assets/materials/` — palette reference + four distance glyphs
  - `assets/decals/` — surface decals (wall, floor, ceiling)
  - `assets/lights/` — point lights
  - `assets/ui_elements/` — data-driven UI widgets
  - `assets/ui_layouts/` — UI screen composition

See `assets/README.md` for the file format details.

## Editors

The main menu exposes two asset editors:

  - **Asset Designer** (`APP_STATE_ASSET_DESIGNER`) — decal canvas editor
  - **Live Editor** (`APP_STATE_LIVE_EDITOR`) — combined decal/material
    editor with a live raycast preview
