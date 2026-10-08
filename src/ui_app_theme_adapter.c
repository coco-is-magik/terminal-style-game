#include "ui_app_theme_adapter.h"

#include "ui_theme.h"

static SDL_Color to_sdl_color(UiThemeColor color) {
    return (SDL_Color){color.red, color.green, color.blue, color.alpha};
}

/* How the selected menu button reads. The perimeter frame marks focus, so these
   schemes are deliberately neutral — none of them re-introduces the old green
   selection fill. */
static void menu_highlight_colors(const UiThemePalette *theme, UiMenuHighlight scheme,
                                  SDL_Color *foreground, SDL_Color *background) {
    switch (scheme) {
        case UI_MENU_HIGHLIGHT_PLAIN:
            *foreground = to_sdl_color(theme->text_secondary);
            *background = to_sdl_color(theme->canvas);
            return;
        case UI_MENU_HIGHLIGHT_INVERSE:
            *foreground = to_sdl_color(theme->canvas);
            *background = to_sdl_color(theme->text_primary);
            return;
        case UI_MENU_HIGHLIGHT_BED:
            *foreground = to_sdl_color(theme->text_primary);
            *background = to_sdl_color(theme->elevated);
            return;
        case UI_MENU_HIGHLIGHT_BRIGHT:
        default:
            *foreground = to_sdl_color(theme->text_primary);
            *background = to_sdl_color(theme->canvas);
            return;
    }
}

bool ui_app_theme_menu_palette_for(UiAppMenuPalette *out_palette,
                                   UiMenuHighlight scheme) {
    const UiThemePalette *theme;
    UiAppMenuPalette palette;
    if (!out_palette || scheme < 0 || scheme >= UI_MENU_HIGHLIGHT_COUNT) return false;
    theme = &ui_theme_provisional_tokens()->palette;
    menu_highlight_colors(theme, scheme, &palette.selected_foreground,
                          &palette.selected_background);
    palette.unselected_foreground = to_sdl_color(theme->text_secondary);
    palette.unselected_background = to_sdl_color(theme->canvas);
    *out_palette = palette;
    return true;
}

bool ui_app_theme_menu_palette(UiAppMenuPalette *out_palette) {
    return ui_app_theme_menu_palette_for(out_palette, UI_MENU_HIGHLIGHT_DEFAULT);
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