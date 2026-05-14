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
    /* Reject invalid dimensions */
    if (width <= 0 || height <= 0) return NULL;

    /* Allocate the Grid struct itself */
    Grid *grid = malloc(sizeof(Grid));
    if (!grid) return NULL;

    grid->width  = width;
    grid->height = height;

    /* Allocate the cell array.  calloc zeros every byte, which means
     * every Cell starts with glyph=0, fg={0,0,0,0}, bg={0,0,0,0}. */
    grid->cells = calloc(width * height, sizeof(Cell));
    if (!grid->cells) {
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
    free(grid);                  /* Free the Grid struct */
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
bool grid_set(Grid *grid, int x, int y, uint8_t glyph, SDL_Color fg, SDL_Color bg) {
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
 * grid_print() — Write a multi-line text string into the grid
 *
 * Walks through the text character by character:
 *   - A '\n' newline resets the cursor to the starting X and increments Y
 *   - Any other character is written to the current cursor position via grid_set()
 *
 * This is used for the HUD overlay (FPS, frame times) and any other
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
void grid_print(Grid *grid, int x, int y, const char *text, SDL_Color fg, SDL_Color bg) {
    if (!grid || !text) return;

    int cx = x;   /* Current column cursor */
    int cy = y;   /* Current row cursor */

    for (size_t i = 0; i < strlen(text); i++) {
        if (text[i] == '\n') {
            /* Newline: reset column to start X, advance row by 1 */
            cx = x;
            cy++;
            continue;
        }
        /* Write the character at the current cursor position */
        grid_set(grid, cx, cy, text[i], fg, bg);
        cx++;   /* Advance column */
    }
}