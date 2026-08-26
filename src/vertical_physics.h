/**
 * vertical_physics.h — Deterministic heightfield movement reconciliation
 *
 * Consumes borrowed scene data and mutates runtime camera/state only. It owns no
 * authored state, rendering, input, allocation, or global time.
 */
#ifndef VERTICAL_PHYSICS_H
#define VERTICAL_PHYSICS_H

#include "camera.h"
#include "height_view.h"
#include "map.h"
#include "optical_runtime_view.h"

#include <stdbool.h>

typedef struct {
    double velocity_x;
    double velocity_y;
    double velocity_z;
    bool grounded;
    bool initialized;
} VerticalPhysicsState;

typedef enum {
    VERTICAL_PHYSICS_OK = 0,
    VERTICAL_PHYSICS_BLOCKED_STEP,
    VERTICAL_PHYSICS_BLOCKED_CLEARANCE,
    VERTICAL_PHYSICS_NOT_GROUNDED,
    VERTICAL_PHYSICS_INVALID_ARGUMENT
} VerticalPhysicsResult;

void vertical_physics_init(VerticalPhysicsState *state);

/** Snap runtime Z to the current floor and clear velocity. */
VerticalPhysicsResult vertical_physics_reset(
    VerticalPhysicsState *state,
    Camera *camera,
    const Map *map,
    const SceneHeightView *heights
);
VerticalPhysicsResult vertical_physics_reset_optical(
    VerticalPhysicsState *state, Camera *camera, const Map *map,
    const SceneHeightView *heights, const OpticalRuntimeView *optical_view,
    uint32_t optical_generation
);

/**
 * Reconcile a camera movement proposal and advance gravity for delta seconds.
 * previous_x/y are the position before the caller's ordinary ground movement.
 */
VerticalPhysicsResult vertical_physics_step(
    VerticalPhysicsState *state,
    Camera *camera,
    const Map *map,
    const SceneHeightView *heights,
    double previous_x,
    double previous_y,
    double delta_seconds
);
VerticalPhysicsResult vertical_physics_step_optical(
    VerticalPhysicsState *state, Camera *camera, const Map *map,
    const SceneHeightView *heights, double previous_x, double previous_y,
    double delta_seconds, const OpticalRuntimeView *optical_view,
    uint32_t optical_generation
);

/** Apply jump_impulse opposite the effective local gravity vector. */
VerticalPhysicsResult vertical_physics_jump(
    VerticalPhysicsState *state,
    Camera *camera,
    const Map *map,
    const SceneHeightView *heights
);
VerticalPhysicsResult vertical_physics_jump_optical(
    VerticalPhysicsState *state, Camera *camera, const Map *map,
    const SceneHeightView *heights, const OpticalRuntimeView *optical_view,
    uint32_t optical_generation
);

#endif /* VERTICAL_PHYSICS_H */