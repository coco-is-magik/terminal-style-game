/**
 * scale.c — Integer scaling calculation for glyph grid to window mapping
 *
 * This file implements the scale_calculate() function, which computes
 * how a glyph grid of a given configuration should be integer-scaled
 * to fit inside a window, centred, and with a uniform integer scale factor.
 *
 * The calculation answers the question:
 *   "What integer scale factor can I use so that my grid of (grid_w × cell_w)
 *    pixels fits inside a window of (win_w × win_h) pixels, centred?"
 *
 * This is used by test harnesses and headless tools that need to know
 * exact pixel positions without relying on SDL's GPU-based logical
 * presentation (which is used by the main renderer in renderer.c).
 *
 * Algorithm:
 *   1. Compute logical pixels: logical_w = grid_w × cell_w, logical_h = grid_h × cell_h
 *   2. Compute raw integer scale factors for each axis: win_w / logical_w, win_h / logical_h
 *   3. Pick the smaller of the two — this ensures the entire grid fits
 *   4. Clamp to minimum 1 (never scale below 1×)
 *   5. Centre the scaled grid: offset = (window_dim - (logical_dim × scale)) / 2
 */

#include "scale.h"        /* ScaleResult struct, scale_calculate() declaration */

/**
 * scale_calculate() — Compute best-fit integer scale and centering offsets
 *
 * Takes a window size and a glyph grid configuration, and returns:
 *
 *   logical_w / logical_h — the pixel dimensions of the grid
 *   scale_factor          — the smaller per-axis integer scale, clamped to
 *                            minimum 1 (may clip if the grid is larger)
 *   offset_x / offset_y   — pixel offsets to centre the scaled grid
 *                            horizontally and vertically in the window
 *   valid                 — false if any input dimension is ≤ 0
 *
 * Example:
 *   Window: 1920×1080, Grid: 260×160, Cell: 8×8
 *   logical: 2080×1280  → scale_x = 1920/2080 = 0, scale_y = 1080/1280 = 0
 *   → scale_factor = min(0, 0) = 0 → clamped to 1
 *   → fitted: 2080×1280 (larger than window!), offsets negative
 *   This means the grid can't fit at 1× without clipping.
 *
 * @param win_w   Window width in screen pixels (must be > 0)
 * @param win_h   Window height in screen pixels (must be > 0)
 * @param grid_w  Number of glyph columns in the grid (must be > 0)
 * @param grid_h  Number of glyph rows in the grid (must be > 0)
 * @param cell_w  Width of each glyph cell in pixels (must be > 0)
 * @param cell_h  Height of each glyph cell in pixels (must be > 0)
 * @return        ScaleResult with computed values (valid=false on bad input)
 */
ScaleResult scale_calculate(int win_w, int win_h, int grid_w, int grid_h, int cell_w, int cell_h) {
    /* Zero-initialise the result struct.  valid defaults to false. */
    ScaleResult res = {0};

    /* ---- Input validation ---- */
    /* Reject any non-positive dimension — the calculation would be
     * meaningless or produce division-by-zero issues. */
    if (win_w <= 0 || win_h <= 0 || grid_w <= 0 || grid_h <= 0 || cell_w <= 0 || cell_h <= 0) {
        res.valid = false;
        return res;
    }

    /* ---- Step 1: Compute logical pixel dimensions of the grid ---- */
    /* This is the total pixel area the grid occupies at 1× scale.
     * For example, a 260×160 grid with 8×8 cells = 2080×1280 pixels. */
    res.logical_w = grid_w * cell_w;
    res.logical_h = grid_h * cell_h;

    /* ---- Step 2: Compute integer scale factors for each axis ---- */
    /* Integer division truncates toward zero, which gives us the largest
     * integer that fits within the window for that axis. */
    int scale_x = win_w / res.logical_w;
    int scale_y = win_h / res.logical_h;

    /* ---- Step 3: Pick the limiting axis ---- */
    /* We choose the smaller of the two scale factors so the entire grid
     * (not just one dimension) fits inside the window. */
    res.scale_factor = scale_x < scale_y ? scale_x : scale_y;

    /* ---- Step 4: Enforce minimum scale of 1 ---- */
    /* Even if the grid is larger than the window (both scale_x and scale_y
     * are 0), we never go below 1×.  The grid will simply be clipped. */
    if (res.scale_factor < 1) {
        res.scale_factor = 1;
    }

    /* ---- Step 5: Compute centering offsets ---- */
    /* After scaling, the grid occupies (logical_w × scale) pixels.
     * We centre this in the window by offsetting by half the difference. */
    int fitted_w = res.logical_w * res.scale_factor;
    int fitted_h = res.logical_h * res.scale_factor;

    res.offset_x = (win_w - fitted_w) / 2;
    res.offset_y = (win_h - fitted_h) / 2;

    /* ---- Mark as valid ---- */
    res.valid = true;

    return res;
}