#include "ui_motion_demo_runtime.h"

#include "input.h"
#include "timing.h"
#include "ui_compositor.h"
#include "ui_motion_demo.h"
#include "ui_theme.h"

#include <SDL3/SDL.h>
#include <string.h>

static double now_ms(void) {
    return (double)SDL_GetPerformanceCounter() * 1000.0 /
           (double)SDL_GetPerformanceFrequency();
}

bool ui_motion_demo_runtime_arguments_valid(const Renderer *renderer,
                                             const Grid *grid, int target_fps) {
    return renderer && renderer->window && renderer->sdl_ren &&
        renderer->screen_texture && renderer->pixel_buffer && grid && grid->cells &&
        renderer->logical_w > 0 && renderer->logical_h > 0 && target_fps > 0;
}

UiMotionDemoRuntimeResult ui_motion_demo_runtime_run(Renderer *renderer, Grid *grid,
                                                      int target_fps) {
    UiMotionDemoState state;
    UiCanvas *canvas;
    InputState input;
    bool should_exit = false;
    double target_ms;
    if (!ui_motion_demo_runtime_arguments_valid(renderer, grid, target_fps))
        return UI_MOTION_DEMO_RUNTIME_INVALID_ARGUMENT;
    (void)SDL_SetWindowTitle(renderer->window,
                             "ASCII FPS - Accepted UI Motion Demo");
    canvas = ui_canvas_create(UI_MOTION_DEMO_WIDTH, UI_MOTION_DEMO_HEIGHT);
    if (!canvas) return UI_MOTION_DEMO_RUNTIME_OUT_OF_MEMORY;
    memset(&input, 0, sizeof(input));
    ui_motion_demo_state_init(&state, now_ms());
    target_ms = timing_target_ms(target_fps);
    while (!should_exit) {
        uint64_t frame_start = SDL_GetPerformanceCounter();
        double sample_time = now_ms();
        UiLayerList layers;
        UiLayer layer;
        const UiThemeColor background_token = ui_theme_provisional_tokens()->palette.canvas;
        SDL_Color background = {background_token.red, background_token.green,
                                background_token.blue, background_token.alpha};
        double frame_ms;
        uint32_t sleep_ms;
        UiMotionDemoRenderResult render_result;

        input_process(&input, false);
        if (!ui_motion_demo_apply_input(&state, &input, sample_time, &should_exit)) {
            ui_canvas_destroy(canvas);
            return UI_MOTION_DEMO_RUNTIME_RENDER_FAILED;
        }
        if (should_exit) break;
        render_result = ui_motion_demo_render(&state, sample_time, canvas);
        if (render_result != UI_MOTION_DEMO_RENDER_OK) {
            ui_canvas_destroy(canvas);
            return render_result == UI_MOTION_DEMO_RENDER_OUT_OF_MEMORY
                ? UI_MOTION_DEMO_RUNTIME_OUT_OF_MEMORY
                : UI_MOTION_DEMO_RUNTIME_RENDER_FAILED;
        }
        grid_clear(grid, background);
        ui_layer_list_clear(&layers);
        layer = (UiLayer){
            1, canvas, UI_ANCHOR_CENTER,
            {0, 0, renderer->logical_w, renderer->logical_h},
            UI_SCALE_EXPLICIT_PRESET, state.scale_percent, 1, true, 0U
        };
        if (!ui_layer_list_add(&layers, &layer)) {
            ui_canvas_destroy(canvas);
            return UI_MOTION_DEMO_RUNTIME_RENDER_FAILED;
        }
        renderer_draw_layers(renderer, grid, &layers, 100);
        frame_ms = (double)(SDL_GetPerformanceCounter() - frame_start) * 1000.0 /
                   (double)SDL_GetPerformanceFrequency();
        sleep_ms = timing_sleep_ms(timing_spare_ms(frame_ms, target_ms));
        if (sleep_ms > 0U) SDL_Delay(sleep_ms);
    }
    ui_canvas_destroy(canvas);
    return UI_MOTION_DEMO_RUNTIME_OK;
}