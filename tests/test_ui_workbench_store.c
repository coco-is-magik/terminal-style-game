#define _POSIX_C_SOURCE 200809L

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

#include <cmocka.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../src/ui_ele.h"
#include "../src/ui_workbench_store.h"

static void write_text(const char *path, const char *text) {
    FILE *file = fopen(path, "wb");
    assert_non_null(file);
    assert_true(fputs(text, file) >= 0);
    assert_int_equal(fclose(file), 0);
}

static void test_legacy_defaults_and_valid_metadata(void **state) {
    char legacy[] = "build/tsg_ui_workbench_legacy_XXXXXX";
    char metadata[] = "build/tsg_ui_workbench_metadata_XXXXXX";
    int descriptor;
    UiElement *element;
    (void)state;
    descriptor = mkstemp(legacy);
    assert_true(descriptor >= 0);
    assert_int_equal(close(descriptor), 0);
    write_text(legacy, "name=legacy\ntype=button\nx=1\ny=2\nwidth=10\nheight=1\n");
    element = ui_ele_load(legacy, NULL);
    assert_non_null(element);
    assert_string_equal(element->style, "plain");
    assert_string_equal(element->transition, "none");
    assert_string_equal(element->focus_effect, "none");
    ui_ele_destroy(element);
    assert_int_equal(unlink(legacy), 0);

    descriptor = mkstemp(metadata);
    assert_true(descriptor >= 0);
    assert_int_equal(close(descriptor), 0);
    write_text(metadata,
               "name=styled\ntype=button\nstyle=bracket\n"
               "transition=perimeter_burst\nfocus_effect=focus_glitch\n");
    element = ui_ele_load(metadata, NULL);
    assert_non_null(element);
    assert_string_equal(element->style, "bracket");
    assert_string_equal(element->transition, "perimeter_burst");
    assert_string_equal(element->focus_effect, "focus_glitch");
    ui_ele_destroy(element);
    assert_int_equal(unlink(metadata), 0);
}

static void test_invalid_presets_are_rejected(void **state) {
    char path[] = "build/tsg_ui_workbench_invalid_XXXXXX";
    int descriptor;
    (void)state;
    descriptor = mkstemp(path);
    assert_true(descriptor >= 0);
    assert_int_equal(close(descriptor), 0);
    write_text(path, "name=bad\ntype=text\nstyle=bracket\n");
    assert_null(ui_ele_load(path, NULL));
    write_text(path, "name=bad\ntype=button\ntransition=unknown\n");
    assert_null(ui_ele_load(path, NULL));
    write_text(path, "name=bad\ntype=button\nfocus_effect=unknown\n");
    assert_null(ui_ele_load(path, NULL));
    assert_int_equal(unlink(path), 0);
}

static void test_atomic_store_round_trip(void **state) {
    char path[] = "build/tsg_ui_workbench_store_XXXXXX";
    int descriptor;
    UiElement *element;
    UiElement *reloaded;
    UiWorkbenchStoreResult result;
    (void)state;
    descriptor = mkstemp(path);
    assert_true(descriptor >= 0);
    assert_int_equal(close(descriptor), 0);
    write_text(path,
               "name=item\ntype=button\nparent=container\nx=2\ny=3\n"
               "coords=relative\nwidth=20\nheight=1\nalign=center\n"
               "action=start_game\ncontent=START\n");
    element = ui_ele_load(path, NULL);
    assert_non_null(element);
    element->layout.x = 7;
    element->layout.y = 9;
    (void)snprintf(element->style, sizeof(element->style), "inverse");
    (void)snprintf(element->transition, sizeof(element->transition), "center_out");
    (void)snprintf(element->focus_effect, sizeof(element->focus_effect), "focus_pulse");
    result = ui_workbench_store_element(element, path);
    assert_true(result == UI_WORKBENCH_STORE_OK ||
                result == UI_WORKBENCH_STORE_OK_DURABILITY_WARNING);
    reloaded = ui_ele_load(path, NULL);
    assert_non_null(reloaded);
    assert_int_equal(reloaded->layout.x, 7);
    assert_int_equal(reloaded->layout.y, 9);
    assert_string_equal(reloaded->parent_name, "container");
    assert_string_equal(reloaded->style, "inverse");
    assert_string_equal(reloaded->transition, "center_out");
    assert_string_equal(reloaded->focus_effect, "focus_pulse");
    assert_string_equal(reloaded->action, "start_game");
    assert_string_equal(reloaded->content, "START");
    ui_ele_destroy(reloaded);
    ui_ele_destroy(element);
    assert_int_equal(unlink(path), 0);
}

static void test_invalid_store_preserves_destination(void **state) {
    char path[] = "build/tsg_ui_workbench_preserve_XXXXXX";
    int descriptor;
    UiElement element = {0};
    char bytes[64];
    FILE *file;
    (void)state;
    descriptor = mkstemp(path);
    assert_true(descriptor >= 0);
    assert_int_equal(close(descriptor), 0);
    write_text(path, "original\n");
    assert_int_equal(ui_workbench_store_element(&element, path),
                     UI_WORKBENCH_STORE_INVALID_ELEMENT);
    file = fopen(path, "rb");
    assert_non_null(file);
    assert_non_null(fgets(bytes, sizeof(bytes), file));
    assert_int_equal(fclose(file), 0);
    assert_string_equal(bytes, "original\n");
    (void)snprintf(element.name, sizeof(element.name), "same");
    (void)snprintf(element.parent_name, sizeof(element.parent_name), "same");
    element.type = UI_ELE_TEXT;
    element.visible = 1;
    (void)snprintf(element.style, sizeof(element.style), "plain");
    (void)snprintf(element.transition, sizeof(element.transition), "none");
    (void)snprintf(element.focus_effect, sizeof(element.focus_effect), "none");
    assert_int_equal(ui_workbench_store_element(&element, path),
                     UI_WORKBENCH_STORE_INVALID_ELEMENT);
    assert_int_equal(unlink(path), 0);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_legacy_defaults_and_valid_metadata),
        cmocka_unit_test(test_invalid_presets_are_rejected),
        cmocka_unit_test(test_atomic_store_round_trip),
        cmocka_unit_test(test_invalid_store_preserves_destination)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}