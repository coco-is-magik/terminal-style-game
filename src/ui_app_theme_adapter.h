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

typedef struct {
    SDL_Color primary_text;
    SDL_Color secondary_text;
    SDL_Color canvas;
    SDL_Color border;
} UiAppWorkbenchPalette;

/** Resolves provisional application-menu colors; output is unchanged on failure. */
bool ui_app_theme_menu_palette(UiAppMenuPalette *out_palette);
bool ui_app_theme_workbench_palette(UiAppWorkbenchPalette *out_palette);

#endif