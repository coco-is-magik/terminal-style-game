#include "ui_theme_demo_runtime.h"

#include "input.h"
#include "timing.h"
#include "ui_compositor.h"
#include "ui_theme.h"
#include "ui_theme_demo.h"

#include <SDL3/SDL.h>
#include <string.h>

bool ui_theme_demo_runtime_arguments_valid(const Renderer *renderer,
                                           const Grid *grid, int target_fps) {
    return renderer && renderer->window && renderer->sdl_ren &&
        renderer->screen_texture && renderer->pixel_buffer && grid && grid->cells &&
        renderer->logical_w > 0 && renderer->logical_h > 0 && target_fps > 0;
}

UiThemeDemoRuntimeResult ui_theme_demo_runtime_run(Renderer *renderer, Grid *grid,
                                                    int target_fps) {
    UiThemeDemoState state;
    UiCanvas *canvas;
    InputState input;
    bool should_exit = false;
    bool dirty = true;
    double target_ms;
    if (!ui_theme_demo_runtime_arguments_valid(renderer, grid, target_fps)) {
        return UI_THEME_DEMO_RUNTIME_INVALID_ARGUMENT;
    }
    (void)SDL_SetWindowTitle(renderer->window,
                             "ASCII FPS - Accepted UI Theme Demo");
    canvas = ui_canvas_create(UI_THEME_DEMO_WIDTH, UI_THEME_DEMO_HEIGHT);
    if (!canvas) return UI_THEME_DEMO_RUNTIME_OUT_OF_MEMORY;
    memset(&input, 0, sizeof(input));
    ui_theme_demo_state_init(&state);
    target_ms = timing_target_ms(target_fps);
    while (!should_exit) {
        uint64_t frame_start = SDL_GetPerformanceCounter();
        int previous_scale = state.scale_percent;
        UiLayerList layers;
        UiLayer layer;
        SDL_Color background;
        double frame_ms;
        uint32_t sleep_ms;

        input_process(&input, false);
        if (!ui_theme_demo_apply_input(&state, &input, &should_exit)) {
            ui_canvas_destroy(canvas);
            return UI_THEME_DEMO_RUNTIME_RENDER_FAILED;
        }
        if (should_exit) break;
        dirty = dirty || state.scale_percent != previous_scale;
        if (dirty) {
            UiThemeDemoRenderResult render_result = ui_theme_demo_render(&state, canvas);
            if (render_result != UI_THEME_DEMO_RENDER_OK) {
                ui_canvas_destroy(canvas);
                return render_result == UI_THEME_DEMO_RENDER_OUT_OF_MEMORY
                    ? UI_THEME_DEMO_RUNTIME_OUT_OF_MEMORY
                    : UI_THEME_DEMO_RUNTIME_RENDER_FAILED;
            }
        }
        dirty = false;
        background = (SDL_Color){
            ui_theme_provisional_tokens()->palette.canvas.red,
            ui_theme_provisional_tokens()->palette.canvas.green,
            ui_theme_provisional_tokens()->palette.canvas.blue,
            ui_theme_provisional_tokens()->palette.canvas.alpha
        };
        grid_clear(grid, background);
        ui_layer_list_clear(&layers);
        layer = (UiLayer){
            1, canvas, UI_ANCHOR_CENTER,
            {0, 0, renderer->logical_w, renderer->logical_h},
            UI_SCALE_EXPLICIT_PRESET, state.scale_percent, 1, true, 0U
        };
        if (!ui_layer_list_add(&layers, &layer)) {
            ui_canvas_destroy(canvas);
            return UI_THEME_DEMO_RUNTIME_RENDER_FAILED;
        }
        renderer_draw_layers(renderer, grid, &layers, 100);
        frame_ms = (double)(SDL_GetPerformanceCounter() - frame_start) * 1000.0 /
                   (double)SDL_GetPerformanceFrequency();
        sleep_ms = timing_sleep_ms(timing_spare_ms(frame_ms, target_ms));
        if (sleep_ms > 0U) SDL_Delay(sleep_ms);
    }
    ui_canvas_destroy(canvas);
    return UI_THEME_DEMO_RUNTIME_OK;
}