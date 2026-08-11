/**
 * surface_view.h — Borrowed authored horizontal-surface render view
 *
 * The view owns nothing. Its cells remain owned by SceneDocument and are valid
 * only while that document's authored-cell storage remains unchanged.
 */
#ifndef SURFACE_VIEW_H
#define SURFACE_VIEW_H

#include "scene_types.h"

#include <stddef.h>

typedef struct {
    const SceneAuthoredCell *cells;
    size_t cell_count;
    int width;
    int height;
} SceneSurfaceView;

#endif /* SURFACE_VIEW_H */
