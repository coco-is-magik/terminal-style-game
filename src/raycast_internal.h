#ifndef RAYCAST_INTERNAL_H
#define RAYCAST_INTERNAL_H

#include "heightfield_trace.h"
#include "raycast.h"

/* Private compatibility implementation. The deprecated public entry point and
 * current renderer's invalid-view safety fallback delegate here; production and
 * tests must not call it directly. */
void raycast_render_height_legacy_impl(
    Grid *grid, Map *map, Camera *cam, AssetRegistry *assets, WorldState *world,
    const SceneSurfaceView *surfaces, const SceneHeightView *heights
);

/* Shared prepared-heightfield sampling used by the current renderer's inherited
 * fast path. Callers remain responsible for overlays. */
void raycast_render_heightfield_opaque_impl(
    Grid *grid, Map *map, Camera *cam, AssetRegistry *assets,
    const SceneHeightView *heights, double *z_buffer
);


Cell raycast_sample_heightfield_hit(const Map *map,
                                    const AssetRegistry *assets,
                                    const HeightfieldHit *hit);
uint64_t raycast_world_hit_key(const HeightfieldHit *hit);
double raycast_heightfield_column_depth(const HeightfieldTraceColumn *column);
void raycast_render_world_overlays(Grid *grid, Map *map, Camera *camera,
                                   AssetRegistry *assets, WorldState *world,
                                   const SceneHeightView *heights,
                                   bool bounded_occlusion);

#endif /* RAYCAST_INTERNAL_H */