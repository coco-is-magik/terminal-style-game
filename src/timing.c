#include "timing.h"

double timing_target_ms(int fps) {
    if (fps <= 0) return 0.0;
    return 1000.0 / fps;
}

double timing_spare_ms(double frame_time_ms, double target_ms) {
    return target_ms - frame_time_ms;
}

double timing_over_budget_ms(double spare_time_ms) {
    return spare_time_ms < 0.0 ? -spare_time_ms : 0.0;
}

uint32_t timing_sleep_ms(double spare_time_ms) {
    return spare_time_ms > 0.0 ? (uint32_t)spare_time_ms : 0;
}

void perf_stats_init(PerfStats *stats) {
    if (!stats) return;
    stats->elapsed_time_ms = 0.0;
    stats->frame_count = 0;
    stats->worst_frame_time_ms = 0.0;
    stats->min_spare_time_ms = 1000000.0; // arbitrarily high
    stats->total_frame_time_ms = 0.0;
    
    stats->pub_avg_fps = 0.0;
    stats->pub_avg_frame_time_ms = 0.0;
    stats->pub_worst_frame_time_ms = 0.0;
    stats->pub_min_spare_time_ms = 0.0;
}

void perf_stats_update(PerfStats *stats, double delta_time_ms, double frame_time_ms, double spare_time_ms) {
    if (!stats) return;
    
    stats->elapsed_time_ms += delta_time_ms;
    stats->frame_count++;
    stats->total_frame_time_ms += frame_time_ms;
    
    if (frame_time_ms > stats->worst_frame_time_ms) {
        stats->worst_frame_time_ms = frame_time_ms;
    }
    
    if (spare_time_ms < stats->min_spare_time_ms) {
        stats->min_spare_time_ms = spare_time_ms;
    }
    
    // Evaluate if 1-second rolling window expired
    if (stats->elapsed_time_ms >= 1000.0) {
        stats->pub_avg_fps = ((double)stats->frame_count / stats->elapsed_time_ms) * 1000.0;
        stats->pub_avg_frame_time_ms = stats->total_frame_time_ms / stats->frame_count;
        stats->pub_worst_frame_time_ms = stats->worst_frame_time_ms;
        stats->pub_min_spare_time_ms = stats->min_spare_time_ms;
        
        // Reset counters while preserving the published data
        stats->elapsed_time_ms = 0.0;
        stats->frame_count = 0;
        stats->worst_frame_time_ms = 0.0;
        stats->min_spare_time_ms = 1000000.0;
        stats->total_frame_time_ms = 0.0;
    }
}
