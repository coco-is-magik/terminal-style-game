# SMC Stream State Tracker Integration Plan

## Objective

Integrate SMC's `smc_state_diff_indexed_streams()` API as a new renderer state-tracking mode (`USE_SMC_STREAM_STATE_TRACKER=1`). This will eliminate the ~0.48 ms/frame state-packing overhead currently measured in packed batch mode, potentially making SMC-based dirty tracking faster than the custom implementation.

---

## 1. Cell Layout Verification (Confirmed)

Verified layout for `Cell` struct (compiler: gcc with default packing):

| Field | Offset | Size | Contiguity |
|-------|--------|------|------------|
| glyph | 0 | 1 byte | N/A |
| fg.r | 1 | 1 byte | contiguous with fg.g, fg.b |
| fg.g | 2 | 1 byte | contiguous |
| fg.b | 3 | 1 byte | contiguous |
| fg.a | 4 | 1 byte | excluded (alpha ignored in packing) |
| bg.r | 5 | 1 byte | contiguous with bg.g, bg.b |
| bg.g | 6 | 1 byte | contiguous |
| bg.b | 7 | 1 byte | contiguous |
| sizeof(Cell) | - | **9 bytes** | no trailing padding |

This confirms the `[1,3,3]` stream signature is directly supported.

---

## 2. File-by-File Integration Plan

### 2.1 `vendor/src/smc/` (update to new commit)

**Update target**: Commit `3783ae9` (or later with stream API)

**Verification required after update**:
- `include/smc.h` declares `smc_state_stream_t`
- `include/smc.h` declares `smc_state_diff_indexed_streams()`
- `src/c/smc_state.c` contains optimized stream kernels
- `src/c/smc_state.c` contains `SMC_DISABLE_OPTIMIZED_STREAM_KERNELS` guard
- `src/c/smc_runtime_stub.c` contains `smc_state_diff_indexed_streams()` wrapper

### 2.2 `Makefile`

**Changes proposed**:

```makefile
USE_SMC_STREAM_STATE_TRACKER ?= 0

# Update mutual exclusivity check
ifneq ($(shell expr $(USE_DIRTY_CELLS) + $(USE_SMC_STATE_TRACKER) + $(USE_SMC_INDEXED_STATE_TRACKER) + $(USE_SMC_BATCH_STATE_TRACKER) + $(USE_SMC_STREAM_STATE_TRACKER)),0)
  ifneq ($(shell expr ...),1)
    $(error Only one of USE_DIRTY_CELLS, USE_SMC_STATE_TRACKER, USE_SMC_INDEXED_STATE_TRACKER, USE_SMC_BATCH_STATE_TRACKER, USE_SMC_STREAM_STATE_TRACKER may be set)
  endif
endif

# Add explicit SMC_CFLAGS for optimization disabling
SMC_CFLAGS ?=

ifeq ($(USE_SMC_STREAM_STATE_TRACKER),1)
  SMC_DIR := vendor/src/smc
  SMC_INCLUDES := -I"$(SMC_DIR)/include" -I"$(SMC_DIR)/src/c"
  SMC_STREAM_DEFS := -DUSE_SMC_STREAM_STATE_TRACKER=1
  SMC_STREAM_LIBS := -lm
  SMC_STREAM_FILES := $(SMC_DIR)/src/c/smc_runtime_stub.c $(SMC_DIR)/src/c/smc_artifact.c $(SMC_DIR)/src/c/smc_state.c
endif
```

### 2.3 `src/smc_indexed_state_tracker.h`

```c
int smc_indexed_state_tracker_diff_streams(
    const smc_state_stream_t *streams,
    size_t stream_count,
    size_t record_count,
    uint32_t *dirty_indices,
    size_t dirty_capacity,
    size_t *out_dirty_count);
```

### 2.4 `src/smc_indexed_state_tracker.c`

```c
static smc_context_t *g_smc_indexed_ctx = NULL;

int smc_indexed_state_tracker_init(size_t cell_count) {
    // ... context creation ...
    
    smc_state_indexed_config_t cfg;
    cfg.count = cell_count;
    cfg.memory_budget_bytes = 0;
    
#if defined(USE_SMC_STREAM_STATE_TRACKER)
    cfg.state_size = 7;  // 1 + 3 + 3
#else
    cfg.state_size = sizeof(CellState);  // 8 bytes
#endif
    
    // ... rest of init ...
}

#ifdef USE_SMC_STREAM_STATE_TRACKER
int smc_indexed_state_tracker_diff_streams(...) {
    if (!g_smc_indexed_ctx || !out_dirty_count) return SMC_ERR_INVALID;
    return smc_state_diff_indexed_streams(g_smc_indexed_ctx, streams, stream_count,
                                          record_count, dirty_indices, dirty_capacity,
                                          out_dirty_count);
}
#endif
```

### 2.5 `src/renderer.h`

```c
#if defined(USE_SMC_BATCH_STATE_TRACKER)
     void           *batch_state_buffer;  /* CellState[cell_count] for batch mode */
#endif
#if defined(USE_SMC_BATCH_STATE_TRACKER) || defined(USE_SMC_STREAM_STATE_TRACKER)
     uint32_t       *batch_dirty_indices; /* dirty_indices[cell_count] */
     size_t         batch_buffer_size;
#endif
```

### 2.6 `src/renderer.c`

**Add compile-time C99 assertions**:
```c
#define CASSERT(name, expr) typedef char name[(expr) ? 1 : -1]
#include <stddef.h>

CASSERT(cell_glyph_offset_0, offsetof(Cell, glyph) == 0);
CASSERT(cell_fg_offset_1, offsetof(Cell, fg) == 1);
CASSERT(cell_bg_offset_5, offsetof(Cell, bg) == 5);
CASSERT(cell_size_9, sizeof(Cell) == 9);
CASSERT(color_r_offset_0, offsetof(SDL_Color, r) == 0);
CASSERT(color_g_offset_1, offsetof(SDL_Color, g) == 1);
CASSERT(color_b_offset_2, offsetof(SDL_Color, b) == 2);
```

**Add global fallback counter** (define exactly once):
```c
uint64_t renderer_smc_fallback_count = 0;
```

**Shared dirty-index rendering helper**:
```c
static void render_dirty_indices(Renderer *ren, Grid *grid, 
                                uint32_t *dirty_indices, size_t dirty_count) {
    // Unified implementation for batch and stream modes
}
```

**Stream mode path in renderer_draw()**:
```c
#elif defined(USE_SMC_STREAM_STATE_TRACKER)
    size_t cell_count = (size_t)grid->width * (size_t)grid->height;
    size_t dirty_count = 0;

#if PROFILE_FRAME
    profile_phase_start = profile_now_ms();
#endif

    if (cell_count > 0) {
        smc_state_stream_t streams[3] = {
            { &grid->cells[0].glyph, sizeof(Cell), 1 },
            { &grid->cells[0].fg.r, sizeof(Cell), 3 },
            { &grid->cells[0].bg.r, sizeof(Cell), 3 },
        };
        
        int rc = smc_indexed_state_tracker_diff_streams(streams, 3, cell_count,
            ren->batch_dirty_indices, cell_count, &dirty_count);
        
        if (rc != SMC_OK) {
            for (size_t i = 0; i < cell_count; i++) {
                ren->batch_dirty_indices[i] = (uint32_t)i;
            }
            dirty_count = cell_count;
            renderer_smc_fallback_count++;
        }
    }
    
#if PROFILE_FRAME
    profile_phase_end = profile_now_ms();
    g_frame_profile.smc_stream_diff_ms += (profile_phase_end - profile_phase_start);
#endif
    
    renderer_cells_skipped = cell_count - dirty_count;
    render_dirty_indices(ren, grid, ren->batch_dirty_indices, dirty_count);
```

### 2.7 `src/timing.h`

Add `smc_stream_diff_ms` field alongside existing `smc_diff_ms`.

### 2.8 `src/app.c`

Add stream mode initialization and stats reporting. Reset `renderer_smc_fallback_count` at benchmark start.

---

## 3. Correctness Verification Requirements

| Check | Expected |
|-------|----------|
| state_size | 7 |
| stream_count | 3 (`[1,3,3]`) |
| record_count | cell_count |
| dirty_capacity | cell_count |
| bytes_compared | checks * 7 |
| out_of_range | 0 |
| fallback_count | 0 |
| dirty_count | matches packed batch |

---

## 4. Benchmark Procedure

```bash
# Baseline
make clean && make PROFILE_FRAME=1
./build/ascii-fps --benchmark-raycast 5

# Custom dirty cells
make clean && make USE_DIRTY_CELLS=1 PROFILE_FRAME=1
./build/ascii-fps --benchmark-raycast 5

# SMC packed batch
make clean && make USE_SMC_BATCH_STATE_TRACKER=1 PROFILE_FRAME=1
./build/ascii-fps --benchmark-raycast 5

# SMC stream (optimized)
make clean && make USE_SMC_STREAM_STATE_TRACKER=1 PROFILE_FRAME=1
./build/ascii-fps --benchmark-raycast 5

# SMC stream (baseline - verify SMC_CFLAGS propagation)
make clean && make USE_SMC_STREAM_STATE_TRACKER=1 PROFILE_FRAME=1 SMC_CFLAGS+=-DSMC_DISABLE_OPTIMIZED_STREAM_KERNELS V=1
# Confirm vendor/src/smc/src/c/smc_state.c compile command includes -DSMC_DISABLE_OPTIMIZED_STREAM_KERNELS
./build/ascii-fps --benchmark-raycast 5
```

Run 3 passes per mode, report medians.

---

## 5. Decision Rules

**Stream mode is successful if:**
- SMC stream frame time ≤ SMC packed batch frame time
- dirty_count matches packed batch
- out_of_range = 0
- fallback_count = 0
- No correctness regressions

**Stream mode is inconclusive if:**
- Microbenchmark wins but real frame time flat due to rasterization dominating

**Stream mode should remain non-default if:**
- Stream mode consistently > +0.1 ms/frame slower than packed batch
- OR out_of_range > 0
- OR fallback_count > 0

**Even if stream wins: Do not remove packed batch.**

---

## 6. Final Verification Questions

**What would prove SMC stream mode is correct?**
- `out_of_range == 0`
- `bytes_compared == checks * 7`
- `fallback_count == 0`
- `dirty_count` matches packed batch

**What would prove it beats packed batch in the renderer?**
- Median avg_render_ms(stream) < median avg_render_ms(packed batch)
- state_pack_ms = 0.0
- Frame time improvement ≥ 0.1 ms

**What would prove it should become the default SMC renderer mode?**
- avg_render_ms(stream) ≤ avg_render_ms(custom dirty)
- Consistent across stress/raycast benchmarks
- No correctness regressions

**What would prove packed batch should remain preferred?**
- Stream consistently > +0.1 ms/frame than packed batch
- OR out_of_range > 0
- OR fallback_count > 0