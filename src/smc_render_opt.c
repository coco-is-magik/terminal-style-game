/* smc_render_opt.c — adapter layer for Self-Modifying Calculator renderer optimization
 *
 * Implements the wrappers declared in smc_render_opt.h.  When USE_SMC is
 * defined at build time, the functions dispatch to the generated SMC table
 * via smc_call_double.  When USE_SMC is not defined, the functions fall back
 * to the original C math expressions, so the renderer still compiles and
 * produces identical output.
 */

#include "smc_render_opt.h"

#include <stdio.h>

#ifdef USE_SMC
#include "smc.h"
#include "../build/smc_generated.c"

/* Stable expression IDs from the generated dispatch table.
 * These must match the IDs emitted by scripts/generate-smc-renderer.lisp.
 * We include smc_generated.c above so the macros are visible here. */
#define SMC_EXPR_RAY_ANGLE_OFFSET  SMC_EXPR_EXPR_EXPR_ATAN_LPAREN__LPAREN_X__MUL__TAN_LPAREN__LPAREN_Y__DIV__2_RPAREN__RPAREN__RPAREN__RPAREN
#define SMC_EXPR_FISHEYE_CORRECT   SMC_EXPR_EXPR_EXPR__LPAREN_X__MUL__COS_LPAREN__LPAREN_Y__MINUS__Z_RPAREN__RPAREN__RPAREN
#define SMC_EXPR_TRUE_DISTANCE     SMC_EXPR_EXPR_EXPR__LPAREN_X__DIV__COS_LPAREN__LPAREN_Y__MINUS__Z_RPAREN__RPAREN__RPAREN
#define SMC_EXPR_LIGHT_SCREEN_X    SMC_EXPR_EXPR_EXPR__LPAREN_TAN_LPAREN_X_RPAREN___DIV__TAN_LPAREN__LPAREN_Y__DIV__2_RPAREN__RPAREN__RPAREN

static int smc_initialized = 0;

static double smc_call_or_fallback(smc_expr_id_t id, const double *args, size_t argc,
                                   double fallback) {
    double out = fallback;
    int rc = smc_call_double(id, args, argc, &out);
    if (rc != SMC_OK) {
        /* On any error, return the fallback value.  The caller is responsible
         * for checking statistics to ensure this does not happen in
         * production benchmarks. */
        return fallback;
    }
    return out;
}

int smc_render_opt_init(void) {
    if (smc_initialized) return 0;
    int rc = smc_init();
    if (rc != SMC_OK) {
        fprintf(stderr, "smc_render_opt_init: smc_init failed: %s\n",
                smc_error_string(rc));
        return rc;
    }

    /* Sanity-check that the generated table contains the expressions we
     * expect.  This catches a stale or mismatched generated file early. */
    if (smc_expr_count() < 10) {
        fprintf(stderr, "smc_render_opt_init: generated table too small (%d expressions)\n",
                smc_expr_count());
        smc_shutdown();
        return SMC_ERR_INIT;
    }
    if (smc_expr_arity(SMC_EXPR_RAY_ANGLE_OFFSET) != 2 ||
        smc_expr_arity(SMC_EXPR_FISHEYE_CORRECT) != 3 ||
        smc_expr_arity(SMC_EXPR_TRUE_DISTANCE) != 3 ||
        smc_expr_arity(SMC_EXPR_LIGHT_SCREEN_X) != 2) {
        fprintf(stderr, "smc_render_opt_init: generated expression arity mismatch\n");
        smc_shutdown();
        return SMC_ERR_INIT;
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

double smc_ray_angle_offset(double camera_x, double fov) {
    double args[2] = { camera_x, fov };
    return smc_call_or_fallback(SMC_EXPR_RAY_ANGLE_OFFSET, args, 2,
                                 atan(camera_x * tan(fov / 2.0)));
}

double smc_fisheye_correct(double dist, double ray_angle, double cam_angle) {
    double args[3] = { dist, ray_angle, cam_angle };
    return smc_call_or_fallback(SMC_EXPR_FISHEYE_CORRECT, args, 3,
                                 dist * cos(ray_angle - cam_angle));
}

double smc_true_distance(double currentDist, double ray_angle, double cam_angle) {
    double args[3] = { currentDist, ray_angle, cam_angle };
    return smc_call_or_fallback(SMC_EXPR_TRUE_DISTANCE, args, 3,
                                 currentDist / cos(ray_angle - cam_angle));
}

double smc_light_screen_x(double angle_diff, double fov) {
    double args[2] = { angle_diff, fov };
    return smc_call_or_fallback(SMC_EXPR_LIGHT_SCREEN_X, args, 2,
                                 tan(angle_diff) / tan(fov / 2.0));
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
