/**
 * grid.h — 2D character grid buffer
 *
 * Defines the Cell and Grid types that form the engine's core framebuffer.
 * Each Cell stores a glyph character + foreground/background colours.
 * The Grid is a 2D array of Cells written each frame by the renderer
 * and then transferred to the screen.
 *
 * See grid.c for the implementation.
 */

#ifndef GRID_H
#define GRID_H

#include <stdint.h>     /* uint8_t */
#include <stdbool.h>     /* bool */
#include <SDL3/SDL.h>    /* SDL_Color */

/**
 * Cell — A single character cell in the grid
 *
 * glyph — a Unicode codepoint in the BMP (U+0000–FFFF); 0 is blank and 0xFFFF is
 *         transparent (V1_5_CELL_IDENTITY_DECISION_2026-11-22.md). "Missing" is not a
 *         stored value: glyph_source decides it at draw time.
 * fg    — Foreground colour (the glyph itself)
 * bg    — Background colour (the cell behind the glyph)
 *
 * 16 bits, not 32, because the state fingerprint is one 64-bit word: 16 + 24 + 24 = 64.
 */
typedef struct {
    uint16_t glyph;           /* Source codepoint in the BMP */
    SDL_Color fg;             /* Foreground colour */
    SDL_Color bg;             /* Background colour */
} Cell;

/**
 * Grid — A 2D array of character cells
 *
 * width  — Number of columns (horizontal cells)
 * height — Number of rows (vertical cells)
 * cells  — Linear array of Cells in row-major order (index = y * width + x)
 * column_depths — Reusable width-sized world-render depth workspace
 * world_depths/world_hit_keys — Reusable per-cell bounded-render occlusion data
 * overlay_depths — Reusable nearest decal/sprite depth frontier
 */
typedef struct {
    int width;               /* Grid width in cells */
    int height;              /* Grid height in cells */
    Cell *cells;             /* Cell data (heap-allocated, width × height) */
    Cell *prev_cells;        /* Previous frame cells (for dirty tracking) */
    double *column_depths;   /* Per-column world depth; owned by Grid */
    double *world_depths;    /* Per-cell nearest bounded-world depth; owned by Grid */
    uint64_t *world_hit_keys;/* Per-cell packed hit identity; owned by Grid */
    double *overlay_depths;  /* Per-cell nearest world-overlay depth; owned by Grid */
} Grid;

/* ---- Grid API ---- */

Grid*  grid_create(int width, int height);
void   grid_destroy(Grid *grid);
void   grid_clear(Grid *grid, SDL_Color bg);
bool   grid_clear_region_zero(Grid *grid, int x, int y, int width, int height);
void   grid_swap_prev(Grid *grid);   /* Swap cells/prev_cells after frame */
bool   grid_set(Grid *grid, int x, int y, uint16_t glyph, SDL_Color fg, SDL_Color bg);
bool   grid_get(Grid *grid, int x, int y, Cell *out_cell);
/* Write a UTF-8 string into the grid, one cell per **grapheme cluster**, advancing one column each.
 *
 * '\n' still means "next line". Every other cluster's first codepoint is written, which is the same
 * rule the cell model uses: one cluster, one codepoint identity, one cell.
 *
 * Returns false when the walk stopped early, and the cells written before that point are left in
 * place. There are three ways to stop, and a caller that cares can tell them apart by validating
 * first: malformed UTF-8, a cluster longer than TEXT_UTF8_MAX_CLUSTER_BYTES, or a codepoint above
 * U+FFFF — which the 16-bit cell cannot address, so it is refused rather than truncated to a
 * different character. A bool rather than an enum because every current caller either ignores it or
 * needs only "did it all fit"; the reasons are `text_utf8`'s to report when someone asks. */
bool   grid_print(Grid *grid, int x, int y, const char *text, SDL_Color fg, SDL_Color bg);

#endif /* GRID_H */
