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
    const int controls_y = 2;
    (void)state;
    ui_workbench_init(&workbench);
    grid = grid_create(UI_WORKBENCH_CHROME_COLUMNS, UI_WORKBENCH_CHROME_ROWS);
    assert_non_null(grid);
    assert_true(ui_workbench_runtime_footer_size(200, &width, &height));
    assert_int_equal(width, 130);
    assert_int_equal(height, 8);
    canvas = ui_canvas_create(width, height);
    assert_non_null(canvas);
    assert_true(ui_app_theme_workbench_palette(&palette));
    assert_int_equal(ui_workbench_open(&workbench, MENU_MAIN),
                     UI_WORKBENCH_OK);
    assert_true(ui_workbench_runtime_compose_footer_canvas(
        canvas, grid, &workbench, &palette, tooltip, 200, false));
    assert_true(ui_canvas_is_touched(canvas, 1, controls_y));
    assert_int_equal(canvas->cells[(size_t)controls_y *
                                   (size_t)canvas->width + 1U].glyph,
                     'U');
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
    static const uint64_t expected[] = {
        UINT64_C(4283456333583077511), UINT64_C(6485980519635347163),
        UINT64_C(5506666399887804633), UINT64_C(5100523525012112801)
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
        assert_int_equal(ui_workbench_open(&workbench, (MenuId)context), UI_WORKBENCH_OK);
        assert_true(grid_clear_region_zero(expected, 0, 0, 260, 160));
        ui_layout_set_focus(workbench.layout, -1);
        ui_layout_render(workbench.layout, expected, palette.secondary_text, palette.canvas);
        ui_canvas_copy_grid_region(reference, expected, 0, 0);
        for (int event = UI_ANIMATION_EVENT_CONTEXT_ENTER;
             event <= UI_ANIMATION_EVENT_WHILE_VISIBLE; event++) {
            for (size_t t = 0; t < sizeof(times) / sizeof(times[0]); t++) {
                assert_true(ui_workbench_runtime_preview_event(canvas, actual, &workbench,
                    &palette, (UiAnimationEvent)event, times[t], false));
                for (size_t i = 0; i < 260U * 160U; i++)
                    if (reference->touched[i])
                        assert_memory_equal(&actual->cells[i], &reference->cells[i], sizeof(Cell));
                assert_int_equal(workbench.context, context);
                assert_int_equal(workbench.element_index, 0);
            }
        }
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
    for (int card = 0; card < 4; card++) {
        bool visible = false;
        for (int y = 2; y < 9; y++)
            for (int x = card * 65 + 2; x < card * 65 + 63; x++) {
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

int main(void) {
    const struct CMUnitTest tests[] = {
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