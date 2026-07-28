#ifndef APP_OPTIONS_H
#define APP_OPTIONS_H

#include "config.h"

typedef struct {
    RunMode mode;
    VisualMode visual_mode;
    double run_duration_seconds;
    const char *benchmark_scenario;
    int benchmark_frames;
} AppOptions;

typedef enum {
    APP_OPTIONS_OK = 0,
    APP_OPTIONS_INVALID_ARGUMENTS,
    APP_OPTIONS_UNKNOWN_OPTION,
    APP_OPTIONS_MISSING_VALUE,
    APP_OPTIONS_INVALID_VALUE,
    APP_OPTIONS_CONFLICT
} AppOptionsResult;

void app_options_init(AppOptions *options);
AppOptionsResult app_options_parse(int argc, char *const argv[], AppOptions *out);
const char *app_options_result_string(AppOptionsResult result);

#endif