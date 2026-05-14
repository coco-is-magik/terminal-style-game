/**
 * scale.h — Integer scaling calculation for glyph grid to window mapping
 *
 * This header defines the data structure and function for computing how
 * a glyph grid of a given size should be integer-scaled to fit within
 * a window while maintaining the correct aspect ratio and centring it.
 *
 * Unlike renderer.c's SDL logical presentation (which uses fractional
 * scaling/letterboxing via the GPU), this module is intended for
 * headless / test-harness use where you need to compute the exact
 * pixel positions and scale factor manually.
 *
 * The scaling strategy:
 *   1. Compute the logical pixel size of the grid: (grid_w × cell_w) × (grid_h × cell_h)
 *   2. Determine the smaller per-axis integer scale factor, clamped to ≥ 1
 *   3. Centre the scaled grid in the window by computing offsets
 */

#ifndef SCALE_H
#define SCALE_H

#include <stdbool.h>

/**
 * ScaleResult — Result of an integer scale calculation
 *
 * Contains the computed logical dimensions, the best-fit integer scale
 * factor, and the centering offsets.  The `valid` flag indicates whether
 * the input parameters were valid.
 */
typedef struct {
    int logical_w;        /* Grid width in logical pixels  (grid_w × cell_w) */
    int logical_h;        /* Grid height in logical pixels (grid_h × cell_h) */
    int scale_factor;     /* Best-fit integer scale factor (≥ 1) */
    int offset_x;         /* Horizontal centering offset in window pixels */
    int offset_y;         /* Vertical centering offset in window pixels */
    bool valid;           /* true if the computation succeeded */
} ScaleResult;

/**
 * scale_calculate() — Compute integer scale factor and centering offsets
 *
 * Given a window size and a glyph grid configuration, calculates:
 *   - logical_w / logical_h : the total pixel dimensions of the grid
 *   - scale_factor          : the smaller per-axis integer scale, clamped to ≥ 1
 *   - offset_x / offset_y   : centering offsets to centre the scaled grid
 *
 * Minimum scale is 1 (never smaller than 1×), so oversized grids may clip.
 * Returns valid=false if any dimension is ≤ 0.
 *
 * @param win_w   Window width in pixels (must be > 0)
 * @param win_h   Window height in pixels (must be > 0)
 * @param grid_w  Number of glyph columns (must be > 0)
 * @param grid_h  Number of glyph rows (must be > 0)
 * @param cell_w  Width of each glyph in pixels (must be > 0)
 * @param cell_h  Height of each glyph in pixels (must be > 0)
 * @return        ScaleResult struct with computed values
 */
ScaleResult scale_calculate(int win_w, int win_h, int grid_w, int grid_h, int cell_w, int cell_h);

#endif /* SCALE_H */