/**
 * decal_painter.h — Headless editing for rectangular decal pattern grids
 *
 * This module owns no UI, input, file path, or decal placement behavior. A
 * painter either owns a newly allocated PatternCell array or borrows an existing
 * one. Borrowed storage is never freed by decal_painter_destroy().
 */

#ifndef DECAL_PAINTER_H
#define DECAL_PAINTER_H

#include "assets.h"

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    DECAL_PAINTER_OK = 0,
    DECAL_PAINTER_NO_CHANGE,
    DECAL_PAINTER_INVALID_ARGUMENT,
    DECAL_PAINTER_INVALID_DIMENSIONS,
    DECAL_PAINTER_SIZE_OVERFLOW,
    DECAL_PAINTER_OUT_OF_BOUNDS,
    DECAL_PAINTER_OUT_OF_MEMORY
} DecalPainterResult;

typedef struct {
    PatternCell *cells;
    size_t cols;
    size_t rows;
    bool owns_cells;
} DecalPainter;

void decal_painter_init(DecalPainter *painter);
void decal_painter_destroy(DecalPainter *painter);

DecalPainterResult decal_painter_create(
    DecalPainter *painter,
    size_t cols,
    size_t rows
);

DecalPainterResult decal_painter_attach(
    DecalPainter *painter,
    PatternCell *cells,
    size_t cols,
    size_t rows
);

DecalPainterResult decal_painter_get_cell(
    const DecalPainter *painter,
    size_t x,
    size_t y,
    PatternCell *out_cell
);

DecalPainterResult decal_painter_paint_cell(
    DecalPainter *painter,
    size_t x,
    size_t y,
    PatternCell cell
);

DecalPainterResult decal_painter_erase_cell(
    DecalPainter *painter,
    size_t x,
    size_t y
);

DecalPainterResult decal_painter_fill(
    DecalPainter *painter,
    PatternCell cell
);

DecalPainterResult decal_painter_clear(DecalPainter *painter);

#endif /* DECAL_PAINTER_H */