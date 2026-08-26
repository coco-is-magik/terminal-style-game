#include "r9_mirror_trace.h"

#ifdef R9_OPTICAL_RESEARCH

#include <math.h>

bool r9_mirror_reflect_direction(double incoming_x, double incoming_y,
                                 int mirror_side, double *out_x, double *out_y) {
    double length;
    if (!out_x || !out_y || !isfinite(incoming_x) || !isfinite(incoming_y) ||
        (mirror_side != 0 && mirror_side != 1)) return false;
    length = sqrt(incoming_x * incoming_x + incoming_y * incoming_y);
    if (!isfinite(length) || length <= 0.0) return false;
    incoming_x /= length;
    incoming_y /= length;
    *out_x = mirror_side == 0 ? -incoming_x : incoming_x;
    *out_y = mirror_side == 1 ? -incoming_y : incoming_y;
    return true;
}

bool r9_mirror_sample_once(const HeightfieldTraceColumn *incoming_column,
                           const HeightfieldHit *mirror_hit,
                           const R9OpticalResolved *mirror_optical,
                           int screen_y, double max_distance,
                           R9MirrorSample *out_sample) {
    R9MirrorSample result = {0};
    Camera reflected_camera;
    HeightfieldTraceColumn reflected_column;
    double reflected_x;
    double reflected_y;
    if (!incoming_column || !incoming_column->valid || !mirror_hit ||
        !mirror_hit->hit || mirror_hit->kind != HEIGHTFIELD_HIT_WALL ||
        !mirror_optical || mirror_optical->reflectivity == 0U || !out_sample ||
        screen_y < 0 || screen_y >= incoming_column->viewport_height ||
        !isfinite(max_distance) || max_distance <= 0.0 ||
        !isfinite(mirror_hit->world_x) || !isfinite(mirror_hit->world_y) ||
        !isfinite(mirror_hit->world_z) ||
        !r9_mirror_reflect_direction(
            incoming_column->direction_x, incoming_column->direction_y,
            mirror_hit->side, &reflected_x, &reflected_y)) return false;
    reflected_camera = *incoming_column->camera;
    reflected_camera.transform.pos.x =
        mirror_hit->world_x + reflected_x * R9_MIRROR_ORIGIN_EPSILON;
    reflected_camera.transform.pos.y =
        mirror_hit->world_y + reflected_y * R9_MIRROR_ORIGIN_EPSILON;
    reflected_camera.transform.angle = atan2(reflected_y, reflected_x);
    reflected_camera.z = mirror_hit->world_z;
    if (!heightfield_trace_prepare_column(
            &reflected_column, &reflected_camera, incoming_column->map,
            incoming_column->heights, 1, incoming_column->viewport_height,
            0, max_distance)) return false;
    result.reflected_hit = heightfield_trace_prepared_sample(
        &reflected_column, screen_y);
    result.kind = result.reflected_hit.hit
        ? R9_MIRROR_RESULT_REFLECTED_HIT
        : R9_MIRROR_RESULT_DARKNESS_FALLBACK;
    result.reflected_direction_x = reflected_x;
    result.reflected_direction_y = reflected_y;
    result.bounce_count = R9_MIRROR_MAX_BOUNCES;
    *out_sample = result;
    return true;
}

#else

typedef int r9_mirror_trace_disabled_translation_unit;

#endif /* R9_OPTICAL_RESEARCH */