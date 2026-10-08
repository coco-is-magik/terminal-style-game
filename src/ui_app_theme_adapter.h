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
    SDL_Color editor_selection;
} UiAppWorkbenchPalette;

/** Resolves provisional application-menu colors; output is unchanged on failure. */
bool ui_app_theme_menu_palette(UiAppMenuPalette *out_palette);

/*
 * Menu button highlight.
 *
 * The focus perimeter already marks the hovered button, so the highlight is a
 * separate, swappable choice rather than a fixed coloured fill. Unselected
 * buttons always use unselected_foreground/unselected_background; a scheme only
 * decides how the *selected* button reads. Pick one with
 * UI_MENU_HIGHLIGHT_DEFAULT below.
 */
typedef enum {
    UI_MENU_HIGHLIGHT_PLAIN = 0, /* no change; the perimeter alone marks focus  */
    UI_MENU_HIGHLIGHT_BRIGHT,    /* the hovered label brightens, no fill        */
    UI_MENU_HIGHLIGHT_INVERSE,   /* the hovered label becomes a solid block     */
    UI_MENU_HIGHLIGHT_BED,       /* the hovered label sits on a neutral bed     */
    UI_MENU_HIGHLIGHT_COUNT
} UiMenuHighlight;

/* Change this one line to switch the menu highlight scheme. */
#define UI_MENU_HIGHLIGHT_DEFAULT UI_MENU_HIGHLIGHT_BRIGHT

/** Resolves the menu palette for a specific highlight scheme (false if invalid). */
bool ui_app_theme_menu_palette_for(UiAppMenuPalette *out_palette,
                                   UiMenuHighlight scheme);

bool ui_app_theme_workbench_palette(UiAppWorkbenchPalette *out_palette);

#endif