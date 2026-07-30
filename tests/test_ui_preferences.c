#define _POSIX_C_SOURCE 200809L

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "../src/ui_preferences.h"

static char root[] = "/tmp/tsg_ui_preferences_XXXXXX";

static void path_for(char *out, size_t out_size, const char *name) {
    snprintf(out, out_size, "%s/%s", root, name);
}

static void write_text(const char *path, const char *text) {
    FILE *file = fopen(path, "wb");
    assert_non_null(file);
    assert_true(fputs(text, file) >= 0);
    assert_int_equal(fclose(file), 0);
}

static int setup(void **state) {
    (void)state;
    memcpy(root, "/tmp/tsg_ui_preferences_XXXXXX",
           sizeof("/tmp/tsg_ui_preferences_XXXXXX"));
    return mkdtemp(root) ? 0 : -1;
}

static int teardown(void **state) {
    char path[512];
    (void)state;
    path_for(path, sizeof(path), "default.ini"); remove(path);
    path_for(path, sizeof(path), "user.ini"); remove(path);
    path_for(path, sizeof(path), "blocked"); rmdir(path);
    return rmdir(root);
}

static void test_precedence_and_missing_user(void **state) {
    UiPreferences preferences;
    char defaults[512];
    char user[512];
    (void)state;
    path_for(defaults, sizeof(defaults), "default.ini");
    path_for(user, sizeof(user), "user.ini");
    write_text(defaults, "version = 1\nui_scale_percent = 125\n");

    ui_preferences_init(&preferences, defaults, user);
    assert_int_equal(preferences.default_load_result, UI_PREFERENCES_IO_OK);
    assert_int_equal(preferences.user_load_result, UI_PREFERENCES_IO_MISSING);
    assert_int_equal(ui_preferences_scale(&preferences), 125);
    assert_int_equal(access(user, F_OK), -1);

    write_text(user, "version = 1\nui_scale_percent = 200\n");
    ui_preferences_init(&preferences, defaults, user);
    assert_int_equal(ui_preferences_scale(&preferences), 200);
    assert_int_equal(ui_preferences_default_scale(&preferences), 125);
}

static void test_invalid_files_are_transactionally_rejected(void **state) {
    static const char *invalid[] = {
        "version = 2\nui_scale_percent = 150\n",
        "version = 1\nversion = 1\nui_scale_percent = 150\n",
        "version = 1\n",
        "version = one\nui_scale_percent = 150\n",
        "version = 1\nui_scale_percent = 175\n",
        "version = 1\nunknown = 3\nui_scale_percent = 150\n"
    };
    UiPreferences preferences;
    char defaults[512];
    char user[512];
    size_t i;
    (void)state;
    path_for(defaults, sizeof(defaults), "default.ini");
    path_for(user, sizeof(user), "user.ini");
    write_text(defaults, "version = 1\nui_scale_percent = 100\n");
    for (i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
        write_text(user, invalid[i]);
        ui_preferences_init(&preferences, defaults, user);
        assert_int_equal(preferences.user_load_result, UI_PREFERENCES_IO_INVALID);
        assert_int_equal(ui_preferences_scale(&preferences), 100);
    }
    write_text(defaults, "version = 1\nui_scale_percent = 100\nxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\n");
    ui_preferences_init(&preferences, defaults, "/missing/user.ini");
    assert_int_equal(preferences.default_load_result, UI_PREFERENCES_IO_INVALID);
    assert_int_equal(ui_preferences_scale(&preferences), 150);
}

static void test_transitions_reset_and_endpoint_no_write(void **state) {
    UiPreferences preferences;
    char defaults[512];
    char user[512];
    struct stat before;
    struct stat after;
    (void)state;
    path_for(defaults, sizeof(defaults), "default.ini");
    path_for(user, sizeof(user), "user.ini");
    write_text(defaults, "version = 1\nui_scale_percent = 125\n");
    ui_preferences_init(&preferences, defaults, user);
    assert_int_equal(ui_preferences_increase(&preferences), UI_PREFERENCES_CHANGE_SAVED);
    assert_int_equal(ui_preferences_scale(&preferences), 150);
    assert_int_equal(ui_preferences_increase(&preferences), UI_PREFERENCES_CHANGE_SAVED);
    assert_int_equal(ui_preferences_scale(&preferences), 200);
    assert_int_equal(stat(user, &before), 0);
    assert_int_equal(ui_preferences_increase(&preferences), UI_PREFERENCES_CHANGE_UNCHANGED);
    assert_int_equal(stat(user, &after), 0);
    assert_int_equal(before.st_mtim.tv_sec, after.st_mtim.tv_sec);
    assert_int_equal(before.st_mtim.tv_nsec, after.st_mtim.tv_nsec);
    assert_int_equal(ui_preferences_decrease(&preferences), UI_PREFERENCES_CHANGE_SAVED);
    assert_int_equal(ui_preferences_reset(&preferences), UI_PREFERENCES_CHANGE_SAVED);
    assert_int_equal(ui_preferences_scale(&preferences), 125);
}

static void test_failed_save_keeps_active_and_destination(void **state) {
    UiPreferences preferences;
    char defaults[512];
    char blocked[512];
    (void)state;
    path_for(defaults, sizeof(defaults), "default.ini");
    path_for(blocked, sizeof(blocked), "blocked");
    write_text(defaults, "version = 1\nui_scale_percent = 150\n");
    assert_int_equal(mkdir(blocked, 0700), 0);
    ui_preferences_init(&preferences, defaults, blocked);
    assert_int_equal(ui_preferences_increase(&preferences),
                     UI_PREFERENCES_CHANGE_ACTIVE_NOT_SAVED);
    assert_int_equal(ui_preferences_scale(&preferences), 200);
    assert_int_equal(access(blocked, F_OK), 0);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_precedence_and_missing_user, setup, teardown),
        cmocka_unit_test_setup_teardown(test_invalid_files_are_transactionally_rejected, setup, teardown),
        cmocka_unit_test_setup_teardown(test_transitions_reset_and_endpoint_no_write, setup, teardown),
        cmocka_unit_test_setup_teardown(test_failed_save_keeps_active_and_destination, setup, teardown),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}