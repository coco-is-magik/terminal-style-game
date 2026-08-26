#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <cmocka.h>

#include "../src/assets.h"
#include "../src/r9_optical_semantics.h"
#include "../src/scene_types.h"

#ifndef R9_OPTICAL_RESEARCH
#error "P1 focused runner requires R9_OPTICAL_RESEARCH=1"
#endif

static void test_legacy_defaults_are_explicit(void **state) {
    R9OpticalResolved wall = r9_optical_legacy_semantics(true);
    R9OpticalResolved empty = r9_optical_legacy_semantics(false);
    (void)state;
    assert_true(wall.player_blocks);
    assert_true(wall.ray_blocks);
    assert_true(wall.light_blocks);
    assert_int_equal(wall.opacity, UINT8_MAX);
    assert_int_equal(wall.transmission, 0U);
    assert_int_equal(wall.reflectivity, 0U);
    assert_false(empty.player_blocks);
    assert_false(empty.ray_blocks);
    assert_false(empty.light_blocks);
    assert_int_equal(empty.opacity, 0U);
    assert_int_equal(empty.transmission, UINT8_MAX);
    assert_int_equal(empty.reflectivity, 0U);
}

static void test_zero_extension_preserves_legacy_exactly(void **state) {
    R9OpticalResolved legacy = r9_optical_legacy_semantics(true);
    R9OpticalResolved resolved = {0};
    R9OpticalExtension extension = {0};
    (void)state;
    assert_true(r9_optical_resolve(&legacy, &extension, &resolved));
    assert_memory_equal(&resolved, &legacy, sizeof(legacy));
}

static void test_semantic_channels_are_independent(void **state) {
    R9OpticalResolved legacy = r9_optical_legacy_semantics(true);
    R9OpticalResolved resolved = {0};
    R9OpticalExtension extension = {
        R9_OPTICAL_OVERRIDE_PLAYER_BLOCKS |
            R9_OPTICAL_OVERRIDE_RAY_BLOCKS |
            R9_OPTICAL_OVERRIDE_LIGHT_BLOCKS |
            R9_OPTICAL_OVERRIDE_OPACITY |
            R9_OPTICAL_OVERRIDE_TRANSMISSION |
            R9_OPTICAL_OVERRIDE_REFLECTIVITY,
        1U, 0U, 1U, 96U, 180U, 64U, 0U
    };
    (void)state;
    assert_true(r9_optical_resolve(&legacy, &extension, &resolved));
    assert_true(resolved.player_blocks);
    assert_false(resolved.ray_blocks);
    assert_true(resolved.light_blocks);
    assert_int_equal(resolved.opacity, 96U);
    assert_int_equal(resolved.transmission, 180U);
    assert_int_equal(resolved.reflectivity, 64U);

    extension.override_mask = R9_OPTICAL_OVERRIDE_RAY_BLOCKS;
    extension.ray_blocks = 0U;
    assert_true(r9_optical_resolve(&legacy, &extension, &resolved));
    assert_true(resolved.player_blocks);
    assert_false(resolved.ray_blocks);
    assert_true(resolved.light_blocks);
    assert_int_equal(resolved.opacity, UINT8_MAX);
}

static void test_invalid_extensions_are_rejected_transactionally(void **state) {
    R9OpticalResolved legacy = r9_optical_legacy_semantics(true);
    R9OpticalResolved sentinel = {false, false, false, 3U, 4U, 5U};
    R9OpticalResolved output = sentinel;
    R9OpticalExtension extension = {0};
    (void)state;
    extension.override_mask = UINT8_C(0x80);
    assert_false(r9_optical_resolve(&legacy, &extension, &output));
    assert_memory_equal(&output, &sentinel, sizeof(output));
    extension = (R9OpticalExtension){R9_OPTICAL_OVERRIDE_PLAYER_BLOCKS,
                                     2U, 0U, 0U, 0U, 0U, 0U, 0U};
    assert_false(r9_optical_resolve(&legacy, &extension, &output));
    assert_memory_equal(&output, &sentinel, sizeof(output));
    extension = (R9OpticalExtension){0};
    extension.reserved = 1U;
    assert_false(r9_optical_extension_is_valid(&extension));
    assert_false(r9_optical_resolve(NULL, &extension, &output));
    assert_false(r9_optical_resolve(&legacy, &extension, NULL));
}

static void test_exact_memory_costs_and_break_even(void **state) {
    R9OpticalMemoryReport report;
    (void)state;
    assert_true(r9_optical_memory_report(
        SCENE_MAX_WIDTH, SCENE_MAX_HEIGHT, ASSET_ID_CAPACITY, 1024U, &report));
    assert_int_equal(report.cell_count, 131072U);
    assert_int_equal(report.extension_bytes, 8U);
    assert_int_equal(report.sparse_record_bytes, 12U);
    assert_int_equal(report.material_extension_bytes, 524288U);
    assert_int_equal(report.dense_cell_extension_bytes, 1048576U);
    assert_int_equal(report.sparse_cell_extension_bytes, 12288U);
    assert_int_equal(report.material_plus_sparse_bytes, 536576U);
    assert_int_equal(report.dense_break_even_sparse_count, 87381U);
    assert_int_equal(report.material_plus_sparse_break_even_count, 43690U);
    assert_int_equal(report.material_text_payload_bytes, 1441792U);
    assert_int_equal(report.dense_cell_text_payload_bytes, 2228224U);
    assert_int_equal(report.sparse_cell_text_payload_bytes, 23552U);
    assert_int_equal(report.material_plus_sparse_text_payload_bytes, 1465344U);
}

static void test_memory_report_rejects_invalid_inputs(void **state) {
    R9OpticalMemoryReport report;
    (void)state;
    assert_false(r9_optical_memory_report(0, 1, 1U, 0U, &report));
    assert_false(r9_optical_memory_report(1, 1, 0U, 0U, &report));
    assert_false(r9_optical_memory_report(1, 1, 1U, 2U, &report));
    assert_false(r9_optical_memory_report(1, 1, 1U, 0U, NULL));
}

static int print_memory_report(void) {
    R9OpticalMemoryReport report;
    if (!r9_optical_memory_report(SCENE_MAX_WIDTH, SCENE_MAX_HEIGHT,
                                  ASSET_ID_CAPACITY, 1024U, &report)) return 1;
    printf("{\n");
    printf("  \"map_width\": %d,\n", SCENE_MAX_WIDTH);
    printf("  \"map_height\": %d,\n", SCENE_MAX_HEIGHT);
    printf("  \"cell_count\": %zu,\n", report.cell_count);
    printf("  \"material_capacity\": %zu,\n", report.material_capacity);
    printf("  \"extension_bytes\": %zu,\n", report.extension_bytes);
    printf("  \"sparse_record_bytes\": %zu,\n", report.sparse_record_bytes);
    printf("  \"material_extension_bytes\": %zu,\n",
           report.material_extension_bytes);
    printf("  \"dense_cell_extension_bytes\": %zu,\n",
           report.dense_cell_extension_bytes);
    printf("  \"sparse_1024_bytes\": %zu,\n",
           report.sparse_cell_extension_bytes);
    printf("  \"material_plus_sparse_1024_bytes\": %zu,\n",
           report.material_plus_sparse_bytes);
    printf("  \"dense_break_even_sparse_count\": %zu,\n",
           report.dense_break_even_sparse_count);
    printf("  \"material_plus_sparse_break_even_count\": %zu,\n",
           report.material_plus_sparse_break_even_count);
    printf("  \"material_text_payload_bytes\": %zu,\n",
           report.material_text_payload_bytes);
    printf("  \"dense_cell_text_payload_bytes\": %zu,\n",
           report.dense_cell_text_payload_bytes);
    printf("  \"sparse_1024_text_payload_bytes\": %zu,\n",
           report.sparse_cell_text_payload_bytes);
    printf("  \"material_plus_sparse_1024_text_payload_bytes\": %zu\n",
           report.material_plus_sparse_text_payload_bytes);
    printf("}\n");
    return 0;
}

int main(int argc, char **argv) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_legacy_defaults_are_explicit),
        cmocka_unit_test(test_zero_extension_preserves_legacy_exactly),
        cmocka_unit_test(test_semantic_channels_are_independent),
        cmocka_unit_test(test_invalid_extensions_are_rejected_transactionally),
        cmocka_unit_test(test_exact_memory_costs_and_break_even),
        cmocka_unit_test(test_memory_report_rejects_invalid_inputs)
    };
    if (argc == 2 && strcmp(argv[1], "--memory-report") == 0)
        return print_memory_report();
    if (argc != 1) return 2;
    return cmocka_run_group_tests(tests, NULL, NULL);
}
