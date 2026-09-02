#ifndef SPRITE_RENDER_H
#define SPRITE_RENDER_H

#include "assets.h"
#include "camera.h"
#include "grid.h"
#include "height_view.h"
#include "map.h"
#include "world.h"

#include <stdbool.h>

/* Draw decorative billboard sprites into an already rendered world grid.
 * overlay_depth is a grid-sized nearest-overlay workspace initialized by the
 * caller. Sprites read, but never modify, world or column depth. */
void sprite_render_world_overlays(
    Grid *grid, const Map *map, const Camera *camera,
    const AssetRegistry *assets, const WorldState *world,
    const SceneHeightView *heights, const double *column_depths,
    bool bounded_occlusion, double *overlay_depth
);

#endif /* SPRITE_RENDER_H */