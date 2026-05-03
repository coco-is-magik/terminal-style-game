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

## Run

make run

## Test

make test
make benchmark
make stability

## Clean

make clean
