#include "ui_animation.h"

#include "ui_motion.h"
#include "ui_theme.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define PAUSE_GLITCH_STABLE_ID UINT32_C(0x50415553)

typedef bool (*SetCell)(void *, int, int, uint8_t, SDL_Color, SDL_Color);

static bool set_grid(void *destination, int x, int y, uint8_t glyph,
                     SDL_Color fg, SDL_Color bg) {
    return grid_set((Grid *)destination, x, y, glyph, fg, bg);
}

static bool set_canvas(void *destination, int x, int y, uint8_t glyph,
                       SDL_Color fg, SDL_Color bg) {
    return ui_canvas_set((UiCanvas *)destination, x, y, glyph, fg, bg);
}

static uint32_t stable_id(const char *name) {
    uint32_t value = UINT32_C(2166136261);
    const unsigned char *cursor = (const unsigned char *)(name ? name : "");
    while (*cursor) {
        value ^= *cursor++;
        value *= UINT32_C(16777619);
    }
    return value == 0U ? 1U : value;
}

static SDL_Color color(UiThemeColor value) {
    return (SDL_Color){value.red, value.green, value.blue, value.alpha};
}

static UiMotionOffset orient(UiMotionOffset offset, const char *orientation) {
    if (orientation && strcmp(orientation, "vertical") == 0)
        return (UiMotionOffset){offset.y, offset.x};
    return offset;
}

static bool render_pause_glitch(void *destination, SetCell set,
                                const UiElementLayout *bounds, const char *name,
                                const char *orientation, bool randomize,
                                double progress,
                                bool reduced_motion) {
    static const uint8_t glyphs[] = {'+', '-', ':', '+', '-', ':', '-', '+', ':', '-'};
    const UiThemePalette *palette = &ui_theme_provisional_tokens()->palette;
    SDL_Color background = color(palette->canvas);
    size_t i;
    if (!destination || !set || !bounds || !isfinite(progress) ||
        progress < 0.0 || progress > 1.0) return false;
    if (reduced_motion || progress >= 1.0) return true;
    for (i = 0U; i < sizeof(glyphs) / sizeof(glyphs[0]); i++) {
        UiMotionOffset offset;
        uint32_t seed = stable_id(name) ^ (uint32_t)(i * 2654435761U);
        int x = randomize && bounds->width > 0
            ? bounds->x + (int)(seed % (uint32_t)bounds->width)
            : bounds->x - 3 + (int)(i % 5U) * 3;
        int y = randomize && bounds->height > 0
            ? bounds->y + (int)((seed >> 8) % (uint32_t)bounds->height)
            : i < 5U ? bounds->y - 4 : bounds->y + bounds->height - 3;
        SDL_Color foreground = (i % 2U) == 0U
            ? color(palette->accent) : color(palette->focus);
        if (!ui_motion_glyph_offset(randomize ? stable_id(name) : PAUSE_GLITCH_STABLE_ID, i,
                                    sizeof(glyphs) / sizeof(glyphs[0]),
                                    progress, false, &offset)) return false;
        offset = orient(offset, orientation);
        if (!set(destination, x + offset.x, y + offset.y, glyphs[i],
                 foreground, background)) return false;
    }
    return true;
}

bool ui_animation_render_pause_glitch_canvas(UiCanvas *canvas,
                                             const UiElementLayout *bounds,
                                             double progress,
                                             bool reduced_motion) {
    return render_pause_glitch(canvas, set_canvas, bounds, "pause_glitch",
                               "horizontal", false, progress, reduced_motion);
}

static UiElement *find_element(UiLayout *layout, const char *name) {
    int i;
    if (!layout || !name) return NULL;
    for (i = 0; i < layout->element_count; i++) {
        UiElement *element = layout->elements[i];
        UiElement *cursor = element;
        while (cursor) {
            if (strcmp(cursor->name, name) == 0) return cursor;
            cursor = cursor->parent;
        }
    }
    return NULL;
}

static bool animation_progress(const UiElement *unit, double elapsed_ms,
                               bool preview_loop, bool *visible,
                               double *out_progress) {
    double elapsed = elapsed_ms;
    double progress;
    UiThemeMotionRole role = UI_THEME_MOTION_MAJOR_ENTER;
    double duration;
    if (strcmp(unit->trigger, "context_exit") == 0) role = UI_THEME_MOTION_MAJOR_EXIT;
    else if (strcmp(unit->trigger, "focus") == 0 || strcmp(unit->trigger, "activate") == 0)
        role = UI_THEME_MOTION_FEEDBACK;
    else if (strcmp(unit->trigger, "while_visible") == 0) role = UI_THEME_MOTION_RELATIONSHIP;
    duration = (double)ui_theme_motion_duration_ms(role, false);
    *visible = true;
    if (unit->loop || preview_loop || strcmp(unit->trigger, "while_visible") == 0)
        elapsed = fmod(elapsed_ms, duration);
    else if (elapsed_ms >= duration) *visible = false;
    if (!ui_theme_motion_progress(role, elapsed,
                                  false, &progress)) return false;
    *out_progress = strcmp(unit->trigger, "context_exit") == 0
        ? 1.0 - progress : progress;
    return true;
}

static bool render_center_out(Grid *grid, const UiElementLayout *bounds,
                              double progress, SDL_Color fg, SDL_Color bg) {
    Cell source[UI_LAYOUT_MAX_ELEMS * UI_LAYOUT_MAX_ELEMS];
    int source_x[UI_LAYOUT_MAX_ELEMS * UI_LAYOUT_MAX_ELEMS];
    int source_y[UI_LAYOUT_MAX_ELEMS * UI_LAYOUT_MAX_ELEMS];
    int count = 0;
    int max_cells = UI_LAYOUT_MAX_ELEMS * UI_LAYOUT_MAX_ELEMS;
    int center_x = bounds->x + bounds->width / 2;
    int center_y = bounds->y + bounds->height / 2;
    int y;
    int x;
    (void)fg;
    (void)bg;
    if (progress >= 1.0) return true;
    for (y = bounds->y; y < bounds->y + bounds->height && count < max_cells; y++) {
        for (x = bounds->x; x < bounds->x + bounds->width && count < max_cells; x++) {
            Cell cell;
            if (grid_get(grid, x, y, &cell) && cell.glyph != 0U && cell.glyph != ' ') {
                source[count] = cell;
                source_x[count] = x;
                source_y[count] = y;
                count++;
            }
        }
    }
    for (int i = 0; i < count; i++) {
        int dx = source_x[i] == center_x ? 0 : source_x[i] < center_x ? -1 : 1;
        int dy = source_y[i] == center_y ? 0 : source_y[i] < center_y ? -1 : 1;
        int distance = 1 + (int)lround((1.0 - progress) * 4.0);
        (void)grid_set(grid, source_x[i] + dx * distance,
                       source_y[i] + dy * distance, source[i].glyph,
                       source[i].fg, source[i].bg);
    }
    return true;
}

static bool render_unit(UiElement *unit, UiLayout *layout, Grid *grid,
                        double elapsed_ms, bool reduced_motion, bool preview_loop,
                        UiAnimationEvent event) {
    const UiThemePalette *palette = &ui_theme_provisional_tokens()->palette;
    UiElement *target = find_element(layout, unit->target);
    UiElementLayout bounds;
    bool visible;
    double progress;
    int x;
    int y;
    int width;
    int height;
    if (!target || !ui_ele_absolute_bounds(target, &x, &y, &width, &height)) return false;
    bounds = (UiElementLayout){x + unit->layout.x, y + unit->layout.y,
                               UI_COORD_ABSOLUTE,
                               unit->layout.width > 0 ? unit->layout.width : width,
                               unit->layout.height > 0 ? unit->layout.height : height};
    if (reduced_motion) return true;
    if (!preview_loop && event != UI_ANIMATION_EVENT_PREVIEW) {
        if (strcmp(unit->trigger, "context_enter") == 0 &&
            event != UI_ANIMATION_EVENT_CONTEXT_ENTER) return true;
        if (strcmp(unit->trigger, "context_exit") == 0 &&
            event != UI_ANIMATION_EVENT_CONTEXT_EXIT) return true;
        if (strcmp(unit->trigger, "focus") == 0 &&
            event != UI_ANIMATION_EVENT_FOCUS) return true;
        if (strcmp(unit->trigger, "activate") == 0 &&
            event != UI_ANIMATION_EVENT_ACTIVATE) return true;
        if (strcmp(unit->trigger, "while_visible") == 0 &&
            event != UI_ANIMATION_EVENT_WHILE_VISIBLE) return true;
    }
    if (strcmp(unit->trigger, "focus") == 0 && !target->focused) return true;
    if (!animation_progress(unit, elapsed_ms, preview_loop, &visible, &progress))
        return false;
    if (!visible) return true;
    if (strcmp(unit->preset, "pause_glitch") == 0)
        return render_pause_glitch(grid, set_grid, &bounds, unit->name,
                                   unit->orientation, unit->randomize != 0,
                                   progress, false);
    if (strcmp(unit->preset, "center_out") == 0)
        return render_center_out(grid, &bounds, progress,
                                 color(palette->focus), color(palette->canvas));
    if (strcmp(unit->preset, "perimeter_burst") == 0) {
        int offset = 1 + (int)lround(progress * 3.0);
        (void)grid_set(grid, bounds.x - offset, bounds.y, '*',
                       color(palette->accent), color(palette->canvas));
        (void)grid_set(grid, bounds.x + bounds.width - 1 + offset, bounds.y, '*',
                       color(palette->accent), color(palette->canvas));
        return true;
    }
    if (strcmp(unit->preset, "local_glitch") == 0) {
        int offset = (int)((uint64_t)(elapsed_ms / 80.0) % 3U) - 1;
        (void)grid_set(grid, bounds.x + bounds.width / 2 + offset, bounds.y - 1,
                       ':', color(palette->accent), color(palette->canvas));
        return true;
    }
    return false;
}

static bool render_layout_units(UiLayout *layout, Grid *grid, double elapsed_ms,
                                bool reduced_motion, bool preview_loop,
                                UiAnimationEvent event) {
    UiElement *processed[UI_LAYOUT_MAX_ELEMS];
    int processed_count = 0;
    int i;
    if (!layout || !grid || !isfinite(elapsed_ms) || elapsed_ms < 0.0) return false;
    for (i = 0; i < layout->element_count; i++) {
        UiElement *element = layout->elements[i];
        UiElement *cursor = element;
        while (cursor) {
            bool known = false;
            for (int p = 0; p < processed_count; p++)
                if (processed[p] == cursor) known = true;
            if (!known && (event == UI_ANIMATION_EVENT_CONTEXT_ENTER ||
                           event == UI_ANIMATION_EVENT_CONTEXT_EXIT ||
                           event == UI_ANIMATION_EVENT_PREVIEW) &&
                cursor->type != UI_ELE_ANIMATION && cursor->visible &&
                strcmp(cursor->transition, "none") != 0) {
            UiElement unit = {0};
            unit.type = UI_ELE_ANIMATION;
            unit.visible = 1;
            unit.layout = cursor->layout;
            unit.layout.x = 0;
            unit.layout.y = 0;
            (void)snprintf(unit.name, sizeof(unit.name), "%s", cursor->name);
            (void)snprintf(unit.preset, sizeof(unit.preset), "%s", cursor->transition);
            (void)snprintf(unit.target, sizeof(unit.target), "%s", cursor->name);
            (void)snprintf(unit.trigger, sizeof(unit.trigger), "%s",
                event == UI_ANIMATION_EVENT_CONTEXT_EXIT ? "context_exit" : "context_enter");
            (void)snprintf(unit.orientation, sizeof(unit.orientation), "radial");
            if (!render_unit(&unit, layout, grid, elapsed_ms,
                             reduced_motion, preview_loop,
                              event == UI_ANIMATION_EVENT_CONTEXT_EXIT
                                  ? UI_ANIMATION_EVENT_CONTEXT_EXIT
                                  : UI_ANIMATION_EVENT_CONTEXT_ENTER)) return false;
            }
            if (!known && processed_count < UI_LAYOUT_MAX_ELEMS)
                processed[processed_count++] = cursor;
            cursor = cursor->parent;
        }
        if (element && element->type == UI_ELE_ANIMATION && element->visible &&
            !render_unit(element, layout, grid, elapsed_ms,
                         reduced_motion, preview_loop, event)) return false;
    }
    return true;
}

bool ui_animation_render_layout(UiLayout *layout, Grid *grid, double elapsed_ms,
                                bool reduced_motion, bool preview_loop,
                                UiAnimationEvent event) {
    Grid *mask;
    UiCanvas *authored;
    bool result;
    size_t count;
    SDL_Color unused = {0};
    if (!layout || !grid || !grid->cells || !isfinite(elapsed_ms) || elapsed_ms < 0 ||
        event < UI_ANIMATION_EVENT_CONTEXT_ENTER || event > UI_ANIMATION_EVENT_PREVIEW)
        return false;
    if (reduced_motion) return true;
    if (event == UI_ANIMATION_EVENT_PREVIEW)
        return render_layout_units(layout, grid, elapsed_ms, false, preview_loop, event);
    /* A fresh layout render identifies authored cells, including authored spaces,
       independently of the caller's backdrop. Save their original colors too. */
    mask = grid_create(grid->width, grid->height);
    if (!mask) return false;
    authored = ui_canvas_create(grid->width, grid->height);
    if (!authored) {
        grid_destroy(mask);
        return false;
    }
    ui_layout_render(layout, mask, unused, unused);
    ui_canvas_copy_grid_region(authored, mask, 0, 0);
    count = (size_t)grid->width * (size_t)grid->height;
    for (size_t i = 0; i < count; i++)
        if (authored->touched[i]) authored->cells[i] = grid->cells[i];
    grid_destroy(mask);
    result = render_layout_units(layout, grid, elapsed_ms, false, preview_loop, event);
    for (size_t i = 0; i < count; i++)
        if (authored->touched[i]) grid->cells[i] = authored->cells[i];
    ui_canvas_destroy(authored);
    return result;
}
