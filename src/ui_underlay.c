/** ui_underlay.c — Deterministic frozen-frame underlay. See ui_underlay.h. */
#include "ui_underlay.h"
#include "checked_size.h"

#include <stdlib.h>
#include <string.h>

#define UI_UNDERLAY_STRENGTH_MAX 100

static int strength_clamp(int percent) {
    if (percent < 0) return 0;
    if (percent > UI_UNDERLAY_STRENGTH_MAX) return UI_UNDERLAY_STRENGTH_MAX;
    return percent;
}

/* Rounded move of one channel toward `target` by `percent`. */
static int channel_from_percent(int base, int target, int percent) {
    return (base * (UI_UNDERLAY_STRENGTH_MAX - percent) + target * percent + 50) /
           UI_UNDERLAY_STRENGTH_MAX;
}

/* Rec.601 luma in thousandths: the neutral axis the grey response lifts every
   channel toward. Integer math, so the result is identical on every platform. */
static int channel_luma(UiThemeColor color) {
    int weighted = 299 * (int)color.red + 587 * (int)color.green +
                   114 * (int)color.blue;
    return (weighted + 500) / 1000;
}

static UiThemeColor theme_color_of(SDL_Color value) {
    UiThemeColor color;
    color.red = value.r;
    color.green = value.g;
    color.blue = value.b;
    color.alpha = value.a;
    return color;
}

static SDL_Color sdl_color_of(UiThemeColor value) {
    SDL_Color color;
    color.r = value.red;
    color.g = value.green;
    color.b = value.blue;
    color.a = value.alpha;
    return color;
}

bool ui_underlay_response(UiThemeColor source, int dim_percent, int grey_percent,
                          UiThemeColor *out_color) {
    UiThemeColor resolved;
    int grey;
    int dim;
    int luma;

    if (!out_color) return false;
    grey = strength_clamp(grey_percent);
    dim = strength_clamp(dim_percent);
    luma = channel_luma(source);
    resolved.red = (uint8_t)channel_from_percent(
        channel_from_percent((int)source.red, luma, grey), 0, dim);
    resolved.green = (uint8_t)channel_from_percent(
        channel_from_percent((int)source.green, luma, grey), 0, dim);
    resolved.blue = (uint8_t)channel_from_percent(
        channel_from_percent((int)source.blue, luma, grey), 0, dim);
    resolved.alpha = source.alpha;
    *out_color = resolved;
    return true;
}

bool ui_underlay_init(UiUnderlay *underlay, int width, int height) {
    size_t count = 0;

    if (!underlay || width <= 0 || height <= 0) return false;
    if (!checked_size_2d(width, height, &count)) return false;
    memset(underlay, 0, sizeof(*underlay));
    underlay->cells = calloc(count, sizeof(Cell));
    if (!underlay->cells) return false;
    underlay->width = width;
    underlay->height = height;
    underlay->cell_count = count;
    return true;
}

void ui_underlay_destroy(UiUnderlay *underlay) {
    if (!underlay) return;
    free(underlay->cells);
    memset(underlay, 0, sizeof(*underlay));
}

void ui_underlay_forget(UiUnderlay *underlay) {
    if (!underlay) return;
    underlay->captured = false;
}

bool ui_underlay_has_frame(const UiUnderlay *underlay) {
    return underlay && underlay->cells && underlay->captured;
}

UiUnderlayResult ui_underlay_capture(UiUnderlay *underlay, const Grid *frame) {
    size_t index;
    bool has_content = false;

    if (!underlay || !underlay->cells || !frame || !frame->cells) {
        return UI_UNDERLAY_INVALID_ARGUMENT;
    }
    if (frame->width != underlay->width || frame->height != underlay->height) {
        return UI_UNDERLAY_SIZE_MISMATCH;
    }
    for (index = 0; index < underlay->cell_count; index++) {
        if (frame->cells[index].glyph != 0U) {
            has_content = true;
            break;
        }
    }
    /* An empty frame carries no background colour, so it is not a background:
       the previous capture (and the plain-black fallback) stand. */
    if (!has_content) return UI_UNDERLAY_EMPTY_FRAME;
    memcpy(underlay->cells, frame->cells, underlay->cell_count * sizeof(Cell));
    underlay->captured = true;
    return UI_UNDERLAY_OK;
}

UiUnderlayResult ui_underlay_apply(const UiUnderlay *underlay, Grid *grid,
                                   int dim_percent, int grey_percent) {
    size_t index;

    if (!underlay || !underlay->cells || !grid || !grid->cells) {
        return UI_UNDERLAY_INVALID_ARGUMENT;
    }
    if (!underlay->captured) return UI_UNDERLAY_NO_FRAME;
    if (grid->width != underlay->width || grid->height != underlay->height) {
        return UI_UNDERLAY_SIZE_MISMATCH;
    }
    for (index = 0; index < underlay->cell_count; index++) {
        const Cell *source = &underlay->cells[index];
        UiThemeColor fg;
        UiThemeColor bg;

        if (!ui_underlay_response(theme_color_of(source->fg), dim_percent,
                                  grey_percent, &fg) ||
            !ui_underlay_response(theme_color_of(source->bg), dim_percent,
                                  grey_percent, &bg)) {
            return UI_UNDERLAY_INVALID_ARGUMENT;
        }
        grid->cells[index].glyph = source->glyph;
        grid->cells[index].fg = sdl_color_of(fg);
        grid->cells[index].bg = sdl_color_of(bg);
    }
    return UI_UNDERLAY_OK;
}
