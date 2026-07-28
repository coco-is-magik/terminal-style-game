#include "app_options.h"

#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

void app_options_init(AppOptions *options) {
    if (!options) {
        return;
    }
    options->mode = RUN_MODE_NORMAL;
    options->visual_mode = VISUAL_RAYCAST;
    options->run_duration_seconds = 0.0;
    options->benchmark_scenario = NULL;
    options->benchmark_frames = 600;
}

static bool parse_positive_double(const char *text, double *out) {
    char *end;
    double value;

    if (!text || !out || text[0] == '\0') {
        return false;
    }
    errno = 0;
    end = NULL;
    value = strtod(text, &end);
    if (errno == ERANGE || end == text || *end != '\0' || !isfinite(value) || value <= 0.0) {
        return false;
    }
    *out = value;
    return true;
}

static bool parse_positive_int(const char *text, int *out) {
    char *end;
    long value;

    if (!text || !out || text[0] == '\0') {
        return false;
    }
    errno = 0;
    end = NULL;
    value = strtol(text, &end, 10);
    if (errno == ERANGE || end == text || *end != '\0' || value <= 0 || value > INT_MAX) {
        return false;
    }
    *out = (int)value;
    return true;
}

static bool scenario_is_valid(const char *scenario) {
    static const char *const names[] = {
        "idle", "camera", "rotate", "flicker", "ui", "fullchange"
    };
    size_t count = sizeof(names) / sizeof(names[0]);

    if (!scenario) {
        return false;
    }
    for (size_t i = 0; i < count; i++) {
        if (strcmp(scenario, names[i]) == 0) {
            return true;
        }
    }
    return false;
}

static AppOptionsResult select_run_mode(AppOptions *options, RunMode mode,
                                        VisualMode visual_mode) {
    if (options->mode != RUN_MODE_NORMAL) {
        return APP_OPTIONS_CONFLICT;
    }
    options->mode = mode;
    options->visual_mode = visual_mode;
    return APP_OPTIONS_OK;
}

AppOptionsResult app_options_parse(int argc, char *const argv[], AppOptions *out) {
    AppOptions candidate;
    bool frames_seen = false;

    if (!out || argc < 0 || (argc > 0 && !argv)) {
        return APP_OPTIONS_INVALID_ARGUMENTS;
    }
    app_options_init(&candidate);

    for (int i = 1; i < argc; i++) {
        const char *option = argv[i];
        AppOptionsResult result;

        if (!option) {
            return APP_OPTIONS_INVALID_ARGUMENTS;
        }
        if (strcmp(option, "--benchmark-scenario") == 0) {
            if (++i >= argc) return APP_OPTIONS_MISSING_VALUE;
            if (!scenario_is_valid(argv[i])) return APP_OPTIONS_INVALID_VALUE;
            result = select_run_mode(&candidate, RUN_MODE_BENCHMARK_SCENARIO, VISUAL_RAYCAST);
            if (result != APP_OPTIONS_OK) return result;
            candidate.benchmark_scenario = argv[i];
        } else if (strcmp(option, "--smoke-test") == 0) {
            result = select_run_mode(&candidate, RUN_MODE_SMOKE, VISUAL_RAYCAST);
            if (result != APP_OPTIONS_OK) return result;
        } else if (strcmp(option, "--frames") == 0) {
            if (++i >= argc) return APP_OPTIONS_MISSING_VALUE;
            if (frames_seen || !parse_positive_int(argv[i], &candidate.benchmark_frames)) {
                return APP_OPTIONS_INVALID_VALUE;
            }
            frames_seen = true;
        } else if (strcmp(option, "--benchmark-stress") == 0 ||
                   strcmp(option, "--benchmark-raycast") == 0 ||
                   strcmp(option, "--stability-test") == 0 ||
                   strcmp(option, "--benchmark-lighting") == 0) {
            RunMode mode;
            VisualMode visual_mode = VISUAL_RAYCAST;
            if (++i >= argc) return APP_OPTIONS_MISSING_VALUE;
            if (!parse_positive_double(argv[i], &candidate.run_duration_seconds)) {
                return APP_OPTIONS_INVALID_VALUE;
            }
            if (strcmp(option, "--benchmark-stress") == 0) {
                mode = RUN_MODE_BENCHMARK_STRESS;
                visual_mode = VISUAL_STRESS;
            } else if (strcmp(option, "--benchmark-raycast") == 0) {
                mode = RUN_MODE_BENCHMARK_RAYCAST;
            } else if (strcmp(option, "--stability-test") == 0) {
                mode = RUN_MODE_STABILITY;
            } else {
                mode = RUN_MODE_BENCHMARK_LIGHTING;
            }
            result = select_run_mode(&candidate, mode, visual_mode);
            if (result != APP_OPTIONS_OK) return result;
        } else if (strcmp(option, "--mode") == 0) {
            if (++i >= argc) return APP_OPTIONS_MISSING_VALUE;
            if (strcmp(argv[i], "normal") == 0) candidate.visual_mode = VISUAL_NORMAL;
            else if (strcmp(argv[i], "stress") == 0) candidate.visual_mode = VISUAL_STRESS;
            else if (strcmp(argv[i], "raycast") == 0) candidate.visual_mode = VISUAL_RAYCAST;
            else return APP_OPTIONS_INVALID_VALUE;
        } else {
            return APP_OPTIONS_UNKNOWN_OPTION;
        }
    }

    if (frames_seen && candidate.mode != RUN_MODE_BENCHMARK_SCENARIO) {
        return APP_OPTIONS_CONFLICT;
    }
    *out = candidate;
    return APP_OPTIONS_OK;
}

const char *app_options_result_string(AppOptionsResult result) {
    switch (result) {
        case APP_OPTIONS_OK: return "ok";
        case APP_OPTIONS_INVALID_ARGUMENTS: return "invalid arguments";
        case APP_OPTIONS_UNKNOWN_OPTION: return "unknown option";
        case APP_OPTIONS_MISSING_VALUE: return "missing option value";
        case APP_OPTIONS_INVALID_VALUE: return "invalid option value";
        case APP_OPTIONS_CONFLICT: return "conflicting options";
        default: return "invalid result";
    }
}