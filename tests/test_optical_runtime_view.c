#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <string.h>

#include <cmocka.h>

#include "../src/optical_runtime_view.h"

static void assert_resolved_equal(const OpticalResolved *left,
                                  const OpticalResolved *right) {
    assert_int_equal(left->player_blocks, right->player_blocks);
    assert_int_equal(left->ray_blocks, right->ray_blocks);
    assert_int_equal(left->light_blocks, right->light_blocks);
    assert_int_equal(left->opacity, right->opacity);
    assert_int_equal(left->transmission, right->transmission);
    assert_int_equal(left->reflectivity, right->reflectivity);
}

static void test_legacy_defaults_are_exact(void **state) {
    OpticalResolved occupied = optical_resolved_legacy(true);
    OpticalResolved empty = optical_resolved_legacy(false);
    (void)state;
    assert_true(occupied.player_blocks);
    assert_true(occupied.ray_blocks);
    assert_true(occupied.light_blocks);
    assert_int_equal(occupied.opacity, 255U);
    assert_int_equal(occupied.transmission, 0U);
    assert_int_equal(occupied.reflectivity, 0U);
    assert_false(empty.player_blocks);
    assert_false(empty.ray_blocks);
    assert_false(empty.light_blocks);
    assert_int_equal(empty.opacity, 0U);
    assert_int_equal(empty.transmission, 255U);
    assert_int_equal(empty.reflectivity, 0U);
}

static void test_empty_view_is_legacy_parity_path(void **state) {
    OpticalRuntimeView view;
    OpticalResolved actual;
    OpticalResolved expected;
    (void)state;
    assert_true(optical_runtime_view_init(
        &view, 16U, NULL, 0U, NULL, 0U, UINT32_C(77)));
    assert_true(view.valid);
    assert_int_equal(view.source_generation, UINT32_C(77));
    assert_true(optical_runtime_view_is_current(&view, UINT32_C(77)));
    assert_false(optical_runtime_view_is_current(&view, UINT32_C(78)));
    assert_false(optical_runtime_view_is_current(NULL, UINT32_C(77)));
    for (size_t i = 0U; i < 16U; i++) {
        bool occupied = (i & 1U) != 0U;
        expected = optical_resolved_legacy(occupied);
        assert_true(optical_runtime_view_resolve(
            &view, i, UINT16_C(65535), occupied, &actual));
        assert_resolved_equal(&actual, &expected);
    }
}

static void test_material_then_cell_override_precedence(void **state) {
    OpticalExtension materials[4] = {0};
    OpticalCellOverride overrides[2] = {0};
    OpticalRuntimeView view;
    OpticalResolved resolved;
    (void)state;
    materials[2] = (OpticalExtension){
        OPTICAL_OVERRIDE_RAY_BLOCKS | OPTICAL_OVERRIDE_OPACITY |
            OPTICAL_OVERRIDE_TRANSMISSION,
        0U, 0U, 0U, 96U, 180U, 0U, 0U
    };
    overrides[0].cell_index = 3U;
    overrides[0].optical = (OpticalExtension){
        OPTICAL_OVERRIDE_RAY_BLOCKS | OPTICAL_OVERRIDE_REFLECTIVITY,
        0U, 1U, 0U, 0U, 0U, 64U, 0U
    };
    overrides[1].cell_index = 9U;
    overrides[1].optical = (OpticalExtension){
        OPTICAL_OVERRIDE_PLAYER_BLOCKS, 0U, 0U, 0U, 0U, 0U, 0U, 0U
    };
    assert_true(optical_runtime_view_init(
        &view, 12U, materials, 4U, overrides, 2U, UINT32_C(4)));
    assert_true(optical_runtime_view_resolve(&view, 3U, 2U, true, &resolved));
    assert_true(resolved.player_blocks);
    assert_true(resolved.ray_blocks);
    assert_true(resolved.light_blocks);
    assert_int_equal(resolved.opacity, 96U);
    assert_int_equal(resolved.transmission, 180U);
    assert_int_equal(resolved.reflectivity, 64U);
    assert_true(optical_runtime_view_resolve(&view, 4U, 2U, true, &resolved));
    assert_false(resolved.ray_blocks);
    assert_int_equal(resolved.opacity, 96U);
    assert_int_equal(resolved.reflectivity, 0U);
}

static void test_independent_fields_and_material_fallback(void **state) {
    OpticalExtension materials[2] = {0};
    OpticalRuntimeView view;
    OpticalResolved resolved;
    (void)state;
    materials[1] = (OpticalExtension){
        OPTICAL_OVERRIDE_PLAYER_BLOCKS | OPTICAL_OVERRIDE_LIGHT_BLOCKS |
            OPTICAL_OVERRIDE_REFLECTIVITY,
        1U, 0U, 0U, 0U, 0U, 200U, 0U
    };
    assert_true(optical_runtime_view_init(
        &view, 2U, materials, 2U, NULL, 0U, 0U));
    assert_true(optical_runtime_view_resolve(&view, 0U, 1U, false, &resolved));
    assert_true(resolved.player_blocks);
    assert_false(resolved.ray_blocks);
    assert_false(resolved.light_blocks);
    assert_int_equal(resolved.opacity, 0U);
    assert_int_equal(resolved.transmission, 255U);
    assert_int_equal(resolved.reflectivity, 200U);
    assert_true(optical_runtime_view_resolve(
        &view, 0U, UINT16_C(60000), true, &resolved));
    assert_resolved_equal(&resolved, &(OpticalResolved){true, true, true, 255U, 0U, 0U});
}

static void test_init_rejects_invalid_sparse_inputs_transactionally(void **state) {
    OpticalRuntimeView sentinel;
    OpticalRuntimeView output;
    OpticalCellOverride overrides[2] = {0};
    (void)state;
    memset(&sentinel, 0x5a, sizeof(sentinel));
    output = sentinel;
    overrides[0].cell_index = 4U;
    overrides[1].cell_index = 4U;
    assert_false(optical_runtime_view_init(
        &output, 8U, NULL, 0U, overrides, 2U, 0U));
    assert_memory_equal(&output, &sentinel, sizeof(output));
    overrides[1].cell_index = 3U;
    assert_false(optical_runtime_view_init(
        &output, 8U, NULL, 0U, overrides, 2U, 0U));
    overrides[0].cell_index = 8U;
    assert_false(optical_runtime_view_init(
        &output, 8U, NULL, 0U, overrides, 1U, 0U));
    assert_memory_equal(&output, &sentinel, sizeof(output));
}

static void test_init_rejects_invalid_extensions_and_pointer_pairs(void **state) {
    OpticalExtension invalid = {0};
    OpticalCellOverride override = {0};
    OpticalRuntimeView view;
    (void)state;
    invalid.override_mask = UINT8_C(0x80);
    assert_false(optical_runtime_view_init(
        &view, 1U, &invalid, 1U, NULL, 0U, 0U));
    invalid = (OpticalExtension){OPTICAL_OVERRIDE_RAY_BLOCKS,
                                 0U, 2U, 0U, 0U, 0U, 0U, 0U};
    assert_false(optical_extension_is_valid(&invalid));
    override.cell_index = 0U;
    override.optical.reserved = 1U;
    assert_false(optical_runtime_view_init(
        &view, 1U, NULL, 0U, &override, 1U, 0U));
    assert_false(optical_runtime_view_init(
        &view, 1U, NULL, 1U, NULL, 0U, 0U));
    assert_false(optical_runtime_view_init(
        &view, 1U, NULL, 0U, NULL, 1U, 0U));
    assert_false(optical_runtime_view_init(
        &view, 0U, NULL, 0U, NULL, 0U, 0U));
    assert_false(optical_runtime_view_init(
        NULL, 1U, NULL, 0U, NULL, 0U, 0U));
}

static void test_resolve_rejects_invalid_without_output_mutation(void **state) {
    OpticalRuntimeView view;
    OpticalResolved sentinel = {true, false, true, 1U, 2U, 3U};
    OpticalResolved output = sentinel;
    (void)state;
    assert_true(optical_runtime_view_init(
        &view, 2U, NULL, 0U, NULL, 0U, 0U));
    assert_false(optical_runtime_view_resolve(&view, 2U, 0U, false, &output));
    assert_memory_equal(&output, &sentinel, sizeof(output));
    view.valid = false;
    assert_false(optical_runtime_view_resolve(&view, 0U, 0U, false, &output));
    assert_false(optical_runtime_view_resolve(NULL, 0U, 0U, false, &output));
    assert_false(optical_runtime_view_resolve(&view, 0U, 0U, false, NULL));
    assert_memory_equal(&output, &sentinel, sizeof(output));
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_legacy_defaults_are_exact),
        cmocka_unit_test(test_empty_view_is_legacy_parity_path),
        cmocka_unit_test(test_material_then_cell_override_precedence),
        cmocka_unit_test(test_independent_fields_and_material_fallback),
        cmocka_unit_test(test_init_rejects_invalid_sparse_inputs_transactionally),
        cmocka_unit_test(test_init_rejects_invalid_extensions_and_pointer_pairs),
        cmocka_unit_test(test_resolve_rejects_invalid_without_output_mutation)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
