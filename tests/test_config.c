#define _POSIX_C_SOURCE 200809L

#include <setjmp.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <cmocka.h>

#include "../src/config.h"

static char path[] = "build/tsg_config_XXXXXX";

static void write_config(const char *text) {
    int descriptor = mkstemp(path);
    FILE *file;
    assert_true(descriptor >= 0);
    file = fdopen(descriptor, "w");
    assert_non_null(file);
    assert_true(fputs(text, file) >= 0);
    assert_int_equal(fclose(file), 0);
}

static void reset_path(void) {
    memcpy(path, "build/tsg_config_XXXXXX", sizeof("build/tsg_config_XXXXXX"));
}

static void test_defaults_and_valid_override(void **state) {
    (void)state;
    config_init_defaults();
    assert_int_equal(config_get()->grid_width, 260);
    write_config("grid_width = 80\nambient_light = 0.35\n"
                 "debug_display_enabled = 0\nunknown_key = retained_compatibility\n");
    assert_true(config_load_from_file(path));
    assert_int_equal(config_get()->grid_width, 80);
    assert_float_equal(config_get()->ambient_light, 0.35, 0.000001);
    assert_false(config_get()->debug_display_enabled);
    assert_int_equal(remove(path), 0);
    reset_path();
}

static void test_invalid_and_overlong_files_are_transactional(void **state) {
    EngineConfig before;
    char long_line[300];
    (void)state;
    config_init_defaults();
    before = *config_get();
    write_config("grid_width = 80\ncell_width = 16\n");
    assert_false(config_load_from_file(path));
    assert_memory_equal(config_get(), &before, sizeof(before));
    assert_int_equal(remove(path), 0);
    reset_path();

    memset(long_line, 'x', sizeof(long_line));
    long_line[sizeof(long_line) - 2U] = '\n';
    long_line[sizeof(long_line) - 1U] = '\0';
    write_config(long_line);
    assert_false(config_load_from_file(path));
    assert_memory_equal(config_get(), &before, sizeof(before));
    assert_int_equal(remove(path), 0);
    reset_path();
}

static void test_missing_and_invalid_set_preserve_active_config(void **state) {
    EngineConfig before;
    EngineConfig invalid;
    (void)state;
    config_init_defaults();
    before = *config_get();
    assert_false(config_load_from_file("/tmp/tsg_config_missing_file"));
    assert_memory_equal(config_get(), &before, sizeof(before));
    invalid = before;
    invalid.ambient_light = 2.0;
    assert_false(config_set(&invalid));
    assert_memory_equal(config_get(), &before, sizeof(before));
    assert_false(config_set(NULL));
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_defaults_and_valid_override),
        cmocka_unit_test(test_invalid_and_overlong_files_are_transactional),
        cmocka_unit_test(test_missing_and_invalid_set_preserve_active_config)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
