/**
 * renderer.c — SDL3 window management and software glyph rasterizer
 *
 * This file implements the display layer of the engine.  It manages the
 * SDL window, SDL renderer, screen texture, and glyph atlas, and performs
 * the final rasterization step: converting the abstract Grid (characters +
 * colours) into actual pixels on the screen.
 *
 * The rendering pipeline works in two stages:
 *
 *   1. Software rasterization (CPU):
 *      renderer_draw() iterates over every cell in the Grid and directly
 *      writes 8×8 pixel blocks into a heap-allocated uint32_t pixel buffer.
 *      For each cell it reads the monochrome font bitmap from font8x8_basic,
 *      selects either fg or bg colour per pixel, and writes the result.
 *      This completely avoids per-cell SDL API calls.
 *
 *   2. GPU blit (one call per frame):
 *      The finished pixel buffer is uploaded to an SDL_Texture via
 *      SDL_UpdateTexture(), then the texture is rendered to the screen
 *      with SDL_RenderTexture() and presented with SDL_RenderPresent().
 *
 * Instrumentation:
 *   Three global counters track allocations and texture creation so the
 *   main loop in app.c can detect per-frame allocation bugs:
 *     renderer_alloc_count          — incremented on every malloc (via ren_malloc)
 *     renderer_free_count           — incremented on every free   (via ren_free)
 *     renderer_texture_create_count — incremented on SDL texture creation
 *
 * Logical presentation:
 *   The renderer uses SDL's logical presentation mode (letterboxing) to
 *   scale the fixed-size logical rendering area up to the actual window
 *   size while maintaining the correct aspect ratio.  The logical size is
 *   (grid_width × cell_width) × (grid_height × cell_height).
 */

#include "renderer.h"        /* Renderer struct, function declarations */
#include "smc_state_tracker.h" /* SMC v2 dirty-state tracking (conditional) */
#include "smc_indexed_state_tracker.h" /* SMC v2.1 indexed state tracking */
#if defined(USE_SMC_STATE_TRACKER) || defined(USE_SMC_INDEXED_STATE_TRACKER) || defined(USE_SMC_BATCH_STATE_TRACKER) || defined(USE_SMC_STREAM_STATE_TRACKER)
#include "smc.h"           /* SMC types (SMC_OK, etc.) */
#endif

/* Mutually exclusive dirty/state tracking modes */
#if (defined(USE_DIRTY_CELLS) + defined(USE_SMC_STATE_TRACKER) + defined(USE_SMC_INDEXED_STATE_TRACKER) + defined(USE_SMC_BATCH_STATE_TRACKER) + defined(USE_SMC_STREAM_STATE_TRACKER)) > 1
#error "Only one of USE_DIRTY_CELLS, USE_SMC_STATE_TRACKER, USE_SMC_INDEXED_STATE_TRACKER, USE_SMC_BATCH_STATE_TRACKER, USE_SMC_STREAM_STATE_TRACKER may be defined"
#endif
#include <stdio.h>            /* fprintf(), stderr */
#include <stdlib.h>           /* malloc(), free(), size_t */
#include <string.h>           /* memset() */
#include <stddef.h>           /* offsetof */

/* C99 compile-time assertions for Cell layout used by stream mode */
#define CASSERT(name, expr) typedef char name[(expr) ? 1 : -1]
CASSERT(cell_glyph_offset_0, offsetof(Cell, glyph) == 0);
CASSERT(cell_fg_offset_1, offsetof(Cell, fg) == 1);
CASSERT(cell_bg_offset_5, offsetof(Cell, bg) == 5);
CASSERT(cell_size_9, sizeof(Cell) == 9);
CASSERT(color_r_offset_0, offsetof(SDL_Color, r) == 0);
CASSERT(color_g_offset_1, offsetof(SDL_Color, g) == 1);
CASSERT(color_b_offset_2, offsetof(SDL_Color, b) == 2);

/* ===================================================================
 *  Global allocation-tracking counters
 *  These are declared 'extern' in renderer.h so other files can read them.
 * =================================================================== */

int renderer_alloc_count          = 0;   /* Number of heap allocations */
int renderer_free_count           = 0;   /* Number of heap frees */
int renderer_texture_create_count = 0;   /* Number of SDL textures created */

/* Profiling counters */
double renderer_time_raster_ms    = 0.0; /* Time spent compositing cells */
double renderer_time_upload_ms    = 0.0; /* Time spent in SDL_UpdateTexture */
double renderer_time_present_ms   = 0.0; /* Time spent in render/present */
uint64_t renderer_cells_processed = 0;  /* Total cells rasterized */
uint64_t renderer_cells_skipped   = 0;  /* Cells skipped via dirty tracking */
uint64_t renderer_cache_hits      = 0;  /* Glyph cache hits */
uint64_t renderer_cache_misses    = 0;  /* Glyph cache misses */
uint64_t renderer_smc_fallback_count = 0; /* SMC stream fallback count */

#if PROFILE_FRAME
FrameProfileStats g_frame_profile = {0}; /* Accumulated frame phase timings */
#endif

/* ===================================================================
 *  Allocation wrappers (for tracking)
 * =================================================================== */

/**
 * ren_malloc() — Tracked malloc wrapper
 *
 * Calls malloc() and increments the allocation counter.  Using these
 * wrappers throughout the renderer subsystem allows app.c's main loop
 * to detect per-frame heap allocations (which would indicate a bug).
 *
 * @param size  Number of bytes to allocate
 * @return      Pointer to the allocated memory, or NULL on failure
 */
static void* ren_malloc(size_t size) {
    renderer_alloc_count++;
    return malloc(size);
}

/**
 * ren_free() — Tracked free wrapper
 *
 * Calls free() (only if ptr is non-NULL) and increments the free counter.
 *
 * @param ptr  Pointer to memory to free (NULL-safe)
 */
static void ren_free(void* ptr) {
    if (ptr) {
        renderer_free_count++;
        free(ptr);
    }
}

/* ===================================================================
 *  Colour packing helper (endian-aware)
 * =================================================================== */

/**
 * COLOR_TO_UINT32 — Pack an SDL_Color into a uint32_t pixel value
 *
 * SDL_PIXELFORMAT_RGBA32 expects pixels in a specific byte order depending
 * on the CPU architecture:
 *   - Little-endian (x86, ARM):   the format is effectively BGRA in memory
 *     because RGBA32 means R is in the least significant byte on little-endian.
 *     So we pack as A|B|G|R → (a << 24) | (b << 16) | (g << 8) | r
 *   - Big-endian:                  the format is RGBA in memory, so we pack
 *     as R|G|B|A → (r << 24) | (g << 16) | (b << 8) | a
 *
 * This macro is used per-cell in renderer_draw() to set pixel values.
 */
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
    #define COLOR_TO_UINT32(c) (((c).r << 24) | ((c).g << 16) | ((c).b << 8) | (c).a)
#else
    #define COLOR_TO_UINT32(c) (((c).a << 24) | ((c).b << 16) | ((c).g << 8) | (c).r)
#endif

/* ===================================================================
 *  Embedded font bitmap reference
 * =================================================================== */

/* Access the built-in 8×8 monochrome font defined in font8x8.h.
 * This is the same data used to build the glyph atlas, but we reference
 * it here again for the software pixel-buffer path.  Only ASCII codes
 * 0–127 are valid; codes >= 128 are forced to 32 (space). */
extern const unsigned char font8x8_basic[128][8];

/* ===================================================================
 *  Renderer creation
 * =================================================================== */

/**
 * renderer_create() — Create the SDL window, renderer, textures, and atlas
 *
 * This is the main initialisation function for the display subsystem.
 * It performs the following steps in order:
 *
 *   1. Allocate the Renderer struct (tracked)
 *   2. Compute logical dimensions: (grid_w × cell_w) × (grid_h × cell_h)
 *   3. Allocate the software pixel buffer (width × height × 4 bytes)
 *   4. Initialise SDL video subsystem
 *   5. Create the SDL window (resizable, initial size win_w × win_h)
 *   6. Create the SDL renderer
 *   7. Enable logical presentation (letterbox) at the logical resolution
 *   8. Create a streaming SDL_Texture for the pixel buffer upload
 *   9. Build the glyph atlas texture for potential use (though the
 *      software rasterizer uses the raw font data directly)
 *
 * If any step fails, all previously allocated resources are freed and
 * NULL is returned (clean failure).
 *
 * @param win_w   Initial window width in screen pixels
 * @param win_h   Initial window height in screen pixels
 * @param grid_w  Number of glyph columns in the grid
 * @param grid_h  Number of glyph rows in the grid
 * @param cell_w  Width of each glyph in pixels (typically 8)
 * @param cell_h  Height of each glyph in pixels (typically 8)
 * @return        Pointer to the new Renderer, or NULL on failure
 */
Renderer* renderer_create(int win_w, int win_h, int grid_w, int grid_h, int cell_w, int cell_h) {
    /* Reject invalid grid/cell dimensions */
    if (grid_w <= 0 || grid_h <= 0 || cell_w <= 0 || cell_h <= 0) return NULL;

    /* --- Step 1: Allocate the Renderer struct --- */
    Renderer *ren = ren_malloc(sizeof(Renderer));
    if (!ren) return NULL;

    ren->cell_w = cell_w;
    ren->cell_h = cell_h;
    ren->logical_w = grid_w * cell_w;       /* Logical width in pixels */
    ren->logical_h = grid_h * cell_h;       /* Logical height in pixels */
    ren->atlas = NULL;

    /* --- Step 3: Allocate the software pixel buffer --- */
    /* One uint32_t per pixel in the logical area.  This buffer is written
     * to every frame by renderer_draw() and then uploaded to the GPU. */
    ren->pixel_buffer = ren_malloc(ren->logical_w * ren->logical_h * sizeof(uint32_t));
    if (!ren->pixel_buffer) {
        ren_free(ren);
        return NULL;
    }

    /* --- Step 4: Initialise SDL video subsystem --- */
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        ren_free(ren->pixel_buffer);
        ren_free(ren);
        return NULL;
    }

    /* --- Step 5: Create the window --- */
    ren->window = SDL_CreateWindow("ASCII FPS Prototype", win_w, win_h, SDL_WINDOW_RESIZABLE);
    if (!ren->window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        ren_free(ren->pixel_buffer);
        ren_free(ren);
        return NULL;
    }

    /* --- Step 6: Create the SDL renderer --- */
    ren->sdl_ren = SDL_CreateRenderer(ren->window, NULL);
    if (!ren->sdl_ren) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(ren->window);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        ren_free(ren->pixel_buffer);
        ren_free(ren);
        return NULL;
    }

    /* --- Step 7: Set up logical presentation (letterboxing) --- */
    /* This tells SDL to scale the logical rendering area (logical_w ×
     * logical_h) to fit the window while preserving aspect ratio, with
     * black bars if necessary. */
    SDL_SetRenderLogicalPresentation(ren->sdl_ren, ren->logical_w, ren->logical_h,
                                     SDL_LOGICAL_PRESENTATION_LETTERBOX);

    /* --- Step 8: Create the streaming screen texture --- */
    /* This texture is the GPU-side destination for the pixel buffer.
     * We use STREAMING access because we update it every frame. */
    ren->screen_texture = SDL_CreateTexture(ren->sdl_ren, SDL_PIXELFORMAT_RGBA32,
                                            SDL_TEXTUREACCESS_STREAMING,
                                            ren->logical_w, ren->logical_h);
    if (!ren->screen_texture) {
        fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
        SDL_DestroyRenderer(ren->sdl_ren);
        SDL_DestroyWindow(ren->window);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        ren_free(ren->pixel_buffer);
        ren_free(ren);
        return NULL;
    }
    renderer_texture_create_count++;     /* Track this texture creation */

    /* --- Step 9: Build the glyph atlas (for future GPU-accelerated path) --- */
    /* Currently the software rasterizer in renderer_draw() uses the raw
     * font8x8_basic data directly, bypassing the atlas.  The atlas exists
     * for a potential future GPU-accelerated rendering path. */
    ren->atlas = glyph_atlas_create_builtin(ren->sdl_ren);
    if (!ren->atlas) {
        fprintf(stderr, "glyph_atlas_create_builtin failed\n");
        SDL_DestroyTexture(ren->screen_texture);
        SDL_DestroyRenderer(ren->sdl_ren);
        SDL_DestroyWindow(ren->window);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        ren_free(ren->pixel_buffer);
        ren_free(ren);
        return NULL;
    }

#if defined(USE_SMC_BATCH_STATE_TRACKER) || defined(USE_SMC_STREAM_STATE_TRACKER)
    /* Allocate batch/stream mode buffers after successful atlas creation */
    size_t cell_count = (size_t)grid_w * (size_t)grid_h;
    size_t state_buffer_bytes, dirty_indices_bytes;
    smc_indexed_state_tracker_get_buffer_sizes(cell_count,
                                                &state_buffer_bytes,
                                                &dirty_indices_bytes);
#ifdef USE_SMC_BATCH_STATE_TRACKER
    ren->batch_state_buffer = ren_malloc(state_buffer_bytes);
    if (!ren->batch_state_buffer) {
        fprintf(stderr, "renderer_create: failed to allocate batch state buffer\n");
        glyph_atlas_destroy(ren->atlas);
        SDL_DestroyTexture(ren->screen_texture);
        SDL_DestroyRenderer(ren->sdl_ren);
        SDL_DestroyWindow(ren->window);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        ren_free(ren->pixel_buffer);
        ren_free(ren);
        return NULL;
    }
#endif
    ren->batch_dirty_indices = ren_malloc(dirty_indices_bytes);
    if (!ren->batch_dirty_indices) {
        fprintf(stderr, "renderer_create: failed to allocate dirty indices buffer\n");
#ifdef USE_SMC_BATCH_STATE_TRACKER
        ren_free(ren->batch_state_buffer);
#endif
        glyph_atlas_destroy(ren->atlas);
        SDL_DestroyTexture(ren->screen_texture);
        SDL_DestroyRenderer(ren->sdl_ren);
        SDL_DestroyWindow(ren->window);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        ren_free(ren->pixel_buffer);
        ren_free(ren);
        return NULL;
    }
    ren->batch_buffer_size = cell_count;
#endif

    /* Enable SDL text input so SDL_EVENT_TEXT_INPUT fires for all windows.
     * This is required for the Asset Designer save-prompt to receive typed
     * characters.  We leave it enabled globally — consuming code (asset_designer.c)
     * reads text_input only when in AD_SAVE_PROMPT mode. */
    SDL_StartTextInput(ren->window);

    return ren;
}

/* ===================================================================
 *  Renderer destruction
 * =================================================================== */

/**
 * renderer_destroy() — Free all renderer resources (reverse of create)
 *
 * Destroys in order: atlas → screen_texture → SDL_renderer → window →
 * pixel_buffer → Renderer struct.  Also quits the SDL video subsystem.
 * Safe to call with NULL (no-op).
 *
 * @param ren  Renderer to destroy (NULL-safe)
 */
void renderer_destroy(Renderer *ren) {
    if (!ren) return;
    if (ren->atlas)          glyph_atlas_destroy(ren->atlas);
    if (ren->screen_texture) SDL_DestroyTexture(ren->screen_texture);
    if (ren->sdl_ren)        SDL_DestroyRenderer(ren->sdl_ren);
    if (ren->window)         SDL_DestroyWindow(ren->window);
    if (ren->pixel_buffer)   ren_free(ren->pixel_buffer);
#ifdef USE_SMC_BATCH_STATE_TRACKER
    if (ren->batch_state_buffer)  ren_free(ren->batch_state_buffer);
#endif
#if defined(USE_SMC_BATCH_STATE_TRACKER) || defined(USE_SMC_STREAM_STATE_TRACKER)
    if (ren->batch_dirty_indices) ren_free(ren->batch_dirty_indices);
#endif
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    ren_free(ren);
}

/* ===================================================================
 *  Frame rendering — software rasterizer
 * =================================================================== */

/**
 * renderer_draw() — Rasterize the Grid into the pixel buffer and present it
 *
 * This is the hot-path rendering function, called once per frame.  It
 * works in two phases:
 *
 *   Phase 1 — Software rasterization (CPU):
 *     Iterates over every cell in the Grid.  For each cell:
 *       - Reads the 8×8 monochrome font bitmap from font8x8_basic[]
 *         (valid glyph codes 0–127; anything >= 128 becomes space/32)
 *       - For each of the 8 rows, checks the 8 bits of the row byte:
 *           bit = 1 → set pixel to foreground colour
 *           bit = 0 → set pixel to background colour
 *       - Writes directly into the pixel buffer at the correct position
 *     This is unrolled for the inner 8-pixel loop for performance.
 *
 *   Phase 2 — GPU upload & present:
 *     - Uploads the pixel buffer to the screen texture via SDL_UpdateTexture()
 *     - Clears the renderer, draws the texture, and presents it.
 *     This is the only SDL upload/present sequence per frame; there are no
 *     per-cell SDL rendering calls.
 *
 * The current renderer interprets bit 0 as output pixel 0, bit 1 as output
 * pixel 1, and so on.  This matches glyph_atlas_create_builtin().
 *
 * @param ren   The Renderer (provides pixel buffer, SDL assets)
 * @param grid  The Grid containing the cells to render
 */
void renderer_draw(Renderer *ren, Grid *grid) {
    uint32_t *pixels = ren->pixel_buffer;
    int pitch_pixels = ren->logical_w;   /* Number of uint32_t per row */

#if PROFILE_FRAME
    double profile_frame_start = profile_now_ms();
    double profile_phase_start = profile_frame_start;
    double profile_phase_end;
#endif

    /* ---- Phase 1 Timing ---- */
    uint64_t phase1_start = SDL_GetPerformanceCounter();

#ifdef USE_GLYPH_CACHE
    renderer_cache_hits = 0;
    renderer_cache_misses = 0;
#endif

    /* ---- Phase 1: Software rasterization ---- */
#if defined(USE_SMC_BATCH_STATE_TRACKER) || defined(USE_SMC_STREAM_STATE_TRACKER)
    /* Batch/stream mode: call SMC once, render only dirty cells */
    size_t cell_count = (size_t)grid->width * (size_t)grid->height;
    size_t dirty_count = 0;
    uint32_t *dirty_indices = ren->batch_dirty_indices;
    int rc = SMC_OK;

#if PROFILE_FRAME
    profile_phase_start = profile_now_ms();
#endif

#ifdef USE_SMC_BATCH_STATE_TRACKER
    _Static_assert(sizeof(CellState) == 8, "CellState must be exactly 8 bytes for SMC fixed-size batch kernel");
    /* Populate state buffer with packed deterministic 8-byte states */
    CellState *states = (CellState *)ren->batch_state_buffer;
    for (size_t i = 0; i < cell_count; i++) {
        states[i] = pack_cell_state(&grid->cells[i]);
    }
#if PROFILE_FRAME
    profile_phase_end = profile_now_ms();
    g_frame_profile.state_pack_ms += (profile_phase_end - profile_phase_start);
    profile_phase_start = profile_phase_end;
#endif
    rc = smc_indexed_state_tracker_diff_batch(states, cell_count,
                                               dirty_indices, cell_count,
                                               &dirty_count);
#else
    /* Stream mode: feed grid fields directly to SMC without packing */
    if (cell_count > 0) {
        smc_state_stream_t streams[3] = {
            { &grid->cells[0].glyph, sizeof(Cell), 1 },
            { &grid->cells[0].fg.r,  sizeof(Cell), 3 },
            { &grid->cells[0].bg.r,  sizeof(Cell), 3 },
        };
        rc = smc_indexed_state_tracker_diff_streams(streams, 3, cell_count,
                                                     dirty_indices, cell_count,
                                                     &dirty_count);
    }
#if PROFILE_FRAME
    profile_phase_end = profile_now_ms();
    g_frame_profile.state_pack_ms += 0.0; /* No packing in stream mode */
    profile_phase_start = profile_phase_end;
#endif
#endif

#if PROFILE_FRAME
    profile_phase_end = profile_now_ms();
    g_frame_profile.smc_diff_ms += (profile_phase_end - profile_phase_start);
    profile_phase_start = profile_phase_end;
#endif
    
    /* If SMC call failed, render all cells (fallback) */
    if (rc != SMC_OK) {
        for (size_t i = 0; i < cell_count; i++) {
            dirty_indices[i] = (uint32_t)i;
        }
        dirty_count = cell_count;
        renderer_smc_fallback_count++;
    }
    
    /* Count skipped cells */
    renderer_cells_skipped = cell_count - dirty_count;
    
    /* Render only dirty cells */
    for (size_t d = 0; d < dirty_count; d++) {
        uint32_t idx = dirty_indices[d];
        int cy = (int)(idx / grid->width);
        int cx = (int)(idx % grid->width);
        Cell c = grid->cells[idx];
        renderer_cells_processed++;
        
        /* Pack foreground and background colours into uint32_t once */
        uint32_t fg = COLOR_TO_UINT32(c.fg);
        uint32_t bg = COLOR_TO_UINT32(c.bg);
        
        /* Look up the 8×8 font bitmap for this glyph. */
        const uint8_t *glyph_data = font8x8_basic[c.glyph < 128 ? c.glyph : 32];
        
        /* Top-left pixel of this cell's 8×8 block in the pixel buffer */
        int base_x = cx * 8;
        int base_y = cy * 8;
        
        /* Rasterize the 8 rows of this glyph */
        for (int y = 0; y < 8; y++) {
            uint8_t row = glyph_data[y];
            uint32_t *row_pixels = &pixels[(base_y + y) * pitch_pixels + base_x];
            row_pixels[0] = (row & 1)   ? fg : bg;
            row_pixels[1] = (row & 2)   ? fg : bg;
            row_pixels[2] = (row & 4)   ? fg : bg;
            row_pixels[3] = (row & 8)   ? fg : bg;
            row_pixels[4] = (row & 16)  ? fg : bg;
            row_pixels[5] = (row & 32)  ? fg : bg;
            row_pixels[6] = (row & 64)  ? fg : bg;
            row_pixels[7] = (row & 128) ? fg : bg;
        }
    }
#if PROFILE_FRAME
    profile_phase_end = profile_now_ms();
    g_frame_profile.dirty_iter_ms += (profile_phase_end - profile_phase_start);
    g_frame_profile.raster_ms += (profile_phase_end - profile_phase_start);
    profile_phase_start = profile_phase_end;
#endif
#else
    /* Per-cell or no dirty tracking modes */
    renderer_cells_processed = 0;
    renderer_cells_skipped = 0;

#if PROFILE_FRAME
    profile_phase_start = profile_now_ms();
#endif
    for (int cy = 0; cy < grid->height; cy++) {
        for (int cx = 0; cx < grid->width; cx++) {
            Cell c = grid->cells[cy * grid->width + cx];

#ifdef USE_DIRTY_CELLS
            /* Compare with previous frame - skip if unchanged */
            int idx = cy * grid->width + cx;
            Cell prev = grid->prev_cells[idx];
            
            if (c.glyph == prev.glyph &&
                c.fg.r == prev.fg.r && c.fg.g == prev.fg.g && c.fg.b == prev.fg.b &&
                c.bg.r == prev.bg.r && c.bg.g == prev.bg.g && c.bg.b == prev.bg.b) {
                renderer_cells_skipped++;
                continue;  /* Cell unchanged, skip rasterization */
            }
#endif

#ifdef USE_SMC_INDEXED_STATE_TRACKER
            /* Check state change via SMC indexed state tracker (no hash overhead) */
            uint32_t cell_index = (uint32_t)(cy * grid->width + cx);
            CellState state = pack_cell_state(&c);
            int changed = 1;
            int rc = smc_indexed_state_tracker_cell_changed(cell_index, &state, &changed);
            if (rc == SMC_OK && !changed) {
                renderer_cells_skipped++;
                continue;  /* Cell unchanged, skip rasterization */
            }
#endif

#ifdef USE_SMC_STATE_TRACKER
            /* Check state change via SMC v2 dirty-state tracker */
            uint32_t cell_index = (uint32_t)(cy * grid->width + cx);
            CellState state = pack_cell_state(&c);
            int changed = 1;
            int rc = smc_state_tracker_cell_changed(cell_index, &state, &changed);
            if (rc == SMC_OK && !changed) {
                renderer_cells_skipped++;
                continue;  /* Cell unchanged, skip rasterization */
            }
#endif
            renderer_cells_processed++;

            /* Pack foreground and background colours into uint32_t once */
            uint32_t fg = COLOR_TO_UINT32(c.fg);
            uint32_t bg = COLOR_TO_UINT32(c.bg);

            /* Look up the 8×8 font bitmap for this glyph.
             * Valid range is 0–127; anything outside becomes space (32). */
            const uint8_t *glyph_data = font8x8_basic[c.glyph < 128 ? c.glyph : 32];

            /* Top-left pixel of this cell's 8×8 block in the pixel buffer */
            int base_x = cx * 8;
            int base_y = cy * 8;

            /* Rasterize the 8 rows of this glyph */
            for (int y = 0; y < 8; y++) {
                uint8_t row = glyph_data[y];   /* 8-bit mask for this row */
                uint32_t *row_pixels = &pixels[(base_y + y) * pitch_pixels + base_x];

                /* Unrolled inner loop — check each of the 8 bits:
                 *   bit 0 (value 1)   → row_pixels[0]
                 *   bit 1 (value 2)   → row_pixels[1]
                 *   ...
                 *   bit 7 (value 128) → row_pixels[7]
                 *
                 * For each set bit we write fg; for clear bits we write bg. */
                row_pixels[0] = (row & 1)   ? fg : bg;
                row_pixels[1] = (row & 2)   ? fg : bg;
                row_pixels[2] = (row & 4)   ? fg : bg;
                row_pixels[3] = (row & 8)   ? fg : bg;
                row_pixels[4] = (row & 16)  ? fg : bg;
                row_pixels[5] = (row & 32)  ? fg : bg;
                row_pixels[6] = (row & 64)  ? fg : bg;
                row_pixels[7] = (row & 128) ? fg : bg;
            }
        }
    }
#if PROFILE_FRAME
    profile_phase_end = profile_now_ms();
    g_frame_profile.dirty_check_ms += (profile_phase_end - profile_phase_start);
    g_frame_profile.dirty_iter_ms += (profile_phase_end - profile_phase_start);
    g_frame_profile.raster_ms += (profile_phase_end - profile_phase_start);
    profile_phase_start = profile_phase_end;
#endif
#endif

    /* ---- Phase 2 Timing ---- */
    uint64_t phase2_start = SDL_GetPerformanceCounter();
    double raster_time = (double)((phase2_start - phase1_start) * 1000) / SDL_GetPerformanceFrequency();
    renderer_time_raster_ms = raster_time;

    /* ---- Phase 2: GPU upload and presentation ---- */
    /* Upload the entire pixel buffer to the streaming texture.
     * pitch_pixels * sizeof(uint32_t) = bytes per row. */
    SDL_UpdateTexture(ren->screen_texture, NULL, pixels, pitch_pixels * sizeof(uint32_t));

    /* Clear the renderer (fills with the clear colour, typically black),
     * then copy the texture to the render target, and finally present. */
    SDL_RenderClear(ren->sdl_ren);
    SDL_RenderTexture(ren->sdl_ren, ren->screen_texture, NULL, NULL);
    SDL_RenderPresent(ren->sdl_ren);

    /* ---- Phase 3 Timing ---- */
    uint64_t phase3_end = SDL_GetPerformanceCounter();
    double upload_time = (double)((phase3_end - phase2_start) * 1000) / SDL_GetPerformanceFrequency();
    renderer_time_upload_ms = upload_time;

#if PROFILE_FRAME
    profile_phase_end = profile_now_ms();
    g_frame_profile.sdl_update_ms += (profile_phase_end - profile_phase_start);
    g_frame_profile.frame_total_ms += (profile_phase_end - profile_frame_start);
    g_frame_profile.frames++;
#endif

#ifdef USE_DIRTY_CELLS
    /* After rendering, swap prev_cells for next frame's comparison */
    grid_swap_prev(grid);
#endif
}

/* ===================================================================
 *  Framebuffer checksum (for correctness validation)
 * =================================================================== */

/**
 * framebuffer_checksum() — FNV-1a 32-bit hash of the pixel buffer
 *
 * Computes a simple checksum over the entire framebuffer for comparing
 * output across different dirty-tracking modes.
 */
static uint32_t framebuffer_checksum(Renderer *ren) {
    if (!ren || !ren->pixel_buffer) return 0;
    
    uint32_t hash = 2166136261u;
    uint32_t *pixels = ren->pixel_buffer;
    size_t pixel_count = (size_t)ren->logical_w * (size_t)ren->logical_h;
    
    for (size_t i = 0; i < pixel_count; i++) {
        hash ^= pixels[i] & 0xFF;
        hash *= 16777619u;
    }
    return hash;
}

uint32_t renderer_framebuffer_checksum(Renderer *ren) {
    return framebuffer_checksum(ren);
}

/* ===================================================================
 *  Event processing (standalone)
 * =================================================================== */

/**
 * renderer_process_events() — Poll SDL events and return quit status
 *
 * This is a simple standalone event loop that checks for quit and ESC.
 * It is NOT used by the main input path (which uses input_process() in
 * input.c instead).  It exists as a utility for simpler test programs
 * or headless-adjacent use cases.
 *
 * @return  true if the application should continue running,
 *          false if quit was requested
 */
bool renderer_process_events(void) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_EVENT_QUIT) {
            return false;
        }
        if (e.type == SDL_EVENT_KEY_DOWN) {
            if (e.key.key == SDLK_ESCAPE) {
                return false;
            }
        }
    }
    return true;
}
