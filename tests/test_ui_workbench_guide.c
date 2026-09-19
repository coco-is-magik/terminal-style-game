#define _POSIX_C_SOURCE 200809L

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

#include <cmocka.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../src/checked_size.h"
#include "../src/grid.h"
#include "../src/map_catalog.h"
#include "../src/menu_state.h"
#include "../src/ui_app_theme_adapter.h"
#include "../src/ui_canvas.h"
#include "../src/ui_ele.h"
#include "../src/ui_theme.h"
#include "../src/ui_workbench.h"
#include "../src/ui_workbench_chrome.h"
#include "../src/ui_workbench_frame.h"
#include "../src/ui_workbench_guide.h"
#include "../src/ui_workbench_store.h"

static bool open_fixture(UiWorkbench *workbench, MenuId context) {
    return workbench && ui_workbench_open(workbench, context) == UI_WORKBENCH_OK;
}

static void test_rejects_invalid_arguments(void **state) {
    (void)state;
    assert_string_equal(ui_workbench_guide_result_string(UI_WORKBENCH_GUIDE_OK),
                        "ok");
    assert_string_equal(
        ui_workbench_guide_result_string(UI_WORKBENCH_GUIDE_INVALID_ARGUMENT),
        "invalid argument");
    assert_string_equal(
        ui_workbench_guide_result_string(UI_WORKBENCH_GUIDE_LOAD_FAILED),
        "load failed");
    assert_int_equal(ui_workbench_guide_load(NULL, "assets/editor_tooltips.txt"),
                     UI_WORKBENCH_GUIDE_INVALID_ARGUMENT);
    assert_null(ui_workbench_guide_tooltip(NULL, NULL));
    assert_false(ui_workbench_guide_footer_stable(NULL, 260, 160));
}

static void test_tooltip_text_covers_property_action_and_mode(void **state) {
    UiWorkbench workbench;
    UiWorkbenchGuide guide;
    static const UiWorkbenchProperty properties[] = {
        UI_WORKBENCH_PROPERTY_PARENT, UI_WORKBENCH_PROPERTY_STYLE,
        UI_WORKBENCH_PROPERTY_TRANSITION, UI_WORKBENCH_PROPERTY_FOCUS_EFFECT,
        UI_WORKBENCH_PROPERTY_PRESET, UI_WORKBENCH_PROPERTY_TARGET,
        UI_WORKBENCH_PROPERTY_TRIGGER, UI_WORKBENCH_PROPERTY_ORIENTATION,
        UI_WORKBENCH_PROPERTY_LOOP, UI_WORKBENCH_PROPERTY_RANDOMIZE,
        UI_WORKBENCH_PROPERTY_WIDTH, UI_WORKBENCH_PROPERTY_HEIGHT
    };
    size_t i;
    (void)state;
    ui_workbench_init(&workbench);
    assert_true(open_fixture(&workbench, MENU_MAIN));
    assert_int_equal(ui_workbench_guide_load(&guide,
                                             "assets/editor_tooltips.txt"),
                     UI_WORKBENCH_GUIDE_OK);
    assert_int_equal(ui_workbench_guide_missing_count(&guide, &workbench), 0U);
    workbench.editing = true;
    for (i = 0U; i < sizeof(properties) / sizeof(properties[0]); i++) {
        const char *text;
        workbench.mode = UI_WORKBENCH_MODE_BROWSE;
        workbench.property = properties[i];
        text = ui_workbench_guide_tooltip(&guide, &workbench);
        assert_non_null(text);
        assert_true(text[0] != '\0');
    }
    workbench.mode = UI_WORKBENCH_MODE_ADD;
    assert_non_null(ui_workbench_guide_tooltip(&guide, &workbench));
    workbench.mode = UI_WORKBENCH_MODE_REMOVE_CONFIRM;
    assert_non_null(ui_workbench_guide_tooltip(&guide, &workbench));
    workbench.mode = UI_WORKBENCH_MODE_BROWSE;
    workbench.editing = false;
    assert_non_null(ui_workbench_guide_tooltip(&guide, &workbench));
    ui_workbench_guide_destroy(&guide);
    ui_workbench_destroy(&workbench);
}

static void test_tooltips_stay_inside_footer_at_all_scales(void **state) {
    UiWorkbench workbench;
    Grid *grid;
    UiAppWorkbenchPalette palette;
    static const int scales[] = {100, 125, 150, 200};
    static const MenuId contexts[] = {MENU_MAIN, MENU_PAUSE, MENU_SETTINGS,
                                      MENU_CONFIRM_QUIT};
    size_t context_index;
    size_t scale_index;
    (void)state;
    ui_workbench_init(&workbench);
    grid = grid_create(260, 160);
    assert_non_null(grid);
    assert_true(ui_app_theme_workbench_palette(&palette));
    for (context_index = 0U;
         context_index < sizeof(contexts) / sizeof(contexts[0U]);
         context_index++) {
        assert_true(open_fixture(&workbench, contexts[context_index]));
        for (scale_index = 0U;
             scale_index < sizeof(scales) / sizeof(scales[0U]);
             scale_index++) {
            UiWorkbenchFrameInput input;
            input.grid = grid;
            input.workbench = &workbench;
            input.palette = &palette;
            input.tooltip_text = NULL;
            input.scale_percent = scales[scale_index];
            input.elapsed_ms = 0.0;
            input.reduced_motion = false;
            input.pointer_row = 0;
            input.pointer_column = 0;
            input.pointer_active = false;
            assert_true(ui_workbench_frame_render(input));
            assert_true(ui_workbench_guide_footer_stable(grid, 260, 160));
        }
    }
    grid_destroy(grid);
    ui_workbench_destroy(&workbench);
}

static void test_malformed_guidance_is_rejected(void **state) {
    static const char *const invalid[] = {"same=one\nsame=two\n", "missing divider\n", "empty=\n"};
    size_t i;
    (void)state;
    for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
        char path[] = "build/tsg_guide_invalid_XXXXXX";
        int fd = mkstemp(path);
        FILE *file;
        UiWorkbenchGuide guide;
        assert_true(fd >= 0);
        file = fdopen(fd, "w");
        assert_non_null(file);
        assert_true(fputs(invalid[i], file) >= 0);
        assert_int_equal(fclose(file), 0);
        assert_int_equal(ui_workbench_guide_load(&guide, path), UI_WORKBENCH_GUIDE_LOAD_FAILED);
        assert_false(guide.loaded);
        assert_int_equal(guide.count, 0);
        assert_int_equal(unlink(path), 0);
    }
}

static void test_help_pages_preserve_edit(void **state) {
    UiWorkbench workbench;
    UiWorkbenchGuide guide;
    UiElement *selected;
    size_t page;
    (void)state;
    ui_workbench_init(&workbench);
    assert_int_equal(ui_workbench_open_help(NULL), UI_WORKBENCH_INVALID_ARGUMENT);
    assert_true(open_fixture(&workbench, MENU_MAIN));
    selected = ui_workbench_current_element(&workbench);
    workbench.editing = true;
    assert_int_equal(ui_workbench_guide_load(&guide, "assets/editor_tooltips.txt"),
                     UI_WORKBENCH_GUIDE_OK);
    assert_int_equal(ui_workbench_open_help(&workbench), UI_WORKBENCH_OK);
    for (page = 0; page < 5; page++) {
        assert_int_equal(workbench.help_page, page);
        assert_non_null(ui_workbench_guide_tooltip(&guide, &workbench));
        ui_workbench_cycle_help(&workbench, 1);
    }
    assert_int_equal(workbench.help_page, 0);
    ui_workbench_cycle_help(&workbench, -1);
    assert_int_equal(workbench.help_page, 4);
    ui_workbench_cancel_mode(&workbench);
    assert_true(workbench.editing);
    assert_ptr_equal(ui_workbench_current_element(&workbench), selected);
    ui_workbench_guide_destroy(&guide);
    ui_workbench_destroy(&workbench);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_rejects_invalid_arguments),
        cmocka_unit_test(test_malformed_guidance_is_rejected),
        cmocka_unit_test(test_help_pages_preserve_edit),
        cmocka_unit_test(test_tooltip_text_covers_property_action_and_mode),
        cmocka_unit_test(test_tooltips_stay_inside_footer_at_all_scales)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}