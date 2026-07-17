/**
 * timing.h — Frame timing utilities and performance statistics
 *
 * This header provides two categories of functions:
 *
 *   Frame-budget helpers:
 *     timing_target_ms(fps)     — compute the target frame time for a given FPS
 *     timing_spare_ms(...)      — how much time is left in the frame budget
 *     timing_over_budget_ms(...) — how far over budget we are (0 if within budget)
 *     timing_sleep_ms(...)      — how many ms we can safely sleep (only if under budget)
 *
 *   Rolling performance statistics:
 *     PerfStats struct — accumulates frame metrics over a 1-second window,
 *                        then publishes smoothed averages for the HUD overlay
 *     perf_stats_init()   — zero-initialise the rolling stats window
 *     perf_stats_update() — feed per-frame data into the rolling window
 *
 * The 1-second rolling window is designed to give the user/developer smooth,
 * stable readings of FPS and frame times on the HUD, rather than jumping
 * around every single frame.
 */

#ifndef TIMING_H
#define TIMING_H

#include <stdint.h>
#include <stdbool.h>

/* ===================================================================
 *  Frame-budget calculation functions
 * =================================================================== */

/**
 * timing_target_ms() — Compute the target frame time for a given FPS
 *
 * At 60 FPS each frame should take 1000/60 ≈ 16.67 ms.
 * At 120 FPS each frame should take 1000/120 ≈ 8.33 ms.
 *
 * @param fps  Target frames per second (must be > 0)
 * @return     Target frame time in milliseconds
 */
double timing_target_ms(int fps);

/**
 * timing_spare_ms() — Calculate how much spare/free time remains in the frame
 *
 * spare_time = target_time - actual_frame_time
 *
 * A positive value means we finished early (have time to spare).
 * A negative value means we exceeded the budget (over budget).
 *
 * @param frame_time_ms  Actual time this frame took (ms)
 * @param target_ms      Target frame time from timing_target_ms() (ms)
 * @return               Spare time in ms (negative = over budget)
 */
double timing_spare_ms(double frame_time_ms, double target_ms);

/**
 * timing_over_budget_ms() — Calculate how far over budget we are
 *
 * Returns 0.0 if within budget (spare_time >= 0).
 * Returns the absolute over-budget amount if over budget (spare_time < 0).
 *
 * @param spare_time_ms  Result from timing_spare_ms()
 * @return               Milliseconds over budget (≥ 0)
 */
double timing_over_budget_ms(double spare_time_ms);

/**
 * timing_sleep_ms() — Determine how many ms to sleep before the next frame
 *
 * Returns (uint32_t)spare_time_ms if spare_time > 0, otherwise 0.
 * The returned value can be passed directly to SDL_Delay() to yield the CPU.
 *
 * @param spare_time_ms  Result from timing_spare_ms()
 * @return               Sleep duration in ms (0 = don't sleep)
 */
uint32_t timing_sleep_ms(double spare_time_ms);

/* ===================================================================
 *  Rolling 1-second performance statistics
 * =================================================================== */

/**
 * PerfStats — Accumulator for rolling 1-second performance metrics
 *
 * This struct accumulates frame-by-frame data (elapsed time, frame count,
 * worst frame time, worst spare time, total frame time).  Once 1000 ms of
 * real time have elapsed, the accumulated data is "published" into the
 * pub_* fields and the accumulators are reset for the next window.
 *
 * This gives smooth 1-second averages that are displayed on the HUD
 * overlay (draw_ui_overlay in app.c).
 */
typedef struct {
    /* Accumulators (reset every ~1 second) */
    double elapsed_time_ms;       /* Total elapsed time in the current window */
    int    frame_count;           /* Number of frames in the current window */
    double worst_frame_time_ms;   /* Longest single frame in this window */
    double min_spare_time_ms;     /* Least spare time (most over budget) in this window */
    double total_frame_time_ms;   /* Sum of all frame times in this window */

    /* Published values (read-only by external code) */
    double pub_avg_fps;               /* Average FPS over the last window */
    double pub_avg_frame_time_ms;     /* Average frame time over the last window */
    double pub_worst_frame_time_ms;   /* Worst frame time in the last window */
    double pub_min_spare_time_ms;     /* Worst spare time in the last window */
} PerfStats;

/**
 * perf_stats_init() — Initialise a PerfStats struct
 *
 * Sets all fields to zero, except min_spare_time_ms which is set to an
 * arbitrarily high value (1,000,000 ms) so the first frame will always
 * be lower and get recorded as the new minimum.
 *
 * @param stats  Pointer to PerfStats to initialise (NULL-safe)
 */
void perf_stats_init(PerfStats *stats);

/**
 * perf_stats_update() — Feed per-frame timing data into the rolling window
 *
 * Increments the accumulators and updates running min/max values.
 * Once elapsed_time_ms reaches 1000.0 (1 second of wall-clock time),
 * the accumulated data is published to the pub_* fields and the
 * accumulators are reset.
 *
 * This function is called once per frame from the main loop in app.c.
 *
 * @param stats            Pointer to PerfStats to update (NULL-safe)
 * @param delta_time_ms    Time since the last frame began
 * @param frame_time_ms    Total time this frame took (from loop start to loop end)
 * @param spare_time_ms    Spare/budget time for this frame
 */
void perf_stats_update(PerfStats *stats, double delta_time_ms, double frame_time_ms, double spare_time_ms);

/* ===================================================================
 *  Optional per-frame phase profiling (PROFILE_FRAME=1)
 * =================================================================== */

#if PROFILE_FRAME

/**
 * profile_now_ms() — Return the current monotonic time in milliseconds
 *
 * Uses SDL_GetPerformanceCounter() / SDL_GetPerformanceFrequency().
 * Only available when PROFILE_FRAME is defined at build time.
 */
double profile_now_ms(void);

/**
 * FrameProfileStats — Accumulated timings for renderer phases
 *
 * All fields are sums over the whole benchmark run.  Divide by frames
 * at report time to get per-frame averages.  Fields are zero when a
 * phase does not exist in the active renderer mode.
 */
typedef struct {
    double raycast_grid_ms;   /* grid/raycast generation (world content) */
    double state_pack_ms;     /* building the packed uint64_t state array */
    double smc_batch_diff_ms; /* SMC batch diff call (batch mode only) */
    double smc_stream_diff_ms; /* SMC stream diff call (stream mode only) */
    double dirty_check_ms;    /* custom dirty-cell comparison / decision */
    double dirty_iter_ms;     /* walking dirty indices and selecting cells */
    double raster_ms;         /* actual 8x8 glyph rasterization */
    double sdl_update_ms;     /* SDL_UpdateTexture + RenderTexture + Present */
    double frame_total_ms;    /* total renderer_draw() wall time */
    uint64_t frames;          /* number of frames accumulated */
} FrameProfileStats;

/**
 * frame_profile_init() — Zero a FrameProfileStats struct.
 */
void frame_profile_init(FrameProfileStats *stats);

/**
 * frame_profile_print() — Print the final per-frame average summary.
 *
 * @param stats   Accumulated stats (will be divided by frames internally)
 * @param mode    Human-readable mode name for the header
 */
void frame_profile_print(const FrameProfileStats *stats, const char *mode);

#endif /* PROFILE_FRAME */

#endif /* TIMING_H */
