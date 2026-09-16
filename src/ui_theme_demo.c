#include "ui_theme_demo.h"

#include "ui_theme.h"

#include <stdio.h>
#include <string.h>

static SDL_Color color(UiThemeColor value) {
    return (SDL_Color){value.red, value.green, value.blue, value.alpha};
}

static bool valid_scale(int scale) {
    return scale == 100 || scale == 125 || scale == 150 || scale == 200;
}

static int next_scale(int scale, int direction) {
    static const int scales[] = {100, 125, 150, 200};
    size_t i;
    for (i = 0U; i < sizeof(scales) / sizeof(scales[0]); i++) {
        if (scales[i] == scale) {
            int count = (int)(sizeof(scales) / sizeof(scales[0]));
            return scales[((int)i + direction + count) % count];
        }
    }
    return 100;
}

void ui_theme_demo_state_init(UiThemeDemoState *state) {
    if (state) state->scale_percent = 100;
}

bool ui_theme_demo_apply_input(UiThemeDemoState *state, const InputState *input,
                               bool *out_should_exit) {
    UiThemeDemoState candidate;
    bool should_exit;
    if (!state || !input || !out_should_exit) return false;
    candidate = *state;
    should_exit = input->quit || input->esc;
    if (input->ui_scale_reset_pressed) candidate.scale_percent = 100;
    else if (input->arrow_right || input->ui_scale_increase_pressed)
        candidate.scale_percent = next_scale(candidate.scale_percent, 1);
    else if (input->arrow_left || input->ui_scale_decrease_pressed)
        candidate.scale_percent = next_scale(candidate.scale_percent, -1);
    if (!valid_scale(candidate.scale_percent)) return false;
    *state = candidate;
    *out_should_exit = should_exit;
    return true;
}

static bool fill(UiCanvas *canvas, int x, int y, int width, int height,
                 uint8_t glyph, SDL_Color fg, SDL_Color bg) {
    int row;
    int column;
    if (!canvas || x < 0 || y < 0 || width < 0 || height < 0 ||
        x + width > canvas->width || y + height > canvas->height) return false;
    for (row = y; row < y + height; row++)
        for (column = x; column < x + width; column++)
            if (!ui_canvas_set(canvas, column, row, glyph, fg, bg)) return false;
    return true;
}

static bool print(UiCanvas *canvas, int x, int y, const char *text,
                  SDL_Color fg, SDL_Color bg) {
    size_t length;
    size_t i;
    if (!canvas || !text || x < 0 || y < 0 || y >= canvas->height) return false;
    length = strlen(text);
    if (length > (size_t)(canvas->width - x)) return false;
    for (i = 0U; i < length; i++)
        if (!ui_canvas_set(canvas, x + (int)i, y, (uint8_t)text[i], fg, bg)) return false;
    return true;
}

static bool box(UiCanvas *canvas, int x, int y, int width, int height,
                SDL_Color border, SDL_Color background) {
    int row;
    int column;
    if (!fill(canvas, x, y, width, height, ' ', border, background)) return false;
    for (column = x; column < x + width; column++) {
        if (!ui_canvas_set(canvas, column, y, '-', border, background) ||
            !ui_canvas_set(canvas, column, y + height - 1, '-', border, background))
            return false;
    }
    for (row = y; row < y + height; row++) {
        if (!ui_canvas_set(canvas, x, row, '|', border, background) ||
            !ui_canvas_set(canvas, x + width - 1, row, '|', border, background))
            return false;
    }
    return ui_canvas_set(canvas, x, y, '+', border, background) &&
        ui_canvas_set(canvas, x + width - 1, y, '+', border, background) &&
        ui_canvas_set(canvas, x, y + height - 1, '+', border, background) &&
        ui_canvas_set(canvas, x + width - 1, y + height - 1, '+', border, background);
}

static bool swatch(UiCanvas *canvas, int x, int y, const char *label,
                   const char *hex, SDL_Color role, SDL_Color background,
                   char marker) {
    char text[40];
    int written = snprintf(text, sizeof(text), "%c %-13s %-7s", marker, label, hex);
    if (written < 0 || (size_t)written >= sizeof(text)) return false;
    return fill(canvas, x, y, 30, 2, ' ', role, background) &&
        print(canvas, x + 1, y, text, role, background) &&
        print(canvas, x + 1, y + 1, "SAMPLE Aa09 +-#*!", role, background);
}

static bool state_row(UiCanvas *canvas, int y, const char *name,
                      const char *sample, SDL_Color foreground,
                      SDL_Color background) {
    return print(canvas, 2, y, name, foreground, color(ui_theme_provisional_tokens()->palette.canvas)) &&
        fill(canvas, 20, y, 32, 1, ' ', foreground, background) &&
        print(canvas, 21, y, sample, foreground, background);
}

static bool render_specimen(const UiThemeDemoState *state, UiCanvas *canvas) {
    const UiThemePalette *p = &ui_theme_provisional_tokens()->palette;
    SDL_Color canvas_color = color(p->canvas);
    SDL_Color panel = color(p->panel);
    SDL_Color elevated = color(p->elevated);
    SDL_Color primary = color(p->text_primary);
    SDL_Color secondary = color(p->text_secondary);
    SDL_Color border = color(p->border);
    char scale[64];
    int scale_written;

    if (!fill(canvas, 0, 0, canvas->width, canvas->height,
              ' ', primary, canvas_color)) return false;
    if (!print(canvas, 2, 1, "ACCEPTED UI THEME SPECIMEN", primary, canvas_color) ||
        !print(canvas, 2, 2, "DIAGNOSTIC ONLY - NOT PRODUCT UI", color(p->warning), canvas_color))
        return false;
    scale_written = snprintf(scale, sizeof(scale),
                             "Scale: %d%% | Raster: 8x8 | Canvas: 96x56",
                             state->scale_percent);
    if (scale_written < 0 || (size_t)scale_written >= sizeof(scale) ||
        !print(canvas, 2, 3, scale, secondary, canvas_color) ||
        !print(canvas, 2, 4,
               "Left/Right or Ctrl-/Ctrl+ change scale | Ctrl+0 reset | Esc exit",
               secondary, canvas_color)) return false;

    if (!box(canvas, 2, 6, 28, 8, border, canvas_color) ||
        !print(canvas, 4, 7, "CANVAS", primary, canvas_color) ||
        !print(canvas, 4, 9, "Primary text", primary, canvas_color) ||
        !print(canvas, 4, 10, "Secondary text", secondary, canvas_color) ||
        !print(canvas, 4, 11, "Accent signal", color(p->accent), canvas_color)) return false;
    if (!box(canvas, 34, 6, 28, 8, border, panel) ||
        !print(canvas, 36, 7, "PANEL", primary, panel) ||
        !print(canvas, 36, 9, "Primary text", primary, panel) ||
        !print(canvas, 36, 10, "Secondary text", secondary, panel) ||
        !print(canvas, 36, 11, "Accent signal", color(p->accent), panel)) return false;
    if (!box(canvas, 66, 6, 28, 8, color(p->focus), elevated) ||
        !print(canvas, 68, 7, "ELEVATED / MODAL", primary, elevated) ||
        !print(canvas, 68, 9, "Primary text", primary, elevated) ||
        !print(canvas, 68, 10, "Secondary text", secondary, elevated) ||
        !print(canvas, 68, 11, "> Focus edge <", color(p->focus), elevated)) return false;

    if (!print(canvas, 2, 15, "SEMANTIC ROLES", primary, canvas_color) ||
        !swatch(canvas, 2, 17, "TEXT PRIMARY", "#F2F7F8", primary, canvas_color, 'A') ||
        !swatch(canvas, 34, 17, "TEXT SECONDARY", "#A8B4B8", secondary, panel, 'a') ||
        !swatch(canvas, 66, 17, "BORDER", "#64757C", border, panel, '+') ||
        !swatch(canvas, 2, 20, "ACCENT", "#67F5C2", color(p->accent), canvas_color, '*') ||
        !swatch(canvas, 34, 20, "FOCUS", "#A8FFE1", color(p->focus), panel, '>') ||
        !swatch(canvas, 66, 20, "SELECTION BG", "#123D32", primary,
                color(p->selection_background), '*') ||
        !swatch(canvas, 2, 23, "DISABLED", "#8D999D", color(p->disabled_text),
                color(p->disabled_background), '!') ||
        !swatch(canvas, 34, 23, "WARNING", "#FFD166", color(p->warning), panel, '!') ||
        !swatch(canvas, 66, 23, "ERROR", "#FF6B7A", color(p->error), panel, 'X') ||
        !swatch(canvas, 2, 26, "SUCCESS", "#71F79F", color(p->success), panel, '+') ||
        !swatch(canvas, 34, 26, "DESTRUCTIVE", "#FF8894", color(p->destructive), panel, '!') ||
        !swatch(canvas, 66, 26, "ELEVATED", "#162126", primary, elevated, '+')) return false;

    if (!print(canvas, 2, 30, "STATIC COMPONENT STATE MATRIX", primary, canvas_color) ||
        !state_row(canvas, 32, "NORMAL", "[   OPEN PROJECT   ]", primary, panel) ||
        !state_row(canvas, 34, "HOVER REF", "[ ~ OPEN PROJECT ~ ]", color(p->accent), panel) ||
        !state_row(canvas, 36, "SELECTED", "[ * OPEN PROJECT * ]", primary,
                   color(p->selection_background)) ||
        !state_row(canvas, 38, "FOCUSED", "[ > OPEN PROJECT < ]", color(p->focus), panel) ||
        !state_row(canvas, 40, "PRESSED", "[ # OPEN PROJECT # ]", canvas_color, color(p->accent)) ||
        !state_row(canvas, 42, "DISABLED", "[ ! OPEN PROJECT ! ]", color(p->disabled_text),
                   color(p->disabled_background)) ||
        !state_row(canvas, 44, "FOCUS+ERROR", "[ > RETRY SAVE < ] ! ERROR", color(p->focus),
                   color(p->selection_background)) ||
        !state_row(canvas, 46, "DESTRUCTIVE", "[ ! DELETE ITEM ! ]", color(p->destructive), panel))
        return false;

    if (!print(canvas, 58, 32, "STATUS MESSAGES", primary, canvas_color) ||
        !print(canvas, 58, 34, "[+] SUCCESS  Project saved", color(p->success), canvas_color) ||
        !print(canvas, 58, 36, "[!] WARNING  Unsaved changes", color(p->warning), canvas_color) ||
        !print(canvas, 58, 38, "[X] ERROR    Save failed", color(p->error), canvas_color) ||
        !print(canvas, 58, 40, "[!] DELETE   Cannot be undone", color(p->destructive), canvas_color) ||
        !print(canvas, 58, 42, "[-] DISABLED Action unavailable", color(p->disabled_text),
               color(p->disabled_background)) ||
        !print(canvas, 2, 51, "STATIC REFERENCES: hover/pressed states are samples, not behavior.",
               secondary, canvas_color) ||
        !print(canvas, 2, 53, "Palette accepted; diagnostic only, not application UI.",
               color(p->success), canvas_color)) return false;
    return true;
}

UiThemeDemoRenderResult ui_theme_demo_render(const UiThemeDemoState *state,
                                              UiCanvas *canvas) {
    UiCanvas *candidate;
    size_t count;
    if (!state || !canvas || canvas->width != UI_THEME_DEMO_WIDTH ||
        canvas->height != UI_THEME_DEMO_HEIGHT || !canvas->cells || !canvas->touched ||
        !valid_scale(state->scale_percent)) return UI_THEME_DEMO_RENDER_INVALID_ARGUMENT;
    candidate = ui_canvas_create(canvas->width, canvas->height);
    if (!candidate) return UI_THEME_DEMO_RENDER_OUT_OF_MEMORY;
    if (!render_specimen(state, candidate)) {
        ui_canvas_destroy(candidate);
        return UI_THEME_DEMO_RENDER_INVARIANT_FAILED;
    }
    count = (size_t)canvas->width * (size_t)canvas->height;
    memcpy(canvas->cells, candidate->cells, count * sizeof(*canvas->cells));
    memcpy(canvas->touched, candidate->touched, count * sizeof(*canvas->touched));
    ui_canvas_destroy(candidate);
    return UI_THEME_DEMO_RENDER_OK;
}