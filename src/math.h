/**
 * math.h — Common math constants, types, and utility function declarations
 *
 * This header provides the basic mathematical primitives used throughout
 * the engine:
 *
 *   PI        — A high-precision definition of π for trigonometric calculations
 *   Vec2      — A simple 2D vector/point struct used by entities, camera,
 *               lights, sprites, and decals throughout the codebase
 *   normalize_angle() — Wraps any angle (in radians) into the range [0, 2π)
 *
 * Note: This is a minimal utility header.  More complex math operations
 * (ray-wall intersection, distance calculations, etc.) are implemented
 * inline where they're needed rather than factored into shared functions.
 */

#ifndef MATH_UTILS_H
#define MATH_UTILS_H

/**
 * PI — The mathematical constant π (pi)
 *
 * Defined to 20 decimal places, which provides enough precision for all
 * double-precision trigonometry in the engine.  Used for:
 *   - Converting between degrees and radians
 *   - Defining field-of-view angles (e.g. PI/2 = 90°)
 *   - Computing perpendicular vectors (angle + PI/2 for strafing)
 *   - Raycasting angle calculations
 */
#define PI 3.14159265358979323846

/**
 * Vec2 — A 2D floating-point vector / point
 *
 * Used throughout the engine for:
 *   - Entity positions (camera, player)
 *   - Light source positions
 *   - Sprite and decal world coordinates
 *   - Direction vectors (normalised for movement)
 *   - Map / grid coordinates (cast to int when needed)
 *
 * Stored as (x, y) where +X = east, +Y = north/south (depending on
 * convention; the map grid uses row-major: X = column, Y = row).
 */
typedef struct {
    double x;    /* X component: horizontal position / direction */
    double y;    /* Y component: vertical position / direction */
} Vec2;

/**
 * normalize_angle() — Wrap an angle into the range [0, 2π)
 *
 * Repeatedly adds or subtracts 2π until the angle falls within the
 * standard [0, 2π) range.  This is important because angles accumulate
 * over time (from mouse rotation) and can grow arbitrarily large,
 * which degrades the precision of sin()/cos() calculations and makes
 * comparisons unreliable.
 *
 * @param angle  Any angle in radians (positive, negative, or very large)
 * @return       The equivalent angle in the range [0, 2π)
 */
double normalize_angle(double angle);

#endif /* MATH_UTILS_H */