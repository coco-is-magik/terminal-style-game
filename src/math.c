/**
 * math.c — Mathematical utility functions
 *
 * This file provides implementations for the simple math utilities declared
 * in math.h.  Currently it contains only a single function:
 *
 *   normalize_angle() — Wraps any angle into the range [0, 2π)
 *
 * The Vec2 type and PI constant are defined in math.h; the actual complex
 * raycasting math (DDA, wall intersection) lives in raycast.c.
 */

#include "math.h"    /* Vec2, PI, normalize_angle() declaration */
#include <math.h>     /* (Standard C math library — not directly used here,
                         but included for consistency with other modules) */

/**
 * normalize_angle() — Wrap an angle into the range [0, 2π)
 *
 * Ensures the angle is always within the canonical [0, 2π) range by
 * repeatedly adding or subtracting 2π as needed.  This is necessary
 * because:
 *
 *   1. Mouse rotation accumulates — after a few full turns the angle
 *      could be ±20π or more.
 *   2. Floating-point precision degrades for very large angles,
 *      making sin()/cos() unreliable.
 *   3. Comparisons like "angle < PI/4" become meaningless for
 *      angles outside the standard range.
 *
 * The implementation uses two simple while loops.  For most real-world
 * cases only 0–1 iterations are needed (angles rarely exceed ±2π by
 * much), so performance is fine despite the loop approach.
 *
 * @param angle  Any angle in radians (positive or negative, can be very large)
 * @return       The equivalent angle in the range [0, 2π)
 */
double normalize_angle(double angle) {
    /* Shift negative angles up by multiples of 2π until they're ≥ 0 */
    while (angle < 0) angle += 2 * PI;

    /* Shift positive angles down by multiples of 2π until they're < 2π */
    while (angle >= 2 * PI) angle -= 2 * PI;

    return angle;
}