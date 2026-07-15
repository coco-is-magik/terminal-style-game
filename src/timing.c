/**
 * timing.c — Frame timing utilities and rolling performance statistics
 *
 * This file implements two categories of functions:
 *
 *   1. Frame-budget helpers:
 *        timing_target_ms()     — convert FPS to milliseconds per frame
 *        timing_spare_ms()      — subtract actual time from budget
 *        timing_over_budget_ms() — positive-only over-budget amount
 *        timing_sleep_ms()      — safe sleep duration (only if under budget)
 *
 *   2. Rolling 1-second performance statistics:
 *        perf_stats_init()      — initialise a PerfStats accumulator
 *        perf_stats_update()    — feed frame data into the rolling window
 *
 * The rolling-window approach gives smooth, readable FPS/frame-time readings
 * on the HUD instead of frame-by-frame jitter.  Every 1000 ms the accumulated
 * data is "published" and the accumulators are reset.
 */

#include "timing.h"        /* PerfStats, function declarations */
#include <SDL3/SDL.h>      /* SDL_GetPerformanceCounter, SDL_GetPerformanceFrequency */
#include <stdio.h>         /* fprintf(), stderr */

/* ===================================================================
 *  Frame-budget calculation functions
 * =================================================================== */

/**
 * timing_target_ms() — Compute the target duration per frame for a given FPS
 *
 * Simple computation: 1000 milliseconds ÷ frames per second = ms per frame.
 *
 * @param fps  Frames per second target (must be > 0; returns 0.0 if ≤ 0)
 * @return     Target frame time in milliseconds (e.g. 16.67 for 60 FPS)
 */
double timing_target_ms(int fps) {
    if (fps <= 0) return 0.0;
    return 1000.0 / fps;
}

/**
 * timing_spare_ms() — Calculate how much time is left in the frame budget
 *
 * Positive spare_time → we finished the frame early; we can sleep or
 * use the time for other work.
 * Negative spare_time → we exceeded the budget; the next frame will be
 * delayed (we're running behind).
 *
 * @param frame_time_ms  How long this frame actually took
 * @param target_ms      How long we wanted it to take
 * @return               Spare time (target − actual), can be negative
 */
double timing_spare_ms(double frame_time_ms, double target_ms) {
    return target_ms - frame_time_ms;
}

/**
 * timing_over_budget_ms() — How far over budget we are, as a positive value
 *
 * If spare_time is negative (over budget), returns the absolute value.
 * If spare_time is zero or positive (within budget), returns 0.0.
 *
 * This is used in the benchmark report to quantify how badly the frame
 * budget was missed.
 *
 * @param spare_time_ms  Result from timing_spare_ms()
 * @return               Over-budget amount in ms (always ≥ 0)
 */
double timing_over_budget_ms(double spare_time_ms) {
    return spare_time_ms < 0.0 ? -spare_time_ms : 0.0;
}

/**
 * timing_sleep_ms() — Determine a safe sleep duration to yield the CPU
 *
 * If we have spare time (finished the frame early), we can sleep for
 * up to that many milliseconds before the next frame is due.  This
 * reduces CPU usage and power consumption.
 *
 * If we're over budget or exactly on time, returns 0 (no sleeping).
 *
 * The returned value is cast to uint32_t and can be passed directly
 * to SDL_Delay().
 *
 * @param spare_time_ms  Result from timing_spare_ms()
 * @return               Milliseconds to sleep (0 = don't sleep)
 */
uint32_t timing_sleep_ms(double spare_time_ms) {
    return spare_time_ms > 0.0 ? (uint32_t)spare_time_ms : 0;
}

/* ===================================================================
 *  Rolling 1-second performance statistics
 * =================================================================== */

/**
 * perf_stats_init() — Initialise a PerfStats accumulator for a new window
 *
 * Sets all accumulators to zero.  min_spare_time_ms is set to an
 * arbitrarily large value (1,000,000 ms) so that the first frame's
 * spare_time (which might be, say, 8 ms) will be smaller and correctly
 * recorded as the new minimum.
 *
 * @param stats  Pointer to PerfStats to initialise (NULL-safe)
 */
void perf_stats_init(PerfStats *stats) {
    if (!stats) return;
    stats->elapsed_time_ms     = 0.0;
    stats->frame_count         = 0;
    stats->worst_frame_time_ms = 0.0;
    stats->min_spare_time_ms   = 1000000.0;   /* Start extremely high */
    stats->total_frame_time_ms = 0.0;

    /* Published values start at zero — they'll be filled after the first
     * 1-second window completes. */
    stats->pub_avg_fps           = 0.0;
    stats->pub_avg_frame_time_ms = 0.0;
    stats->pub_worst_frame_time_ms = 0.0;
    stats->pub_min_spare_time_ms = 0.0;
}

/**
 * perf_stats_update() — Feed per-frame data into the rolling 1-second window
 *
 * Called once per frame from app.c.  This function:
 *
 *   1. Accumulates elapsed time and frame count for the current window
 *   2. Tracks the worst (longest) frame time seen so far
 *   3. Tracks the worst (most negative) spare time seen so far
 *   4. Checks if 1000 ms have elapsed → if so, "publishes" the stats by
 *      computing averages and storing them in the pub_* fields, then
 *      resets all accumulators for the next window.
 *
 * The published values are read by draw_ui_overlay() in app.c for the HUD.
 *
 * @param stats            Pointer to PerfStats to update (NULL-safe)
 * @param delta_time_ms    Time between the start of this frame and the last
 * @param frame_time_ms    Total time this frame consumed (loop start → end)
 * @param spare_time_ms    Spare time for this frame (target − actual)
 */
void perf_stats_update(PerfStats *stats, double delta_time_ms, double frame_time_ms, double spare_time_ms) {
    if (!stats) return;

    /* ---- Accumulate data for the current window ---- */
    stats->elapsed_time_ms += delta_time_ms;
    stats->frame_count++;
    stats->total_frame_time_ms += frame_time_ms;

    /* Track the worst (longest) single frame */
    if (frame_time_ms > stats->worst_frame_time_ms) {
        stats->worst_frame_time_ms = frame_time_ms;
    }

    /* Track the worst (most negative = most over budget) spare time */
    if (spare_time_ms < stats->min_spare_time_ms) {
        stats->min_spare_time_ms = spare_time_ms;
    }

    /* ---- Check if the 1-second window has expired ---- */
    if (stats->elapsed_time_ms >= 1000.0) {
        /* Publish the accumulated data */
        stats->pub_avg_fps           = ((double)stats->frame_count / stats->elapsed_time_ms) * 1000.0;
        stats->pub_avg_frame_time_ms = stats->total_frame_time_ms / stats->frame_count;
        stats->pub_worst_frame_time_ms = stats->worst_frame_time_ms;
        stats->pub_min_spare_time_ms = stats->min_spare_time_ms;

        /* Reset accumulators for the next 1-second window.
         * The published values (pub_*) are preserved until the next update. */
        stats->elapsed_time_ms     = 0.0;
        stats->frame_count         = 0;
        stats->worst_frame_time_ms = 0.0;
        stats->min_spare_time_ms   = 1000000.0;   /* Reset to high sentinel */
        stats->total_frame_time_ms = 0.0;
    }
}

/* ===================================================================
 *  Optional per-frame phase profiling (PROFILE_FRAME=1)
 * =================================================================== */

#if PROFILE_FRAME

/**
 * profile_now_ms() — Current monotonic time in milliseconds
 *
 * Uses SDL's high-resolution performance counter.
 */
double profile_now_ms(void) {
    static uint64_t freq = 0;
    if (freq == 0) {
        freq = SDL_GetPerformanceFrequency();
    }
    uint64_t counter = SDL_GetPerformanceCounter();
    return (double)(counter * 1000) / (double)freq;
}

/**
 * frame_profile_init() — Zero all accumulators.
 */
void frame_profile_init(FrameProfileStats *stats) {
    if (!stats) return;
    stats->raycast_grid_ms = 0.0;
    stats->state_pack_ms   = 0.0;
    stats->smc_diff_ms     = 0.0;
    stats->dirty_check_ms  = 0.0;
    stats->dirty_iter_ms   = 0.0;
    stats->raster_ms       = 0.0;
    stats->sdl_update_ms   = 0.0;
    stats->frame_total_ms  = 0.0;
    stats->frames          = 0;
}

/**
 * frame_profile_print() — Print per-frame average phase timings.
 *
 * Computes averages from accumulated sums and prints a formatted summary.
 * The "other/unaccounted" line is total frame time minus the sum of the
 * explicitly profiled phases.
 */
void frame_profile_print(const FrameProfileStats *stats, const char *mode) {
    if (!stats || stats->frames == 0) {
        fprintf(stderr, "Frame profile: no frames accumulated\n");
        return;
    }

    double inv = 1.0 / (double)stats->frames;
    double grid_ms   = stats->raycast_grid_ms * inv;
    double pack_ms   = stats->state_pack_ms * inv;
    double diff_ms   = stats->smc_diff_ms * inv;
    double check_ms  = stats->dirty_check_ms * inv;
    double iter_ms   = stats->dirty_iter_ms * inv;
    double raster_ms = stats->raster_ms * inv;
    double sdl_ms    = stats->sdl_update_ms * inv;
    double total_ms  = stats->frame_total_ms * inv;

    double accounted = grid_ms + pack_ms + diff_ms + check_ms + iter_ms + raster_ms + sdl_ms;
    double other_ms  = total_ms - accounted;
    if (other_ms < 0.0) other_ms = 0.0;  /* Guard against timing noise */

    fprintf(stderr, "\nFrame profile (%s):\n", mode ? mode : "unknown");
    fprintf(stderr, "  frames:              %llu\n", (unsigned long long)stats->frames);
    fprintf(stderr, "  grid/raycast:        %.3f ms/frame\n", grid_ms);
    fprintf(stderr, "  state packing:       %.3f ms/frame\n", pack_ms);
    fprintf(stderr, "  smc diff:            %.3f ms/frame\n", diff_ms);
    fprintf(stderr, "  dirty decision:      %.3f ms/frame\n", check_ms);
    fprintf(stderr, "  dirty iteration:     %.3f ms/frame\n", iter_ms);
    fprintf(stderr, "  rasterization:       %.3f ms/frame\n", raster_ms);
    fprintf(stderr, "  SDL/update/present:  %.3f ms/frame\n", sdl_ms);
    fprintf(stderr, "  other/unaccounted:   %.3f ms/frame\n", other_ms);
    fprintf(stderr, "  total profiled:      %.3f ms/frame\n", total_ms);
}

#endif /* PROFILE_FRAME */
