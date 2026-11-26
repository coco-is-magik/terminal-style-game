/**
 * tests/test_glyph_missing_sentinel.c — D8's missing-glyph contract, end to end at the module level
 *
 * The name is the one `ERROR_CATALOG.md` has promised for `TSG-ASSET-ENV-0001` since 2026-11-06. It
 * asserts the two halves that used to be only half-true: that a source which HAS a set and lacks the
 * codepoint reports MISSING rather than borrowing the built-in face, and that "exactly one diagnostic
 * per (face, codepoint)" is exact rather than approximate.
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <string.h>

#include "../src/glyph_missing.h"
#include "../src/glyph_source.h"
#include "../src/glyph_set.h"
/* Defines font8x8_basic, which glyph_source.c references as extern — the same arrangement
   test_glyph_paint.c uses, so the built-in face has exactly one definition per binary. */
#include "../src/font8x8.h"

static GlyphSet build_set(unsigned codepoint, uint8_t *storage, size_t storage_size,
                          uint8_t provenance_byte) {
    GlyphSetEntry entries[1];
    GlyphSet set;
    GlyphSetError error;
    uint8_t provenance[SHA256_DIGEST_SIZE];
    size_t written = 0U;

    memset(&set, 0, sizeof(set));
    memset(entries, 0, sizeof(entries));
    entries[0].codepoint = codepoint;
    entries[0].rows[0] = 0x5AU;
    memset(provenance, provenance_byte, sizeof(provenance));

    assert_true(glyph_set_write(entries, 1U, provenance, storage, storage_size, &written));
    assert_int_equal(glyph_set_from_bytes(storage, written, &set, &error), GLYPH_SET_OK);
    return set;
}

/* The gap D8 exists for: a set that does not contain the codepoint must not borrow the built-in face,
   even for ASCII, and it must report once no matter how many frames ask. */
static void test_missing_ascii_reports_once_across_frames(void **state) {
    uint8_t storage[128];
    GlyphMissingState missing;
    GlyphMissingReport report;
    GlyphSource source;
    GlyphSourceGlyph glyph;
    GlyphSet set;
    unsigned frame;

    (void)state;
    set = build_set(0x0042U, storage, sizeof(storage), 0x11U); /* the set has 'B', not 'A' */
    glyph_source_init(&source, &set);
    glyph_missing_reset(&missing);

    glyph = glyph_source_resolve(&source, 0x0041U);
    assert_int_equal(glyph.kind, GLYPH_SOURCE_MISSING);
    assert_memory_equal(glyph.rows, glyph_source_resolve(NULL, 0x0058U).rows,
                        GLYPH_SOURCE_CELL_SIZE); /* the sentinel, which is 'X' from the built-in face */

    /* Frame one: the first sighting is reported, with the coordinate the caller had. */
    glyph_missing_begin_frame(&missing);
    glyph_missing_note(&missing, &source, 0x0041U, 3, 4);
    assert_int_equal(glyph_missing_report_count(&missing), 1U);
    assert_true(glyph_missing_report(&missing, 0U, &report));
    assert_int_equal(report.codepoint, 0x0041U);
    assert_int_equal(report.cell_x, 3);
    assert_int_equal(report.cell_y, 4);

    /* Every later frame: the cell is still counted, and nothing is reported again. The bitmap is what
       makes that true across frame boundaries, which is the part a "recently seen" list could not do
       without an overflow rule. */
    for (frame = 0U; frame < 99U; frame++) {
        glyph_missing_begin_frame(&missing);
        glyph_missing_note(&missing, &source, 0x0041U, 3, 4);
        assert_int_equal(glyph_missing_report_count(&missing), 0U);
        assert_int_equal(glyph_missing_affected_this_frame(&missing), 1U);
    }
}

/* The other half: everything that can answer must not generate a diagnostic. */
static void test_answerable_codepoints_produce_no_report(void **state) {
    uint8_t storage[128];
    GlyphMissingState missing;
    GlyphSource source;
    GlyphSet set;

    (void)state;
    set = build_set(0x0041U, storage, sizeof(storage), 0x22U);
    glyph_source_init(&source, &set);
    glyph_missing_reset(&missing);
    glyph_missing_begin_frame(&missing);

    assert_int_equal(glyph_source_resolve(&source, 0x0041U).kind, GLYPH_SOURCE_SET);

    /* A source with no set answers ASCII from the built-in face — the shipping state, and the chrome. */
    assert_int_equal(glyph_source_resolve(NULL, 0x0041U).kind, GLYPH_SOURCE_BUILTIN);

    assert_int_equal(glyph_missing_report_count(&missing), 0U);
    assert_int_equal(glyph_missing_affected_this_frame(&missing), 0U);
}

/* A diagnostic belongs to a face. The bitmap is keyed to the face that produced it, so installing a
   different set clears it and the same codepoint is reported again — once per (face, codepoint), not
   once per frame, which is what D8 actually requires. The trade-off is stated rather than hidden: a
   face swapped out and back re-reports once, because only the current face's bitmap is live. */
static void test_each_face_reports_its_own_gap(void **state) {
    uint8_t storage_a[128];
    uint8_t storage_b[128];
    GlyphMissingState missing;
    GlyphSource a;
    GlyphSource b;
    GlyphSet set_a;
    GlyphSet set_b;

    (void)state;
    set_a = build_set(0x0042U, storage_a, sizeof(storage_a), 0x33U);
    set_b = build_set(0x0043U, storage_b, sizeof(storage_b), 0x44U);
    glyph_source_init(&a, &set_a);
    glyph_source_init(&b, &set_b);

    glyph_missing_reset(&missing);
    glyph_missing_begin_frame(&missing);
    glyph_missing_note(&missing, &a, 0x3042U, 0, 0);
    assert_int_equal(glyph_missing_report_count(&missing), 1U);

    /* The same codepoint under a different face is a different subject, so it reports too. */
    glyph_missing_begin_frame(&missing);
    glyph_missing_note(&missing, &b, 0x3042U, 0, 0);
    assert_int_equal(glyph_missing_report_count(&missing), 1U);

    /* Within one face it stays quiet across frames. */
    glyph_missing_begin_frame(&missing);
    glyph_missing_note(&missing, &b, 0x3042U, 0, 0);
    assert_int_equal(glyph_missing_report_count(&missing), 0U);
    assert_int_equal(glyph_missing_affected_this_frame(&missing), 1U);
}

/* More distinct misses in one frame than the report list holds: the count is named, not dropped. */
static void test_overflow_is_counted(void **state) {
    GlyphMissingState missing;
    uint32_t codepoint;

    (void)state;
    glyph_missing_reset(&missing);
    glyph_missing_begin_frame(&missing);
    for (codepoint = 0x3042U; codepoint < 0x304CU; codepoint++) { /* ten distinct */
        glyph_missing_note(&missing, NULL, codepoint, 0, 0);
    }

    assert_int_equal(glyph_missing_report_count(&missing), GLYPH_MISSING_MAX_REPORTS);
    assert_int_equal(glyph_missing_overflow_this_frame(&missing),
                     (uint32_t)(10U - GLYPH_MISSING_MAX_REPORTS));
    assert_int_equal(glyph_missing_affected_this_frame(&missing), 10U);
}

static void test_reset_nulls_and_face_text(void **state) {
    uint8_t storage[128];
    GlyphMissingState missing;
    GlyphSource source;
    GlyphSet set;
    char text[GLYPH_MISSING_FACE_TEXT_SIZE];

    (void)state;

    /* A NULL state is tolerated everywhere: a diagnostic path must not become a crash path. */
    glyph_missing_reset(NULL);
    glyph_missing_begin_frame(NULL);
    glyph_missing_note(NULL, NULL, 0x3042U, 0, 0);
    assert_int_equal(glyph_missing_report_count(NULL), 0U);
    assert_int_equal(glyph_missing_affected_this_frame(NULL), 0U);
    assert_int_equal(glyph_missing_overflow_this_frame(NULL), 0U);
    assert_false(glyph_missing_report(NULL, 0U, NULL));
    glyph_missing_face_text(NULL, text);
    assert_string_equal(text, "builtin");

    /* The face text names the installed face by provenance, and reset returns it to the built-in. */
    set = build_set(0x0042U, storage, sizeof(storage), 0xABU);
    glyph_source_init(&source, &set);
    glyph_missing_reset(&missing); /* required before first use: see glyph_missing.h */
    glyph_missing_note(&missing, &source, 0x3042U, 1, 2);
    assert_int_equal(glyph_missing_report_count(&missing), 1U);
    glyph_missing_face_text(&missing, text);
    assert_string_equal(text, "abababababababab");

    glyph_missing_reset(&missing);
    assert_int_equal(glyph_missing_report_count(&missing), 0U);
    assert_int_equal(glyph_missing_affected_this_frame(&missing), 0U);
    glyph_missing_face_text(&missing, text);
    assert_string_equal(text, "builtin");
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_missing_ascii_reports_once_across_frames),
        cmocka_unit_test(test_answerable_codepoints_produce_no_report),
        cmocka_unit_test(test_each_face_reports_its_own_gap),
        cmocka_unit_test(test_overflow_is_counted),
        cmocka_unit_test(test_reset_nulls_and_face_text),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}

