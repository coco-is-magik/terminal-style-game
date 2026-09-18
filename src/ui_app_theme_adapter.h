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
    SDL_Color panel;
    SDL_Color elevated;
    SDL_Color accent;
    SDL_Color focus;
    SDL_Color selection_background;
    SDL_Color disabled_text;
    SDL_Color disabled_background;
    SDL_Color warning;
    SDL_Color error;
    SDL_Color success;
    SDL_Color destructive;
} UiAppWorkbenchPalette;

/** Resolves provisional application-menu colors; output is unchanged on failure. */
bool ui_app_theme_menu_palette(UiAppMenuPalette *out_palette);
bool ui_app_theme_workbench_palette(UiAppWorkbenchPalette *out_palette);

#endif