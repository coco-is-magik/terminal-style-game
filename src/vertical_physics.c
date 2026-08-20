#include "vertical_physics.h"

#include "height_projection.h"

#include <math.h>
#include <string.h>

#define VERTICAL_PHYSICS_MAX_STEP_SECONDS (1.0 / 120.0)
#define VERTICAL_PHYSICS_EPSILON 0.000001

static bool cell_at(const Map *map, const SceneHeightView *heights,
                    double x, double y, const SceneAuthoredCell **out_cell) {
    int map_x;
    int map_y;
    size_t index;
    if (!map || !out_cell || !isfinite(x) || !isfinite(y) ||
        !scene_height_view_is_valid(heights, map->width, map->height)) return false;
    map_x = (int)floor(x);
    map_y = (int)floor(y);
    if (!map_in_bounds(map, map_x, map_y)) return false;
    index = (size_t)map_y * (size_t)map->width + (size_t)map_x;
    if (map->cells[index].material_id != 0 ||
        heights->cells[index].occupancy != SCENE_CELL_OCCUPANCY_EMPTY) return false;
    *out_cell = &heights->cells[index];
    return true;
}

static double floor_height(const SceneAuthoredCell *cell) {
    return scene_height_world(cell->floor_height_step);
}

static double ceiling_height(const SceneAuthoredCell *cell) {
    return scene_height_world(cell->ceiling_height_step);
}

static bool interval_allows(const SceneAuthoredCell *cell,
                            const SceneMovementParameters *movement) {
    return cell->floor_present && cell->ceiling_present &&
           ceiling_height(cell) - floor_height(cell) + VERTICAL_PHYSICS_EPSILON >=
           movement->head_clearance;
}

static bool body_fits_interval(const SceneAuthoredCell *cell,
                               const SceneMovementParameters *movement,
                               double camera_z) {
    double base_z = camera_z - movement->eye_height;
    return interval_allows(cell, movement) &&
        base_z >= floor_height(cell) - VERTICAL_PHYSICS_EPSILON &&
        base_z + movement->head_clearance <=
            ceiling_height(cell) + VERTICAL_PHYSICS_EPSILON;
}

static SceneGravityOrientation effective_orientation(
    const SceneAuthoredCell *cell, const SceneMovementParameters *movement
) {
    return cell->gravity_orientation == SCENE_GRAVITY_INHERIT
        ? movement->gravity_orientation
        : (SceneGravityOrientation)cell->gravity_orientation;
}

static double effective_magnitude(const SceneAuthoredCell *cell,
                                  const SceneMovementParameters *movement) {
    if (cell->gravity_scale_step == 0U) return movement->gravity_magnitude;
    return movement->gravity_magnitude *
           (double)cell->gravity_scale_step /
           (double)SCENE_HEIGHT_STEPS_PER_UNIT;
}

static void gravity_vector(const SceneAuthoredCell *cell,
                           const SceneMovementParameters *movement,
                           double *out_x, double *out_y, double *out_z) {
    double magnitude = effective_magnitude(cell, movement);
    *out_x = 0.0; *out_y = 0.0; *out_z = 0.0;
    switch (effective_orientation(cell, movement)) {
        case SCENE_GRAVITY_DOWN: *out_z = -magnitude; break;
        case SCENE_GRAVITY_UP: *out_z = magnitude; break;
        case SCENE_GRAVITY_NORTH: *out_y = -magnitude; break;
        case SCENE_GRAVITY_SOUTH: *out_y = magnitude; break;
        case SCENE_GRAVITY_EAST: *out_x = magnitude; break;
        case SCENE_GRAVITY_WEST: *out_x = -magnitude; break;
        default: break;
    }
}

void vertical_physics_init(VerticalPhysicsState *state) {
    if (!state) return;
    memset(state, 0, sizeof(*state));
}

VerticalPhysicsResult vertical_physics_reset(
    VerticalPhysicsState *state, Camera *camera, const Map *map,
    const SceneHeightView *heights
) {
    const SceneAuthoredCell *cell;
    if (!state || !camera || !cell_at(map, heights,
            camera->transform.pos.x, camera->transform.pos.y, &cell)) {
        return VERTICAL_PHYSICS_INVALID_ARGUMENT;
    }
    if (!interval_allows(cell, &heights->movement))
        return VERTICAL_PHYSICS_BLOCKED_CLEARANCE;
    memset(state, 0, sizeof(*state));
    camera->z = floor_height(cell) + heights->movement.eye_height;
    state->grounded = true;
    state->initialized = true;
    return VERTICAL_PHYSICS_OK;
}

VerticalPhysicsResult vertical_physics_jump(
    VerticalPhysicsState *state, Camera *camera, const Map *map,
    const SceneHeightView *heights
) {
    const SceneAuthoredCell *cell;
    double gx, gy, gz;
    double magnitude;
    if (!state || !camera || !state->initialized || !state->grounded ||
        !cell_at(map, heights, camera->transform.pos.x,
                 camera->transform.pos.y, &cell)) {
        return state && state->initialized
            ? VERTICAL_PHYSICS_NOT_GROUNDED
            : VERTICAL_PHYSICS_INVALID_ARGUMENT;
    }
    gravity_vector(cell, &heights->movement, &gx, &gy, &gz);
    magnitude = sqrt(gx * gx + gy * gy + gz * gz);
    if (!isfinite(magnitude) || magnitude <= 0.0)
        return VERTICAL_PHYSICS_INVALID_ARGUMENT;
    state->velocity_x = -gx / magnitude * heights->movement.jump_impulse;
    state->velocity_y = -gy / magnitude * heights->movement.jump_impulse;
    state->velocity_z = -gz / magnitude * heights->movement.jump_impulse;
    state->grounded = false;
    return VERTICAL_PHYSICS_OK;
}

static VerticalPhysicsResult reconcile_ground_move(
    VerticalPhysicsState *state, Camera *camera, const Map *map,
    const SceneHeightView *heights, double previous_x, double previous_y
) {
    const SceneAuthoredCell *target;
    double base_z = camera->z - heights->movement.eye_height;
    double target_floor;
    double delta;
    if (!cell_at(map, heights, camera->transform.pos.x,
                 camera->transform.pos.y, &target)) {
        camera->transform.pos.x = previous_x;
        camera->transform.pos.y = previous_y;
        return VERTICAL_PHYSICS_BLOCKED_CLEARANCE;
    }
    if (!interval_allows(target, &heights->movement) ||
        base_z + heights->movement.head_clearance >
            ceiling_height(target) + VERTICAL_PHYSICS_EPSILON) {
        camera->transform.pos.x = previous_x;
        camera->transform.pos.y = previous_y;
        return VERTICAL_PHYSICS_BLOCKED_CLEARANCE;
    }
    target_floor = floor_height(target);
    delta = target_floor - base_z;
    if (delta > heights->movement.step_height + VERTICAL_PHYSICS_EPSILON) {
        camera->transform.pos.x = previous_x;
        camera->transform.pos.y = previous_y;
        return VERTICAL_PHYSICS_BLOCKED_STEP;
    }
    if (delta >= -VERTICAL_PHYSICS_EPSILON) {
        camera->z = target_floor + heights->movement.eye_height;
        state->velocity_z = 0.0;
        state->grounded = true;
    } else {
        state->grounded = false;
    }
    return VERTICAL_PHYSICS_OK;
}

static void integrate_airborne(VerticalPhysicsState *state, Camera *camera,
                               const Map *map, const SceneHeightView *heights,
                               double seconds) {
    const SceneAuthoredCell *cell;
    double ax, ay, az;
    double proposed_x;
    double proposed_y;
    double proposed_z;
    double base_z;
    SceneGravityOrientation orientation;
    if (!cell_at(map, heights, camera->transform.pos.x,
                 camera->transform.pos.y, &cell)) return;
    gravity_vector(cell, &heights->movement, &ax, &ay, &az);
    orientation = effective_orientation(cell, &heights->movement);
    proposed_x = camera->transform.pos.x + state->velocity_x * seconds +
                 0.5 * ax * seconds * seconds;
    proposed_y = camera->transform.pos.y + state->velocity_y * seconds +
                 0.5 * ay * seconds * seconds;
    proposed_z = camera->z + state->velocity_z * seconds +
                 0.5 * az * seconds * seconds;
    state->velocity_x += ax * seconds;
    state->velocity_y += ay * seconds;
    state->velocity_z += az * seconds;
    {
        const SceneAuthoredCell *target;
        if (cell_at(map, heights, proposed_x, proposed_y, &target) &&
            body_fits_interval(target, &heights->movement, proposed_z)) {
            camera->transform.pos.x = proposed_x;
            camera->transform.pos.y = proposed_y;
            cell = target;
        } else {
            state->velocity_x = 0.0;
            state->velocity_y = 0.0;
        }
    }
    base_z = proposed_z - heights->movement.eye_height;
    if (base_z <= floor_height(cell) + VERTICAL_PHYSICS_EPSILON &&
        (state->velocity_z < -VERTICAL_PHYSICS_EPSILON ||
         orientation == SCENE_GRAVITY_DOWN)) {
        camera->z = floor_height(cell) + heights->movement.eye_height;
        state->velocity_x = 0.0;
        state->velocity_y = 0.0;
        state->velocity_z = 0.0;
        state->grounded = true;
    } else if (base_z + heights->movement.head_clearance >=
                   ceiling_height(cell) - VERTICAL_PHYSICS_EPSILON &&
               state->velocity_z >= 0.0) {
        camera->z = ceiling_height(cell) - heights->movement.head_clearance +
                    heights->movement.eye_height;
        state->velocity_z = 0.0;
    } else {
        camera->z = proposed_z;
    }
}

VerticalPhysicsResult vertical_physics_step(
    VerticalPhysicsState *state, Camera *camera, const Map *map,
    const SceneHeightView *heights, double previous_x, double previous_y,
    double delta_seconds
) {
    VerticalPhysicsResult result = VERTICAL_PHYSICS_OK;
    double remaining;
    if (!state || !camera || !map ||
        !scene_height_view_is_valid(heights, map->width, map->height) ||
        !isfinite(previous_x) || !isfinite(previous_y) ||
        !isfinite(delta_seconds) || delta_seconds < 0.0) {
        return VERTICAL_PHYSICS_INVALID_ARGUMENT;
    }
    if (!state->initialized) {
        double proposed_x = camera->transform.pos.x;
        double proposed_y = camera->transform.pos.y;
        camera->transform.pos.x = previous_x;
        camera->transform.pos.y = previous_y;
        result = vertical_physics_reset(state, camera, map, heights);
        camera->transform.pos.x = proposed_x;
        camera->transform.pos.y = proposed_y;
        if (result != VERTICAL_PHYSICS_OK) return result;
    }
    if (state->grounded) {
        result = reconcile_ground_move(state, camera, map, heights,
                                       previous_x, previous_y);
        if (result != VERTICAL_PHYSICS_OK) return result;
    } else {
        const SceneAuthoredCell *target;
        if (!cell_at(map, heights, camera->transform.pos.x,
                     camera->transform.pos.y, &target) ||
            !body_fits_interval(target, &heights->movement, camera->z)) {
            camera->transform.pos.x = previous_x;
            camera->transform.pos.y = previous_y;
        }
    }
    remaining = delta_seconds;
    while (!state->grounded && remaining > 0.0) {
        double step = remaining > VERTICAL_PHYSICS_MAX_STEP_SECONDS
            ? VERTICAL_PHYSICS_MAX_STEP_SECONDS : remaining;
        integrate_airborne(state, camera, map, heights, step);
        remaining -= step;
    }
    return result;
}