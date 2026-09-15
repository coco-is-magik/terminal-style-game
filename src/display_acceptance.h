#ifndef DISPLAY_ACCEPTANCE_H
#define DISPLAY_ACCEPTANCE_H

#include "config.h"
#include "input.h"
#include "renderer.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    int initial_window_width;
    int initial_window_height;
    int window_width;
    int window_height;
    int pixel_width;
    int pixel_height;
    float scale;
    bool presented;
    bool resized;
    bool keyboard;
    bool pointer_motion;
    bool pointer_down;
    bool pointer_up;
    bool state_transition;
} DisplayAcceptance;

bool display_acceptance_json_text(char *destination, size_t capacity,
                                  const char *source);
void display_acceptance_init(DisplayAcceptance *acceptance, const Renderer *renderer);
void display_acceptance_observe_input(DisplayAcceptance *acceptance, const InputState *input);
void display_acceptance_observe_window(DisplayAcceptance *acceptance, const Renderer *renderer);
void display_acceptance_observe_transition(DisplayAcceptance *acceptance,
                                           AppState before, AppState after);
void display_acceptance_observe_presentation(DisplayAcceptance *acceptance);
bool display_acceptance_passed(const DisplayAcceptance *acceptance);
void display_acceptance_print_start(const DisplayAcceptance *acceptance,
                                    const Renderer *renderer);
void display_acceptance_print_result(const DisplayAcceptance *acceptance,
                                     double duration_seconds, uint64_t frames);

#endif