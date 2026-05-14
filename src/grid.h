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
 * glyph — ASCII or extended character code (0–255)
 * fg    — Foreground colour (the glyph itself)
 * bg    — Background colour (the cell behind the glyph)
 */
typedef struct {
    uint8_t glyph;            /* Character to display */
    SDL_Color fg;             /* Foreground colour */
    SDL_Color bg;             /* Background colour */
} Cell;

/**
 * Grid — A 2D array of character cells
 *
 * width  — Number of columns (horizontal cells)
 * height — Number of rows (vertical cells)
 * cells  — Linear array of Cells in row-major order (index = y * width + x)
 */
typedef struct {
    int width;               /* Grid width in cells */
    int height;              /* Grid height in cells */
    Cell *cells;             /* Cell data (heap-allocated, width × height) */
} Grid;

/* ---- Grid API ---- */

Grid*  grid_create(int width, int height);
void   grid_destroy(Grid *grid);
void   grid_clear(Grid *grid, SDL_Color bg);
bool   grid_set(Grid *grid, int x, int y, uint8_t glyph, SDL_Color fg, SDL_Color bg);
bool   grid_get(Grid *grid, int x, int y, Cell *out_cell);
void   grid_print(Grid *grid, int x, int y, const char *text, SDL_Color fg, SDL_Color bg);

#endif /* GRID_H */