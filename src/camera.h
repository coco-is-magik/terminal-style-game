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

/**
 * Camera — The player's viewpoint in the world
 *
 * transform — position (x, y) and yaw angle (radians)
 * fov       — horizontal field of view (radians, e.g. PI/2 = 90°)
 * pitch     — vertical look offset in grid cells (positive shifts horizon downward)
 */
typedef struct {
    Entity transform;      /* Position + yaw angle */
    double fov;            /* Horizontal field of view (radians) */
    double pitch;          /* Vertical look offset (cells, clamped ±100) */
} Camera;

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
 */
void camera_update(Camera *cam, Map *map, InputState *input, double delta_time_sec);

#endif /* CAMERA_H */