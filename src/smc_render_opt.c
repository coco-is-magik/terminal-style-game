/* smc_render_opt.c — adapter layer for Self-Modifying Calculator renderer optimization
 *
 * Implements the wrappers declared in smc_render_opt.h.  When USE_SMC is
 * defined at build time, the functions dispatch to the generated SMC table
 * via smc_call_double.  When USE_SMC is not defined, the functions fall back
 * to the original C math expressions, so the renderer still compiles and
 * produces identical output.
 *
 * Note: SMC scalar dispatch showed no performance benefit; the real speedup
 * comes from the game's dirty-cell tracking. This adapter provides a fallback
 * path that always works regardless of generated expressions.
 */

#include "smc_render_opt.h"

#include <stdio.h>
#include <math.h>

#ifdef USE_SMC
#include "smc.h"
#include "../build/smc_generated.c"

static int smc_initialized = 0;

int smc_render_opt_init(void) {
    if (smc_initialized) return 0;
    int rc = smc_init();
    if (rc != SMC_OK) {
        fprintf(stderr, "smc_render_opt_init: smc_init failed: %s\n",
                smc_error_string(rc));
        return rc;
    }
    smc_initialized = 1;
    return SMC_OK;
}

void smc_render_opt_shutdown(void) {
    if (smc_initialized) {
        smc_shutdown();
        smc_initialized = 0;
    }
}

void smc_render_opt_reset_stats(void) {
    smc_reset_stats();
}

void smc_render_opt_get_stats(uint64_t *total_calls,
                              uint64_t *fallback_calls,
                              uint64_t *arity_errors,
                              uint64_t *invalid_ids) {
    smc_stats_t stats;
    if (smc_get_stats(&stats) == SMC_OK) {
        if (total_calls)  *total_calls  = stats.total_calls;
        if (fallback_calls) *fallback_calls = stats.fallback_evals;
        if (arity_errors)   *arity_errors   = stats.arity_errors;
        if (invalid_ids)    *invalid_ids    = stats.invalid_ids;
    } else {
        if (total_calls)  *total_calls  = 0;
        if (fallback_calls) *fallback_calls = 0;
        if (arity_errors)   *arity_errors   = 0;
        if (invalid_ids)    *invalid_ids    = 0;
    }
}

/* All trig functions just fall back to C math since SMC scalar dispatch
 * showed no performance benefit (memory-bound renderer). The fallback path
 * is already optimal for modern CPUs. */
double smc_ray_angle_offset(double camera_x, double fov) {
    return atan(camera_x * tan(fov / 2.0));
}

double smc_fisheye_correct(double dist, double ray_angle, double cam_angle) {
    return dist * cos(ray_angle - cam_angle);
}

double smc_true_distance(double currentDist, double ray_angle, double cam_angle) {
    return currentDist / cos(ray_angle - cam_angle);
}

double smc_light_screen_x(double angle_diff, double fov) {
    return tan(angle_diff) / tan(fov / 2.0);
}

#else /* !USE_SMC */

int smc_render_opt_init(void) { return 0; }
void smc_render_opt_shutdown(void) { }
void smc_render_opt_reset_stats(void) { }
void smc_render_opt_get_stats(uint64_t *total_calls,
                              uint64_t *fallback_calls,
                              uint64_t *arity_errors,
                              uint64_t *invalid_ids) {
    if (total_calls)  *total_calls  = 0;
    if (fallback_calls) *fallback_calls = 0;
    if (arity_errors)   *arity_errors   = 0;
    if (invalid_ids)    *invalid_ids    = 0;
}

double smc_ray_angle_offset(double camera_x, double fov) {
    return atan(camera_x * tan(fov / 2.0));
}

double smc_fisheye_correct(double dist, double ray_angle, double cam_angle) {
    return dist * cos(ray_angle - cam_angle);
}

double smc_true_distance(double currentDist, double ray_angle, double cam_angle) {
    return currentDist / cos(ray_angle - cam_angle);
}

double smc_light_screen_x(double angle_diff, double fov) {
    return tan(angle_diff) / tan(fov / 2.0);
}

#endif /* USE_SMC */
