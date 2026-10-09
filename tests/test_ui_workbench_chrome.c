#define _POSIX_C_SOURCE 200809L

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

#include <cmocka.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/checked_size.h"
#include "../src/grid.h"
#include "../src/map_catalog.h"
#include "../src/menu_state.h"
#include "../src/ui_app_theme_adapter.h"
#include "../src/ui_canvas.h"
#include "../src/ui_ele.h"
#include "../src/ui_theme.h"
#include "../src/ui_animation.h"
#include "../src/ui_workbench.h"
#include "../src/ui_workbench_chrome.h"
#include "../src/ui_workbench_frame.h"
#include "../src/ui_workbench_guide.h"
#include "../src/ui_workbench_runtime.h"
#include "../src/ui_workbench_store.h"

#include <stdint.h>

static void test_rejects_invalid_arguments(void **state) {
    UiAppWorkbenchPalette palette;
    Grid *grid;
    UiCanvas *canvas;
    SDL_Color color;
    (void)state;
    assert_non_null(ui_app_theme_workbench_palette(&palette) ? &palette : NULL);
    assert_true(ui_app_theme_workbench_palette(&palette));
    assert_string_equal(ui_workbench_chrome_result_string(UI_WORKBENCH_CHROME_OK),
                        "ok");
    assert_string_equal(
        ui_workbench_chrome_result_string(UI_WORKBENCH_CHROME_INVALID_ARGUMENT),
        "invalid argument");
    assert_true(ui_workbench_chrome_contract_dimensions(260, 160));
    assert_false(ui_workbench_chrome_contract_dimensions(120, 40));
    assert_false(ui_workbench_chrome_role_color(NULL,
                                                UI_WORKBENCH_CHROME_ROLE_PANEL,
                                                &color));
    assert_false(ui_workbench_chrome_role_color(
        &palette, UI_WORKBENCH_CHROME_ROLE_COUNT, &color));
    assert_false(ui_workbench_chrome_role_color(
        &palette, UI_WORKBENCH_CHROME_ROLE_PANEL, NULL));
    assert_int_equal(ui_workbench_chrome_preview_first_row(), 1);
    assert_int_equal(ui_workbench_chrome_footer_first_row(160), 157);
    assert_int_equal(ui_workbench_chrome_footer_first_row(120), -1);
    assert_int_equal(ui_workbench_chrome_left_pane_width(), 48);
    assert_int_equal(ui_workbench_chrome_right_pane_width(), 72);
    canvas = ui_canvas_create(260, 156);
    assert_non_null(canvas);
    assert_false(ui_workbench_chrome_scale_preview_rows(NULL, canvas, 156,
                                                        100));
    assert_false(ui_workbench_chrome_scale_preview_rows(canvas, NULL, 156,
                                                        100));
    assert_false(ui_workbench_chrome_scale_preview_rows(canvas, canvas, 0,
                                                        100));
    assert_true(ui_workbench_chrome_scale_preview_rows(canvas, canvas, 156,
                                                       100));
    assert_false(ui_workbench_chrome_placement_cover_row(NULL, canvas, 156,
                                                         100, 157));
    assert_false(ui_workbench_chrome_placement_cover_row(canvas, NULL, 156,
                                                         100, 157));
    assert_true(ui_workbench_chrome_placement_cover_row(canvas, canvas, 156,
                                                        100, 157));
    assert_false(ui_workbench_chrome_blend_preview(NULL, canvas, 156, 100));
    assert_false(ui_workbench_chrome_blend_matches(NULL, canvas, 1, 156));
    ui_canvas_destroy(canvas);
    assert_false(ui_workbench_chrome_paint_panel(NULL, &palette, 0, 0, 10, 10,
                                                 false));
    grid = grid_create(260, 160);
    assert_non_null(grid);
    assert_false(ui_workbench_chrome_paint_panel(
        grid, NULL, 0, 0, 10, 10, false));
    assert_false(ui_workbench_chrome_paint_panel(
        grid, &palette, -1, 0, 10, 10, false));
    assert_false(ui_workbench_chrome_footer_rows(NULL, &palette, NULL, NULL,
                                                 100, false));
    grid_destroy(grid);
}

static void assert_sdl_color(SDL_Color actual, uint8_t red, uint8_t green,
                             uint8_t blue, uint8_t alpha) {
    assert_int_equal(actual.r, red);
    assert_int_equal(actual.g, green);
    assert_int_equal(actual.b, blue);
    assert_int_equal(actual.a, alpha);
}

static void test_all_role_colors_come_from_tokens(void **state) {
    UiAppWorkbenchPalette palette;
    const UiThemePalette *tokens = &ui_theme_provisional_tokens()->palette;
    SDL_Color color;
    (void)state;
    assert_true(ui_app_theme_workbench_palette(&palette));
    assert_true(ui_workbench_chrome_role_color(
        &palette, UI_WORKBENCH_CHROME_ROLE_PANEL, &color));
    assert_sdl_color(color, tokens->panel.red, tokens->panel.green,
                     tokens->panel.blue, tokens->panel.alpha);
    assert_true(ui_workbench_chrome_role_color(
        &palette, UI_WORKBENCH_CHROME_ROLE_ACCENT, &color));
    assert_sdl_color(color, tokens->accent.red, tokens->accent.green,
                     tokens->accent.blue, tokens->accent.alpha);
    assert_true(ui_workbench_chrome_role_color(
        &palette, UI_WORKBENCH_CHROME_ROLE_FOCUS, &color));
    assert_sdl_color(color, tokens->focus.red, tokens->focus.green,
                     tokens->focus.blue, tokens->focus.alpha);
    assert_true(ui_workbench_chrome_role_color(
        &palette, UI_WORKBENCH_CHROME_ROLE_WARNING, &color));
    assert_sdl_color(color, tokens->warning.red, tokens->warning.green,
                     tokens->warning.blue, tokens->warning.alpha);
    assert_true(ui_workbench_chrome_role_color(
        &palette, UI_WORKBENCH_CHROME_ROLE_ERROR, &color));
    assert_sdl_color(color, tokens->error.red, tokens->error.green,
                     tokens->error.blue, tokens->error.alpha);
}

static void test_panels_use_token_borders_and_focus(void **state) {
    UiAppWorkbenchPalette palette;
    Grid *grid;
    Cell corner;
    Cell fill;
    (void)state;
    assert_true(ui_app_theme_workbench_palette(&palette));
    grid = grid_create(260, 160);
    assert_non_null(grid);
    assert_true(ui_workbench_chrome_paint_panel(grid, &palette, 2, 2, 10, 6,
                                                false));
    assert_true(grid_get(grid, 2, 2, &corner));
    assert_int_equal(corner.glyph, '+');
    assert_int_equal(corner.fg.r, palette.secondary_text.r);
    assert_true(grid_get(grid, 4, 4, &fill));
    assert_int_equal(fill.glyph, ' ');
    assert_int_equal(fill.bg.r, palette.panel.r);
    assert_true(ui_workbench_chrome_paint_panel(grid, &palette, 20, 2, 10, 6,
                                                true));
    assert_true(grid_get(grid, 20, 2, &corner));
    assert_int_equal(corner.glyph, '+');
    assert_int_equal(corner.fg.r, palette.accent.r);
    assert_true(grid_get(grid, 22, 4, &fill));
    assert_int_equal(fill.glyph, ' ');
    assert_int_equal(fill.bg.r, palette.elevated.r);
    assert_int_equal(fill.fg.r, palette.secondary_text.r);
    grid_destroy(grid);
}

static void test_footer_rows_render_identity_controls_diagnostics(void **state) {
    UiWorkbench workbench;
    Grid *grid;
    UiAppWorkbenchPalette palette;
    Cell status;
    Cell controls;
    Cell diagnostic;
    (void)state;
    ui_workbench_init(&workbench);
    grid = grid_create(260, 160);
    assert_non_null(grid);
    assert_true(ui_app_theme_workbench_palette(&palette));
    assert_int_equal(ui_workbench_open(&workbench, MENU_MAIN),
                     UI_WORKBENCH_OK);
    assert_true(ui_workbench_chrome_footer_rows(grid, &palette, &workbench,
                                                NULL, 100, false));
    assert_true(grid_get(grid, 1, 157, &status));
    assert_true(grid_get(grid, 1, 158, &controls));
    assert_true(grid_get(grid, 1, 159, &diagnostic));
    assert_int_equal(status.fg.r, palette.primary_text.r);
    assert_int_equal(controls.fg.r, palette.secondary_text.r);
    assert_int_equal(diagnostic.fg.r, palette.secondary_text.r);
    assert_true(status.glyph == 'U' || status.glyph == 'A');
    grid_destroy(grid);
    ui_workbench_destroy(&workbench);
}

static bool open_fixture(UiWorkbench *workbench, MenuId context) {
    return workbench && ui_workbench_open(workbench, context) == UI_WORKBENCH_OK;
}

static bool render_fixture(UiWorkbench *workbench, Grid *grid,
                           UiAppWorkbenchPalette *palette, int scale_percent,
                           double elapsed_ms, bool *ok) {
    UiWorkbenchFrameInput input;
    bool rendered;
    if (!ok) return false;
    input.grid = grid;
    input.workbench = workbench;
    input.palette = palette;
    input.tooltip_text = NULL;
    input.scale_percent = scale_percent;
    input.elapsed_ms = elapsed_ms;
    input.reduced_motion = false;
    input.pointer_row = 0;
    input.pointer_column = 0;
    input.pointer_active = false;
    rendered = ui_workbench_frame_render(input);
    *ok = rendered;
    return rendered;
}

static void test_scaled_preview_keeps_authored_cells(void **state) {
    UiWorkbench workbench;
    Grid *grid;
    UiAppWorkbenchPalette palette;
    UiCanvas *authored;
    UiCanvas *scaled;
    Cell expect;
    Cell actual;
    bool rendered = false;
    (void)state;
    ui_workbench_init(&workbench);
    grid = grid_create(UI_WORKBENCH_FRAME_CONTRACT_COLUMNS,
                       UI_WORKBENCH_FRAME_CONTRACT_ROWS);
    assert_non_null(grid);
    assert_true(ui_app_theme_workbench_palette(&palette));
    assert_true(open_fixture(&workbench, MENU_MAIN));
    assert_true(render_fixture(&workbench, grid, &palette, 100, 0.0, &rendered));
    assert_true(rendered);
    authored = ui_canvas_create(UI_WORKBENCH_FRAME_CONTRACT_COLUMNS, 156);
    assert_non_null(authored);
    scaled = ui_canvas_create(UI_WORKBENCH_FRAME_CONTRACT_COLUMNS, 156);
    assert_non_null(scaled);
    ui_canvas_copy_grid_region(authored, grid, 0, 1);
    assert_true(ui_workbench_chrome_scale_preview_rows(scaled, authored, 156,
                                                       100));
    assert_true(ui_canvas_is_touched(scaled, 2, 4));
    assert_true(grid_get(grid, 2, 5, &actual));
    expect = scaled->cells[4U * 260U + 2U];
    assert_int_equal(actual.glyph, expect.glyph);
    assert_int_equal(actual.fg.r, expect.fg.r);
    ui_canvas_destroy(scaled);
    ui_canvas_destroy(authored);
    grid_destroy(grid);
    ui_workbench_destroy(&workbench);
}

static void test_tooltip_text_sits_beside_status(void **state) {
    UiWorkbench workbench;
    Grid *grid;
    UiAppWorkbenchPalette palette;
    static const char *const tooltip = "Browse mode. Pick a row.";
    size_t length;
    size_t i;
    size_t status_length;
    (void)state;
    ui_workbench_init(&workbench);
    grid = grid_create(260, 160);
    assert_non_null(grid);
    assert_true(ui_app_theme_workbench_palette(&palette));
    assert_int_equal(ui_workbench_open(&workbench, MENU_MAIN),
                     UI_WORKBENCH_OK);
    assert_true(ui_workbench_chrome_footer_rows(grid, &palette, &workbench,
                                                tooltip, 100, false));
    length = strlen(tooltip);
    assert_true(length > 0U && length < 96U);
    status_length = strlen(workbench.status);
    assert_true(status_length > 0U);
    assert_true(status_length + 3U + length < 224U);
    for (i = 0U; i < length; i++) {
        Cell diagnostic;
        assert_true(grid_get(grid, 1 + (int)status_length + 3 + (int)i, 159,
                             &diagnostic));
        assert_int_equal(diagnostic.glyph, (uint8_t)tooltip[i]);
    }
    grid_destroy(grid);
    ui_workbench_destroy(&workbench);
}

static void test_runtime_footer_canvas_contains_scaled_footer(void **state) {
    UiWorkbench workbench;
    Grid *grid;
    UiCanvas *canvas;
    UiAppWorkbenchPalette palette;
    static const char *const tooltip = "Runtime footer must scale.";
    size_t tooltip_length;
    size_t i;
    int width;
    int height;
    const int footer_y = 6;
    const int controls_y = 74;
    (void)state;
    ui_workbench_init(&workbench);
    grid = grid_create(UI_WORKBENCH_CHROME_COLUMNS, UI_WORKBENCH_CHROME_ROWS);
    assert_non_null(grid);
    assert_true(ui_workbench_runtime_interface_size(200, false, &width, &height));
    assert_int_equal(width, 130);
    assert_int_equal(height, 80);
    canvas = ui_canvas_create(width, height);
    assert_non_null(canvas);
    assert_true(ui_app_theme_workbench_palette(&palette));
    assert_int_equal(ui_workbench_open(&workbench, MENU_MAIN),
                     UI_WORKBENCH_OK);
    workbench.mode = UI_WORKBENCH_MODE_HELP;
    assert_true(ui_workbench_runtime_compose_footer_canvas(
        canvas, grid, &workbench, &palette, tooltip, 200, false));
    assert_true(ui_canvas_is_touched(canvas, 1, controls_y));
    assert_int_equal(canvas->cells[(size_t)controls_y *
                                   (size_t)canvas->width + 1U].glyph,
                     'H');
    tooltip_length = strlen(tooltip);
    assert_true(tooltip_length > 0U);
    for (i = 0U; i < tooltip_length; i++) {
        size_t x = i;
        size_t index = (size_t)footer_y * (size_t)canvas->width + x;
        assert_true(ui_canvas_is_touched(canvas, (int)x, footer_y));
        assert_int_equal(canvas->cells[index].glyph, (uint8_t)tooltip[i]);
    }
    grid_destroy(grid);
    ui_canvas_destroy(canvas);
    ui_workbench_destroy(&workbench);
}

static void test_runtime_layers_use_independent_scales(void **state) {
    UiCanvas *preview;
    UiCanvas *footer;
    UiLayerList layers;
    SDL_Color black = {0, 0, 0, 255};
    static const int scales[] = {100, 125, 150, 200};
    size_t authored_index;
    size_t workbench_index;
    (void)state;
    preview = ui_canvas_create(UI_WORKBENCH_CHROME_COLUMNS,
                               UI_WORKBENCH_CHROME_ROWS);
    assert_non_null(preview);
    footer = ui_canvas_create(UI_WORKBENCH_CHROME_COLUMNS,
                              UI_WORKBENCH_CHROME_FOOTER_ROWS);
    assert_non_null(footer);
    assert_true(ui_canvas_set(preview, 0, 0, 'P', black, black));
    assert_true(ui_canvas_set(footer, 0, 0, 'F', black, black));
    for (authored_index = 0U;
         authored_index < sizeof(scales) / sizeof(scales[0]);
         authored_index++) {
        for (workbench_index = 0U;
             workbench_index < sizeof(scales) / sizeof(scales[0]);
             workbench_index++) {
            assert_true(ui_workbench_runtime_build_layers(
                &layers, preview, footer, 2080, 1280,
                scales[authored_index], scales[workbench_index]));
            assert_int_equal((int)layers.count, 2);
            assert_int_equal(layers.layers[0].role_id, 1);
            assert_int_equal(layers.layers[0].anchor, UI_ANCHOR_CENTER);
            assert_int_equal(layers.layers[0].scale_policy,
                             UI_SCALE_EXPLICIT_PRESET);
            assert_int_equal(layers.layers[0].explicit_scale_percent,
                             scales[authored_index]);
            assert_int_equal(layers.layers[1].role_id, 2);
            assert_int_equal(layers.layers[1].anchor,
                             UI_ANCHOR_BOTTOM_LEFT);
            assert_int_equal(layers.layers[1].scale_policy,
                             UI_SCALE_EXPLICIT_PRESET);
            assert_int_equal(layers.layers[1].explicit_scale_percent,
                             scales[workbench_index]);
        }
    }
    assert_false(ui_workbench_runtime_build_layers(NULL, preview, footer,
                                                   2080, 1280, 150, 200));
    assert_false(ui_workbench_runtime_build_layers(&layers, preview, footer,
                                                   2080, 1280, 175, 200));
    ui_canvas_destroy(footer);
    ui_canvas_destroy(preview);
}

static void test_runtime_workbench_scale_steps_independently(void **state) {
    (void)state;
    assert_int_equal(ui_workbench_runtime_step_scale(100, 1), 125);
    assert_int_equal(ui_workbench_runtime_step_scale(125, 1), 150);
    assert_int_equal(ui_workbench_runtime_step_scale(150, 1), 200);
    assert_int_equal(ui_workbench_runtime_step_scale(200, 1), 200);
    assert_int_equal(ui_workbench_runtime_step_scale(200, -1), 150);
    assert_int_equal(ui_workbench_runtime_step_scale(100, -1), 100);
    assert_int_equal(ui_workbench_runtime_step_scale(150, 0), 150);
}

static void test_wrapped_guidance_all_scales_and_modes(void **state) {
    static const int scales[] = {100, 125, 150, 200};
    UiWorkbench workbench;
    UiAppWorkbenchPalette palette;
    Grid *grid;
    char tooltip[UI_WORKBENCH_GUIDE_TEXT_MAX];
    size_t scale;
    int mode;
    (void)state;
    memset(tooltip, 'T', sizeof(tooltip) - 1);
    tooltip[sizeof(tooltip) - 1] = '\0';
    ui_workbench_init(&workbench);
    assert_true(open_fixture(&workbench, MENU_MAIN));
    assert_true(ui_app_theme_workbench_palette(&palette));
    grid = grid_create(260, 160);
    assert_non_null(grid);
    assert_false(ui_workbench_runtime_footer_size(175, &mode, &mode));
    for (scale = 0; scale < sizeof(scales) / sizeof(scales[0]); scale++) {
        int width;
        int height;
        int rows;
        UiCanvas *canvas;
        assert_true(ui_workbench_runtime_interface_size(scales[scale], false, &width, &height));
        assert_true(width * scales[scale] <= 26000);
        assert_true(height * scales[scale] <= 16000);
        canvas = ui_canvas_create(width, height);
        assert_non_null(canvas);
        rows = (260 + width - 1) / width;
        for (mode = UI_WORKBENCH_MODE_BROWSE; mode <= UI_WORKBENCH_MODE_TEXT; mode++) {
            size_t i;
            workbench.mode = (UiWorkbenchMode)mode;
            assert_true(ui_workbench_runtime_compose_footer_canvas(
                canvas, grid, &workbench, &palette, tooltip, scales[scale], true));
            for (i = 0; mode == UI_WORKBENCH_MODE_HELP && i < strlen(tooltip); i++) {
                size_t index = (size_t)(3 * rows) * (size_t)width + i;
                assert_int_equal(canvas->cells[index].glyph, 'T');
            }
            assert_true(canvas->cells[(size_t)(height - rows) * (size_t)width + 1].glyph != 0);
        }
        ui_canvas_destroy(canvas);
    }
    grid_destroy(grid);
    ui_workbench_destroy(&workbench);
}

static void test_runtime_preview_independent_rendering(void **state) {
    UiWorkbench workbench;
    UiAppWorkbenchPalette palette;
    Grid *actual = grid_create(260, 160);
    Grid *expected = grid_create(260, 160);
    UiCanvas *canvas = ui_canvas_create(260, 160);
    int context;
    (void)state;
    assert_non_null(actual);
    assert_non_null(expected);
    assert_non_null(canvas);
    assert_true(ui_app_theme_workbench_palette(&palette));
    ui_workbench_init(&workbench);
    for (context = MENU_MAIN; context <= MENU_CONFIRM_QUIT; context++) {
        assert_int_equal(ui_workbench_open(&workbench, (MenuId)context), UI_WORKBENCH_OK);
        assert_true(grid_clear_region_zero(expected, 0, 0, 260, 160));
        ui_layout_set_focus(workbench.layout, -1);
        ui_layout_render(workbench.layout, expected, palette.secondary_text, palette.canvas);
        assert_true(ui_animation_render_layout(workbench.layout, expected, 40.0,
                                                true, false, UI_ANIMATION_EVENT_PREVIEW));
        assert_true(ui_workbench_runtime_preview(canvas, actual, &workbench, &palette,
                                                  40.0, true));
        assert_memory_equal(actual->cells, expected->cells, 260U * 160U * sizeof(Cell));
    }
    assert_false(ui_workbench_runtime_preview(canvas, actual, &workbench, &palette, -1, true));
    ui_workbench_destroy(&workbench);
    ui_canvas_destroy(canvas);
    grid_destroy(expected);
    grid_destroy(actual);
}

static void test_runtime_snapshot_scale_isolation(void **state) {
    UiWorkbench workbench;
    UiAppWorkbenchPalette palette;
    const size_t count = 2080U * 1280U;
    uint32_t *first = malloc(count * sizeof(*first));
    uint32_t *second = malloc(count * sizeof(*second));
    static const int scales[] = {100, 125, 150, 200};
    size_t i;
    (void)state;
    assert_non_null(first);
    assert_non_null(second);
    assert_true(ui_app_theme_workbench_palette(&palette));
    ui_workbench_init(&workbench);
    assert_int_equal(ui_workbench_open(&workbench, MENU_MAIN), UI_WORKBENCH_OK);
    assert_true(ui_workbench_runtime_snapshot(&workbench, &palette, "Guidance", 150, 100,
                                             0, true, first, count));
    for (i = 0; i < sizeof(scales) / sizeof(scales[0]); i++) {
        assert_true(ui_workbench_runtime_snapshot(&workbench, &palette, "Guidance", 150,
                                                 scales[i], 90, true, second, count));
        /* Top identity/tabs and bottom cards scale; the central authored region does not. */
        assert_memory_equal(first + 2080U * 128U, second + 2080U * 128U,
                            2080U * 768U * sizeof(*first));
    }
    assert_false(ui_workbench_runtime_snapshot(&workbench, &palette, NULL, 175, 100,
                                              0, true, second, count));
    ui_workbench_destroy(&workbench);
    free(second);
    free(first);
}

static void test_edit_panels_progressive_and_scaled(void **state) {
    static const int scales[] = {100, 125, 150, 200};
    UiWorkbench workbench;
    UiAppWorkbenchPalette palette;
    Grid *grid = grid_create(260, 160);
    (void)state;
    assert_non_null(grid);
    ui_workbench_init(&workbench);
    assert_true(open_fixture(&workbench, MENU_MAIN));
    assert_true(ui_app_theme_workbench_palette(&palette));
    for (size_t i = 0; i < sizeof(scales) / sizeof(scales[0]); i++) {
        int width;
        int browse_height;
        int height;
        UiCanvas *canvas;
        assert_true(ui_workbench_runtime_interface_size(scales[i], false, &width, &browse_height));
        assert_true(ui_workbench_runtime_interface_size(scales[i], true, &width, &height));
        assert_int_equal(height, browse_height);
        assert_true(width * scales[i] <= 26000);
        assert_true(height * scales[i] <= 16000);
        canvas = ui_canvas_create(width, height);
        assert_non_null(canvas);
        workbench.editing = true;
        assert_true(ui_workbench_runtime_compose_footer_canvas(canvas, grid, &workbench,
                                                               &palette, "Help", scales[i], true));
        {
            int rows = (260 + width - 1) / width;
            int tray = height - 12 - 3 * rows;
            assert_int_equal(canvas->cells[tray * width].glyph, '+');
            assert_int_equal(canvas->cells[(tray + 11) * width].glyph, '+');
            assert_false(ui_canvas_is_touched(canvas, width / 2, height / 2));
        }
        ui_canvas_destroy(canvas);
    }
    ui_workbench_destroy(&workbench);
    grid_destroy(grid);
}

static void test_runtime_pixel_fixtures(void **state) {
    /* Reviewed refresh 2026-10-08 (docs/reviews/2026-10-08-menu-surface.md):
       all four contexts changed because a menu is now a surface over the whole
       display instead of an 80x40 centred canvas, and the backdrop field fills
       that surface (element `extent=surface`). Nothing else re-tuned.
       Reviewed refresh 2026-10-09 (docs/reviews/2026-10-09-backdrop-fabric.md):
       all four contexts changed because the `living_field` backdrop became a
       solid fabric carried by one travelling diagonal wave with a chromatic
       wake (change of direction recorded 2026-10-09 in
       UI_LOOK_AND_FEEL_REFERENCE_OF_RECORD.md). The frame is a settled preview —
       `ui_workbench_runtime_preview` never plays a transition, so no tide is in
       it — and the field is sampled at phase 0, so the frame is deterministic.
       No other rendering path changed.
       Reverted, not refreshed 2026-10-09 (state transitions): these four values
       moved when the menus gained their tide elements, and were re-recorded at
       the time. That was wrong. The chrome builds a full-surface specimen of the
       layout and plays the enter on it before copying rows out for the tray, so
       the first surface-extent element to arrive flooded the specimen and every
       tray row taken from it — `ui_workbench_chrome.c` now skips surface-extent
       units in a specimen, and the frames are exactly what they were before the
       tide existed. The fixture caught the artifact; it should not have been
       re-recorded over. */
    static const uint64_t expected[] = {
        UINT64_C(7273567102547718950), UINT64_C(18159151064839384890),
        UINT64_C(2216372606294381406), UINT64_C(12825855358401564365)
    };
    const size_t count = 2080U * 1280U;
    uint32_t *pixels = malloc(count * sizeof(*pixels));
    UiWorkbench workbench;
    UiWorkbenchGuide guide;
    UiAppWorkbenchPalette palette;
    (void)state;
    assert_non_null(pixels);
    ui_workbench_init(&workbench);
    assert_true(ui_app_theme_workbench_palette(&palette));
    assert_int_equal(ui_workbench_guide_load(&guide, "assets/editor_tooltips.txt"), UI_WORKBENCH_GUIDE_OK);
    for (int context = MENU_MAIN; context <= MENU_CONFIRM_QUIT; context++) {
        uint64_t hash = UINT64_C(14695981039346656037);
        const unsigned char *bytes = (const unsigned char *)pixels;
        assert_int_equal(ui_workbench_open(&workbench, (MenuId)context), UI_WORKBENCH_OK);
        assert_true(ui_workbench_runtime_snapshot(&workbench, &palette,
            ui_workbench_guide_tooltip(&guide, &workbench), 100, 100, 0, false, pixels, count));
        for (size_t i = 0; i < count * sizeof(*pixels); i++) {
            hash ^= bytes[i];
            hash *= UINT64_C(1099511628211);
        }
        /* A mismatch has to name the checksum it computed: that is the whole
           workflow for refreshing a recorded frame, and the same contract the
           `check-ui-workbench-frame` gate keeps. */
        if (hash != expected[context - MENU_MAIN]) {
            fprintf(stderr, "FAIL-PRODUCT: menu-context %d checksum=%llu expected=%llu\n",
                    context, (unsigned long long)hash,
                    (unsigned long long)expected[context - MENU_MAIN]);
        }
        assert_true(hash == expected[context - MENU_MAIN]);
    }
    ui_workbench_guide_destroy(&guide);
    ui_workbench_destroy(&workbench);
    free(pixels);
}

static void test_runtime_lifecycle_reduced_matches_static(void **state) {
    UiWorkbench workbench;
    UiAppWorkbenchPalette palette;
    Grid *actual = grid_create(260, 160);
    Grid *expected = grid_create(260, 160);
    UiCanvas *canvas = ui_canvas_create(260, 160);
    (void)state;
    assert_non_null(actual);
    assert_non_null(expected);
    assert_non_null(canvas);
    assert_true(ui_app_theme_workbench_palette(&palette));
    ui_workbench_init(&workbench);
    for (int context = MENU_MAIN; context <= MENU_CONFIRM_QUIT; context++) {
        assert_int_equal(ui_workbench_open(&workbench, (MenuId)context), UI_WORKBENCH_OK);
        assert_true(grid_clear_region_zero(expected, 0, 0, 260, 160));
        ui_layout_set_focus(workbench.layout, -1);
        ui_layout_render(workbench.layout, expected, palette.secondary_text, palette.canvas);
        for (int event = UI_ANIMATION_EVENT_CONTEXT_ENTER;
             event <= UI_ANIMATION_EVENT_WHILE_VISIBLE; event++) {
            assert_true(ui_workbench_runtime_preview_event(canvas, actual, &workbench,
                &palette, (UiAnimationEvent)event, 40.0, true));
            assert_memory_equal(actual->cells, expected->cells, 260U * 160U * sizeof(Cell));
            assert_int_equal(workbench.context, context);
            assert_int_equal(workbench.element_index, 0);
        }
    }
    assert_false(ui_workbench_runtime_preview_event(canvas, actual, &workbench,
        &palette, (UiAnimationEvent)-1, 40.0, false));
    ui_workbench_destroy(&workbench);
    ui_canvas_destroy(canvas);
    grid_destroy(expected);
    grid_destroy(actual);
}

static void test_exit_overlay_preserves_incoming_and_endpoints(void **state) {
    UiWorkbench outgoing;
    UiElement target = {0};
    UiElement unit = {0};
    UiLayout layout = {0};
    UiCanvas *preview = ui_canvas_create(260, 160);
    Grid *staging = grid_create(260, 160);
    Cell protected_cell;
    SDL_Color fg = {255, 255, 255, 255};
    SDL_Color bg = {0, 0, 0, 255};
    (void)state;
    assert_non_null(preview);
    assert_non_null(staging);
    ui_workbench_init(&outgoing);
    (void)snprintf(target.name, sizeof(target.name), "target");
    (void)snprintf(target.transition, sizeof(target.transition), "none");
    target.type = UI_ELE_BUTTON;
    target.visible = 1;
    target.layout = (UiElementLayout){10, 10, UI_COORD_ABSOLUTE, 10, 1};
    unit.type = UI_ELE_ANIMATION;
    unit.visible = 1;
    (void)snprintf(unit.name, sizeof(unit.name), "exit");
    (void)snprintf(unit.target, sizeof(unit.target), "target");
    (void)snprintf(unit.preset, sizeof(unit.preset), "local_glitch");
    (void)snprintf(unit.trigger, sizeof(unit.trigger), "context_exit");
    layout.elements[0] = &target;
    layout.elements[1] = &unit;
    layout.element_count = 2;
    outgoing.layout = &layout;
    assert_true(ui_workbench_runtime_exit_overlay(preview, staging, &outgoing, 40, false));
    assert_true(ui_canvas_is_touched(preview, 14, 9));
    ui_canvas_clear(preview);
    assert_true(ui_canvas_set(preview, 14, 9, 'X', fg, bg));
    protected_cell = preview->cells[9U * 260U + 14U];
    assert_true(ui_workbench_runtime_exit_overlay(preview, staging, &outgoing, 40, false));
    assert_memory_equal(&protected_cell, &preview->cells[9U * 260U + 14U], sizeof(Cell));
    ui_canvas_clear(preview);
    assert_true(ui_workbench_runtime_exit_overlay(preview, staging, &outgoing, 120, false));
    assert_false(ui_canvas_is_touched(preview, 14, 9));
    assert_true(ui_workbench_runtime_exit_overlay(preview, staging, &outgoing, 40, true));
    assert_false(ui_canvas_is_touched(preview, 14, 9));
    assert_false(ui_workbench_runtime_exit_overlay(preview, staging, &outgoing, -1, false));
    outgoing.layout = NULL;
    ui_workbench_destroy(&outgoing);
    ui_canvas_destroy(preview);
    grid_destroy(staging);
}

/* The one primitive sanctioned to pass over authored content is the tide, and
   only for as long as it reports a cover. It paints from its own ramp, so a
   covered authored cell always carries one of these glyphs — never a blank and
   never a field glyph. */
static bool tide_cover_glyph(uint8_t glyph) {
    static const char ramp[] = ". :-=+*#";
    for (size_t i = 0; i < sizeof(ramp) - 1U; i++)
        if (glyph == (uint8_t)ramp[i]) return true;
    return false;
}

/* The window a context event animates over, from the theme's own durations. */
static uint32_t event_window_ms(UiAnimationEvent event) {
    UiThemeMotionRole role = UI_THEME_MOTION_MAJOR_ENTER;
    switch (event) {
        case UI_ANIMATION_EVENT_CONTEXT_EXIT:
            role = UI_THEME_MOTION_MAJOR_EXIT;
            break;
        case UI_ANIMATION_EVENT_FOCUS:
        case UI_ANIMATION_EVENT_ACTIVATE:
            role = UI_THEME_MOTION_FEEDBACK;
            break;
        default:
            break;
    }
    return ui_theme_motion_duration_ms(role, false);
}

static void test_lifecycle_keeps_authored_cells_stable(void **state) {
    UiWorkbench workbench;
    UiAppWorkbenchPalette palette;
    Grid *actual = grid_create(260, 160);
    Grid *expected = grid_create(260, 160);
    UiCanvas *canvas = ui_canvas_create(260, 160);
    UiCanvas *reference = ui_canvas_create(260, 160);
    static const double times[] = {0, 40, 80, 120, 160};
    (void)state;
    assert_non_null(actual);
    assert_non_null(expected);
    assert_non_null(canvas);
    assert_non_null(reference);
    assert_true(ui_app_theme_workbench_palette(&palette));
    ui_workbench_init(&workbench);
    for (int context = MENU_MAIN; context <= MENU_CONFIRM_QUIT; context++) {
        bool saved_visible[UI_LAYOUT_MAX_ELEMS];
        int saved_count = 0;
        assert_int_equal(ui_workbench_open(&workbench, (MenuId)context), UI_WORKBENCH_OK);
        assert_true(grid_clear_region_zero(expected, 0, 0, 260, 160));
        ui_layout_set_focus(workbench.layout, -1);
        /* The reference is the authored layout alone: its own animation elements
           are hidden while it is rendered, so "authored" means a glyph a text or
           control element owns, and never a cell the surface's material filled. */
        for (int e = 0; e < workbench.layout->element_count &&
                        saved_count < UI_LAYOUT_MAX_ELEMS; e++) {
            UiElement *element = workbench.layout->elements[e];
            if (!element || element->type != UI_ELE_ANIMATION) continue;
            saved_visible[saved_count++] = element->visible;
            element->visible = false;
        }
        ui_layout_render(workbench.layout, expected, palette.secondary_text, palette.canvas);
        for (int e = 0, restored = 0; e < workbench.layout->element_count &&
                                      restored < saved_count; e++) {
            UiElement *element = workbench.layout->elements[e];
            if (!element || element->type != UI_ELE_ANIMATION) continue;
            element->visible = saved_visible[restored++];
        }
        ui_canvas_copy_grid_region(reference, expected, 0, 0);
        bool cover_seen = false;
        for (int event = UI_ANIMATION_EVENT_CONTEXT_ENTER;
             event <= UI_ANIMATION_EVENT_WHILE_VISIBLE; event++) {
            bool settled_event = event == UI_ANIMATION_EVENT_WHILE_VISIBLE;
            uint32_t window = settled_event ? 0U : event_window_ms((UiAnimationEvent)event);
            for (size_t t = 0; t < sizeof(times) / sizeof(times[0]); t++) {
                bool settled = settled_event || times[t] >= (double)window;
                assert_true(ui_workbench_runtime_preview_event(canvas, actual, &workbench,
                    &palette, (UiAnimationEvent)event, times[t], false));
                /* Authored cells are the ones the layout drew a glyph into. The
                   surface's own material may pass over them only for as long as
                   the sanctioned cover lasts: inside a transition window a
                   covered cell carries the tide's own ramp, and every settled
                   frame — the looping field, and any moment after a window has
                   closed — is byte-identical to the authored layout, so no
                   control is ever left under water. */
                for (size_t i = 0; i < 260U * 160U; i++) {
                    if (!reference->touched[i] || reference->cells[i].glyph == 0U ||
                        reference->cells[i].glyph == ' ') continue;
                    if (memcmp(&actual->cells[i], &reference->cells[i],
                               sizeof(Cell)) == 0) continue;
                    if (settled) {
                        fprintf(stderr, "FAIL-PRODUCT: context %d event %d t=%.0f left "
                                "authored cell (%zu,%zu) as '%c'\n", context, event,
                                times[t], i % 260U, i / 260U, actual->cells[i].glyph);
                        assert_memory_equal(&actual->cells[i], &reference->cells[i],
                                            sizeof(Cell));
                    }
                    assert_true(tide_cover_glyph(actual->cells[i].glyph));
                    cover_seen = true;
                }
                assert_int_equal(workbench.context, context);
                assert_int_equal(workbench.element_index, 0);
            }
        }
        /* The exception has to actually happen, or the rule above proves nothing:
           opening a context really does flood the authored surface and then give
           it back. */
        assert_true(cover_seen);
    }
    ui_workbench_destroy(&workbench);
    ui_canvas_destroy(reference);
    ui_canvas_destroy(canvas);
    grid_destroy(expected);
    grid_destroy(actual);
}

static void test_overlapping_events_match_independent_composition(void **state) {
    UiWorkbench workbench;
    UiAppWorkbenchPalette palette;
    Grid *actual = grid_create(260, 160);
    Grid *expected = grid_create(260, 160);
    UiCanvas *canvas = ui_canvas_create(260, 160);
    double ages[UI_ANIMATION_EVENT_PREVIEW] = {70, -1, 20, 5, 70};
    (void)state;
    assert_non_null(actual);
    assert_non_null(expected);
    assert_non_null(canvas);
    assert_true(ui_app_theme_workbench_palette(&palette));
    ui_workbench_init(&workbench);
    for (int context = MENU_MAIN; context <= MENU_CONFIRM_QUIT; context++) {
        assert_int_equal(ui_workbench_open(&workbench, (MenuId)context), UI_WORKBENCH_OK);
        assert_true(grid_clear_region_zero(expected, 0, 0, 260, 160));
        ui_layout_set_focus(workbench.layout, -1);
        ui_layout_render(workbench.layout, expected, palette.secondary_text, palette.canvas);
        for (int event = 0; event < UI_ANIMATION_EVENT_PREVIEW; event++)
            if (ages[event] >= 0)
                assert_true(ui_animation_render_layout(workbench.layout, expected,
                    ages[event], false, false, (UiAnimationEvent)event));
        assert_true(ui_workbench_runtime_preview_events(canvas, actual, &workbench,
                                                        &palette, ages, false));
        assert_memory_equal(actual->cells, expected->cells, 260U * 160U * sizeof(Cell));
        assert_true(ui_workbench_runtime_preview_events(canvas, actual, &workbench,
                                                        &palette, ages, true));
        assert_true(grid_clear_region_zero(expected, 0, 0, 260, 160));
        ui_layout_render(workbench.layout, expected, palette.secondary_text, palette.canvas);
        assert_memory_equal(actual->cells, expected->cells, 260U * 160U * sizeof(Cell));
    }
    assert_false(ui_workbench_runtime_preview_events(canvas, actual, &workbench,
                                                     &palette, NULL, false));
    ui_workbench_destroy(&workbench);
    ui_canvas_destroy(canvas);
    grid_destroy(expected);
    grid_destroy(actual);
}

static void test_option_window_pages_without_four_option_limit(void **state) {
    size_t first, count;
    (void)state;
    assert_true(ui_workbench_chrome_option_window(19, 0, 260, &first, &count));
    assert_int_equal(first, 0);
    assert_int_equal(count, 6);
    assert_true(ui_workbench_chrome_option_window(19, 8, 260, &first, &count));
    assert_int_equal(first, 6);
    assert_int_equal(count, 6);
    assert_true(ui_workbench_chrome_option_window(19, 18, 260, &first, &count));
    assert_int_equal(first, 18);
    assert_int_equal(count, 1);
    assert_true(ui_workbench_chrome_option_window(19, 8, 130, &first, &count));
    assert_int_equal(first, 8);
    assert_int_equal(count, 4);
    assert_false(ui_workbench_chrome_option_window(0, 0, 260, &first, &count));
    assert_false(ui_workbench_chrome_option_window(2, 2, 260, &first, &count));
    assert_false(ui_workbench_chrome_option_window(2, 0, 12, &first, &count));
}

static void test_menu_cards_render_visual_content_without_mutation(void **state) {
    UiWorkbench workbench;
    UiAppWorkbenchPalette palette;
    Grid *grid = grid_create(260, 160);
    char before[UI_ELE_NAME_MAX];
    (void)state;
    assert_non_null(grid);
    ui_workbench_init(&workbench);
    assert_int_equal(ui_workbench_open(&workbench, MENU_MAIN), UI_WORKBENCH_OK);
    assert_true(ui_app_theme_workbench_palette(&palette));
    (void)snprintf(before, sizeof(before), "%s", workbench.layout->transition);
    workbench.property = UI_WORKBENCH_PROPERTY_TRANSITION;
    assert_true(ui_workbench_chrome_edit_panels(grid, &palette, &workbench, 260, 12));
    for (int card = 0; card < 6; card++) {
        bool visible = false;
        for (int y = 2; y < 9; y++)
            for (int x = card * 43 + 2; x < card * 43 + 41; x++) {
                uint8_t glyph = grid->cells[y * grid->width + x].glyph;
                if (glyph && glyph != ' ') visible = true;
            }
        assert_true(visible);
    }
    assert_string_equal(workbench.layout->transition, before);
    ui_workbench_destroy(&workbench);
    grid_destroy(grid);
}

static void test_add_cards_follow_catalog_page(void **state) {
    UiWorkbench workbench;
    UiAppWorkbenchPalette palette;
    Grid *grid = grid_create(260, 160);
    size_t first, count;
    (void)state;
    assert_non_null(grid);
    ui_workbench_init(&workbench);
    assert_int_equal(ui_workbench_open(&workbench, MENU_MAIN), UI_WORKBENCH_OK);
    assert_true(ui_app_theme_workbench_palette(&palette));
    assert_int_equal(ui_workbench_begin_add(&workbench), UI_WORKBENCH_OK);
    assert_true(workbench.catalog.count > 6);
    workbench.add_index = 6;
    assert_true(ui_workbench_chrome_option_window(workbench.catalog.count, 6, 260, &first, &count));
    assert_int_equal(first, 6);
    assert_true(count > 0);
    assert_true(ui_workbench_chrome_edit_panels(grid, &palette, &workbench, 260, 12));
    assert_int_equal(grid->cells[10 * 260 + 2].glyph, '>');
    assert_int_equal(grid->cells[10 * 260 + 4].glyph, '7');
    assert_int_equal(workbench.add_index, 6);
    ui_workbench_destroy(&workbench);
    grid_destroy(grid);
}

static void test_cards_preview_runtime_states_without_mutation(void **state) {
    UiWorkbench workbench;
    UiAppWorkbenchPalette palette;
    UiAppMenuPalette menu;
    UiElement button = {0};
    UiElement before;
    UiLayout layout = {0};
    Grid *grid = grid_create(260, 160);
    (void)state;
    assert_non_null(grid);
    ui_workbench_init(&workbench);
    assert_true(ui_app_theme_workbench_palette(&palette));
    assert_true(ui_app_theme_menu_palette(&menu));
    button.type = UI_ELE_BUTTON;
    button.visible = true;
    button.focused = true;
    button.content = "START";
    button.align = UI_ALIGN_CENTER;
    button.layout.width = 20;
    button.layout.height = 1;
    (void)snprintf(button.style, sizeof(button.style), "bracket");
    (void)snprintf(button.focus_effect, sizeof(button.focus_effect), "none");
    before = button;
    layout.elements[0] = &button;
    layout.element_count = 1;
    workbench.layout = &layout;
    workbench.elements[0] = &button;
    workbench.element_count = 1;
    workbench.editing = true;
    workbench.property = UI_WORKBENCH_PROPERTY_STYLE;
    for (int mode = 0; mode < 3; mode++) {
        bool bracket = false, arrow = false;
        int bracket_row = -1, arrow_row = -1;
        workbench.preview_state = (UiWorkbenchPreviewState)mode;
        assert_true(ui_workbench_chrome_edit_panels(grid, &palette, &workbench, 260, 12));
        for (int y = 2; y < 9; y++) for (int x = 88; x < 168; x++) {
            Cell cell = grid->cells[y * 260 + x];
            if (cell.glyph == '[') { bracket = true; bracket_row = y; }
            if (cell.glyph == '>') {
                arrow = true;
                arrow_row = y;
                assert_memory_equal(&cell.fg, &menu.selected_foreground, sizeof(cell.fg));
                assert_memory_equal(&cell.bg, &menu.selected_background, sizeof(cell.bg));
            }
        }
        assert_int_equal(bracket, mode != UI_WORKBENCH_PREVIEW_FOCUSED);
        assert_int_equal(arrow, mode != UI_WORKBENCH_PREVIEW_NORMAL);
        if (mode == UI_WORKBENCH_PREVIEW_COMPARE) assert_true(bracket_row < arrow_row);
        assert_memory_equal(&button, &before, sizeof(button));
    }
    grid_destroy(grid);
}

static int find_card_label(const Grid *grid, int card, int width, const char *text) {
    size_t length = strlen(text);
    for (int y = 2; y < 9; y++) {
        for (int x = card * width + 2; x + (int)length < (card + 1) * width - 2; x++) {
            size_t i = 0;
            while (i < length && grid->cells[y * grid->width + x + (int)i].glyph == (uint8_t)text[i]) i++;
            if (i == length) return x - card * width;
        }
    }
    return -1;
}

static void test_element_category_samples_preserve_source(void **state) {
    UiWorkbench workbench;
    UiAppWorkbenchPalette palette;
    UiElement element = {0}, before;
    UiLayout layout = {0};
    Grid *grid = grid_create(260, 160);
    int positions[3];
    (void)state;
    assert_non_null(grid);
    assert_true(ui_app_theme_workbench_palette(&palette));
    ui_workbench_init(&workbench);
    element.type = UI_ELE_BUTTON;
    element.visible = true;
    element.content = "START";
    element.layout.width = 20;
    element.layout.height = 1;
    (void)snprintf(element.style, sizeof(element.style), "plain");
    before = element;
    layout.elements[0] = &element;
    layout.element_count = 1;
    workbench.layout = &layout;
    workbench.elements[0] = &element;
    workbench.element_count = 1;
    workbench.editing = true;
    workbench.preview_state = UI_WORKBENCH_PREVIEW_NORMAL;
    workbench.property = UI_WORKBENCH_PROPERTY_ALIGN;
    assert_true(ui_workbench_chrome_edit_panels(grid, &palette, &workbench, 260, 12));
    for (int card = 0; card < 3; card++) positions[card] = find_card_label(grid, card, 86, "START");
    assert_true(positions[0] >= 0);
    assert_true(positions[0] < positions[1]);
    assert_true(positions[1] < positions[2]);
    assert_memory_equal(&element, &before, sizeof(element));
    for (int remove = 0; remove < 2; remove++) {
        workbench.property = remove ? UI_WORKBENCH_PROPERTY_REMOVE : UI_WORKBENCH_PROPERTY_VISIBLE;
        for (int choice = 0; choice < 2; choice++) {
            workbench.remove_choice = choice != 0;
            assert_true(ui_workbench_chrome_edit_panels(grid, &palette, &workbench, 260, 12));
            assert_true(find_card_label(grid, 0, 130, "START") >= 0);
            assert_int_equal(find_card_label(grid, 1, 130, "START"), -1);
            assert_int_equal(layout.element_count, 1);
            assert_memory_equal(&element, &before, sizeof(element));
        }
    }
    workbench.property = UI_WORKBENCH_PROPERTY_CONTENT;
    assert_int_equal(ui_workbench_begin_text(&workbench), UI_WORKBENCH_OK);
    assert_int_equal(ui_workbench_text_input(&workbench, "!", false), UI_WORKBENCH_OK);
    assert_true(ui_workbench_chrome_edit_panels(grid, &palette, &workbench, 260, 12));
    assert_true(find_card_label(grid, 0, 260, "START!") >= 0);
    assert_string_equal(element.content, "START");
    assert_memory_equal(&element, &before, sizeof(element));
    ui_workbench_cancel_mode(&workbench);
    assert_true(ui_workbench_chrome_edit_panels(grid, &palette, &workbench, 260, 12));
    assert_int_equal(find_card_label(grid, 0, 260, "START!"), -1);
    grid_destroy(grid);
}

static void test_scoped_row_labels_colors_and_highlight(void **state) {
    static const int scales[] = {100, 125, 150, 200};
    static const char *const labels[] = {"MENU:", "Transition", "Add", "ELEMENT:",
        "Style", "Text", "Visible", "Align", "Remove"};
    UiWorkbench workbench;
    UiLayout layout = {0};
    UiElement element = {0};
    UiAppWorkbenchPalette palette;
    Grid *grid = grid_create(260, 160);
    (void)state;
    assert_non_null(grid);
    assert_true(ui_app_theme_workbench_palette(&palette));
    ui_workbench_init(&workbench);
    element.type = UI_ELE_BUTTON;
    layout.elements[0] = &element;
    layout.element_count = 1;
    workbench.layout = &layout;
    workbench.elements[0] = &element;
    workbench.element_count = 1;
    for (int editing = 0; editing < 2; editing++) {
        workbench.editing = editing != 0;
        workbench.property = editing ? UI_WORKBENCH_PROPERTY_STYLE : UI_WORKBENCH_PROPERTY_TRANSITION;
        for (size_t scale = 0; scale < sizeof(scales) / sizeof(scales[0]); scale++) {
            int width, height;
            char row[131];
            UiCanvas *canvas;
            assert_true(ui_workbench_runtime_interface_size(scales[scale], workbench.editing, &width, &height));
            canvas = ui_canvas_create(width, height);
            assert_non_null(canvas);
            assert_true(ui_workbench_runtime_compose_footer_canvas(canvas, grid, &workbench,
                &palette, NULL, scales[scale], false));
            int offset = ((260 + width - 1) / width) * width;
            for (int i = 0; i < 130; i++) row[i] = (char)canvas->cells[offset + i].glyph;
            row[130] = '\0';
            assert_non_null(strstr(row, editing ? "[Style]" : "[Transition]"));
            assert_null(strstr(row, editing ? "[Transition]" : "[Style]"));
            for (size_t i = 0; i < sizeof(labels) / sizeof(labels[0]); i++) {
                const char *found = strstr(row, labels[i]);
                SDL_Color expected = ((i < 3) != workbench.editing) ? palette.primary_text : palette.disabled_text;
                assert_non_null(found);
                for (size_t j = 0; j < strlen(labels[i]); j++)
                    assert_memory_equal(&canvas->cells[offset + (found - row) + j].fg, &expected, sizeof(expected));
            }
            /* Exact labels above plus absence checks protect against inspector creep. */
            assert_null(strstr(row, "Position"));
            assert_null(strstr(row, "Size"));
            assert_null(strstr(row, "Parent"));
            assert_null(strstr(row, "Coords"));
            ui_canvas_destroy(canvas);
        }
    }
    grid_destroy(grid);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_scoped_row_labels_colors_and_highlight),
        cmocka_unit_test(test_element_category_samples_preserve_source),
        cmocka_unit_test(test_cards_preview_runtime_states_without_mutation),
        cmocka_unit_test(test_rejects_invalid_arguments),
        cmocka_unit_test(test_option_window_pages_without_four_option_limit),
        cmocka_unit_test(test_menu_cards_render_visual_content_without_mutation),
        cmocka_unit_test(test_add_cards_follow_catalog_page),
        cmocka_unit_test(test_overlapping_events_match_independent_composition),
        cmocka_unit_test(test_exit_overlay_preserves_incoming_and_endpoints),
        cmocka_unit_test(test_lifecycle_keeps_authored_cells_stable),
        cmocka_unit_test(test_runtime_lifecycle_reduced_matches_static),
        cmocka_unit_test(test_runtime_pixel_fixtures),
        cmocka_unit_test(test_edit_panels_progressive_and_scaled),
        cmocka_unit_test(test_runtime_preview_independent_rendering),
        cmocka_unit_test(test_runtime_snapshot_scale_isolation),
        cmocka_unit_test(test_wrapped_guidance_all_scales_and_modes),
        cmocka_unit_test(test_all_role_colors_come_from_tokens),
        cmocka_unit_test(test_panels_use_token_borders_and_focus),
        cmocka_unit_test(test_scaled_preview_keeps_authored_cells),
        cmocka_unit_test(test_tooltip_text_sits_beside_status),
        cmocka_unit_test(test_runtime_footer_canvas_contains_scaled_footer),
        cmocka_unit_test(test_runtime_layers_use_independent_scales),
        cmocka_unit_test(test_runtime_workbench_scale_steps_independently),
        cmocka_unit_test(test_footer_rows_render_identity_controls_diagnostics)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}