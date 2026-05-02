#include "grid.h"
#include <stdlib.h>
#include <string.h>

Grid* grid_create(int width, int height) {
    Grid *grid = malloc(sizeof(Grid));
    if (!grid) return NULL;
    grid->width = width;
    grid->height = height;
    grid->cells = calloc(width * height, sizeof(Cell));
    if (!grid->cells) {
        free(grid);
        return NULL;
    }
    return grid;
}

void grid_destroy(Grid *grid) {
    if (!grid) return;
    free(grid->cells);
    free(grid);
}

void grid_clear(Grid *grid, SDL_Color bg) {
    SDL_Color fg = {255, 255, 255, 255};
    for (int i = 0; i < grid->width * grid->height; i++) {
        grid->cells[i].glyph = ' ';
        grid->cells[i].fg = fg;
        grid->cells[i].bg = bg;
    }
}

void grid_set(Grid *grid, int x, int y, uint8_t glyph, SDL_Color fg, SDL_Color bg) {
    if (x < 0 || x >= grid->width || y < 0 || y >= grid->height) return;
    int index = y * grid->width + x;
    grid->cells[index].glyph = glyph;
    grid->cells[index].fg = fg;
    grid->cells[index].bg = bg;
}

void grid_print(Grid *grid, int x, int y, const char *text, SDL_Color fg, SDL_Color bg) {
    int cx = x;
    int cy = y;
    for (size_t i = 0; i < strlen(text); i++) {
        if (text[i] == '\n') {
            cx = x;
            cy++;
            continue;
        }
        grid_set(grid, cx, cy, text[i], fg, bg);
        cx++;
    }
}
