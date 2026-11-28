#include "ui_editor_runtime.h"

#include "asset_loader.h"
#include "input.h"
#include "timing.h"
#include "ui_editor_host.h"
#include "platform_clipboard.h"
#include "ui_editor_presentation.h"
#include "ui_app_theme_adapter.h"

#include <SDL3/SDL.h>

static UiRenderTheme editor_theme_from_palette(void) {
    UiAppWorkbenchPalette palette;
    UiRenderTheme theme;
    if (!ui_app_theme_workbench_palette(&palette)) {
        theme = (UiRenderTheme){{242, 247, 248, 255}, {5, 8, 10, 255},
                                {5, 8, 10, 255}, {242, 247, 248, 255},
                                {110, 120, 124, 255}, {5, 8, 10, 255}};
        return theme;
    }
    theme.focused_foreground = (UiDocumentColor){palette.primary_text.r,
        palette.primary_text.g, palette.primary_text.b, palette.primary_text.a};
    theme.focused_background = (UiDocumentColor){palette.canvas.r,
        palette.canvas.g, palette.canvas.b, palette.canvas.a};
    theme.pressed_foreground = (UiDocumentColor){palette.canvas.r,
        palette.canvas.g, palette.canvas.b, palette.canvas.a};
    theme.pressed_background = (UiDocumentColor){palette.primary_text.r,
        palette.primary_text.g, palette.primary_text.b, palette.primary_text.a};
    theme.disabled_foreground = (UiDocumentColor){palette.disabled_text.r,
        palette.disabled_text.g, palette.disabled_text.b, palette.disabled_text.a};
    theme.disabled_background = (UiDocumentColor){palette.canvas.r,
        palette.canvas.g, palette.canvas.b, palette.canvas.a};
    return theme;
}

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
    UiRenderTheme editor_theme;
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
    editor_theme = editor_theme_from_palette();
    ui_menu_workspace_init(&workspace);
    last_result = ui_menu_workspace_open(&workspace, asset_root);
    if (last_result != UI_MENU_WORKSPACE_OK) {
        ui_menu_workspace_clear(&workspace);
        asset_registry_clear(&assets);
        return UI_EDITOR_RUNTIME_LOAD_FAILED;
    }
    viewport = (UiEditorHostViewport){grid->width, grid->height};
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
        /* The clipboard is read only on the frame the key is pressed, so no allocation happens per frame,
           and only when a field is open, because that is the only place a paste can land. */
        {
            char clipboard_text[UI_DOCUMENT_CONTENT_CAPACITY];
            size_t clipboard_size = 0U;
            const char *clipboard = NULL;
            if (input.editor_paste_pressed && text_mode(workspace.mode) &&
                platform_clipboard_read_text(clipboard_text, sizeof(clipboard_text),
                                             &clipboard_size) == PLATFORM_CLIPBOARD_OK)
                clipboard = clipboard_text;
            last_result = ui_editor_host_apply_input(
                &workspace, &input, &viewport, clipboard, &handled);
        }
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