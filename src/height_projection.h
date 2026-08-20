/**
 * height_projection.h — Shared heightfield projection arithmetic
 *
 * Pure allocation-free helpers used by renderer and editor projection paths.
 */
#ifndef HEIGHT_PROJECTION_H
#define HEIGHT_PROJECTION_H

#include "camera.h"
#include "height_view.h"

#include <math.h>
#include <stdbool.h>
#include <stddef.h>

static inline double scene_height_world(int16_t step) {
    return (double)step / (double)SCENE_HEIGHT_STEPS_PER_UNIT;
}

static inline bool scene_height_view_is_valid(const SceneHeightView *view,
                                              int width, int height) {
    return view && view->cells && width > 0 && height > 0 &&
        view->width == width && view->height == height &&
        view->cell_count == (size_t)width * (size_t)height;
}

static inline double camera_horizon_row(const Camera *camera, int viewport_rows) {
    if (!camera || viewport_rows <= 0 || !isfinite(camera->pitch)) return 0.0;
    return viewport_rows / 2.0 + camera->pitch;
}

static inline double height_project_y(const Camera *camera, int viewport_rows,
                                      double world_z, double perpendicular) {
    if (!camera || viewport_rows <= 0 || !isfinite(world_z) ||
        !isfinite(perpendicular) || perpendicular <= 0.0) return 0.0;
    return camera_horizon_row(camera, viewport_rows) +
        (camera->z - world_z) * viewport_rows / perpendicular;
}

#endif /* HEIGHT_PROJECTION_H */