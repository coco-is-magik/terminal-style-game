/**
 * grid.c — 2D character grid buffer
 *
 * This file implements the Grid, which is the core framebuffer for the
 * terminal-style renderer.  It is a 2D array of Cell structs, where each
 * Cell stores:
 *
 *   - glyph  : an 8-bit character code (ASCII 0–255)
 *   - fg     : the foreground colour (SDL_Color)
 *   - bg     : the background colour (SDL_Color)
 *
 * The Grid is populated each frame by either the pattern generators
 * (draw_world_pattern / draw_stress_pattern) or the raycast renderer
 * (raycast_render), and then transferred to the screen by renderer_draw().
 *
 * Functions provided:
 *   grid_create()   — allocate a new Grid of given dimensions
 *   grid_destroy()  — free a Grid and its cell array
 *   grid_clear()    — reset every cell to a space with uniform fg/bg
 *   grid_set()      — write a single cell at (x, y)
 *   grid_get()      — read a single cell at (x, y)
 *   grid_print()    — write a multi-line string into the grid (for HUD text)
 */

#include "grid.h"        /* Grid struct, Cell struct, function declarations */
#include "checked_size.h"
#include "text_utf8.h"   /* cluster decode — one cluster, one cell */
#include <stdlib.h>       /* malloc(), free(), calloc() */
#include <string.h>       /* strlen() */

/* ===================================================================
 *  Creation & destruction
 * =================================================================== */

/**
 * grid_create() — Allocate and initialise a new Grid
 *
 * Creates a Grid with the specified width and height in cells.
 * All cells are zero-initialised by calloc(): glyph = 0 (NUL), fg = {0,0,0,0},
 * bg = {0,0,0,0}.  This effectively means they start invisible — the caller
 * should grid_clear() or explicitly write cells before rendering.
 *
 * @param width   Number of cells horizontally (must be > 0)
 * @param height  Number of cells vertically (must be > 0)
 * @return        Pointer to the new Grid, or NULL on failure
 */
Grid* grid_create(int width, int height) {
    size_t cell_count;
    size_t bytes;
    size_t depth_bytes;
    size_t world_depth_bytes;
    size_t world_key_bytes;
    size_t overlay_depth_bytes;
    if (!checked_size_2d(width, height, &cell_count) ||
        !checked_size_bytes(cell_count, sizeof(Cell), &bytes) ||
        !checked_size_bytes((size_t)width, sizeof(double), &depth_bytes) ||
        !checked_size_bytes(cell_count, sizeof(double), &world_depth_bytes) ||
        !checked_size_bytes(cell_count, sizeof(uint64_t), &world_key_bytes) ||
        !checked_size_bytes(cell_count, sizeof(double), &overlay_depth_bytes)) return NULL;
    (void)bytes;
    (void)depth_bytes;
    (void)world_depth_bytes;
    (void)world_key_bytes;
    (void)overlay_depth_bytes;

    /* Allocate the Grid struct itself */
    Grid *grid = malloc(sizeof(Grid));
    if (!grid) return NULL;

    grid->width  = width;
    grid->height = height;

    /* Allocate framebuffer and reusable per-grid render workspaces. */
    grid->cells = calloc(cell_count, sizeof(Cell));
    grid->prev_cells = calloc(cell_count, sizeof(Cell));
    grid->column_depths = calloc((size_t)width, sizeof(double));
    grid->world_depths = calloc(cell_count, sizeof(double));
    grid->world_hit_keys = calloc(cell_count, sizeof(uint64_t));
    grid->overlay_depths = calloc(cell_count, sizeof(double));
    if (!grid->cells || !grid->prev_cells || !grid->column_depths ||
        !grid->world_depths || !grid->world_hit_keys || !grid->overlay_depths) {
        free(grid->cells);
        free(grid->prev_cells);
        free(grid->column_depths);
        free(grid->world_depths);
        free(grid->world_hit_keys);
        free(grid->overlay_depths);
        free(grid);
        return NULL;
    }

    return grid;
}

/**
 * grid_destroy() — Free a Grid and its cell array
 *
 * Safe to call with NULL (no-op).
 *
 * @param grid  Pointer to the Grid to free (NULL-safe)
 */
void grid_destroy(Grid *grid) {
    if (!grid) return;
    if (grid->cells) {
        free(grid->cells);       /* Free the cell buffer */
    }
    if (grid->prev_cells) {
        free(grid->prev_cells);  /* Free the previous frame buffer */
    }
    free(grid->column_depths);
    free(grid->world_depths);
    free(grid->world_hit_keys);
    free(grid->overlay_depths);
    free(grid);                  /* Free the Grid struct */
}

/**
 * grid_swap_prev() — Swap cells/prev_cells for dirty tracking
 *
 * Called after frame rendering to prepare for next frame's dirty comparison.
 * On next frame, renderer can skip cells that match their prev_cells values.
 *
 * @param grid  The Grid to swap (NULL-safe)
 */
void grid_swap_prev(Grid *grid) {
    if (!grid) return;
    Cell *tmp = grid->cells;
    grid->cells = grid->prev_cells;
    grid->prev_cells = tmp;
}

/* ===================================================================
 *  Grid operations
 * =================================================================== */

/**
 * grid_clear() — Reset every cell in the grid
 *
 * Each cell is set to:
 *   - glyph = ' '  (space character)
 *   - fg    = white (255, 255, 255, 255)
 *   - bg    = the provided bg colour
 *
 * This is typically called once per frame before drawing new content,
 * although the raycast renderer sometimes draws over the previous frame
 * without clearing (for performance).
 *
 * @param grid  The Grid to clear (NULL-safe)
 * @param bg    The background colour to apply to every cell
 */
void grid_clear(Grid *grid, SDL_Color bg) {
    if (!grid || !grid->cells) return;

    SDL_Color fg = {255, 255, 255, 255};  /* White foreground */
    int total_cells = grid->width * grid->height;

    for (int i = 0; i < total_cells; i++) {
        grid->cells[i].glyph = ' ';
        grid->cells[i].fg = fg;
        grid->cells[i].bg = bg;
    }
}

bool grid_clear_region_zero(Grid *grid, int x, int y, int width, int height) {
    int row;
    if (!grid || !grid->cells || x < 0 || y < 0 || width < 0 || height < 0 ||
        x > grid->width || y > grid->height || width > grid->width - x ||
        height > grid->height - y) {
        return false;
    }
    for (row = 0; row < height; row++) {
        memset(&grid->cells[(y + row) * grid->width + x], 0,
               (size_t)width * sizeof(*grid->cells));
    }
    return true;
}

/**
 * grid_set() — Write a single cell at coordinates (x, y)
 *
 * Bounds-checking is performed: if x or y is out of range, the function
 * returns false and does nothing.
 *
 * @param grid  The Grid to write into (NULL-safe)
 * @param x     Column index (0 = left, must be < grid->width)
 * @param y     Row index (0 = top, must be < grid->height)
 * @param glyph Character code to display (ASCII or extended)
 * @param fg    Foreground colour of the glyph
 * @param bg    Background colour behind the glyph
 * @return      true on success, false if grid is NULL or coordinates are out of bounds
 */
bool grid_set(Grid *grid, int x, int y, uint16_t glyph, SDL_Color fg, SDL_Color bg) {
    if (!grid || !grid->cells) return false;
    if (x < 0 || x >= grid->width || y < 0 || y >= grid->height) return false;

    /* Convert 2D coordinates to a 1D index (row-major order):
     *   index = y * width + x */
    int index = y * grid->width + x;
    grid->cells[index].glyph = glyph;
    grid->cells[index].fg = fg;
    grid->cells[index].bg = bg;
    return true;
}

/**
 * grid_get() — Read a single cell at coordinates (x, y)
 *
 * Copies the cell data into the provided out_cell pointer.
 * Bounds-checked: returns false for out-of-range access.
 *
 * @param grid     The Grid to read from (NULL-safe)
 * @param x        Column index
 * @param y        Row index
 * @param out_cell Pointer to a Cell struct that will receive the data
 * @return         true on success, false on error
 */
bool grid_get(Grid *grid, int x, int y, Cell *out_cell) {
    if (!grid || !grid->cells || !out_cell) return false;
    if (x < 0 || x >= grid->width || y < 0 || y >= grid->height) return false;

    int index = y * grid->width + x;
    *out_cell = grid->cells[index];    /* Struct copy */
    return true;
}

/**
 * grid_print() — Write a multi-line UTF-8 text string into the grid
 *
 * Walks the text cluster by cluster (V1-6 I0):
 *   - A '\n' newline resets the cursor to the starting X and increments Y
 *   - Any other grapheme cluster is written to the current cursor position via grid_set(), using the
 *     cluster's first codepoint, and advances the cursor by exactly one column
 *
 * This is the single path from text to cells: the HUD overlay, authored UI labels and the editor's own
 * chrome all arrive here. It used to read one byte per cell, which drew every multi-byte character as
 * a run of unrelated Latin-1 glyphs.
 *
 * on-screen text.  The text is NOT word-wrapped; if it exceeds the grid
 * boundaries, the extra characters are silently discarded by grid_set()'s
 * bounds checking.
 *
 * @param grid  The Grid to write into (NULL-safe)
 * @param x     Starting column for the text
 * @param y     Starting row for the text
 * @param text  NUL-terminated string to render (may contain newlines)
 * @param fg    Foreground colour for all characters
 * @param bg    Background colour for all characters
 */
bool grid_print(Grid *grid, int x, int y, const char *text, SDL_Color fg, SDL_Color bg) {
    size_t offset = 0U;
    size_t size;
    int cx = x;   /* Current column cursor */
    int cy = y;   /* Current row cursor */

    if (!grid || !text) return false;

    size = strlen(text);
    while (offset < size) {
        uint32_t codepoint = 0U;
        size_t length = 0U;

        if (text[offset] == '\n') {
            /* Newline: reset column to start X, advance row by 1 */
            cx = x;
            cy++;
            offset++;
            continue;
        }
        /* One cluster, one cell. The cluster's first codepoint is what the cell stores — the same
           rule the cell model uses, and the reason a combining mark or a ZWJ sequence does not eat a
           column per byte. */
        if (text_utf8_next_cluster(text, size, &offset, &codepoint, &length) != TEXT_UTF8_OK) {
            return false;   /* malformed, or a cluster too long to be a cell */
        }
        (void)length;
        /* A 4-byte sequence is a codepoint above U+FFFF, which a 16-bit cell cannot address. Refused
           rather than truncated: truncating 0x1F600 to 0xF600 would draw a different character and
           call it success. */
        if (codepoint > 0xFFFFU) return false;
        grid_set(grid, cx, cy, (uint16_t)codepoint, fg, bg);
        cx++;   /* Advance column */
    }
    return true;
}
