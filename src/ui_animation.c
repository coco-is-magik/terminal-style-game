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
 * living_field constants — a solid fabric carried by one travelling wave.
 *
 * The §1.1 reference (docs/inspiration_and_notes/light_and_motion) is light cells
 * that move as one material the way a flag or a water surface does; saturated
 * colour appears only as a thin *fringing* where the light meets empty space, a
 * chromatic aberration rather than a fill, so the sheet is neutral-dominant and
 * only fringe cells are coloured.
 *
 * The density is one diagonal wave plus a finer ripple at a different wavelength
 * — `WAVE_*` and `RIPPLE_*`, both advancing an integer number of wavelengths per
 * AMBIENT loop, which is what makes the loop seamless on any surface size —
 * curved through `BAND_GAMMA` so the sheet stays dim between crests, spread by a
 * static `GRAIN_BANDS` weave so it never reads as bands of one glyph, and folded
 * by the anti-diagonal `FOLD_*`. It is never a flat stripe, a spatial hue ramp, a
 * ring, a grid, or per-cell flicker: all selection is a pure function of position
 * and explicit time.
 */
#define UI_LIVING_FIELD_BASE 0.30        /* fabric density between crests      */
#define UI_LIVING_FIELD_WAVE_CELLS 48    /* main wave wavelength along travel  */
#define UI_LIVING_FIELD_WAVE_CYCLES 3    /* main wave crests crossing per loop */
#define UI_LIVING_FIELD_WAVE_GAIN 0.20   /* main wave lift                     */
#define UI_LIVING_FIELD_RIPPLE_CELLS 17  /* fine ripple wavelength             */
#define UI_LIVING_FIELD_RIPPLE_CYCLES 1  /* fine ripple crests crossing per loop */
#define UI_LIVING_FIELD_RIPPLE_GAIN 0.07 /* fine ripple lift                   */
#define UI_LIVING_FIELD_FOLD_CELLS 43    /* anti-diagonal fold wavelength      */
#define UI_LIVING_FIELD_FOLD_CYCLES 2    /* fold crests crossing per loop      */
#define UI_LIVING_FIELD_FOLD_BANDS 1.0   /* how much the fold lifts the fabric */
#define UI_LIVING_FIELD_BAND_GAMMA 3.0   /* keeps the fabric dim below its crest */
#define UI_LIVING_FIELD_GRAIN_BANDS 0.7  /* per-cell glyph spread, in bands    */
#define UI_LIVING_FIELD_MAX 0.60         /* highest density the fabric reaches */
#define UI_LIVING_FIELD_RUN 5            /* cells per chromatic fringe run     */
#define UI_LIVING_FIELD_FRINGE 0.55      /* fraction of edge runs coloured     */
#define UI_LIVING_FIELD_WAKE 5           /* chromatic wake trailing a crest    */
#define UI_LIVING_FIELD_WAKE_SPECKLE 0.18 /* fraction of wake cells coloured   */
#define UI_LIVING_FIELD_REGION_MAX 16384 /* content-mask cap, in cells         */

/*
 * tide_cover / tide_reveal — the fabric arriving over the surface and leaving it
 * again. The flood travels along the same diagonal as the backdrop wave (from the
 * bottom-right corner up to the top-left one) so a state change reads as the same
 * material moving, not as an unrelated effect (§1.1, §1.2). `UI_TIDE_TRAIL` is how
 * far the flood over-travels past the far corner, so the surface is completely
 * covered at the end of an exit; `UI_TIDE_FRONT` is the prismatic edge that trails
 * the flood's front, the aberration the wave carries with it.
 *
 * The material is driven by the *cover* rather than by a clock, which is what makes
 * the two halves one motion (owner direction 2026-10-09: the characters "surge in
 * and cover the menu like a receding tide, then flow back out" — a first attempt
 * driven by the ambient clock read as a straight edge sweeping a still sheet, which
 * is not a tide at all):
 *   - the wave journeys `UI_TIDE_TRAVEL` of its period across a transition, so every
 *     frame carries moving material and the pattern never returns to where it began
 *     (a whole number of wavelengths is a still picture at both ends);
 *   - an exit ends and the reveal that follows begins on the same cover, so they
 *     share one pattern on the frame the surface is swapped and nothing jumps;
 *   - the reveal therefore retraces the exit exactly: the characters leave the way
 *     they arrived, back to the places they came from;
 *   - the flood is the *fabric*, not a stripe: its band comes from the wave and from
 *     the same per-cell weave and anti-diagonal fold the backdrop uses, and the fold
 *     bends its edge — so it arrives as a wavering sheet instead of a straight wipe.
 */
#define UI_TIDE_TRAIL 28   /* cells of over-travel past the far corner             */
#define UI_TIDE_FRONT 6    /* cells of chromatic front edge                        */
#define UI_TIDE_TRAVEL 0.5 /* wave periods the material journeys per transition    */
#define UI_TIDE_BASE 2.0   /* where the arriving material sits in the ramp          */
#define UI_TIDE_SPAN 3.0   /* ramp bands the flood spans                           */
#define UI_TIDE_BEND 14.0  /* cells the flood's edge swells along the anti-diagonal */
#define UI_TIDE_RAGGED 3.0 /* cells of per-cell raggedness on the same edge        */

/*
 * settle — the transition a surface takes when it hands over to the *world* frame
 * instead of to another surface: the characters that were on screen flow out to
 * lattice positions, cycle through the fabric's glyphs as the wave crosses them,
 * and then the lattice thins away, resolving onto the live frame underneath
 * (2026-10-09 direction record, item 4, Stage C). It rides the same diagonal as
 * the backdrop and carries the same prismatic wake, so a menu leaving for the
 * world reads as the same material changing state.
 *
 * The wave advances over the *transition window* rather than over the AMBIENT
 * loop: a settle is a transient, not a surface at rest, so its material crosses
 * the screen once while the window runs (§1.2). `UI_SETTLE_HOLD` is the window
 * fraction after which the material stops advancing — the last quarter is the
 * settle itself, where the characters are still before the frame is uncovered.
 */
#define UI_SETTLE_LATTICE 4       /* cells between lattice positions               */
#define UI_SETTLE_CYCLE 2.0       /* band at which the wave takes a character over */
#define UI_SETTLE_TAKEOVER 0.3    /* condensation by which the wave may take over  */
#define UI_SETTLE_HOLD 0.75       /* window fraction after which the material holds */
#define UI_SETTLE_THIN_SALT 0x2b7fU    /* stable order the lattice thins in        */
#define UI_SETTLE_SPECKLE_SALT 0x6c13U /* chromatic speckle in the wake            */

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

/* The distance a cell sits from the surface's bottom-right corner, in cells, for
   each authored direction, so the main wave travels from that corner up to the
   top-left one. */
static double living_field_distance(int x, int y, bool radial, bool vertical,
                                    int last_x, int last_y) {
    double dx = (double)(last_x - 1 - x);
    double dy = (double)(last_y - 1 - y);
    if (dx < 0.0) dx = 0.0;
    if (dy < 0.0) dy = 0.0;
    if (vertical) return dy;
    if (radial) return dx > dy ? dx : dy;
    return dx + dy;
}

/* The travelling wave, sampled once per distance coordinate so the per-cell cost
   is a table lookup rather than trigonometry. A main wave and a finer ripple ride
   the same diagonal at different wavelengths, and their beat is what makes the
   sheet lift and settle the way a flag does. Both advance an *integer* number of
   wavelengths per loop, so the material loops seamlessly on any surface without
   the geometry being known to the wave. */
static double living_field_wave(double distance, double phase) {
    static const double two_pi = 6.28318530717958647692;
    double crest = 0.5 + 0.5 * cos(two_pi *
        (distance / (double)UI_LIVING_FIELD_WAVE_CELLS -
         (double)UI_LIVING_FIELD_WAVE_CYCLES * phase));
    double ripple = 0.5 + 0.5 * cos(two_pi *
        (distance / (double)UI_LIVING_FIELD_RIPPLE_CELLS -
         (double)UI_LIVING_FIELD_RIPPLE_CYCLES * phase));
    return UI_LIVING_FIELD_BASE + UI_LIVING_FIELD_WAVE_GAIN * crest +
           UI_LIVING_FIELD_RIPPLE_GAIN * ripple;
}

/* The anti-diagonal fold: a slow breathing across the direction of travel, so the
   sheet lifts and settles the way a flag does. */
static double living_field_fold(double across, double phase) {
    static const double two_pi = 6.28318530717958647692;
    return 0.5 + 0.5 * cos(two_pi * (across / (double)UI_LIVING_FIELD_FOLD_CELLS +
                                     (double)UI_LIVING_FIELD_FOLD_CYCLES * phase));
}

/* The glyph band a density maps to. The curve is deliberately bottom-heavy: the
   fabric stays dim and quiet between crests, so the wave reads as light moving
   through the material rather than as an evenly lit field. It is sampled once per
   distance coordinate with the wave. */
static double living_field_band(double wave) {
    double ratio = wave / UI_LIVING_FIELD_MAX;
    if (ratio < 0.0) ratio = 0.0;
    if (ratio > 1.0) ratio = 1.0;
    return pow(ratio, UI_LIVING_FIELD_BAND_GAMMA) * 6.0;
}

/* The weave: a static per-cell spread of the glyph band, so the sheet reads as
   woven light cells with visible gaps rather than as bands of one glyph (§1.1).
   It is a pure function of position and carries no block structure, so it never
   draws a grid, and it never flickers. */
static double living_field_weave(int x, int y) {
    double jitter = living_field_unit((uint32_t)x, (uint32_t)y, 0x5f3aU);
    return (jitter - 0.5) * UI_LIVING_FIELD_GRAIN_BANDS;
}

/* True when a cell can carry fabric: inside the region and blank. The fabric is a
   substrate, so it fills only cells that nothing else has drawn into — which is
   what makes its fringes run up against text and controls. */
static bool living_field_blank(const uint8_t *fillable, int region_w,
                               int first_x, int first_y, int x, int y) {
    if (!fillable) return true;
    return fillable[(y - first_y + 1) * region_w + (x - first_x + 1)] != 0U;
}

/* Chromatic-aberration fringe: the edge side maps to the shared RGB fringe
   colour (red left, blue right, green horizontal), so the field and the menu
   elements fringe by the same rule. */
static UiChromaFringe living_field_fringe_edge(int side) {
    if (side == 0) return UI_CHROMA_FRINGE_LEFT;
    if (side == 1) return UI_CHROMA_FRINGE_RIGHT;
    return UI_CHROMA_FRINGE_HORIZONTAL;
}

/* How far a cell sits behind the nearest crest of the main wave, in cells, so the
   chromatic wake can trail every crest rather than only one. */
static double living_field_behind(double distance, double phase) {
    double travel = (double)UI_LIVING_FIELD_WAVE_CELLS *
                    (double)UI_LIVING_FIELD_WAVE_CYCLES * phase;
    double behind = fmod(distance - travel, (double)UI_LIVING_FIELD_WAVE_CELLS);
    if (behind < 0.0) behind += (double)UI_LIVING_FIELD_WAVE_CELLS;
    return (double)UI_LIVING_FIELD_WAVE_CELLS - behind;
}

/* The chromatic wake that trails the swell: one additive primary per third of
   the trail, so the aberration reads as a prismatic smear following the wave —
   and it is a trail, never a fill, because only a fraction of its runs are
   coloured. `width` is the trail's length in cells, so the field's wake and the
   tide's front edge are coloured by the same rule. */
static UiChromaFringe living_field_wake_edge(double behind, double width) {
    double third = width / 3.0;
    if (behind < third) return UI_CHROMA_FRINGE_LEFT;
    if (behind < 2.0 * third) return UI_CHROMA_FRINGE_HORIZONTAL;
    return UI_CHROMA_FRINGE_RIGHT;
}

/*
 * living_field — a solid fabric with one travelling swell, fringed by chromatic
 * aberration.
 *
 * Reference of record: UI_LOOK_AND_FEEL_REFERENCE_OF_RECORD.md §1.1 ("fluid
 * motion expressed through discrete cells", "coordinated patterns rather than
 * independent flicker"), §1.3; UI_DISPLAY_SURFACE_TARGET_2026-10-07.md §3.1,
 * §5. The reference is a sheet of light cells that moves as one material, the
 * way a flag or a water surface does; saturated colour appears only as a thin
 * chromatic-aberration trail, so the field stays a substrate for the display and
 * never a colour fill.
 *
 * The fabric is **solid**: every blank cell in the region carries a glyph from the
 * density ramp, and its structure comes from that ramp plus a static per-cell
 * grain, never from gaps between shapes. Its motion is one **swell** crossing the
 * surface from the bottom-right corner to the top-left once per loop — the
 * centrepiece — with a shorter **ripple** riding it and an anti-diagonal **fold**
 * breathing across it, so the whole sheet lifts and settles the way a flag does.
 * The motion is a pure function of explicit time
 * (phase = fmod(elapsed, AMBIENT)/AMBIENT), so the loop is seamless.
 *
 * Colour is **only the three additive primaries**. The swell's **wake** carries
 * one primary per third of the trail — the aberration follows the wave — and a
 * fabric cell whose edge meets a drawn glyph or the region's border fringes by
 * direction (red left, blue right, green horizontal), so the material also
 * aberrates where it hits a control. Never a secondary colour, never a spatial
 * hue ramp, never a fill. The field paints only blank cells, so it fills *behind*
 * text and controls and its fringes run up against them. Reduced motion never
 * reaches here.
 */
static bool render_living_field(Grid *grid, const UiElementLayout *bounds,
                                double elapsed_ms, const char *orientation,
                                SDL_Color background) {
    static const uint8_t ramp[] = {'.', ':', '-', '=', '+', '*', '#'};
    const UiThemeTokens *tokens = ui_theme_provisional_tokens();
    UiThemeColor neutral;
    double wave[UI_LIVING_FIELD_TABLE];
    double band_ref[UI_LIVING_FIELD_TABLE];
    double fold[UI_LIVING_FIELD_TABLE];
    uint8_t fillable[UI_LIVING_FIELD_REGION_MAX];
    const uint8_t *fillable_mask;
    unsigned int period;
    double phase;
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
    phase = fmod(elapsed_ms, (double)period) / (double)period;
    radial = orientation && strcmp(orientation, "radial") == 0;
    vertical = orientation && strcmp(orientation, "vertical") == 0;
    first_x = bounds->x < 0 ? 0 : bounds->x;
    first_y = bounds->y < 0 ? 0 : bounds->y;
    last_x = bounds->x + bounds->width;
    if (last_x > grid->width) last_x = grid->width;
    last_y = bounds->y + bounds->height;
    if (last_y > grid->height) last_y = grid->height;
    if (last_x <= first_x || last_y <= first_y) return true;
    (void)living_field_distance(first_x, first_y, radial, vertical, last_x,
                                last_y);
    /* Sample the wave and the fold once per coordinate, so the per-cell cost is a
       lookup and a hash, never per-cell trigonometry. */
    for (int coord = 0; coord < UI_LIVING_FIELD_TABLE; coord++) {
        wave[coord] = living_field_wave((double)coord, phase);
        band_ref[coord] = living_field_band(wave[coord]);
        fold[coord] = living_field_fold((double)coord, phase);
    }
    /* Capture a one-cell-padded mask of *blank* cells. The fabric draws only
       blank cells and fringes where it meets a drawn glyph, so it integrates
       text and controls instead of carving a black margin around them. */
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
            double distance;
            double weave;
            double behind;
            int wave_index;
            int fold_index;
            int band;
            Cell occupied;
            SDL_Color fg;
            bool open_l;
            bool open_r;
            bool open_u;
            bool open_d;
            /* The fabric is a substrate: it paints only blank cells, so it can
               never obscure authored text, a control, or a focus marker — it
               fills the surface *behind* them. */
            if (grid_get(grid, x, y, &occupied) &&
                occupied.glyph != 0U && occupied.glyph != ' ') continue;
            distance = living_field_distance(x, y, radial, vertical, last_x,
                                             last_y);
            wave_index = (int)distance;
            if (wave_index < 0) wave_index = 0;
            if (wave_index >= UI_LIVING_FIELD_TABLE)
                wave_index = UI_LIVING_FIELD_TABLE - 1;
            fold_index = (x - first_x) - (y - first_y) + (last_y - first_y - 1);
            if (fold_index < 0) fold_index = 0;
            if (fold_index >= UI_LIVING_FIELD_TABLE)
                fold_index = UI_LIVING_FIELD_TABLE - 1;
            weave = living_field_weave(x, y);
            band = (int)lround(band_ref[wave_index] +
                               (fold[fold_index] - 0.5) *
                                   UI_LIVING_FIELD_FOLD_BANDS +
                               weave);
            if (band < 0) band = 0;
            if (band > 6) band = 6;
            /* A fabric cell sits on an edge when a neighbour is not fabric —
               a drawn glyph or the region's border — and those edges fringe by
               direction, so the material aberrates where it hits a control. */
            open_l = x - 1 < first_x || !living_field_blank(
                fillable_mask, region_w, first_x, first_y, x - 1, y);
            open_r = x + 1 >= last_x || !living_field_blank(
                fillable_mask, region_w, first_x, first_y, x + 1, y);
            open_u = y - 1 < first_y || !living_field_blank(
                fillable_mask, region_w, first_x, first_y, x, y - 1);
            open_d = y + 1 >= last_y || !living_field_blank(
                fillable_mask, region_w, first_x, first_y, x, y + 1);
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
            } else {
                /* The chromatic wake trails the crest as a prismatic *speckle*:
                   only some cells are coloured, and which cells is a static
                   function of position, so each cell shows its primary for the
                   moment the wake passes over it and then returns to the neutral
                   fabric. The aberration follows the wave; it never tints the
                   sheet. */
                behind = living_field_behind(distance, phase);
                if (behind < (double)UI_LIVING_FIELD_WAKE &&
                    living_field_unit((uint32_t)x, (uint32_t)y, 0x7d21U) <
                        UI_LIVING_FIELD_WAKE_SPECKLE)
                    fg = color(ui_theme_chroma_fringe(
                        living_field_wake_edge(behind,
                                               (double)UI_LIVING_FIELD_WAKE)));
            }
            if (!grid_set(grid, x, y, ramp[band], fg, background))
                return false;
        }
    }
    return true;
}

/*
 * The tide: the fabric surges in from the surface's bottom-right corner and
 * covers everything, then drains back out revealing the next context. `progress`
 * runs 1 -> 0 for a context exit and 0 -> 1 for a context enter, so `1 - progress`
 * is the covered fraction in both directions — the exit ends where the enter
 * begins and the material is continuous across the change of state (§1.2, change
 * of direction recorded 2026-10-09).
 *
 * This is the one primitive that may pass over authored content: the element's
 * spec declares `covers_content`, and the evaluator only honours the cover while
 * this function reports one. At `covered == 0` it draws nothing and reports no
 * cover, so the authored cells are restored and the controls are intact again —
 * the exception lasts exactly as long as the transition does.
 */
static bool render_tide(Grid *grid, const UiElementLayout *bounds, double progress,
                        bool covering, SDL_Color background, bool *out_covers) {
    static const uint8_t ramp[] = {'.', ':', '-', '=', '+', '*', '#'};
    const UiThemeTokens *tokens = ui_theme_provisional_tokens();
    double wave[UI_LIVING_FIELD_TABLE];
    double band_ref[UI_LIVING_FIELD_TABLE];
    double fold[UI_LIVING_FIELD_TABLE];
    double phase;
    double eased;
    double linear;
    double smooth;
    double covered;
    double span;
    double front;
    SDL_Color neutral;
    int first_x;
    int first_y;
    int last_x;
    int last_y;
    if (!grid || !grid->cells || !bounds || !tokens || !isfinite(progress) ||
        progress < 0.0 || progress > 1.0)
        return false;
    if (out_covers) *out_covers = false;
    /* The shared motion curve is a fast-start pop: right for a control that
       acknowledges a press, wrong for water, because it would put most of the
       surface under the flood in the first half of the window. Recover the linear
       fraction of the window from the shared ease and lay the tide's own curve
       over it — leaving the corner slowly, crossing at a steady pace, pressing
       home gently (2026-10-09 direction record: the characters surge in and cover
       the menu like a receding tide). */
    eased = 1.0 - progress;
    linear = covering ? 1.0 - cbrt(1.0 - eased) : 1.0 - cbrt(eased);
    smooth = linear * linear * (3.0 - 2.0 * linear);
    covered = covering ? smooth : 1.0 - smooth;
    if (covered <= 0.0) return true;
    /* One journey of the wave across the transition — see the constants above. The
       cover is shared by both directions at their handover, so the material is
       continuous across it and the reveal unwinds exactly what the cover wound. */
    phase = covered * UI_TIDE_TRAVEL;
    neutral = color(tokens->palette.text_secondary);
    first_x = bounds->x < 0 ? 0 : bounds->x;
    first_y = bounds->y < 0 ? 0 : bounds->y;
    last_x = bounds->x + bounds->width;
    if (last_x > grid->width) last_x = grid->width;
    last_y = bounds->y + bounds->height;
    if (last_y > grid->height) last_y = grid->height;
    if (last_x <= first_x || last_y <= first_y) return true;
    span = (double)((last_x - 1 - first_x) + (last_y - 1 - first_y) +
                    UI_TIDE_TRAIL);
    front = covered * span - (double)UI_TIDE_TRAIL;
    if (front < 0.0) return true;
    /* Sample the wave, its band and the fold once per coordinate, as the backdrop
       does, so the per-cell cost is a lookup and a hash. */
    for (int coord = 0; coord < UI_LIVING_FIELD_TABLE; coord++) {
        wave[coord] = living_field_wave((double)coord, phase);
        band_ref[coord] = living_field_band(wave[coord]);
        fold[coord] = living_field_fold((double)coord, phase);
    }
    for (int y = first_y; y < last_y; y++) {
        for (int x = first_x; x < last_x; x++) {
            double distance = (double)((last_x - 1 - x) + (last_y - 1 - y));
            int across = (x - first_x) - (y - first_y);
            int wave_index = (int)distance;
            int fold_index = across + (last_y - first_y - 1);
            double bend;
            double depth;
            double band;
            int index;
            SDL_Color fg;
            if (wave_index < 0) wave_index = 0;
            if (wave_index >= UI_LIVING_FIELD_TABLE)
                wave_index = UI_LIVING_FIELD_TABLE - 1;
            if (fold_index < 0) fold_index = 0;
            if (fold_index >= UI_LIVING_FIELD_TABLE)
                fold_index = UI_LIVING_FIELD_TABLE - 1;
            /* The edge rides the anti-diagonal fold, so the material arrives as a
               broad swell, with a cell-scale raggedness on top of it. It is the same
               fold the backdrop breathes with, at the transition's own phase, so the
               swell travels as it advances: water crossing a surface, where the first
               attempt was a straight line crossing a screen. */
            bend = (fold[fold_index] - 0.5) * UI_TIDE_BEND +
                   (living_field_unit((uint32_t)x, (uint32_t)y, 0x3c11U) - 0.5) *
                       UI_TIDE_RAGGED;
            if (distance + bend > front) continue;
            depth = front - distance;
            /* The same fabric as the backdrop, mapped into the top of the ramp: the
               arriving material is denser than the calm sheet and still woven, where a
               constant lift would clamp every cell to one glyph and read as a flat
               stripe. */
            band = UI_TIDE_BASE + band_ref[wave_index] / 6.0 * UI_TIDE_SPAN +
                   living_field_weave(x, y);
            if (band < 0.0) band = 0.0;
            if (band > 6.0) band = 6.0;
            index = (int)lround(band);
            fg = neutral;
            if (depth < (double)UI_TIDE_FRONT)
                fg = color(ui_theme_chroma_fringe(
                    living_field_wake_edge(depth, (double)UI_TIDE_FRONT)));
            if (!grid_set(grid, x, y, ramp[index], fg, background))
                return false;
        }
    }
    if (out_covers) *out_covers = true;
    return true;
}

/*
 * The settle: a surface handing over to the world frame. `progress` arrives from
 * the shared evaluator as the exit curve (progress runs 1 -> 0, a fast-start pop
 * shaped for a control acknowledging a press), so the linear fraction of the
 * window is recovered from it and the material is driven by that: the settle
 * occupies exactly its window and no literal duration appears here.
 *
 * The material that was on screen flows to its lattice: every cell belongs to a
 * block of `UI_SETTLE_LATTICE` cells and slides toward that block's far corner,
 * which is where a lattice position is. In the first half of the window the
 * characters outside a shrinking diamond leave, so the surface's characters
 * condense onto the lattice; from the halfway point the lattice is all that
 * remains, and it holds and then thins away in a stable order, so the last frame
 * draws nothing and the live frame underneath is simply uncovered. Where the main
 * crest of the backdrop wave crosses a lattice position, the wave takes the
 * character over and it cycles through the fabric's own glyphs, accented by the
 * same prismatic wake the backdrop carries.
 *
 * Like the tide this primitive passes over authored content, and only for its
 * window: it reports a cover for as long as it draws, and at the end of the
 * window it draws nothing and reports none, so the caller drops the overlay with
 * it (2026-10-09 direction record, item 4, Stage C).
 */
static bool render_settle(Grid *grid, const UiElementLayout *bounds, double progress,
                          double elapsed_ms, SDL_Color background, bool *out_covers) {
    static const uint8_t ramp[] = {'.', ':', '-', '=', '+', '*', '#'};
    const UiThemeTokens *tokens = ui_theme_provisional_tokens();
    UiThemeColor neutral;
    double wave_band[UI_LIVING_FIELD_TABLE];
    double window;
    double phase;
    double settle;
    double condense;
    double thin;
    double reach;
    int first_x;
    int first_y;
    int last_x;
    int last_y;
    if (!grid || !grid->cells || !bounds || !tokens || !isfinite(progress) ||
        progress < 0.0 || progress > 1.0 || !isfinite(elapsed_ms) || elapsed_ms < 0.0)
        return false;
    if (out_covers) *out_covers = false;
    window = (double)ui_theme_motion_duration_ms(UI_THEME_MOTION_MAJOR_EXIT, false);
    if (window <= 0.0) return true;
    /* The shared curve is (1 - t)^3 over the window, so its cube root recovers the
       fraction of the window the material has actually travelled. */
    settle = 1.0 - cbrt(progress);
    if (settle >= 1.0) return true;
    if (elapsed_ms > window * UI_SETTLE_HOLD) elapsed_ms = window * UI_SETTLE_HOLD;
    neutral = tokens->palette.text_secondary;
    /* One crossing of the backdrop wave while the window runs, then the material
       holds for the settle. */
    phase = elapsed_ms / window;
    first_x = bounds->x < 0 ? 0 : bounds->x;
    first_y = bounds->y < 0 ? 0 : bounds->y;
    last_x = bounds->x + bounds->width;
    if (last_x > grid->width) last_x = grid->width;
    last_y = bounds->y + bounds->height;
    if (last_y > grid->height) last_y = grid->height;
    if (last_x <= first_x || last_y <= first_y) return true;
    /* Sample the fabric's band once per distance coordinate, as the backdrop does. */
    for (int coord = 0; coord < UI_LIVING_FIELD_TABLE; coord++)
        wave_band[coord] = living_field_band(living_field_wave((double)coord, phase));
    condense = settle * 2.0;
    if (condense > 1.0) condense = 1.0;
    thin = settle * 2.0 - 1.0;
    if (thin < 0.0) thin = 0.0;
    /* A cell keeps its character while it sits inside the diamond that reaches from
       its own lattice position; the diamond closes over the first half of the
       window, so at the halfway point only the lattice positions still carry one. */
    reach = (1.0 - condense) * (double)(2 * (UI_SETTLE_LATTICE - 1));
    for (int y = first_y; y < last_y; y++) {
        for (int x = first_x; x < last_x; x++) {
            int anchor_x = first_x + ((x - first_x) / UI_SETTLE_LATTICE + 1) *
                                          UI_SETTLE_LATTICE - 1;
            int anchor_y = first_y + ((y - first_y) / UI_SETTLE_LATTICE + 1) *
                                          UI_SETTLE_LATTICE - 1;
            double distance;
            double behind;
            int wave_index;
            int band;
            Cell source;
            SDL_Color fg;
            int sample_x;
            int sample_y;
            if (anchor_x > last_x - 1) anchor_x = last_x - 1;
            if (anchor_y > last_y - 1) anchor_y = last_y - 1;
            if ((double)((anchor_x - x) + (anchor_y - y)) > reach ||
                (thin > 0.0 && living_field_unit((uint32_t)anchor_x,
                                                 (uint32_t)anchor_y,
                                                 UI_SETTLE_THIN_SALT) < thin)) {
                /* The character has left this cell: the material has moved on, and
                   what it leaves behind is not covered at all. The empty glyph is
                   the one the compositor's copy skips, so the live frame underneath
                   shows through from this cell while the rest is still material —
                   the sheet thins on screen instead of blanking the surface and
                   being dropped at the end. */
                if (!grid_set(grid, x, y, 0U, color(neutral), background)) return false;
                continue;
            }
            distance = living_field_distance(x, y, false, false, last_x, last_y);
            wave_index = (int)distance;
            if (wave_index < 0) wave_index = 0;
            if (wave_index >= UI_LIVING_FIELD_TABLE)
                wave_index = UI_LIVING_FIELD_TABLE - 1;
            band = (int)lround(wave_band[wave_index]);
            if (band < 0) band = 0;
            if (band > 6) band = 6;
            if ((double)band >= UI_SETTLE_CYCLE && condense >= UI_SETTLE_TAKEOVER) {
                /* Under the crest, once the material has started to move, the wave
                   has taken the character over and the fabric's own glyph is what it
                   cycles to, coloured by the wake while it passes. The takeover
                   waits for the material to be moving so that the window's first
                   frame is the surface exactly as it stood: the change is the
                   motion, never a step at the moment of handover. */
                fg = color(neutral);
                behind = living_field_behind(distance, phase);
                if (behind < (double)UI_LIVING_FIELD_WAKE &&
                    living_field_unit((uint32_t)x, (uint32_t)y,
                                      UI_SETTLE_SPECKLE_SALT) <
                        UI_LIVING_FIELD_WAKE_SPECKLE)
                    fg = color(ui_theme_chroma_fringe(
                        living_field_wake_edge(behind, (double)UI_LIVING_FIELD_WAKE)));
                if (!grid_set(grid, x, y, ramp[band], fg, background)) return false;
                continue;
            }
            /* Away from the crest a character keeps its own face and slides toward
               the lattice position it belongs to. That position is never behind the
               cell in either axis, so the character this reads has not been written
               yet on this pass: the material never feeds on itself. */
            sample_x = x + (int)lround(condense * (double)(anchor_x - x));
            sample_y = y + (int)lround(condense * (double)(anchor_y - y));
            if (!grid_get(grid, sample_x, sample_y, &source)) return false;
            if (!grid_set(grid, x, y, source.glyph, source.fg, source.bg)) return false;
        }
    }
    if (out_covers) *out_covers = true;
    return true;
}

static bool render_unit(UiElement *unit, UiLayout *layout, Grid *grid,
                        double elapsed_ms, bool reduced_motion, bool preview_loop,
                        UiAnimationEvent event, bool *out_covers) {
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
    if (out_covers) *out_covers = false;
    if (!spec) return false;
    /* A transition is not part of a settled surface, so a static preview never
       plays one: the preview shows the state the surface resolves to. */
    if (event == UI_ANIMATION_EVENT_PREVIEW && spec->covers_content) return true;
    if (!target || !ui_ele_absolute_bounds(target, &x, &y, &width, &height)) return false;
    if (spec->target_type != UI_ELE_ANIMATION && target->type != spec->target_type)
        return false;
    bounds = (UiElementLayout){x + unit->layout.x, y + unit->layout.y,
                               UI_COORD_ABSOLUTE,
                               unit->layout.width > 0 ? unit->layout.width : width,
                               unit->layout.height > 0 ? unit->layout.height : height};
    /* A surface-extent unit fills the surface it renders into (the display in
       normal run, the authored surface in the preview), so its authored
       geometry is not used and no asset carries a grid size. */
    if (unit->extent == UI_EXTENT_SURFACE) {
        bounds = (UiElementLayout){0, 0, UI_COORD_ABSOLUTE, grid->width, grid->height};
    }
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
        case UI_EFFECT_TIDE_COVER:
        case UI_EFFECT_TIDE_REVEAL:
            return render_tide(grid, &bounds, progress,
                               spec->id == UI_EFFECT_TIDE_COVER,
                               color(palette->canvas), out_covers);
        case UI_EFFECT_SETTLE:
            return render_settle(grid, &bounds, progress, elapsed_ms,
                                 color(palette->canvas), out_covers);
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
                                UiAnimationEvent event, bool *out_covers) {
    UiElement *processed[UI_LAYOUT_MAX_ELEMS];
    int processed_count = 0;
    bool covers = false;
    int i;
    if (out_covers) *out_covers = false;
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
            {
                bool unit_covers = false;
                if (!render_unit(&unit, layout, grid, elapsed_ms,
                                 reduced_motion, preview_loop,
                                  event == UI_ANIMATION_EVENT_CONTEXT_EXIT
                                      ? UI_ANIMATION_EVENT_CONTEXT_EXIT
                                      : UI_ANIMATION_EVENT_CONTEXT_ENTER,
                                  &unit_covers)) return false;
                if (unit_covers) covers = true;
            }
            }
            if (!known && processed_count < UI_LAYOUT_MAX_ELEMS)
                processed[processed_count++] = cursor;
            cursor = cursor->parent;
        }
        if (element && element->type == UI_ELE_ANIMATION && element->visible) {
            bool unit_covers = false;
            if (!render_unit(element, layout, grid, elapsed_ms,
                             reduced_motion, preview_loop, event, &unit_covers))
                return false;
            if (unit_covers) covers = true;
        }
    }
    if (out_covers) *out_covers = covers;
    return true;
}

bool ui_animation_render_world_settle(UiLayout *outgoing, Grid *grid,
                                      double elapsed_ms, bool reduced_motion) {
    UiElement unit = {0};
    UiElement *anchor;
    bool covers = false;
    if (!outgoing || !grid || !grid->cells || outgoing->element_count <= 0 ||
        !isfinite(elapsed_ms) || elapsed_ms < 0.0)
        return false;
    /* One unit, named by the primitive registry and addressed at the whole
       surface, synthesised exactly the way the evaluator synthesises an unauthored
       transition for a control. `settle` is the transition a surface takes when it
       hands over to the world; asking for it here rather than authoring it into a
       layout is what keeps the decision with the destination, which is the only
       place that knows a change of state is a menu leaving for the world rather
       than for another menu (2026-10-09 direction record, item 4, Stage C). */
    anchor = outgoing->elements[0];
    unit.type = UI_ELE_ANIMATION;
    unit.visible = 1;
    unit.extent = UI_EXTENT_SURFACE;
    (void)snprintf(unit.name, sizeof(unit.name), "%s", "world_settle");
    (void)snprintf(unit.preset, sizeof(unit.preset), "%s", "settle");
    (void)snprintf(unit.target, sizeof(unit.target), "%s", anchor->name);
    (void)snprintf(unit.trigger, sizeof(unit.trigger), "%s", "context_exit");
    (void)snprintf(unit.orientation, sizeof(unit.orientation), "%s", "radial");
    return render_unit(&unit, outgoing, grid, elapsed_ms, reduced_motion, false,
                       UI_ANIMATION_EVENT_CONTEXT_EXIT, &covers);
}

bool ui_animation_render_layout(UiLayout *layout, Grid *grid, double elapsed_ms,
                                bool reduced_motion, bool preview_loop,
                                UiAnimationEvent event) {
    Grid *mask;
    UiCanvas *authored;
    bool result;
    bool covers = false;
    size_t count;
    SDL_Color unused = {0};
    if (!layout || !grid || !grid->cells || !isfinite(elapsed_ms) || elapsed_ms < 0 ||
        event < UI_ANIMATION_EVENT_CONTEXT_ENTER || event > UI_ANIMATION_EVENT_PREVIEW)
        return false;
    if (reduced_motion) return true;
    if (event == UI_ANIMATION_EVENT_PREVIEW)
        return render_layout_units(layout, grid, elapsed_ms, false, preview_loop,
                                   event, NULL);
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
    result = render_layout_units(layout, grid, elapsed_ms, false, preview_loop,
                                 event, &covers);
    /* A control is never left covered. The authored cells are restored unless a
       sanctioning primitive is *currently* covering the surface — the one
       exception recorded for the tide in the 2026-10-09 direction record — and
       that exception ends with the transition, when the tide reports no cover. */
    if (!covers)
        for (size_t i = 0; i < count; i++)
            if (authored->touched[i]) grid->cells[i] = authored->cells[i];
    ui_canvas_destroy(authored);
    return result;
}
