/**
 * editor_selection.c — Typed selection from the center-camera ray
 *
 * Coordinate convention (confirmed against camera/raycast):
 *   angle 0 = +X/east, PI/2 = +Y/south (Y increases south).
 *
 * Face from DDA side + ray direction (plan §10):
 *   side == 0 && ray_dir_x > 0 -> WEST
 *   side == 0 && ray_dir_x < 0 -> EAST
 *   side == 1 && ray_dir_y > 0 -> NORTH
 *   side == 1 && ray_dir_y < 0 -> SOUTH
 */

#include "editor_selection.h"
#include "height_projection.h"
#include "heightfield_trace.h"
#include "config.h"
#include "raycast.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

static bool selection_targets_equal(SelectionTarget a, SelectionTarget b) {
    if (a.type != b.type) return false;
    if (a.type == SELECTION_WALL_FACE)
        return a.value.wall_face.map_x == b.value.wall_face.map_x &&
            a.value.wall_face.map_y == b.value.wall_face.map_y &&
            a.value.wall_face.face == b.value.wall_face.face;
    if (a.type == SELECTION_FLOOR || a.type == SELECTION_CEILING)
        return a.value.horizontal.map_x == b.value.horizontal.map_x &&
            a.value.horizontal.map_y == b.value.horizontal.map_y;
    if (a.type == SELECTION_LIGHT)
        return a.value.light.id == b.value.light.id;
    if (a.type == SELECTION_DECAL)
        return a.value.decal.id == b.value.decal.id;
    if (a.type == SELECTION_SPRITE)
        return a.value.sprite.id == b.value.sprite.id;
    return a.type == SELECTION_NONE;
}

void editor_selection_set_clear(EditorSelectionSet *set) {
    if (!set) return;
    memset(set, 0, sizeof(*set));
}

bool editor_selection_set_reset(EditorSelectionSet *set, SelectionTarget primary) {
    if (!set || primary.type == SELECTION_NONE) return false;
    editor_selection_set_clear(set);
    set->members[0] = primary;
    set->count = 1U;
    return true;
}

bool editor_selection_set_contains(const EditorSelectionSet *set,
                                   SelectionTarget target) {
    size_t i;
    if (!set) return false;
    for (i = 0U; i < set->count; i++)
        if (selection_targets_equal(set->members[i], target)) return true;
    return false;
}

bool editor_selection_set_add(EditorSelectionSet *set, SelectionTarget target) {
    if (!set || target.type == SELECTION_NONE) return false;
    if (editor_selection_set_contains(set, target)) {
        size_t i;
        for (i = 0U; i < set->count; i++)
            if (selection_targets_equal(set->members[i], target)) {
                set->primary_index = i;
                return true;
            }
    }
    if (set->count >= EDITOR_SELECTION_SET_CAPACITY) return false;
    set->members[set->count] = target;
    set->primary_index = set->count++;
    return true;
}

void editor_selection_set_revalidate(EditorSelectionSet *set, const Map *map) {
    size_t read_index;
    size_t write_index = 0U;
    size_t new_primary = 0U;
    bool primary_survived = false;
    if (!set) return;
    for (read_index = 0U; read_index < set->count; read_index++) {
        if (!editor_selection_is_valid_for_map(set->members[read_index], map)) continue;
        set->members[write_index] = set->members[read_index];
        if (read_index == set->primary_index) {
            new_primary = write_index;
            primary_survived = true;
        }
        write_index++;
    }
    set->count = write_index;
    set->primary_index = write_index == 0U ? 0U
        : (primary_survived ? new_primary : write_index - 1U);
}

const SelectionTarget *editor_selection_set_primary(const EditorSelectionSet *set) {
    if (!set || set->count == 0U || set->primary_index >= set->count) return NULL;
    return &set->members[set->primary_index];
}

WallFace editor_calculate_wall_face(
    int side,
    double ray_dir_x,
    double ray_dir_y
) {
    if (side == 0) {
        return (ray_dir_x > 0.0) ? WALL_FACE_WEST : WALL_FACE_EAST;
    }
    return (ray_dir_y > 0.0) ? WALL_FACE_NORTH : WALL_FACE_SOUTH;
}

WallMaterialRef editor_wall_face_to_material_ref(WallFaceRef face) {
    WallMaterialRef ref;
    ref.map_x = face.map_x;
    ref.map_y = face.map_y;
    return ref;
}

bool editor_selection_is_valid_for_map(
    SelectionTarget selection,
    const Map *map
) {
    if (!map) {
        return false;
    }

    if (selection.type == SELECTION_FLOOR ||
        selection.type == SELECTION_CEILING) {
        return map_in_bounds(map,
            selection.value.horizontal.map_x,
            selection.value.horizontal.map_y);
    }
    if (selection.type != SELECTION_WALL_FACE) return false;

    int x = selection.value.wall_face.map_x;
    int y = selection.value.wall_face.map_y;

    /* map_get / map_in_bounds take non-const Map*; they do not mutate. */
    Map *mutable_map = (Map *)map;
    if (!map_in_bounds(mutable_map, x, y)) {
        return false;
    }

    MapCell *cell = map_get(mutable_map, x, y);
    if (!cell || cell->material_id <= 0) {
        return false;
    }

    return true;
}

EditorHit editor_raycast_selection(
    const Camera *camera,
    const Map *map
) {
    EditorHit hit;
    hit.valid = false;
    hit.distance = 0.0;
    hit.target.type = SELECTION_NONE;

    if (!camera || !map) {
        return hit;
    }

    double max_dist = config_get()->raycast_max_distance;
    if (max_dist <= 0.0) {
        max_dist = 20.0;
    }

    /* raycast_fire does not mutate map or camera; API is non-const historically. */
    RayResult res = raycast_fire((Map *)map, (Camera *)camera,
                                 camera->transform.angle, max_dist);
    if (!res.hit) {
        return hit;
    }

    double ray_dir_x = cos(camera->transform.angle);
    double ray_dir_y = sin(camera->transform.angle);
    WallFace face = editor_calculate_wall_face(res.side, ray_dir_x, ray_dir_y);

    hit.valid = true;
    hit.distance = res.distance;
    hit.target.type = SELECTION_WALL_FACE;
    hit.target.value.wall_face.map_x = res.map_x;
    hit.target.value.wall_face.map_y = res.map_y;
    hit.target.value.wall_face.face = face;
    return hit;
}


EditorHit editor_pick_light_selection(
    const Camera *camera,
    const SceneLight *lights,
    size_t light_count,
    EditorHit wall_hit,
    double max_distance,
    double pick_radius
) {
    EditorHit result = wall_hit;
    double limit;
    double radius_squared;
    double dir_x;
    double dir_y;
    double best_forward = 0.0;
    SceneInstanceId best_id = SCENE_INSTANCE_ID_INVALID;
    size_t i;

    if (!camera || (!lights && light_count > 0U) ||
        !isfinite(max_distance) || max_distance <= 0.0 ||
        !isfinite(pick_radius) || pick_radius <= 0.0) {
        return result;
    }

    limit = wall_hit.valid ? wall_hit.distance : max_distance;
    if (!isfinite(limit) || limit <= 0.0 || limit > max_distance) {
        limit = max_distance;
    }
    radius_squared = pick_radius * pick_radius;
    dir_x = cos(camera->transform.angle);
    dir_y = sin(camera->transform.angle);

    for (i = 0U; i < light_count; i++) {
        const SceneLight *light = &lights[i];
        double offset_x;
        double offset_y;
        double forward;
        double perpendicular;
        double perpendicular_squared;

        if (light->id == SCENE_INSTANCE_ID_INVALID ||
            !isfinite(light->x) || !isfinite(light->y)) {
            continue;
        }
        offset_x = light->x - camera->transform.pos.x;
        offset_y = light->y - camera->transform.pos.y;
        forward = offset_x * dir_x + offset_y * dir_y;
        if (forward <= 0.0 || forward > limit) continue;

        perpendicular = offset_x * dir_y - offset_y * dir_x;
        perpendicular_squared = perpendicular * perpendicular;
        if (perpendicular_squared > radius_squared) continue;

        if (best_id == SCENE_INSTANCE_ID_INVALID ||
            forward < best_forward ||
            (forward == best_forward && light->id < best_id)) {
            best_forward = forward;
            best_id = light->id;
        }
    }

    if (best_id != SCENE_INSTANCE_ID_INVALID) {
        result.valid = true;
        result.distance = best_forward;
        result.target.type = SELECTION_LIGHT;
        result.target.value.light.id = best_id;
    }
    return result;
}

EditorHit editor_pick_sprite_selection(
    const Camera *camera, const SceneSpriteInstance *sprites,
    size_t sprite_count, EditorHit existing_hit, double max_distance,
    double pick_radius
) {
    EditorHit result = existing_hit;
    double limit;
    double radius_squared;
    double dir_x;
    double dir_y;
    double best = 0.0;
    SceneInstanceId best_id = SCENE_INSTANCE_ID_INVALID;
    size_t i;
    if (!camera || (!sprites && sprite_count > 0U) ||
        !isfinite(max_distance) || max_distance <= 0.0 ||
        !isfinite(pick_radius) || pick_radius <= 0.0) return result;
    limit = existing_hit.valid ? existing_hit.distance : max_distance;
    if (!isfinite(limit) || limit <= 0.0 || limit > max_distance) limit = max_distance;
    radius_squared = pick_radius * pick_radius;
    dir_x = cos(camera->transform.angle);
    dir_y = sin(camera->transform.angle);
    for (i = 0U; i < sprite_count; i++) {
        double dx = sprites[i].x - camera->transform.pos.x;
        double dy = sprites[i].y - camera->transform.pos.y;
        double forward = dx * dir_x + dy * dir_y;
        double side = dx * dir_y - dy * dir_x;
        if (sprites[i].id == SCENE_INSTANCE_ID_INVALID || !isfinite(dx) ||
            !isfinite(dy) || forward <= 0.0 || forward >= limit ||
            side * side > radius_squared ||
            (best_id != SCENE_INSTANCE_ID_INVALID && forward >= best)) continue;
        best = forward;
        best_id = sprites[i].id;
    }
    if (best_id != SCENE_INSTANCE_ID_INVALID) {
        result.valid = true;
        result.distance = best;
        result.target.type = SELECTION_SPRITE;
        result.target.value.sprite.id = best_id;
    }
    return result;
}

EditorHit editor_pick_horizontal_surface_selection(
    const Camera *camera,
    const Map *map,
    EditorHit existing_hit,
    int viewport_rows,
    double max_distance
) {
    return editor_pick_horizontal_surface_selection_height(
        camera, map, NULL, existing_hit, viewport_rows, max_distance);
}

EditorHit editor_pick_horizontal_surface_selection_height(
    const Camera *camera, const Map *map, const SceneHeightView *heights,
    EditorHit existing_hit, int viewport_rows, double max_distance
) {
    EditorHit result = existing_hit;
    double distance;
    int map_x;
    int map_y;
    SelectionType type;

    if (!camera || !map || viewport_rows <= 0 ||
        !isfinite(camera->transform.pos.x) ||
        !isfinite(camera->transform.pos.y) ||
        !isfinite(camera->transform.angle) || !isfinite(camera->pitch) ||
        !isfinite(max_distance) || max_distance <= 0.0) return result;
    if (scene_height_view_is_valid(heights, map->width, map->height)) {
        HeightfieldHit traced = heightfield_trace_screen_sample(
            camera, map, heights, 1, viewport_rows, 0, viewport_rows / 2,
            max_distance);
        SelectionType traced_type;
        if (!traced.hit ||
            (traced.kind != HEIGHTFIELD_HIT_FLOOR &&
             traced.kind != HEIGHTFIELD_HIT_CEILING)) return result;
        traced_type = traced.kind == HEIGHTFIELD_HIT_FLOOR
            ? SELECTION_FLOOR : SELECTION_CEILING;
        if (existing_hit.valid && (!isfinite(existing_hit.distance) ||
            existing_hit.distance <= traced.distance)) return result;
        result.valid = true;
        result.distance = traced.distance;
        result.target.type = traced_type;
        result.target.value.horizontal.map_x = traced.map_x;
        result.target.value.horizontal.map_y = traced.map_y;
        return result;
    }
    type = camera->pitch < 0.0 ? SELECTION_FLOOR : SELECTION_CEILING;
    if (!editor_project_horizontal_cell_height(camera, heights, 1, viewport_rows, 0,
            viewport_rows / 2, type, &distance, &map_x, &map_y) ||
        distance > max_distance) return result;
    if (existing_hit.valid && (!isfinite(existing_hit.distance) ||
        existing_hit.distance <= distance)) return result;

    if (!map_in_bounds(map, map_x, map_y)) return result;
    result.valid = true;
    result.distance = distance;
    result.target.type = type;
    result.target.value.horizontal.map_x = map_x;
    result.target.value.horizontal.map_y = map_y;
    return result;
}

bool editor_project_horizontal_cell(
    const Camera *camera, int viewport_width, int viewport_height,
    int screen_x, int screen_y, SelectionType type,
    double *out_distance, int *out_map_x, int *out_map_y
) {
    return editor_project_horizontal_cell_height(
        camera, NULL, viewport_width, viewport_height, screen_x, screen_y, type,
        out_distance, out_map_x, out_map_y);
}

bool editor_project_horizontal_cell_height(
    const Camera *camera, const SceneHeightView *heights,
    int viewport_width, int viewport_height, int screen_x, int screen_y,
    SelectionType type, double *out_distance, int *out_map_x, int *out_map_y
) {
    double camera_x;
    double ray_angle;
    double denominator;
    double current_distance;
    double correction;
    double distance;
    double world_x;
    double world_y;
    if (!camera || !out_distance || !out_map_x || !out_map_y ||
        viewport_width <= 0 || viewport_height <= 0 || screen_x < 0 ||
        screen_x >= viewport_width || screen_y < 0 || screen_y >= viewport_height ||
        (type != SELECTION_FLOOR && type != SELECTION_CEILING) ||
        !isfinite(camera->transform.pos.x) || !isfinite(camera->transform.pos.y) ||
        !isfinite(camera->transform.angle) || !isfinite(camera->fov) ||
        !isfinite(camera->pitch) || camera->fov <= 0.0) return false;
    denominator = type == SELECTION_FLOOR
        ? 2.0 * (screen_y - camera->pitch) - viewport_height
        : viewport_height - 2.0 * (screen_y - camera->pitch);
    if (denominator <= 0.001) return false;
    camera_x = 2.0 * (screen_x + 0.5) / viewport_width - 1.0;
    ray_angle = camera->transform.angle + atan(camera_x * tan(camera->fov / 2.0));
    correction = cos(ray_angle - camera->transform.angle);
    if (!isfinite(correction) || correction <= 0.0) return false;
    current_distance = viewport_height / denominator;
    distance = current_distance / correction;
    world_x = camera->transform.pos.x + distance * cos(ray_angle);
    world_y = camera->transform.pos.y + distance * sin(ray_angle);
    if (scene_height_view_is_valid(heights, heights ? heights->width : 0,
                                   heights ? heights->height : 0)) {
        int map_x = (int)floor(world_x);
        int map_y = (int)floor(world_y);
        if (map_x >= 0 && map_x < heights->width &&
            map_y >= 0 && map_y < heights->height) {
            const SceneAuthoredCell *cell = &heights->cells[
                (size_t)map_y * (size_t)heights->width + (size_t)map_x];
            double plane_z = scene_height_world(type == SELECTION_FLOOR
                ? cell->floor_height_step : cell->ceiling_height_step);
            double vertical_delta = type == SELECTION_FLOOR
                ? camera->z - plane_z : plane_z - camera->z;
            if (vertical_delta <= 0.0 || !isfinite(vertical_delta)) return false;
            current_distance = 2.0 * vertical_delta * viewport_height / denominator;
            distance = current_distance / correction;
            world_x = camera->transform.pos.x + distance * cos(ray_angle);
            world_y = camera->transform.pos.y + distance * sin(ray_angle);
        }
    }
    if (!isfinite(distance) || distance <= 0.0 ||
        !isfinite(world_x) || !isfinite(world_y)) return false;
    *out_distance = distance;
    *out_map_x = (int)floor(world_x);
    *out_map_y = (int)floor(world_y);
    return true;
}
