#include "ui_animation.h"

#include "ui_effect.h"
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
    else if (strcmp(unit->trigger, "while_visible") == 0) role = UI_THEME_MOTION_AMBIENT;
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

static bool render_edge_trace(Grid *grid, const UiElementLayout *bounds,
                              double progress, SDL_Color accent,
                              SDL_Color focus, SDL_Color background) {
    int span;
    int offset;
    if (!grid || !bounds || bounds->width < 1 || !isfinite(progress) ||
        progress < 0.0 || progress > 1.0) return false;
    span = bounds->width - 1;
    offset = (int)lround(progress * (double)span);
    (void)grid_set(grid, bounds->x + offset, bounds->y - 1,
                   '=', accent, background);
    (void)grid_set(grid, bounds->x + span - offset,
                   bounds->y + bounds->height, '=', focus, background);
    return true;
}

static bool render_chromatic_register(Grid *grid, const UiElementLayout *bounds,
                                      double progress, SDL_Color accent,
                                      SDL_Color focus, SDL_Color background) {
    int center;
    int separation;
    if (!grid || !bounds || bounds->width < 1 || !isfinite(progress) ||
        progress < 0.0 || progress > 1.0) return false;
    center = bounds->x + (bounds->width - 1) / 2;
    separation = (int)ceil((1.0 - progress) * 3.0);
    (void)grid_set(grid, center - separation, bounds->y - 1,
                   ':', accent, background);
    (void)grid_set(grid, center + separation, bounds->y + bounds->height,
                   ':', focus, background);
    return true;
}

static bool render_command_flash(Grid *grid, const UiElementLayout *bounds,
                                 double progress, SDL_Color accent,
                                 SDL_Color focus, SDL_Color background) {
    int distance;
    if (!grid || !bounds || bounds->width < 1 || !isfinite(progress) ||
        progress < 0.0 || progress > 1.0) return false;
    distance = 1 + (int)lround((1.0 - progress) * 2.0);
    (void)grid_set(grid, bounds->x - distance, bounds->y,
                   '!', accent, background);
    (void)grid_set(grid, bounds->x + bounds->width - 1 + distance,
                   bounds->y, '!', focus, background);
    return true;
}

static bool render_button_reassemble(Grid *grid, const UiElementLayout *bounds,
                                     double progress, SDL_Color accent,
                                     SDL_Color focus, SDL_Color background) {
    static const uint8_t glyphs[] = {'[', ':', ']', '[', ':', ']'};
    int center;
    int spread;
    if (!grid || !bounds || bounds->width < 1 || !isfinite(progress) ||
        progress < 0.0 || progress > 1.0) return false;
    center = bounds->x + (bounds->width - 1) / 2;
    spread = 1 + (int)lround((1.0 - progress) * 5.0);
    for (size_t i = 0U; i < sizeof(glyphs) / sizeof(glyphs[0]); i++) {
        int side = i < 3U ? -1 : 1;
        int rank = (int)(i % 3U);
        int x = center + side * (spread + rank * 2);
        int y = (i % 2U) == 0U ? bounds->y - 1 : bounds->y + bounds->height;
        (void)grid_set(grid, x, y, glyphs[i],
                       (i % 2U) == 0U ? accent : focus, background);
    }
    return true;
}

static bool render_panel_register(Grid *grid, const UiElementLayout *bounds,
                                  double progress, SDL_Color accent,
                                  SDL_Color focus, SDL_Color background) {
    int inset;
    int left;
    int right;
    int top;
    int bottom;
    if (!grid || !bounds || bounds->width < 1 || bounds->height < 1 ||
        !isfinite(progress) || progress < 0.0 || progress > 1.0) return false;
    inset = (int)lround((1.0 - progress) * 4.0);
    left = bounds->x - 1 - inset;
    right = bounds->x + bounds->width + inset;
    top = bounds->y - 1 - inset;
    bottom = bounds->y + bounds->height + inset;
    (void)grid_set(grid, left, top, '+', accent, background);
    (void)grid_set(grid, right, top, '+', focus, background);
    (void)grid_set(grid, left, bottom, '+', focus, background);
    (void)grid_set(grid, right, bottom, '+', accent, background);
    return true;
}

#define UI_LIVING_FIELD_TABLE 512

/*
 * living_field material selection — a white-dominant flowing fabric fringed by
 * chromatic aberration.
 *
 * The §1.1 reference (docs/inspiration_and_notes/light_and_motion) is a neutral
 * weave of light cells whose density folds and travels; saturated
 * primary/secondary colour appears only as a thin *fringing* where the light
 * meets empty space — a chromatic aberration of the weave's own edges, not a
 * fill. The structure is therefore neutral-dominant and only edge cells are
 * coloured.
 *
 * Occupancy is the product of two travelling axis waves — so lit cells form a
 * coherent grid of *rectangular* patches (the reference's clear rectangular
 * forms) that migrate and breathe as one material — modulated by a slower
 * diagonal wave that folds it. It is never a flat stripe, a spatial hue ramp, a
 * ring, or per-cell flicker. All selection is a pure function of position and
 * explicit time, so the field is deterministic and loops seamlessly over the
 * AMBIENT role.
 */
#define UI_LIVING_FIELD_PERIOD_X 14      /* horizontal travel (cells)          */
#define UI_LIVING_FIELD_PERIOD_Y 11      /* vertical travel (cells)            */
#define UI_LIVING_FIELD_PERIOD_D 16      /* diagonal fold (must divide TABLE)  */
#define UI_LIVING_FIELD_LIT_LEVEL 0.30   /* fabric density above this -> lit   */
#define UI_LIVING_FIELD_FIELD_MAX 1.10   /* max of wave_x*wave_y*fold          */
#define UI_LIVING_FIELD_RUN 5            /* cells per chromatic fringe run     */
#define UI_LIVING_FIELD_FRINGE 0.55      /* fraction of fringe runs coloured   */
#define UI_LIVING_FIELD_REGION_MAX 16384 /* content-mask cap, in cells         */

/* Cheap deterministic 32-bit finalizer (SplitMix-style avalanche). Pure; used
   only to pick decorative material, never to drive layout or semantics. */
static uint32_t living_field_mix(uint32_t a, uint32_t b, uint32_t c) {
    uint32_t h = a * 374761393U + b * 668265263U + c * 2246822519U;
    h ^= h >> 13;
    h *= 1274126177U;
    h ^= h >> 16;
    return h;
}

/* Deterministic unit value in [0,1) for one cell. Pure; decorative only. */
static double living_field_unit(uint32_t x, uint32_t y, uint32_t salt) {
    return (double)(living_field_mix(x, y, salt) >> 8) / 16777216.0;
}

/* Field density at one cell — a pure function of position and explicit phase.
   Used both for the occupancy test and for neighbour lookups that decide
   whether a lit cell sits on the field's edge. */
static double living_field_density(int x, int y, const double *wave_x,
                                   const double *wave_y, const double *fold,
                                   bool radial, bool vertical, int centre_x,
                                   int centre_y) {
    int cx = x;
    int cy = y;
    if (radial) {
        cx = x - centre_x; if (cx < 0) cx = -cx;
        cy = y - centre_y; if (cy < 0) cy = -cy;
    } else if (vertical) {
        cx = y;
        cy = x;
    }
    if (cx < 0) cx = 0;
    if (cy < 0) cy = 0;
    if (cx >= UI_LIVING_FIELD_TABLE) cx = UI_LIVING_FIELD_TABLE - 1;
    if (cy >= UI_LIVING_FIELD_TABLE) cy = UI_LIVING_FIELD_TABLE - 1;
    return wave_x[cx] * wave_y[cy] *
           fold[(uint32_t)(cx + cy) & (UI_LIVING_FIELD_TABLE - 1U)];
}

/* A cell is "field-present" when the weave is dense there *and* the cell is
   blank, since the field only ever draws blank cells. A field edge is a
   field-present cell that touches something which is not — a gap, the region
   border, or a drawn glyph — so the weave fringes *against* text and controls
   rather than carving a margin around them. */
static bool living_field_visible(int x, int y, const double *wave_x,
                                 const double *wave_y, const double *fold,
                                 bool radial, bool vertical, int centre_x,
                                 int centre_y, const uint8_t *fillable,
                                 int region_w, int first_x, int first_y) {
    if (living_field_density(x, y, wave_x, wave_y, fold, radial, vertical,
                             centre_x, centre_y) <= UI_LIVING_FIELD_LIT_LEVEL)
        return false;
    if (fillable &&
        fillable[(y - first_y + 1) * region_w + (x - first_x + 1)] == 0U)
        return false;
    return true;
}

/* Chromatic-aberration fringe: the edge side maps to the shared RGB fringe
   colour (red left, blue right, green horizontal), so the field and the menu
   elements fringe by the same rule. */
static UiChromaFringe living_field_fringe_edge(int side) {
    if (side == 0) return UI_CHROMA_FRINGE_LEFT;
    if (side == 1) return UI_CHROMA_FRINGE_RIGHT;
    return UI_CHROMA_FRINGE_HORIZONTAL;
}

/*
 * living_field — a white-dominant flowing fabric fringed by chromatic
 * aberration.
 *
 * Reference of record: UI_LOOK_AND_FEEL_REFERENCE_OF_RECORD.md §1.1 ("fluid
 * motion expressed through discrete cells", "coordinated patterns rather than
 * independent flicker"), §1.3; UI_DISPLAY_SURFACE_TARGET_2026-10-07.md §3.1,
 * §5. The reference is a neutral weave whose rectangular forms travel and fold
 * as one material; saturated colour appears only as a thin fringing where the
 * light meets a gap — a chromatic aberration of the weave's own edges — so the
 * field is a substrate for the display, never a colour fill.
 *
 * Occupancy is `wave_x * wave_y * fold`, so lit cells form coherent rectangular
 * patches that migrate and breathe together (§1.1) with black gaps between
 * them; the structure is neutral-dominant. Colour rides only the *edges* of
 * those forms and is **only the three additive primaries** — red on left edges,
 * blue on right edges, green on the horizontal edges — a directional chromatic
 * aberration, never a secondary colour and never a spatial hue ramp. The field
 * is a *substrate*: it paints only blank cells, so it fills the surface behind
 * text and controls and its fringes run up against the glyphs, keeping the
 * content part of one continuous material instead of an element on a black
 * plate. Motion is a pure function of explicit time
 * (phase = fmod(elapsed, AMBIENT)/AMBIENT), loops seamlessly, and draws only
 * approved decorative tokens plus a neutral structural tone. Reduced motion
 * never reaches here.
 */
static bool render_living_field(Grid *grid, const UiElementLayout *bounds,
                                double elapsed_ms, const char *orientation,
                                SDL_Color background) {
    static const uint8_t ramp[] = {'.', ':', '-', '=', '+', '*', '#'};
    static const double pi = 3.14159265358979323846;
    const UiThemeTokens *tokens = ui_theme_provisional_tokens();
    UiThemeColor neutral;
    double wave_x[UI_LIVING_FIELD_TABLE];
    double wave_y[UI_LIVING_FIELD_TABLE];
    double fold[UI_LIVING_FIELD_TABLE];
    uint8_t fillable[UI_LIVING_FIELD_REGION_MAX];
    const uint8_t *fillable_mask;
    unsigned int period;
    double phase;
    int centre_x;
    int centre_y;
    bool radial;
    bool vertical;
    bool content_aware;
    int first_x;
    int first_y;
    int last_x;
    int last_y;
    int region_w;
    int region_h;
    if (!grid || !grid->cells || !bounds || !tokens ||
        !isfinite(elapsed_ms) || elapsed_ms < 0.0)
        return false;
    period = ui_theme_motion_duration_ms(UI_THEME_MOTION_AMBIENT, false);
    if (period == 0U) return false;
    neutral = tokens->palette.text_secondary;
    phase = fmod(elapsed_ms, (double)period) / (double)period * 2.0 * pi;
    /* Build the travelling waves once per axis coordinate, so the per-cell cost
       is a few multiplies, never per-cell trigonometry. The fold table is
       indexed by cell sums modulo TABLE; that stays continuous because the
       period divides TABLE. */
    for (int coord = 0; coord < UI_LIVING_FIELD_TABLE; coord++) {
        double value = (double)coord;
        wave_x[coord] = 0.55 + 0.45 * sin(value * (2.0 * pi / UI_LIVING_FIELD_PERIOD_X) + phase);
        wave_y[coord] = 0.55 + 0.45 * sin(value * (2.0 * pi / UI_LIVING_FIELD_PERIOD_Y) + phase);
        fold[coord] = 0.80 + 0.30 * sin(value * (2.0 * pi / UI_LIVING_FIELD_PERIOD_D) - phase);
    }
    radial = orientation && strcmp(orientation, "radial") == 0;
    vertical = orientation && strcmp(orientation, "vertical") == 0;
    centre_x = bounds->x + bounds->width / 2;
    centre_y = bounds->y + bounds->height / 2;
    first_x = bounds->x < 0 ? 0 : bounds->x;
    first_y = bounds->y < 0 ? 0 : bounds->y;
    last_x = bounds->x + bounds->width;
    if (last_x > grid->width) last_x = grid->width;
    last_y = bounds->y + bounds->height;
    if (last_y > grid->height) last_y = grid->height;
    if (last_x <= first_x || last_y <= first_y) return true;
    /* Capture a one-cell-padded mask of *blank* cells. The field draws only
       blank cells and fringes where a dense patch meets a drawn glyph, so the
       weave integrates text and controls instead of carving a black margin
       around them. */
    region_w = last_x - first_x + 2;
    region_h = last_y - first_y + 2;
    content_aware = region_w * region_h <= UI_LIVING_FIELD_REGION_MAX;
    for (int j = 0; content_aware && j < region_h; j++) {
        for (int i = 0; i < region_w; i++) {
            Cell cell;
            bool blank = !grid_get(grid, first_x - 1 + i, first_y - 1 + j,
                                   &cell) || cell.glyph == 0U ||
                         cell.glyph == ' ';
            fillable[j * region_w + i] = (uint8_t)(blank ? 1U : 0U);
        }
    }
    fillable_mask = content_aware ? fillable : NULL;
    for (int y = first_y; y < last_y; y++) {
        for (int x = first_x; x < last_x; x++) {
            double field;
            int band;
            Cell occupied;
            SDL_Color fg;
            bool open_l;
            bool open_r;
            bool open_u;
            bool open_d;
            /* Product of two travelling axis waves: lit cells bunch into
               rectangular patches that migrate and breathe as one material. */
            field = living_field_density(x, y, wave_x, wave_y, fold, radial,
                                         vertical, centre_x, centre_y);
            if (field <= UI_LIVING_FIELD_LIT_LEVEL) continue;
            /* The field is a substrate: it paints only blank cells, so it can
               never obscure authored text, a control, or a focus marker — it
               fills the surface *behind* them. */
            if (grid_get(grid, x, y, &occupied) &&
                occupied.glyph != 0U && occupied.glyph != ' ') continue;
            band = (int)((field - UI_LIVING_FIELD_LIT_LEVEL) /
                         (UI_LIVING_FIELD_FIELD_MAX - UI_LIVING_FIELD_LIT_LEVEL) *
                         6.0);
            if (band < 0) band = 0;
            if (band > 6) band = 6;
            /* A cell sits on the field's edge when a neighbour is not
               field-present — a gap, the border, or a drawn glyph. The interior
               stays neutral; only edges take a fringe, so the weave fringes
               against the text it flows around. */
            open_l = x - 1 < first_x || !living_field_visible(
                x - 1, y, wave_x, wave_y, fold, radial, vertical, centre_x,
                centre_y, fillable_mask, region_w, first_x, first_y);
            open_r = x + 1 >= last_x || !living_field_visible(
                x + 1, y, wave_x, wave_y, fold, radial, vertical, centre_x,
                centre_y, fillable_mask, region_w, first_x, first_y);
            open_u = y - 1 < first_y || !living_field_visible(
                x, y - 1, wave_x, wave_y, fold, radial, vertical, centre_x,
                centre_y, fillable_mask, region_w, first_x, first_y);
            open_d = y + 1 >= last_y || !living_field_visible(
                x, y + 1, wave_x, wave_y, fold, radial, vertical, centre_x,
                centre_y, fillable_mask, region_w, first_x, first_y);
            fg = color(neutral);
            if (open_l || open_r || open_u || open_d) {
                int side;
                int along;
                int run;
                if (open_l) { side = 0; along = y; }
                else if (open_r) { side = 1; along = y; }
                else if (open_u) { side = 2; along = x; }
                else { side = 3; along = x; }
                run = along / UI_LIVING_FIELD_RUN;
                if (living_field_unit((uint32_t)run, (uint32_t)side, 0x5f3aU) <
                    UI_LIVING_FIELD_FRINGE)
                    fg = color(ui_theme_chroma_fringe(
                        living_field_fringe_edge(side)));
            }
            if (!grid_set(grid, x, y, ramp[band], fg, background))
                return false;
        }
    }
    return true;
}

static bool render_unit(UiElement *unit, UiLayout *layout, Grid *grid,
                        double elapsed_ms, bool reduced_motion, bool preview_loop,
                        UiAnimationEvent event) {
    const UiThemePalette *palette = &ui_theme_provisional_tokens()->palette;
    const UiEffectSpec *spec = ui_effect_spec_by_name(unit->preset);
    UiElement *target = find_element(layout, unit->target);
    UiElementLayout bounds;
    bool visible;
    double progress;
    int x;
    int y;
    int width;
    int height;
    if (!spec) return false;
    if (!target || !ui_ele_absolute_bounds(target, &x, &y, &width, &height)) return false;
    if (spec->target_type != UI_ELE_ANIMATION && target->type != spec->target_type)
        return false;
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
    if (spec->id == UI_EFFECT_COMMAND_FLASH && !target->focused) return true;
    if (!animation_progress(unit, elapsed_ms, preview_loop, &visible, &progress))
        return false;
    if (!visible) return true;
    switch (spec->id) {
        case UI_EFFECT_LIVING_FIELD:
            return render_living_field(grid, &bounds, elapsed_ms,
                                       unit->orientation, color(palette->canvas));
        case UI_EFFECT_PAUSE_GLITCH:
            return render_pause_glitch(grid, set_grid, &bounds, unit->name,
                                       unit->orientation, unit->randomize != 0,
                                       progress, false);
        case UI_EFFECT_CENTER_OUT:
            return render_center_out(grid, &bounds, progress,
                                     color(palette->focus), color(palette->canvas));
        case UI_EFFECT_PERIMETER_BURST: {
            int offset = 1 + (int)lround(progress * 3.0);
            (void)grid_set(grid, bounds.x - offset, bounds.y, '*',
                           color(palette->accent), color(palette->canvas));
            (void)grid_set(grid, bounds.x + bounds.width - 1 + offset, bounds.y, '*',
                           color(palette->accent), color(palette->canvas));
            return true;
        }
        case UI_EFFECT_LOCAL_GLITCH: {
            int offset = (int)((uint64_t)(elapsed_ms / 80.0) % 3U) - 1;
            (void)grid_set(grid, bounds.x + bounds.width / 2 + offset, bounds.y - 1,
                           ':', color(palette->accent), color(palette->canvas));
            return true;
        }
        case UI_EFFECT_EDGE_TRACE:
            return render_edge_trace(grid, &bounds, progress,
                                     color(palette->accent), color(palette->focus),
                                     color(palette->canvas));
        case UI_EFFECT_CHROMATIC_REGISTER:
            return render_chromatic_register(grid, &bounds, progress,
                                             color(palette->accent), color(palette->focus),
                                             color(palette->canvas));
        case UI_EFFECT_COMMAND_FLASH:
            return render_command_flash(grid, &bounds, progress,
                                        color(palette->accent), color(palette->focus),
                                        color(palette->canvas));
        case UI_EFFECT_BUTTON_REASSEMBLE:
            return render_button_reassemble(grid, &bounds, progress,
                                            color(palette->accent), color(palette->focus),
                                            color(palette->canvas));
        case UI_EFFECT_PANEL_REGISTER:
            return render_panel_register(grid, &bounds, progress,
                                         color(palette->accent), color(palette->focus),
                                         color(palette->canvas));
        default:
            return false;
    }
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
            const char *transition;
            for (int p = 0; p < processed_count; p++)
                if (processed[p] == cursor) known = true;
            transition = layout->transition[0] && strcmp(layout->transition, "none") != 0
                ? layout->transition : cursor->transition;
            if (!known && (event == UI_ANIMATION_EVENT_CONTEXT_ENTER ||
                           event == UI_ANIMATION_EVENT_CONTEXT_EXIT ||
                           event == UI_ANIMATION_EVENT_PREVIEW) &&
                cursor->type != UI_ELE_ANIMATION && cursor->visible &&
                ((layout->transition[0] && strcmp(layout->transition, "none") != 0) ||
                 strcmp(cursor->transition, "none") != 0) &&
                (strcmp(transition, "button_reassemble") != 0 ||
                 cursor->type == UI_ELE_BUTTON) &&
                (strcmp(transition, "panel_register") != 0 ||
                 cursor->type == UI_ELE_CONTAINER)) {
            UiElement unit = {0};
            unit.type = UI_ELE_ANIMATION;
            unit.visible = 1;
            unit.layout = cursor->layout;
            unit.layout.x = 0;
            unit.layout.y = 0;
            (void)snprintf(unit.name, sizeof(unit.name), "%s", cursor->name);
            (void)snprintf(unit.preset, sizeof(unit.preset), "%s", transition);
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
