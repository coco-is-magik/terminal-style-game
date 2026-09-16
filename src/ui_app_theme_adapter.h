/** ui_app_theme_adapter.h — Accepted application-menu theme mapping. */
#ifndef UI_APP_THEME_ADAPTER_H
#define UI_APP_THEME_ADAPTER_H

#include <stdbool.h>
#include <SDL3/SDL.h>

typedef struct {
    SDL_Color selected_foreground;
    SDL_Color selected_background;
    SDL_Color unselected_foreground;
    SDL_Color unselected_background;
} UiAppMenuPalette;

/** Resolves provisional application-menu colors; output is unchanged on failure. */
bool ui_app_theme_menu_palette(UiAppMenuPalette *out_palette);

#endif