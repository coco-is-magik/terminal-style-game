#include "ui_app_theme_adapter.h"

#include "ui_theme.h"

static SDL_Color to_sdl_color(UiThemeColor color) {
    return (SDL_Color){color.red, color.green, color.blue, color.alpha};
}

bool ui_app_theme_menu_palette(UiAppMenuPalette *out_palette) {
    const UiThemePalette *theme;
    UiAppMenuPalette palette;
    if (!out_palette) return false;
    theme = &ui_theme_provisional_tokens()->palette;
    palette.selected_foreground = to_sdl_color(theme->focus);
    palette.selected_background = to_sdl_color(theme->selection_background);
    palette.unselected_foreground = to_sdl_color(theme->text_secondary);
    palette.unselected_background = to_sdl_color(theme->canvas);
    *out_palette = palette;
    return true;
}

bool ui_app_theme_workbench_palette(UiAppWorkbenchPalette *out_palette) {
    const UiThemePalette *theme;
    UiAppWorkbenchPalette palette;
    if (!out_palette) return false;
    theme = &ui_theme_provisional_tokens()->palette;
    palette.primary_text = to_sdl_color(theme->text_primary);
    palette.secondary_text = to_sdl_color(theme->text_secondary);
    palette.canvas = to_sdl_color(theme->canvas);
    palette.border = to_sdl_color(theme->border);
    palette.panel = to_sdl_color(theme->panel);
    palette.elevated = to_sdl_color(theme->elevated);
    palette.accent = to_sdl_color(theme->accent);
    palette.focus = to_sdl_color(theme->focus);
    palette.selection_background = to_sdl_color(theme->selection_background);
    palette.disabled_text = to_sdl_color(theme->disabled_text);
    palette.disabled_background = to_sdl_color(theme->disabled_background);
    palette.warning = to_sdl_color(theme->warning);
    palette.error = to_sdl_color(theme->error);
    palette.success = to_sdl_color(theme->success);
    palette.destructive = to_sdl_color(theme->destructive);
    palette.editor_selection = to_sdl_color(theme->editor_selection);
    *out_palette = palette;
    return true;
}