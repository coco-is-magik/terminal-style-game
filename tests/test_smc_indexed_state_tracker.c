#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

#include "../src/smc_indexed_state_tracker.h"

static void test_lifecycle_batch_and_bounds(void **state) {
    (void)state;
    CellState cells[3] = {1, 2, 3};
    uint32_t dirty[3] = {99, 99, 99};
    size_t dirty_count = 0;
    size_t state_bytes = 0;
    size_t dirty_bytes = 0;
    int changed = 0;
    IndexedStateStats stats;

    smc_indexed_state_tracker_shutdown();
    assert_int_not_equal(smc_indexed_state_tracker_reset(), 0);
    assert_int_equal(smc_indexed_state_tracker_init(3), 0);
    assert_int_equal(smc_indexed_state_tracker_init(3), 0);

    assert_int_equal(smc_indexed_state_tracker_cell_changed(2, &cells[2], &changed), 0);
    assert_int_equal(changed, 1);
    assert_int_equal(smc_indexed_state_tracker_cell_changed(2, &cells[2], &changed), 0);
    assert_int_equal(changed, 0);
    assert_int_not_equal(smc_indexed_state_tracker_cell_changed(3, &cells[0], &changed), 0);
    assert_int_not_equal(smc_indexed_state_tracker_cell_changed(0, NULL, &changed), 0);
    assert_int_equal(smc_indexed_state_tracker_get_stats(&stats), 0);
    assert_true(stats.out_of_range > 0);

    assert_int_equal(smc_indexed_state_tracker_reset(), 0);
    assert_int_equal(smc_indexed_state_tracker_diff_batch(cells, 3, dirty, 3,
                                                           &dirty_count), 0);
    assert_int_equal(dirty_count, 3);
    assert_int_equal(dirty[0], 0);
    assert_int_equal(dirty[1], 1);
    assert_int_equal(dirty[2], 2);
    assert_int_equal(smc_indexed_state_tracker_diff_batch(cells, 3, dirty, 3,
                                                           &dirty_count), 0);
    assert_int_equal(dirty_count, 0);
    cells[1] = 7;
    assert_int_equal(smc_indexed_state_tracker_diff_batch(cells, 3, dirty, 3,
                                                           &dirty_count), 0);
    assert_int_equal(dirty_count, 1);
    assert_int_equal(dirty[0], 1);
    assert_int_not_equal(smc_indexed_state_tracker_diff_batch(NULL, 3, dirty, 3,
                                                               &dirty_count), 0);

    assert_int_equal(smc_indexed_state_tracker_get_stats(&stats), 0);
    assert_true(stats.checks > 0);
    assert_true(stats.changed > 0);
    assert_true(stats.unchanged > 0);
    smc_indexed_state_tracker_get_buffer_sizes(3, &state_bytes, &dirty_bytes);
    assert_int_equal(state_bytes, 3 * sizeof(CellState));
    assert_int_equal(dirty_bytes, 3 * sizeof(uint32_t));

    smc_indexed_state_tracker_shutdown();
    smc_indexed_state_tracker_shutdown();
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_lifecycle_batch_and_bounds),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}