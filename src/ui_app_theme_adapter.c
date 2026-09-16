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