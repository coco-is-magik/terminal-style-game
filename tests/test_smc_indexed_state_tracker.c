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

/* The stream path is the default build mode, and it compares *fields* with an explicit byte size
   per field rather than a packed state. That is why widening Cell.glyph from 8 to 16 bits had to be
   followed through here: with the field size left at 1, a codepoint change that altered only the
   high byte compared equal and the cell kept stale pixels, silently, in the shipping configuration.

   This test drives the same descriptors the renderer uses (cell_state_streams) and requires a
   high-byte-only change to be reported dirty. It would have failed before the fix. */
#if defined(USE_SMC_STREAM_STATE_TRACKER)
static void test_stream_mode_sees_a_high_byte_codepoint_change(void **state) {
    static Cell previous[2];
    static Cell current[2];
    uint32_t dirty[2] = {0U, 0U};
    size_t dirty_count = 0U;
    size_t index;
    (void)state;

    memset(previous, 0, sizeof(previous));
    for (index = 0U; index < 2U; index++) {
        previous[index].glyph = 0x0041U; /* 'A' */
        previous[index].fg = (SDL_Color){0x10U, 0x20U, 0x30U, 0xFFU};
        previous[index].bg = (SDL_Color){0x40U, 0x50U, 0x60U, 0xFFU};
    }
    current[0] = previous[0];
    current[0].glyph = 0x0141U; /* same low byte, different codepoint */
    current[1] = previous[1];   /* untouched: must not be reported */

    smc_indexed_state_tracker_shutdown();
    assert_int_equal(smc_indexed_state_tracker_init(2U), 0);

    {
        smc_state_stream_t streams[3];
        /* Prime the tracker with the previous frame, then diff the current one. */
        cell_state_streams(previous, streams);
        assert_int_equal(smc_indexed_state_tracker_diff_streams(streams, 3U, 2U, dirty, 2U,
                                                                &dirty_count),
                         0);
        dirty[0] = 0U;
        dirty[1] = 0U;
        dirty_count = 0U;
        cell_state_streams(current, streams);
        assert_int_equal(smc_indexed_state_tracker_diff_streams(streams, 3U, 2U, dirty, 2U,
                                                                &dirty_count),
                         0);
        assert_int_equal(dirty_count, 1U);
        assert_int_equal(dirty[0], 0U);
    }
    smc_indexed_state_tracker_shutdown();
}
#endif /* USE_SMC_STREAM_STATE_TRACKER: this test exists only where the path exists */

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_lifecycle_batch_and_bounds),
#if defined(USE_SMC_STREAM_STATE_TRACKER)
        cmocka_unit_test(test_stream_mode_sees_a_high_byte_codepoint_change),
#endif
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
