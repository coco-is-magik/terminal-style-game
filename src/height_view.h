/**
 * height_view.h — Borrowed authored height and movement view
 *
 * The view owns nothing. Cells remain owned by SceneDocument and are valid only
 * while that document's authored-cell storage remains unchanged.
 */
#ifndef HEIGHT_VIEW_H
#define HEIGHT_VIEW_H

#include "scene_types.h"

#include <stddef.h>

typedef struct {
    const SceneAuthoredCell *cells;
    size_t cell_count;
    int width;
    int height;
    SceneMovementParameters movement;
} SceneHeightView;

#endif /* HEIGHT_VIEW_H */