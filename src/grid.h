#ifndef GRID_H
#define GRID_H

#include <stdint.h>
#include <stdbool.h>
#include <SDL3/SDL.h>

typedef struct {
    uint8_t glyph;
    SDL_Color fg;
    SDL_Color bg;
} Cell;

typedef struct {
    int width;
    int height;
    Cell *cells;
} Grid;

Grid* grid_create(int width, int height);
void grid_destroy(Grid *grid);
void grid_clear(Grid *grid, SDL_Color bg);
bool grid_set(Grid *grid, int x, int y, uint8_t glyph, SDL_Color fg, SDL_Color bg);
bool grid_get(Grid *grid, int x, int y, Cell *out_cell);
void grid_print(Grid *grid, int x, int y, const char *text, SDL_Color fg, SDL_Color bg);

#endif
