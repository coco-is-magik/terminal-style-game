/**
 * decal_painter.c — Headless decal-pattern editing implementation
 */

#include "decal_painter.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static DecalPainterResult validate_dimensions(
    size_t cols,
    size_t rows,
    size_t *out_count
) {
    if (cols == 0 || rows == 0) return DECAL_PAINTER_INVALID_DIMENSIONS;
    if (cols > SIZE_MAX / rows) return DECAL_PAINTER_SIZE_OVERFLOW;

    size_t count = cols * rows;
    if (count > SIZE_MAX / sizeof(PatternCell)) {
        return DECAL_PAINTER_SIZE_OVERFLOW;
    }
    if (out_count) *out_count = count;
    return DECAL_PAINTER_OK;
}

static DecalPainterResult validate_painter(
    const DecalPainter *painter,
    size_t *out_count
) {
    if (!painter || !painter->cells) return DECAL_PAINTER_INVALID_ARGUMENT;
    return validate_dimensions(painter->cols, painter->rows, out_count);
}

static DecalPainterResult cell_index(
    const DecalPainter *painter,
    size_t x,
    size_t y,
    size_t *out_index
) {
    DecalPainterResult result = validate_painter(painter, NULL);
    if (result != DECAL_PAINTER_OK) return result;
    if (x >= painter->cols || y >= painter->rows) {
        return DECAL_PAINTER_OUT_OF_BOUNDS;
    }
    if (!out_index) return DECAL_PAINTER_INVALID_ARGUMENT;

    *out_index = y * painter->cols + x;
    return DECAL_PAINTER_OK;
}

void decal_painter_init(DecalPainter *painter) {
    if (!painter) return;
    memset(painter, 0, sizeof(*painter));
}

void decal_painter_destroy(DecalPainter *painter) {
    if (!painter) return;
    if (painter->owns_cells) free(painter->cells);
    decal_painter_init(painter);
}

DecalPainterResult decal_painter_create(
    DecalPainter *painter,
    size_t cols,
    size_t rows
) {
    size_t count;
    PatternCell *cells;
    DecalPainterResult result;

    if (!painter) return DECAL_PAINTER_INVALID_ARGUMENT;
    result = validate_dimensions(cols, rows, &count);
    if (result != DECAL_PAINTER_OK) return result;

    cells = calloc(count, sizeof(*cells));
    if (!cells) return DECAL_PAINTER_OUT_OF_MEMORY;

    decal_painter_destroy(painter);
    painter->cells = cells;
    painter->cols = cols;
    painter->rows = rows;
    painter->owns_cells = true;
    return DECAL_PAINTER_OK;
}

DecalPainterResult decal_painter_attach(
    DecalPainter *painter,
    PatternCell *cells,
    size_t cols,
    size_t rows
) {
    DecalPainterResult result;

    if (!painter || !cells) return DECAL_PAINTER_INVALID_ARGUMENT;
    result = validate_dimensions(cols, rows, NULL);
    if (result != DECAL_PAINTER_OK) return result;
    if (painter->owns_cells && painter->cells == cells) {
        return DECAL_PAINTER_INVALID_ARGUMENT;
    }

    decal_painter_destroy(painter);
    painter->cells = cells;
    painter->cols = cols;
    painter->rows = rows;
    painter->owns_cells = false;
    return DECAL_PAINTER_OK;
}

DecalPainterResult decal_painter_get_cell(
    const DecalPainter *painter,
    size_t x,
    size_t y,
    PatternCell *out_cell
) {
    size_t index;
    DecalPainterResult result;

    if (!out_cell) return DECAL_PAINTER_INVALID_ARGUMENT;
    result = cell_index(painter, x, y, &index);
    if (result != DECAL_PAINTER_OK) return result;

    *out_cell = painter->cells[index];
    return DECAL_PAINTER_OK;
}

DecalPainterResult decal_painter_paint_cell(
    DecalPainter *painter,
    size_t x,
    size_t y,
    PatternCell cell
) {
    size_t index;
    DecalPainterResult result = cell_index(painter, x, y, &index);
    if (result != DECAL_PAINTER_OK) return result;

    PatternCell *target = &painter->cells[index];
    if (target->glyph == cell.glyph && target->material_id == cell.material_id) {
        return DECAL_PAINTER_NO_CHANGE;
    }
    *target = cell;
    return DECAL_PAINTER_OK;
}

DecalPainterResult decal_painter_erase_cell(
    DecalPainter *painter,
    size_t x,
    size_t y
) {
    const PatternCell empty = {0, 0};
    return decal_painter_paint_cell(painter, x, y, empty);
}

DecalPainterResult decal_painter_fill(
    DecalPainter *painter,
    PatternCell cell
) {
    size_t count;
    bool changed = false;
    DecalPainterResult result = validate_painter(painter, &count);
    if (result != DECAL_PAINTER_OK) return result;

    for (size_t i = 0; i < count; i++) {
        if (painter->cells[i].glyph != cell.glyph ||
            painter->cells[i].material_id != cell.material_id) {
            painter->cells[i] = cell;
            changed = true;
        }
    }
    return changed ? DECAL_PAINTER_OK : DECAL_PAINTER_NO_CHANGE;
}

DecalPainterResult decal_painter_clear(DecalPainter *painter) {
    const PatternCell empty = {0, 0};
    return decal_painter_fill(painter, empty);
}