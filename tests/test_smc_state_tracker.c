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

/* The packed layout is what makes change detection correct, so it is asserted rather than assumed
   (V1_5_CELL_IDENTITY_DECISION_2026-11-22.md, D2): 16-bit codepoint at bit 0, fg RGB at 16/24/32,
   bg RGB at 40/48/56, 64 bits exactly. The failure this guards against is the one that leaves no
   trace: with the old 8-bit offsets, a cell whose glyph differs only in its high byte compares
   equal to the previous frame and keeps stale pixels forever. */
static void test_packed_layout_covers_every_field(void **state) {
    Cell cell;
    Cell other;
    CellState packed;
    (void)state;
    memset(&cell, 0, sizeof(cell));
    cell.glyph = 0x0041U;
    cell.fg = (SDL_Color){0x11U, 0x22U, 0x33U, 0xFFU};
    cell.bg = (SDL_Color){0x44U, 0x55U, 0x66U, 0xFFU};
    packed = pack_cell_state(&cell);

    assert_int_equal(packed & 0xFFFFULL, 0x0041ULL);
    assert_int_equal((packed >> 16) & 0xFFULL, 0x11ULL);
    assert_int_equal((packed >> 24) & 0xFFULL, 0x22ULL);
    assert_int_equal((packed >> 32) & 0xFFULL, 0x33ULL);
    assert_int_equal((packed >> 40) & 0xFFULL, 0x44ULL);
    assert_int_equal((packed >> 48) & 0xFFULL, 0x55ULL);
    assert_int_equal((packed >> 56) & 0xFFULL, 0x66ULL);

    /* Two codepoints that share a low byte must not fingerprint the same. This is exactly the case
       an 8-bit glyph could not distinguish, and the reason the field widened. */
    other = cell;
    other.glyph = 0x0141U;
    assert_int_not_equal(pack_cell_state(&other), packed);
    other.glyph = 0x3042U; /* a CJK codepoint, unreachable at all before this change */
    assert_int_not_equal(pack_cell_state(&other), packed);

    /* And every colour channel is independently visible, so no field silently overlaps another. */
    other = cell;
    other.bg.b = 0x67U;
    assert_int_not_equal(pack_cell_state(&other), packed);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_lifecycle_and_change_detection),
        cmocka_unit_test(test_packed_layout_covers_every_field),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
