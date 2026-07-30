#include "ui_canvas.h"
#include "checked_size.h"

#include <stdlib.h>
#include <string.h>

UiCanvas *ui_canvas_create(int width, int height) {
    UiCanvas *canvas;
    size_t count;
    size_t cell_bytes;
    if (!checked_size_2d(width, height, &count) ||
        !checked_size_bytes(count, sizeof(Cell), &cell_bytes)) return NULL;
    canvas = calloc(1, sizeof(*canvas));
    if (!canvas) return NULL;
    canvas->cells = malloc(cell_bytes);
    canvas->touched = calloc(count, sizeof(*canvas->touched));
    if (!canvas->cells || !canvas->touched) {
        ui_canvas_destroy(canvas);
        return NULL;
    }
    memset(canvas->cells, 0, cell_bytes);
    canvas->width = width;
    canvas->height = height;
    return canvas;
}

void ui_canvas_destroy(UiCanvas *canvas) {
    if (!canvas) return;
    free(canvas->cells);
    free(canvas->touched);
    free(canvas);
}

void ui_canvas_clear(UiCanvas *canvas) {
    size_t count;
    if (!canvas) return;
    count = (size_t)canvas->width * (size_t)canvas->height;
    memset(canvas->touched, 0, count * sizeof(*canvas->touched));
}

bool ui_canvas_set(UiCanvas *canvas, int x, int y, uint8_t glyph,
                   SDL_Color fg, SDL_Color bg) {
    size_t index;
    if (!canvas || x < 0 || y < 0 || x >= canvas->width || y >= canvas->height) {
        return false;
    }
    index = (size_t)y * (size_t)canvas->width + (size_t)x;
    canvas->cells[index] = (Cell){glyph, fg, bg};
    canvas->touched[index] = 1;
    return true;
}

void ui_canvas_print(UiCanvas *canvas, int x, int y, const char *text,
                     SDL_Color fg, SDL_Color bg) {
    int offset = 0;
    if (!canvas || !text) return;
    while (text[offset] != '\0') {
        (void)ui_canvas_set(canvas, x + offset, y, (uint8_t)text[offset], fg, bg);
        offset++;
    }
}

void ui_canvas_copy_grid_region(UiCanvas *canvas, const Grid *grid,
                                int source_x, int source_y) {
    int y;
    int x;
    if (!canvas || !grid || !grid->cells) return;
    ui_canvas_clear(canvas);
    for (y = 0; y < canvas->height; y++) {
        int grid_y = source_y + y;
        if (grid_y < 0 || grid_y >= grid->height) continue;
        for (x = 0; x < canvas->width; x++) {
            int grid_x = source_x + x;
            const Cell *cell;
            if (grid_x < 0 || grid_x >= grid->width) continue;
            cell = &grid->cells[grid_y * grid->width + grid_x];
            if (cell->glyph == 0) continue;
            (void)ui_canvas_set(canvas, x, y, cell->glyph, cell->fg, cell->bg);
        }
    }
}

bool ui_canvas_is_touched(const UiCanvas *canvas, int x, int y) {
    if (!canvas || x < 0 || y < 0 || x >= canvas->width || y >= canvas->height) {
        return false;
    }
    return canvas->touched[(size_t)y * (size_t)canvas->width + (size_t)x] != 0;
}