/**
 * camera.h — First-person camera
 *
 * The Camera extends the basic Entity with a horizontal field-of-view
 * (fov) and a vertical pitch offset for looking up and down.
 *
 * The transform (position + angle) inherited from Entity handles
 * location and yaw.  fov and pitch control the view cone.
 *
 * See camera.c for the implementation.
 */

#ifndef CAMERA_H
#define CAMERA_H

#include "entity.h"    /* Entity struct — camera's position + angle */
#include "map.h"       /* Map — needed for collision in camera_update() */
#include "input.h"     /* InputState — WASD + mouse for movement/look */
#include "optical_runtime_view.h"
#include "world.h"

/**
 * Camera — The player's viewpoint in the world
 *
 * transform — position (x, y) and yaw angle (radians)
 * fov       — horizontal field of view (radians, e.g. PI/2 = 90°)
 * pitch     — horizon offset in logical grid rows (positive shifts horizon down)
 */
typedef struct {
    Entity transform;      /* Position + yaw angle */
    double fov;            /* Horizontal field of view (radians) */
    double pitch;          /* 2.5D horizon offset in logical grid rows */
    double z;              /* Runtime eye Z in world units; not authored. */
} Camera;

double camera_clamp_horizon_offset(double offset, int viewport_rows);

/**
 * camera_init() — Initialise a Camera with position, angle, and FOV
 *
 * @param cam    Pointer to Camera to initialise (NULL-safe)
 * @param x      Starting world X position
 * @param y      Starting world Y position
 * @param angle  Starting yaw angle (radians, 0 = east)
 * @param fov    Horizontal field of view (radians)
 */
void camera_init(Camera *cam, double x, double y, double angle, double fov);

/**
 * camera_update() — Per-frame camera movement and rotation
 *
 * Reads mouse deltas for yaw/pitch and WASD for movement, with
 * collision detection against the map.  Movement is scaled by
 * delta_time_sec for frame-rate independence.
 *
 * @param cam              Camera to update (NULL-safe)
 * @param map              Map for collision queries (NULL-safe)
 * @param input            Current input state
 * @param delta_time_sec   Time since last frame (seconds)
 * @param viewport_rows    Active logical viewport height
 */
void camera_update(Camera *cam, Map *map, InputState *input,
                   double delta_time_sec, int viewport_rows);
void camera_update_optical(
    Camera *cam, Map *map, InputState *input,
    double delta_time_sec, int viewport_rows,
    const OpticalRuntimeView *optical_view, uint32_t optical_generation
);
void camera_update_with_objects(
    Camera *cam, Map *map, InputState *input,
    double delta_time_sec, int viewport_rows,
    const OpticalRuntimeView *optical_view, uint32_t optical_generation,
    const ObjectEntity *objects, size_t object_count
);

#endif /* CAMERA_H */