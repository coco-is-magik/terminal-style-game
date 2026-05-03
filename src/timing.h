#ifndef TIMING_H
#define TIMING_H

#include <stdint.h>
#include <stdbool.h>

double timing_target_ms(int fps);
double timing_spare_ms(double frame_time_ms, double target_ms);
double timing_over_budget_ms(double spare_time_ms);
uint32_t timing_sleep_ms(double spare_time_ms);

typedef struct {
    double elapsed_time_ms;
    int frame_count;
    double worst_frame_time_ms;
    double min_spare_time_ms;
    double total_frame_time_ms;

    // Published 1-second window stats
    double pub_avg_fps;
    double pub_avg_frame_time_ms;
    double pub_worst_frame_time_ms;
    double pub_min_spare_time_ms;
} PerfStats;

void perf_stats_init(PerfStats *stats);
void perf_stats_update(PerfStats *stats, double delta_time_ms, double frame_time_ms, double spare_time_ms);

#endif
