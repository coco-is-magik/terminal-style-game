#include "raycast.h"

#include "config.h"
#include "height_projection.h"
#include "heightfield_trace.h"
#include "mirror_trace.h"
#include "optical_compositor.h"
#include "raycast_internal.h"

#include <float.h>
#include <stddef.h>

static const Cell optical_darkness = {
    ' ', {0U, 0U, 0U, 255U}, {0U, 0U, 0U, 255U}
};

static bool optical_view_requires_composition(const OpticalRuntimeView *view) {
    const uint8_t render_mask = OPTICAL_OVERRIDE_RAY_BLOCKS |
        OPTICAL_OVERRIDE_OPACITY | OPTICAL_OVERRIDE_TRANSMISSION |
        OPTICAL_OVERRIDE_REFLECTIVITY;
    size_t i;
    for (i = 0U; i < view->material_capacity; i++)
        if ((view->material_defaults[i].override_mask & render_mask) != 0U)
            return true;
    for (i = 0U; i < view->cell_override_count; i++)
        if ((view->cell_overrides[i].optical.override_mask & render_mask) != 0U)
            return true;
    return false;
}

static Cell compose_optical_result(
    const Map *map, const AssetRegistry *assets,
    const HeightfieldOpticalResult *traced
) {
    OpticalCompositeLayer layers[OPTICAL_COMPOSITOR_MAX_LAYERS];
    OpticalCompositeResult composite;
    size_t i;
    if (!traced || traced->count == 0U) return optical_darkness;
    for (i = 0U; i < traced->count; i++) {
        layers[i].sampled_cell = raycast_sample_heightfield_hit(
            map, assets, &traced->layers[i].hit);
        layers[i].optical = traced->layers[i].optical;
        layers[i].generated_boundary = traced->layers[i].hit.generated_boundary;
    }
    if (optical_composite_layers(
            layers, traced->count, traced->terminated_by_surface,
            traced->reached_opening, traced->layer_cap_exhausted,
            &optical_darkness, &composite)) return composite.cell;
    return optical_darkness;
}

static void render_optical_sample(Grid *grid, const Map *map,
                                  const AssetRegistry *assets,
                                  const HeightfieldTraceColumn *column,
                                  MirrorTraceColumnCache *mirror_cache,
                                  const OpticalRuntimeView *optical_view,
                                  uint32_t source_generation,
                                  int x, int y) {
    HeightfieldHit nearest = heightfield_trace_prepared_sample(column, y);
    OpticalResolved nearest_optical;
    HeightfieldOpticalResult traced;
    size_t output_index = (size_t)y * (size_t)grid->width + (size_t)x;
    Cell output = optical_darkness;
    if (!nearest.hit) {
        grid->cells[output_index] = output;
        grid->world_depths[output_index] = DBL_MAX;
        grid->world_hit_keys[output_index] = 0U;
        return;
    }
    {
        size_t cell_index = (size_t)nearest.map_y *
                            (size_t)column->heights->width +
                            (size_t)nearest.map_x;
        if (nearest.map_x < 0 || nearest.map_y < 0 ||
            nearest.map_x >= column->heights->width ||
            nearest.map_y >= column->heights->height ||
            !optical_runtime_view_resolve(
                optical_view, cell_index, nearest.material, true,
                &nearest_optical)) {
            grid->cells[output_index] = output;
            grid->world_depths[output_index] = DBL_MAX;
            grid->world_hit_keys[output_index] = 0U;
            return;
        }
    }
    if (nearest.kind == HEIGHTFIELD_HIT_WALL &&
        nearest_optical.reflectivity > 0U) {
        MirrorTraceResult reflected;
        Cell direct = raycast_sample_heightfield_hit(map, assets, &nearest);
        Cell reflection = optical_darkness;
        if (mirror_trace_sample_once(
                mirror_cache, column, &nearest, optical_view,
                source_generation, y, config_get()->raycast_max_distance,
                &reflected) && !reflected.darkness_fallback) {
            reflection = compose_optical_result(map, assets, &reflected.optical);
        }
        if (!optical_mix_reflection(
                &direct, &reflection, nearest_optical.reflectivity, &output))
            output = direct;
        grid->cells[output_index] = output;
        grid->world_depths[output_index] = nearest.perpendicular_distance;
        grid->world_hit_keys[output_index] = raycast_world_hit_key(&nearest);
        return;
    }
    if (nearest_optical.ray_blocks || nearest_optical.transmission == 0U) {
        output = raycast_sample_heightfield_hit(map, assets, &nearest);
        grid->cells[output_index] = output;
        grid->world_depths[output_index] = nearest.perpendicular_distance;
        grid->world_hit_keys[output_index] = raycast_world_hit_key(&nearest);
        return;
    }
    if (!heightfield_trace_continue_after_nearest(
            column, optical_view, source_generation, y,
            &nearest, &nearest_optical, &traced)) {
        grid->cells[output_index] = output;
        grid->world_depths[output_index] = DBL_MAX;
        grid->world_hit_keys[output_index] = 0U;
        return;
    }
    if (traced.count > 0U) {
        output = compose_optical_result(map, assets, &traced);
    }
    grid->cells[output_index] = output;
    grid->world_depths[output_index] = nearest.hit
        ? nearest.perpendicular_distance : DBL_MAX;
    grid->world_hit_keys[output_index] = raycast_world_hit_key(&nearest);
}

void raycast_render_height_optical(
    Grid *grid, Map *map, Camera *cam,
    AssetRegistry *assets, WorldState *world,
    const SceneSurfaceView *surfaces,
    const SceneHeightView *heights,
    const OpticalRuntimeView *optical_view,
    uint32_t source_generation
) {
    OpticalRuntimeView inherited_view;
    const OpticalRuntimeView *active_view = optical_view;
    int x;
    if (!grid || !map || !cam || !assets || !world) return;
    if (scene_height_view_is_valid(heights, map->width, map->height) &&
        !active_view && optical_runtime_view_init(
            &inherited_view, heights->cell_count, NULL, 0U, NULL, 0U,
            source_generation)) active_view = &inherited_view;
    if (!scene_height_view_is_valid(heights, map->width, map->height) ||
        !optical_runtime_view_is_current(active_view, source_generation) ||
        active_view->cell_count != heights->cell_count ||
        !grid->column_depths || !grid->world_depths || !grid->world_hit_keys) {
        raycast_render_height_legacy_impl(
            grid, map, cam, assets, world, surfaces, heights);
        return;
    }
    if (!optical_view_requires_composition(active_view)) {
        raycast_render_heightfield_opaque_impl(
            grid, map, cam, assets, heights, grid->column_depths);
        raycast_render_world_overlays(
            grid, map, cam, assets, world, heights, true);
        return;
    }
    for (x = 0; x < grid->width; x++) {
        HeightfieldTraceColumn column;
        MirrorTraceColumnCache mirror_cache;
        int y;
        mirror_trace_column_cache_init(&mirror_cache);
        if (!heightfield_trace_prepare_column(
                &column, cam, map, heights, grid->width, grid->height, x,
                config_get()->raycast_max_distance)) {
            grid->column_depths[x] = config_get()->raycast_max_distance;
            for (y = 0; y < grid->height; y++) {
                size_t index = (size_t)y * (size_t)grid->width + (size_t)x;
                grid->cells[index] = optical_darkness;
                grid->world_depths[index] = DBL_MAX;
                grid->world_hit_keys[index] = 0U;
            }
            continue;
        }
        grid->column_depths[x] = raycast_heightfield_column_depth(&column);
        for (y = 0; y < grid->height; y++) {
            render_optical_sample(
                grid, map, assets, &column, &mirror_cache, active_view,
                source_generation, x, y);
        }
    }
    raycast_render_world_overlays(
        grid, map, cam, assets, world, heights, true);
}