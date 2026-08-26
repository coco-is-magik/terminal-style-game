#include "mirror_trace.h"

#include <math.h>
#include <string.h>

void mirror_trace_column_cache_init(MirrorTraceColumnCache *cache) {
    if (!cache) return;
    memset(cache, 0, sizeof(*cache));
}

bool mirror_trace_reflect_direction(
    double incoming_x, double incoming_y, int mirror_side,
    double *out_x, double *out_y
) {
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

static bool same_mirror_plane(const MirrorTraceColumnCache *cache,
                              const HeightfieldHit *hit) {
    const double epsilon = MIRROR_TRACE_ORIGIN_EPSILON * 2.0;
    return cache->mirror_side == hit->side &&
        fabs(cache->mirror_world_x - hit->world_x) <= epsilon &&
        fabs(cache->mirror_world_y - hit->world_y) <= epsilon;
}

static bool prepare_reflected_column(
    MirrorTraceColumnCache *cache,
    const HeightfieldTraceColumn *incoming,
    const HeightfieldHit *mirror_hit,
    double reflected_x, double reflected_y,
    double max_distance
) {
    cache->reflected_camera = *incoming->camera;
    cache->reflected_camera.transform.pos.x =
        mirror_hit->world_x + reflected_x * MIRROR_TRACE_ORIGIN_EPSILON;
    cache->reflected_camera.transform.pos.y =
        mirror_hit->world_y + reflected_y * MIRROR_TRACE_ORIGIN_EPSILON;
    cache->reflected_camera.transform.angle = atan2(reflected_y, reflected_x);
    cache->reflected_camera.z = mirror_hit->world_z;
    if (!heightfield_trace_prepare_column(
            &cache->reflected_column, &cache->reflected_camera,
            incoming->map, incoming->heights, 1, incoming->viewport_height,
            0, max_distance)) return false;
    cache->mirror_world_x = mirror_hit->world_x;
    cache->mirror_world_y = mirror_hit->world_y;
    cache->mirror_side = mirror_hit->side;
    cache->preparation_count = 1U;
    cache->prepared = true;
    return true;
}

static bool terminate_at_reflected_mirror(HeightfieldOpticalResult *result) {
    size_t i;
    for (i = 0U; i < result->count; i++) {
        if (result->layers[i].hit.kind == HEIGHTFIELD_HIT_WALL &&
            result->layers[i].optical.reflectivity > 0U) {
            result->count = 0U;
            result->terminated_by_surface = false;
            result->reached_opening = false;
            result->layer_cap_exhausted = false;
            return true;
        }
    }
    return false;
}

bool mirror_trace_sample_once(
    MirrorTraceColumnCache *cache,
    const HeightfieldTraceColumn *incoming_column,
    const HeightfieldHit *mirror_hit,
    const OpticalRuntimeView *optical_view,
    uint32_t source_generation,
    int screen_y,
    double max_distance,
    MirrorTraceResult *out_result
) {
    MirrorTraceResult result = {0};
    double reflected_x;
    double reflected_y;
    if (!cache || !incoming_column || !incoming_column->valid || !mirror_hit ||
        !mirror_hit->hit || mirror_hit->kind != HEIGHTFIELD_HIT_WALL ||
        !optical_runtime_view_is_current(optical_view, source_generation) ||
        optical_view->cell_count != incoming_column->heights->cell_count ||
        !out_result || screen_y < 0 ||
        screen_y >= incoming_column->viewport_height ||
        !isfinite(max_distance) || max_distance <= 0.0 ||
        !isfinite(mirror_hit->world_x) || !isfinite(mirror_hit->world_y) ||
        !isfinite(mirror_hit->world_z) ||
        !mirror_trace_reflect_direction(
            incoming_column->direction_x, incoming_column->direction_y,
            mirror_hit->side, &reflected_x, &reflected_y)) return false;
    result.reflected_direction_x = reflected_x;
    result.reflected_direction_y = reflected_y;
    result.bounce_count = MIRROR_TRACE_MAX_BOUNCES;
    if (cache->prepared && !same_mirror_plane(cache, mirror_hit)) {
        result.darkness_fallback = true;
        *out_result = result;
        return true;
    }
    if (!cache->prepared && !prepare_reflected_column(
            cache, incoming_column, mirror_hit, reflected_x, reflected_y,
            max_distance)) return false;
    cache->reflected_camera.z = mirror_hit->world_z;
    if (!heightfield_trace_selective(
            &cache->reflected_column, optical_view, source_generation,
            screen_y, &result.optical)) return false;
    if (terminate_at_reflected_mirror(&result.optical)) {
        result.darkness_fallback = true;
    } else {
        result.darkness_fallback =
            result.optical.count == 0U ||
            (!result.optical.terminated_by_surface &&
             (result.optical.reached_opening || result.optical.layer_cap_exhausted));
    }
    *out_result = result;
    return true;
}