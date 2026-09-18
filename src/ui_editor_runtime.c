#include "ui_editor_runtime.h"

#include "asset_loader.h"
#include "input.h"
#include "timing.h"
#include "ui_editor_host.h"
#include "ui_editor_presentation.h"

#include <SDL3/SDL.h>

static const UiRenderTheme editor_theme = {
    {120, 220, 160, 255}, {0, 40, 20, 255},
    {20, 20, 20, 255}, {220, 220, 120, 255},
    {120, 120, 120, 255}, {20, 20, 20, 255}
};

static bool text_mode(UiMenuWorkspaceMode mode) {
    return mode == UI_MENU_WORKSPACE_CREATE_NAME ||
           mode == UI_MENU_WORKSPACE_EDIT_CONTENT ||
           mode == UI_MENU_WORKSPACE_EDIT_PORT ||
           mode == UI_MENU_WORKSPACE_EDIT_NAME;
}

UiEditorRuntimeResult ui_editor_runtime_run(
    Renderer *renderer, Grid *grid, int target_fps, const char *asset_root
) {
    UiMenuWorkspace workspace;
    UiMenuWorkspaceResult last_result;
    AssetRegistry assets;
    InputState input = {0};
    UiEditorHostViewport viewport;
    double target_ms;
    bool running = true;
    bool text_input_active = false;
    if (!renderer || !grid || target_fps <= 0 || !asset_root)
        return UI_EDITOR_RUNTIME_INVALID_ARGUMENT;
    if (!asset_registry_init(&assets)) return UI_EDITOR_RUNTIME_LOAD_FAILED;
    if (!asset_loader_load_registry(&assets, asset_root)) {
        asset_registry_clear(&assets);
        return UI_EDITOR_RUNTIME_LOAD_FAILED;
    }
    ui_menu_workspace_init(&workspace);
    last_result = ui_menu_workspace_open(&workspace, asset_root);
    if (last_result != UI_MENU_WORKSPACE_OK) {
        ui_menu_workspace_clear(&workspace);
        asset_registry_clear(&assets);
        return UI_EDITOR_RUNTIME_LOAD_FAILED;
    }
    viewport = (UiEditorHostViewport){grid->width, grid->height, 40, 3};
    target_ms = timing_target_ms(target_fps);
    (void)SDL_SetWindowTitle(renderer->window, "ASCII FPS - UI Scene Editor");
    while (running) {
        uint64_t start = SDL_GetPerformanceCounter();
        bool handled = false;
        float render_x;
        float render_y;
        input_process(&input, false);
        if (input.quit) {
            input.quit = false;
            last_result = ui_menu_workspace_escape(&workspace);
            if (!workspace.active) break;
        }
        if (input.esc && !workspace.has_document) {
            last_result = ui_menu_workspace_escape(&workspace);
            if (!workspace.active) break;
        }
        (void)SDL_GetMouseState(&input.mouse_x, &input.mouse_y);
        input.mouse_grid_valid = SDL_RenderCoordinatesFromWindow(
            renderer->sdl_ren, input.mouse_x, input.mouse_y, &render_x, &render_y);
        if (input.mouse_grid_valid) {
            input.mouse_grid_x = (int)(render_x / (float)renderer->cell_w);
            input.mouse_grid_y = (int)(render_y / (float)renderer->cell_h);
        }
        last_result = ui_editor_host_apply_input(
            &workspace, &input, &viewport, &handled);
        (void)handled;
        if (!workspace.active) break;
        if (running && text_mode(workspace.mode) != text_input_active) {
            if (text_mode(workspace.mode)) {
                if (!SDL_StartTextInput(renderer->window)) {
                    ui_menu_workspace_clear(&workspace);
                    asset_registry_clear(&assets);
                    return UI_EDITOR_RUNTIME_RENDER_FAILED;
                }
                text_input_active = true;
            } else {
                (void)SDL_StopTextInput(renderer->window);
                text_input_active = false;
            }
        }
        if (!ui_editor_presentation_render(&workspace, last_result, &assets,
                &editor_theme, NULL, (double)SDL_GetTicks(), grid)) {
            if (text_input_active) (void)SDL_StopTextInput(renderer->window);
            ui_menu_workspace_clear(&workspace);
            asset_registry_clear(&assets);
            return UI_EDITOR_RUNTIME_RENDER_FAILED;
        }
        renderer_draw(renderer, grid);
        {
            double frame_ms = (double)(SDL_GetPerformanceCounter() - start) * 1000.0 /
                              (double)SDL_GetPerformanceFrequency();
            uint32_t sleep_ms = timing_sleep_ms(timing_spare_ms(frame_ms, target_ms));
            if (sleep_ms > 0U) SDL_Delay(sleep_ms);
        }
    }
    if (text_input_active) (void)SDL_StopTextInput(renderer->window);
    ui_menu_workspace_clear(&workspace);
    asset_registry_clear(&assets);
    return UI_EDITOR_RUNTIME_OK;
}