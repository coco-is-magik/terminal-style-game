#ifndef UI_CANVAS_H
#define UI_CANVAS_H

#include "grid.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    int width;
    int height;
    Cell *cells;
    uint8_t *touched;
} UiCanvas;

UiCanvas *ui_canvas_create(int width, int height);
void ui_canvas_destroy(UiCanvas *canvas);
void ui_canvas_clear(UiCanvas *canvas);
bool ui_canvas_set(UiCanvas *canvas, int x, int y, uint8_t glyph,
                   SDL_Color fg, SDL_Color bg);
void ui_canvas_print(UiCanvas *canvas, int x, int y, const char *text,
                     SDL_Color fg, SDL_Color bg);
void ui_canvas_copy_grid_region(UiCanvas *canvas, const Grid *grid,
                                int source_x, int source_y);
bool ui_canvas_is_touched(const UiCanvas *canvas, int x, int y);

#endif