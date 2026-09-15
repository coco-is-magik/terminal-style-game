#include "display_acceptance.h"

#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

static const char *safe_text(const char *text) {
    return text ? text : "unknown";
}

bool display_acceptance_json_text(char *destination, size_t capacity,
                                  const char *source) {
    size_t used = 0;
    const unsigned char *cursor = (const unsigned char *)safe_text(source);
    static const char hex[] = "0123456789abcdef";
    if (!destination || capacity == 0) return false;
    while (*cursor) {
        char escaped[6];
        size_t count = 1;
        escaped[0] = (char)*cursor;
        if (*cursor == '"' || *cursor == '\\') {
            escaped[0] = '\\';
            escaped[1] = (char)*cursor;
            count = 2;
        } else if (*cursor < 0x20U) {
            escaped[0] = '\\'; escaped[1] = 'u'; escaped[2] = '0'; escaped[3] = '0';
            escaped[4] = hex[*cursor >> 4]; escaped[5] = hex[*cursor & 0x0fU];
            count = 6;
        }
        if (count >= capacity - used) {
            destination[used] = '\0';
            return false;
        }
        memcpy(destination + used, escaped, count);
        used += count;
        cursor++;
    }
    destination[used] = '\0';
    return true;
}

static const char *first_keyboard_name(void) {
    int count = 0;
    SDL_KeyboardID *keyboards = SDL_GetKeyboards(&count);
    const char *name = count > 0 ? SDL_GetKeyboardNameForID(keyboards[0]) : NULL;
    static char copy[128];
    (void)snprintf(copy, sizeof(copy), "%s", safe_text(name));
    SDL_free(keyboards);
    return copy;
}

static const char *first_mouse_name(void) {
    int count = 0;
    SDL_MouseID *mice = SDL_GetMice(&count);
    const char *name = count > 0 ? SDL_GetMouseNameForID(mice[0]) : NULL;
    static char copy[128];
    (void)snprintf(copy, sizeof(copy), "%s", safe_text(name));
    SDL_free(mice);
    return copy;
}

void display_acceptance_init(DisplayAcceptance *acceptance, const Renderer *renderer) {
    if (!acceptance) return;
    memset(acceptance, 0, sizeof(*acceptance));
    display_acceptance_observe_window(acceptance, renderer);
    acceptance->initial_window_width = acceptance->window_width;
    acceptance->initial_window_height = acceptance->window_height;
}

void display_acceptance_observe_input(DisplayAcceptance *acceptance, const InputState *input) {
    if (!acceptance || !input) return;
    acceptance->keyboard = acceptance->keyboard || input->confirm;
    acceptance->pointer_motion = acceptance->pointer_motion ||
        input->mouse_dx != 0.0f || input->mouse_dy != 0.0f;
    acceptance->pointer_down = acceptance->pointer_down || input->mouse_left_pressed;
    acceptance->pointer_up = acceptance->pointer_up || input->mouse_left_released;
}

void display_acceptance_observe_window(DisplayAcceptance *acceptance, const Renderer *renderer) {
    int width = 0;
    int height = 0;
    if (!acceptance || !renderer || !renderer->window) return;
    if (SDL_GetWindowSize(renderer->window, &width, &height)) {
        acceptance->window_width = width;
        acceptance->window_height = height;
        if (acceptance->initial_window_width > 0 &&
            (width != acceptance->initial_window_width ||
             height != acceptance->initial_window_height)) {
            acceptance->resized = true;
        }
    }
    (void)SDL_GetWindowSizeInPixels(renderer->window,
                                    &acceptance->pixel_width,
                                    &acceptance->pixel_height);
    acceptance->scale = SDL_GetWindowDisplayScale(renderer->window);
}

void display_acceptance_observe_transition(DisplayAcceptance *acceptance,
                                           AppState before, AppState after) {
    if (acceptance && before == APP_STATE_MAIN_MENU && after == APP_STATE_PLAYING) {
        acceptance->state_transition = true;
    }
}

void display_acceptance_observe_presentation(DisplayAcceptance *acceptance) {
    if (acceptance) acceptance->presented = true;
}

bool display_acceptance_passed(const DisplayAcceptance *acceptance) {
    return acceptance && acceptance->presented && acceptance->resized &&
        acceptance->keyboard && acceptance->pointer_motion &&
        acceptance->pointer_down && acceptance->pointer_up &&
        acceptance->state_transition;
}

void display_acceptance_print_start(const DisplayAcceptance *acceptance,
                                    const Renderer *renderer) {
    char backend[128];
    char renderer_name[128];
    char keyboard[256];
    char pointer[256];
    if (!acceptance || !renderer) return;
    (void)display_acceptance_json_text(backend, sizeof(backend), SDL_GetCurrentVideoDriver());
    (void)display_acceptance_json_text(renderer_name, sizeof(renderer_name),
                                       SDL_GetRendererName(renderer->sdl_ren));
    (void)display_acceptance_json_text(keyboard, sizeof(keyboard), first_keyboard_name());
    (void)display_acceptance_json_text(pointer, sizeof(pointer), first_mouse_name());
    printf("{\"display_acceptance\":\"ready\",\"video_backend\":\"%s\","
           "\"renderer\":\"%s\",\"keyboard\":\"%s\",\"pointer\":\"%s\","
           "\"logical_width\":%d,\"logical_height\":%d,"
           "\"window_width\":%d,\"window_height\":%d,"
           "\"pixel_width\":%d,\"pixel_height\":%d,\"scale\":%.3f}\n",
           backend, renderer_name, keyboard, pointer, renderer->logical_w, renderer->logical_h,
           acceptance->window_width, acceptance->window_height,
           acceptance->pixel_width, acceptance->pixel_height, acceptance->scale);
    fflush(stdout);
}

void display_acceptance_print_result(const DisplayAcceptance *acceptance,
                                     double duration_seconds, uint64_t frames) {
    if (!acceptance) return;
    printf("{\"display_acceptance\":\"%s\",\"presented\":%s,\"resized\":%s,"
           "\"keyboard\":%s,\"pointer_motion\":%s,\"pointer_down\":%s,"
           "\"pointer_up\":%s,\"state_transition\":%s,"
           "\"window_width\":%d,\"window_height\":%d,"
           "\"pixel_width\":%d,\"pixel_height\":%d,\"scale\":%.3f,"
           "\"duration_seconds\":%.3f,\"frames\":%llu}\n",
           display_acceptance_passed(acceptance) ? "pass" : "fail",
           acceptance->presented ? "true" : "false", acceptance->resized ? "true" : "false",
           acceptance->keyboard ? "true" : "false",
           acceptance->pointer_motion ? "true" : "false",
           acceptance->pointer_down ? "true" : "false",
           acceptance->pointer_up ? "true" : "false",
           acceptance->state_transition ? "true" : "false",
           acceptance->window_width, acceptance->window_height,
           acceptance->pixel_width, acceptance->pixel_height, acceptance->scale,
           duration_seconds, (unsigned long long)frames);
}