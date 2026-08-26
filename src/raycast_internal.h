#ifndef RAYCAST_INTERNAL_H
#define RAYCAST_INTERNAL_H

#include "heightfield_trace.h"
#include "raycast.h"

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