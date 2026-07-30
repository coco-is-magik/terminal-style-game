/**
 * camera.c — First-person camera controller
 *
 * This file implements the player's "eyes" — a Camera struct that holds
 * a position, viewing angle, field-of-view, and vertical pitch.  The two
 * functions here handle:
 *
 *   1. camera_init()   — Set initial position, angle, and FOV
 *   2. camera_update() — Per-frame update: read mouse/input, rotate,
 *                        pitch up/down, move forward/back/strafe with
 *                        collision detection and sliding.
 *
 * The camera uses a simple Entity transform (position + angle) and adds
 * a vertical pitch offset for looking up/down.  Movement is frame-rate
 * independent via delta_time_sec.
 *
 * Collision is handled as axis-aligned sliding: the X and Y components
 * of movement are tested separately against the map, so if you run into
 * a wall at an angle you slide along it instead of stopping dead.
 */

#include "camera.h"     /* Camera struct, camera_init(), camera_update() */
#include <math.h>        /* cos(), sin(), sqrt() */

double camera_clamp_horizon_offset(double offset, int viewport_rows) {
    if (viewport_rows <= 0 || !isfinite(offset)) {
        return 0.0;
    }

    double limit = (double)viewport_rows;
    if (offset > limit) return limit;
    if (offset < -limit) return -limit;
    return offset;
}

/**
 * camera_init() — Initialise a Camera with a starting position and orientation
 *
 * @param cam    Pointer to the Camera to initialise (NULL-safe)
 * @param x      Starting X position in world/grid coordinates
 * @param y      Starting Y position in world/grid coordinates
 * @param angle  Initial viewing angle (radians, 0 = +X/east, PI/2 = +Y)
 * @param fov    Horizontal field of view (radians, e.g. PI/2 = 90°)
 */
void camera_init(Camera *cam, double x, double y, double angle, double fov) {
    if (!cam) return;
    cam->transform.pos.x = x;
    cam->transform.pos.y = y;
    cam->transform.angle = angle;
    cam->fov = fov;
    cam->pitch = 0.0;    /* Start level — no upward/downward tilt */
}

/**
 * camera_update() — Per-frame camera movement and rotation
 *
 * This function is called once per frame (only when visual_mode is
 * VISUAL_RAYCAST, as guarded in app.c).  It performs three tasks:
 *
 *   1. Yaw rotation   — horizontal look using mouse_dx
 *   2. Pitch          — vertical look using mouse_dy (inverted)
 *   3. Translation    — WASD movement with collision + sliding
 *
 * Mouse deltas (mouse_dx, mouse_dy) come from SDL relative-mouse mode,
 * accumulated across frames by input_process().  They represent the
 * number of pixels the mouse has moved since the last frame.
 *
 * Movement speed is multiplied by delta_time_sec to be frame-rate
 * independent.  Diagonal movement is normalised so you don't go faster
 * when pressing two keys at once.
 *
 * Collision uses a "sliding" approach:
 *   - First, try to move on X.  If the resulting cell is blocked
 *     (material_id != 0), the X component is discarded.
 *   - Then, try to move on Y independently.
 * This lets the player slide along walls at shallow angles.
 *
 * @param cam              Camera to update (NULL-safe)
 * @param map              The Map for collision queries (NULL-safe)
 * @param input            Current InputState with mouse deltas and WASD flags
 * @param delta_time_sec   Time elapsed since the last frame (seconds)
 * @param viewport_rows    Active logical viewport height
 */
void camera_update(Camera *cam, Map *map, InputState *input,
                   double delta_time_sec, int viewport_rows) {
    if (!cam || !map || !input) return;

    /* ================================================================
     *  1. Yaw (horizontal look) — mouse X controls left/right rotation
     * ================================================================ */

    /* rot_speed converts raw mouse pixels to radians.  0.002 rad/pixel
     * is a reasonable sensitivity — adjust via config if needed. */
    double rot_speed = 0.002;
    cam->transform.angle += input->mouse_dx * rot_speed;

    /* Keep the angle in the range [0, 2*PI) so it doesn't grow unbounded */
    cam->transform.angle = normalize_angle(cam->transform.angle);

    /* ================================================================
     *  2. Pitch (vertical look) — mouse Y tilts the view up/down
     * ================================================================ */

    /* Invert mouse Y (subtract) so pushing the mouse forward tilts down,
     * which is the conventional FPS behaviour.  The multiplier 0.5
     * controls vertical sensitivity independently of horizontal. */
    cam->pitch -= input->mouse_dy * 0.5;

    /* This is a 2.5D horizon displacement, not angular camera pitch. */
    cam->pitch = camera_clamp_horizon_offset(cam->pitch, viewport_rows);

    /* ================================================================
     *  3. Translation (WASD movement)
     * ================================================================ */

    /* Base movement speed in cells/second.  At 60 FPS with delta_time_sec
     * ≈ 0.0167, each step moves about 0.05 cells — smooth and small
     * enough for collision to work accurately. */
    double move_speed = 3.0 * delta_time_sec;

    /* Unit vectors for the direction the camera is facing */
    double dir_x = cos(cam->transform.angle);    /* Forward-X */
    double dir_y = sin(cam->transform.angle);    /* Forward-Y */

    /* Unit vectors for strafing (perpendicular to facing direction) */
    double right_x = cos(cam->transform.angle + PI / 2.0);   /* Right-X */
    double right_y = sin(cam->transform.angle + PI / 2.0);   /* Right-Y */

    /* Accumulate raw movement from input flags */
    double move_x = 0;
    double move_y = 0;

    if (input->forward)  { move_x += dir_x;    move_y += dir_y;    }
    if (input->backward) { move_x -= dir_x;    move_y -= dir_y;    }
    if (input->right)    { move_x += right_x;  move_y += right_y;  }
    if (input->left)     { move_x -= right_x;  move_y -= right_y;  }

    /* ------ Normalise diagonal movement ------ */
    /* When moving diagonally (e.g. forward + right), the raw vector
     * length is sqrt(2) ≈ 1.414, which would make diagonal movement
     * ~41% faster.  We normalise to length 1 and then scale by
     * move_speed so all directions are equally fast. */
    double length = sqrt(move_x * move_x + move_y * move_y);
    if (length > 0) {
        move_x = (move_x / length) * move_speed;
        move_y = (move_y / length) * move_speed;
    }

    /* ------ Collision (axis-aligned sliding) ------ */

    /* The player is treated as a circle with this radius (in cells).
     * 0.2 means the player occupies a 0.4×0.4 cell area — small enough
     * to fit through 1-cell-wide corridors but large enough to feel
     * solid against walls. */
    double radius = 0.2;

    /* Check X movement first:
     *   - Project the player's position + move_x, offset by radius in
     *     the direction of movement so we collide at the edge, not centre.
     *   - Look up the map cell at that (x, y) position.
     *   - If the cell exists AND its material_id == 0 (empty/void),
     *     the move is allowed; otherwise it's blocked. */
    int map_x = (int)(cam->transform.pos.x + move_x + (move_x > 0 ? radius : -radius));
    MapCell *cell_x = map_get(map, map_x, (int)cam->transform.pos.y);
    if (cell_x && cell_x->material_id == 0) {
        cam->transform.pos.x += move_x;
    }
    /* If blocked, move_x is discarded — no X movement this frame. */

    /* Check Y movement independently (sliding):
     *   - Same logic, but uses the Y component.
     *   - Because X and Y are checked separately, if X is blocked but Y
     *     is free, the player will slide along the wall in Y. */
    int map_y = (int)(cam->transform.pos.y + move_y + (move_y > 0 ? radius : -radius));
    MapCell *cell_y = map_get(map, (int)cam->transform.pos.x, map_y);
    if (cell_y && cell_y->material_id == 0) {
        cam->transform.pos.y += move_y;
    }
    /* If blocked, move_y is discarded. */
}