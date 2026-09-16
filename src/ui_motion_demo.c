#include "ui_motion_demo.h"

#include "ui_motion.h"
#include "ui_theme.h"

#include <math.h>
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

void ui_motion_demo_state_init(UiMotionDemoState *state, double now_ms) {
    if (!state || !isfinite(now_ms) || now_ms < 0.0) return;
    *state = (UiMotionDemoState){100, false, false, now_ms, 0.0};
}

bool ui_motion_demo_elapsed_ms(const UiMotionDemoState *state, double now_ms,
                               double *out_elapsed_ms) {
    double elapsed;
    if (!state || !out_elapsed_ms || !valid_scale(state->scale_percent) ||
        !isfinite(now_ms) || now_ms < 0.0 ||
        !isfinite(state->playback_origin_ms) || state->playback_origin_ms < 0.0 ||
        !isfinite(state->paused_elapsed_ms) || state->paused_elapsed_ms < 0.0) return false;
    elapsed = state->paused ? state->paused_elapsed_ms : now_ms - state->playback_origin_ms;
    if (elapsed < 0.0) return false;
    *out_elapsed_ms = fmod(elapsed, UI_MOTION_DEMO_CYCLE_MS);
    return true;
}

bool ui_motion_demo_apply_input(UiMotionDemoState *state, const InputState *input,
                                double now_ms, bool *out_should_exit) {
    UiMotionDemoState candidate;
    double elapsed;
    bool should_exit;
    if (!state || !input || !out_should_exit ||
        !ui_motion_demo_elapsed_ms(state, now_ms, &elapsed)) return false;
    candidate = *state;
    should_exit = input->quit || input->esc;

    if (input->confirm) {
        candidate.playback_origin_ms = now_ms;
        candidate.paused_elapsed_ms = 0.0;
    }
    if (input->place) {
        candidate.paused = !candidate.paused;
        if (candidate.paused) candidate.paused_elapsed_ms = elapsed;
        else candidate.playback_origin_ms = now_ms - candidate.paused_elapsed_ms;
    }
    if (input->tab) candidate.reduced_motion = !candidate.reduced_motion;
    if (candidate.paused && input->arrow_right)
        candidate.paused_elapsed_ms = fmod(elapsed + UI_MOTION_DEMO_STEP_MS,
                                            UI_MOTION_DEMO_CYCLE_MS);
    else if (candidate.paused && input->arrow_left)
        candidate.paused_elapsed_ms = fmod(elapsed + UI_MOTION_DEMO_CYCLE_MS -
                                            UI_MOTION_DEMO_STEP_MS,
                                            UI_MOTION_DEMO_CYCLE_MS);

    if (input->ui_scale_reset_pressed) candidate.scale_percent = 100;
    else if (input->up || input->ui_scale_increase_pressed)
        candidate.scale_percent = next_scale(candidate.scale_percent, 1);
    else if (input->down || input->ui_scale_decrease_pressed)
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
    size_t i;
    size_t length;
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

static bool reassembled_text(UiCanvas *canvas, int x, int y, const char *text,
                             uint32_t stable_id, double progress,
                             bool reduced_motion, SDL_Color fg, SDL_Color bg) {
    size_t length = strlen(text);
    size_t i;
    for (i = 0U; i < length; i++) {
        UiMotionOffset offset;
        if (!ui_motion_glyph_offset(stable_id, i, length, progress,
                                    reduced_motion, &offset) ||
            !ui_canvas_set(canvas, x + (int)i + offset.x, y + offset.y,
                           (uint8_t)text[i], fg, bg)) return false;
    }
    return true;
}

static bool render_specimen(const UiMotionDemoState *state, double elapsed,
                            UiCanvas *canvas) {
    const UiThemePalette *p = &ui_theme_provisional_tokens()->palette;
    SDL_Color canvas_color = color(p->canvas);
    SDL_Color panel = color(p->panel);
    SDL_Color primary = color(p->text_primary);
    SDL_Color secondary = color(p->text_secondary);
    SDL_Color accent = color(p->accent);
    SDL_Color focus = color(p->focus);
    SDL_Color literal_red = {255U, 48U, 72U, 255U};
    SDL_Color literal_cyan = {48U, 224U, 255U, 255U};
    UiMotionTransition enter = {101U, UI_THEME_MOTION_MAJOR_ENTER, 0.0,
                                0.0, 1.0, state->reduced_motion};
    UiMotionTransition exit = {101U, UI_THEME_MOTION_MAJOR_EXIT, 320.0,
                               1.0, 0.0, state->reduced_motion};
    UiMotionTransition relation = {303U, UI_THEME_MOTION_RELATIONSHIP, 80.0,
                                   0.0, 1.0, state->reduced_motion};
    UiMotionSample major;
    UiMotionSample relation_sample;
    char status[96];
    int relationship_x;

    if (!fill(canvas, 0, 0, canvas->width, canvas->height, ' ', primary, canvas_color))
        return false;
    if (elapsed < 320.0) {
        if (!ui_motion_sample(&enter, elapsed, &major)) return false;
    } else if (!ui_motion_sample(&exit, elapsed, &major)) return false;
    if (!ui_motion_sample(&relation, elapsed, &relation_sample)) return false;

    if (snprintf(status, sizeof(status),
                 "scale %d%% | %s | %s | t=%03d ms",
                 state->scale_percent, state->paused ? "PAUSED" : "PLAYING",
                 state->reduced_motion ? "REDUCED" : "FULL",
                 (int)elapsed) < 0) return false;
    if (!print(canvas, 2, 1, "CONTROLLED REGISTRATION / GLYPH REASSEMBLY", primary,
               canvas_color) ||
        !print(canvas, 2, 3, status, secondary, canvas_color) ||
        !print(canvas, 2, 5,
               "Enter replay | Space pause | Left/Right step | Tab reduced | Up/Down scale",
               secondary, canvas_color) ||
        !box(canvas, 2, 8, 44, 16, color(p->border), panel) ||
        !print(canvas, 4, 9, "MAJOR ENTER 160 ms / EXIT 120 ms", primary, panel) ||
        !print(canvas, 4, 11, "Stable title, focus, control and hit target:", secondary, panel) ||
        !print(canvas, 8, 14, "[ > OPEN WORKSPACE < ]", focus, panel) ||
        !print(canvas, 4, 21, "Decorative material re-registers nearby.", secondary, panel))
        return false;
    if (!reassembled_text(canvas, 13, 18, "REASSEMBLE", 101U, major.value,
                          state->reduced_motion, accent, panel)) return false;

    if (!box(canvas, 50, 8, 44, 16, color(p->border), panel) ||
        !print(canvas, 52, 9, "FEEDBACK 80 ms / RELATIONSHIP 120 ms", primary, panel) ||
        !print(canvas, 52, 12, "[ PRESS ]", elapsed < 80.0 ? accent : primary, panel) ||
        !print(canvas, 52, 15, "SOURCE", primary, panel) ||
        !print(canvas, 82, 15, "TARGET", primary, panel)) return false;
    relationship_x = 61 + (int)lround(18.0 * relation_sample.value);
    if (!ui_canvas_set(canvas, relationship_x, 15, '>', accent, panel) ||
        !print(canvas, 52, 21, "Cue moves; labels and activation stay fixed.", secondary, panel))
        return false;

    if (!box(canvas, 2, 27, 92, 17, color(p->border), panel) ||
        !print(canvas, 4, 28, "CHROMATIC TRACE COMPARISON — EFFECT CHANNELS, NEVER SEMANTIC STATUS", primary,
               panel) ||
        !print(canvas, 4, 32, "PALETTE-NATIVE", secondary, panel) ||
        !print(canvas, 24, 32, "< ACCENT TRACE >", accent, panel) ||
        !print(canvas, 26, 33, "< FOCUS TRACE >", focus, panel) ||
        !print(canvas, 4, 37, "ISOLATED RGB", secondary, panel) ||
        !print(canvas, 24, 37, "< RED CHANNEL >", literal_red, panel) ||
        !print(canvas, 26, 38, "< CYAN CHANNEL >", literal_cyan, panel) ||
        !print(canvas, 4, 41, "Warning/error/success colors are not used as decorative trails.",
               secondary, panel) ||
        !print(canvas, 2, 47, "REDUCED MOTION: immediate, non-spatial, no delayed interaction.",
               state->reduced_motion ? focus : secondary, canvas_color) ||
        !print(canvas, 2, 49, "Renderer seam: cell/glyph/layer composition only; no framebuffer RGB split.",
               secondary, canvas_color) ||
        !print(canvas, 2, 52, "ACCEPTED D6 VOCABULARY — diagnostic only; integration belongs to V1-3.",
               color(p->success), canvas_color) ||
        !print(canvas, 2, 54, "Escape exits. Replay, pause, and inspect all scales or motion modes.",
               secondary, canvas_color)) return false;
    return true;
}

UiMotionDemoRenderResult ui_motion_demo_render(const UiMotionDemoState *state,
                                               double now_ms, UiCanvas *canvas) {
    UiCanvas *candidate;
    double elapsed;
    size_t count;
    if (!state || !canvas || canvas->width != UI_MOTION_DEMO_WIDTH ||
        canvas->height != UI_MOTION_DEMO_HEIGHT || !canvas->cells || !canvas->touched ||
        !ui_motion_demo_elapsed_ms(state, now_ms, &elapsed))
        return UI_MOTION_DEMO_RENDER_INVALID_ARGUMENT;
    candidate = ui_canvas_create(canvas->width, canvas->height);
    if (!candidate) return UI_MOTION_DEMO_RENDER_OUT_OF_MEMORY;
    if (!render_specimen(state, elapsed, candidate)) {
        ui_canvas_destroy(candidate);
        return UI_MOTION_DEMO_RENDER_INVARIANT_FAILED;
    }
    count = (size_t)canvas->width * (size_t)canvas->height;
    memcpy(canvas->cells, candidate->cells, count * sizeof(*canvas->cells));
    memcpy(canvas->touched, candidate->touched, count * sizeof(*canvas->touched));
    ui_canvas_destroy(candidate);
    return UI_MOTION_DEMO_RENDER_OK;
}