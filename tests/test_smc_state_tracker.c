#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

#include "../src/smc_state_tracker.h"

static void test_lifecycle_and_change_detection(void **state) {
    (void)state;
    CellState first = 11;
    CellState second = 12;
    int changed = 0;
    smc_state_stats_t stats;

    smc_state_tracker_shutdown();
    assert_int_not_equal(smc_state_tracker_reset(), 0);
    assert_int_not_equal(smc_state_tracker_cell_changed(0, &first, &changed), 0);
    assert_int_not_equal(smc_state_tracker_get_stats(&stats), 0);

    assert_int_equal(smc_state_tracker_init(8), 0);
    assert_int_equal(smc_state_tracker_init(8), 0);
    assert_int_equal(smc_state_tracker_cell_changed(0, &first, &changed), 0);
    assert_int_equal(changed, 1);
    assert_int_equal(smc_state_tracker_cell_changed(0, &first, &changed), 0);
    assert_int_equal(changed, 0);
    assert_int_equal(smc_state_tracker_cell_changed(0, &second, &changed), 0);
    assert_int_equal(changed, 1);
    assert_int_equal(smc_state_tracker_cell_changed(1, &second, &changed), 0);
    assert_int_equal(changed, 1);

    assert_int_not_equal(smc_state_tracker_cell_changed(0, NULL, &changed), 0);
    assert_int_not_equal(smc_state_tracker_cell_changed(0, &first, NULL), 0);
    assert_int_not_equal(smc_state_tracker_get_stats(NULL), 0);
    assert_int_equal(smc_state_tracker_get_stats(&stats), 0);
    assert_true(stats.checks >= 4);
    assert_true(stats.changed >= 3);
    assert_true(stats.unchanged >= 1);

    assert_int_equal(smc_state_tracker_reset(), 0);
    assert_int_equal(smc_state_tracker_get_stats(&stats), 0);
    assert_int_equal(stats.checks, 0);
    assert_int_equal(smc_state_tracker_cell_changed(0, &first, &changed), 0);
    assert_int_equal(changed, 1);
    smc_state_tracker_shutdown();
    smc_state_tracker_shutdown();
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_lifecycle_and_change_detection),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}