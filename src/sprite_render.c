#include "sprite_render.h"

#include "height_projection.h"

#include <float.h>
#include <math.h>
#include <stddef.h>

#define SPRITE_DEPTH_EPSILON 0.000001
#define WORLD_OCCLUSION_EPSILON 0.01

static bool sprite_anchor_floor(
    const Map *map, const SceneHeightView *heights,
    double x, double y, double *floor_z
) {
    int map_x = (int)floor(x);
    int map_y = (int)floor(y);
    if (!map_in_bounds(map, map_x, map_y)) return false;
    *floor_z = 0.0;
    if (scene_height_view_is_valid(heights, map->width, map->height)) {
        const SceneAuthoredCell *cell = &heights->cells[
            (size_t)map_y * (size_t)map->width + (size_t)map_x];
        if (!cell->floor_present) return false;
        *floor_z = scene_height_world(cell->floor_height_step);
    }
    return true;
}

static LightLevel sprite_light_level(const Map *map, double x, double y) {
    int map_x = (int)floor(x);
    int map_y = (int)floor(y);
    if (!map->light_map || !map_in_bounds(map, map_x, map_y))
        return (LightLevel){1.0, 1.0, 1.0};
    return map->light_map[(size_t)map_y * (size_t)map->width + (size_t)map_x];
}

static bool sprite_project(
    const Grid *grid, const Camera *camera, double world_x, double world_y,
    double *screen_center_x, double *depth
) {
    double dx = world_x - camera->transform.pos.x;
    double dy = world_y - camera->transform.pos.y;
    double dir_x = cos(camera->transform.angle);
    double dir_y = sin(camera->transform.angle);
    double plane_scale = tan(camera->fov / 2.0);
    double plane_x = -dir_y * plane_scale;
    double plane_y = dir_x * plane_scale;
    double determinant = plane_x * dir_y - dir_x * plane_y;
    double inverse;
    double transform_x;
    if (!isfinite(determinant) || fabs(determinant) < 0.000001) return false;
    inverse = 1.0 / determinant;
    transform_x = inverse * (dir_y * dx - dir_x * dy);
    *depth = inverse * (-plane_y * dx + plane_x * dy);
    if (!isfinite(*depth) || *depth <= 0.001) return false;
    *screen_center_x = (grid->width / 2.0) * (1.0 + transform_x / *depth);
    return isfinite(*screen_center_x);
}

static void render_sprite(
    Grid *grid, const Map *map, const Camera *camera,
    const AssetRegistry *assets, const SpriteEntity *instance,
    const SceneHeightView *heights, const double *column_depths,
    bool bounded_occlusion, double *overlay_depth
) {
    const SpriteAsset *sprite;
    LightLevel light;
    double floor_z;
    double center_x;
    double depth;
    double top_y;
    double bottom_y;
    double projected_width;
    int min_x;
    int max_x;
    int min_y;
    int max_y;
    if (instance->sprite_id <= 0 ||
        instance->sprite_id >= (int)SPRITE_ID_CAPACITY) return;
    sprite = asset_registry_get_sprite_frame(
        assets, instance->sprite_id, instance->animation_frame);
    if (!sprite) return;
    if (!sprite->pattern || sprite->cols <= 0 || sprite->rows <= 0 ||
        sprite->cols > 255 || sprite->rows > 32) return;
    if (!sprite_anchor_floor(map, heights, instance->pos.x, instance->pos.y,
                             &floor_z) ||
        !sprite_project(grid, camera, instance->pos.x, instance->pos.y,
                        &center_x, &depth)) return;

    bottom_y = height_project_y(camera, grid->height, floor_z, depth);
    top_y = height_project_y(camera, grid->height, floor_z + 1.0, depth);
    projected_width = (bottom_y - top_y) *
        (double)sprite->cols / (double)sprite->rows;
    if (!isfinite(projected_width) || projected_width <= 0.0) return;
    min_x = (int)floor(center_x - projected_width / 2.0);
    max_x = (int)ceil(center_x + projected_width / 2.0) - 1;
    min_y = (int)floor(top_y);
    max_y = (int)ceil(bottom_y) - 1;
    if (min_x < 0) min_x = 0;
    if (max_x >= grid->width) max_x = grid->width - 1;
    if (min_y < 0) min_y = 0;
    if (max_y >= grid->height) max_y = grid->height - 1;
    if (min_x > max_x || min_y > max_y) return;
    light = sprite_light_level(map, instance->pos.x, instance->pos.y);

    for (int y = min_y; y <= max_y; y++) {
        int source_y = (int)(((y + 0.5) - top_y) * sprite->rows /
                             (bottom_y - top_y));
        if (source_y < 0 || source_y >= sprite->rows) continue;
        for (int x = min_x; x <= max_x; x++) {
            int source_x = (int)(((x + 0.5) -
                (center_x - projected_width / 2.0)) * sprite->cols /
                projected_width);
            size_t output_index;
            PatternCell pattern_cell;
            const Material *material;
            Cell existing;
            if (source_x < 0 || source_x >= sprite->cols) continue;
            pattern_cell = sprite->pattern[
                (size_t)source_y * (size_t)sprite->cols + (size_t)source_x];
            if (pattern_cell.glyph == 0U || pattern_cell.glyph == (uint8_t)' ' ||
                !material_id_is_loaded(assets, pattern_cell.material_id)) continue;
            output_index = (size_t)y * (size_t)grid->width + (size_t)x;
            if (bounded_occlusion) {
                if (grid->world_depths[output_index] + WORLD_OCCLUSION_EPSILON <
                    depth) continue;
            } else if (column_depths && depth > column_depths[x] + 0.001) {
                continue;
            }
            if (depth >= overlay_depth[output_index] - SPRITE_DEPTH_EPSILON)
                continue;
            material = &assets->materials[pattern_cell.material_id];
            if (material->palette_id < 0 || material->palette_id > ASSET_ID_MAX)
                continue;
            if (!grid_get(grid, x, y, &existing)) continue;
            grid_set(grid, x, y, pattern_cell.glyph,
                     palette_sample(&assets->palettes[material->palette_id],
                                    depth, light), existing.bg);
            overlay_depth[output_index] = depth;
        }
    }
}

void sprite_render_world_overlays(
    Grid *grid, const Map *map, const Camera *camera,
    const AssetRegistry *assets, const WorldState *world,
    const SceneHeightView *heights, const double *column_depths,
    bool bounded_occlusion, double *overlay_depth
) {
    if (!grid || !map || !camera || !assets || !world || !overlay_depth ||
        !grid->cells || !grid->world_depths || grid->width <= 0 ||
        grid->height <= 0) return;
    for (int i = 0; i < world->num_sprites && i < MAX_SPRITES; i++) {
        render_sprite(grid, map, camera, assets, &world->sprites[i], heights,
                      column_depths, bounded_occlusion, overlay_depth);
    }
}