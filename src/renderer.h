/**
 * renderer.h — SDL window management and software glyph rasterizer
 *
 * Declares the Renderer struct, instrumentation counters, and the
 * functions for creating/destroying the renderer and drawing frames.
 *
 * See renderer.c for the implementation.
 */

#ifndef RENDERER_H
#define RENDERER_H

#include "grid.h"          /* Grid — the source data to render */
#include <SDL3/SDL.h>      /* SDL_Window, SDL_Renderer, SDL_Texture */
#include <stdbool.h>       /* bool */

/* ---- Instrumentation counters (declared extern) ---- */

extern int renderer_alloc_count;           /* Total malloc() calls */
extern int renderer_free_count;            /* Total free() calls */
extern int renderer_texture_create_count;  /* Total SDL texture creations */

/* ---- Profiling counters (for glyph cache benchmarking) ---- */
extern double renderer_time_raster_ms;       /* Time spent compositing cells */
extern double renderer_time_upload_ms;       /* Time spent in SDL_UpdateTexture */
extern double renderer_time_present_ms;      /* Time spent in render/present */
extern uint64_t renderer_cells_processed;    /* Total cells rasterized */
extern uint64_t renderer_cells_skipped;     /* Cells skipped via dirty tracking */
extern uint64_t renderer_cache_hits;         /* Glyph cache hits (if enabled) */
extern uint64_t renderer_cache_misses;       /* Glyph cache misses (if enabled) */
extern uint64_t renderer_smc_fallback_count; /* SMC stream fallback count */

#if PROFILE_FRAME
#include "timing.h"        /* FrameProfileStats */
extern FrameProfileStats g_frame_profile;    /* Accumulated frame phase timings */
#endif

/* Include the GlyphAtlas definition used by Renderer. */
#include "glyph_atlas.h"    /* GlyphAtlas struct */

/**
 * Renderer — The display subsystem
 *
 * window          — SDL window handle
 * sdl_ren         — SDL renderer handle
 * screen_texture  — Streaming SDL texture for pixel buffer upload
 * atlas           — Glyph atlas texture (for potential GPU path)
 * pixel_buffer    — Software pixel buffer (CPU-side, one uint32_t per pixel)
 * logical_w       — Logical rendering width in pixels (grid_w × cell_w)
 * logical_h       — Logical rendering height in pixels (grid_h × cell_h)
 * cell_w          — Glyph width in pixels
 * cell_h          — Glyph height in pixels
 */
typedef struct {
    SDL_Window     *window;            /* SDL window */
    SDL_Renderer   *sdl_ren;           /* SDL renderer */
    SDL_Texture    *screen_texture;     /* Streaming screen texture */
    GlyphAtlas     *atlas;             /* Glyph atlas (GPU) */
    uint32_t       *pixel_buffer;      /* CPU pixel buffer */
    int logical_w;                     /* Logical pixel width */
    int logical_h;                     /* Logical pixel height */
    int cell_w;                        /* Glyph width (pixels) */
    int cell_h;                        /* Glyph height (pixels) */
#ifdef USE_SMC_BATCH_STATE_TRACKER
     void           *batch_state_buffer;  /* CellState[cell_count] for batch mode */
#endif
#if defined(USE_SMC_BATCH_STATE_TRACKER) || defined(USE_SMC_STREAM_STATE_TRACKER)
     uint32_t       *batch_dirty_indices; /* dirty_indices[cell_count] for batch/stream mode */
     size_t         batch_buffer_size;   /* Number of cells in buffers */
#endif
} Renderer;

/* ---- Renderer API ---- */

Renderer* renderer_create(int win_w, int win_h, int grid_w, int grid_h, int cell_w, int cell_h);
void      renderer_destroy(Renderer *ren);
void      renderer_draw(Renderer *ren, Grid *grid);
bool      renderer_process_events(void);
uint32_t  renderer_framebuffer_checksum(Renderer *ren); /* For benchmark validation */

#endif /* RENDERER_H */