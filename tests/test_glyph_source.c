/**
 * test_glyph_source.c — precedence, and the sentinel as a result rather than an accident
 *
 * The interesting cases are the disagreements: a set that also covers ASCII (which wins?), a set
 * that lacks a codepoint the built-in face has, and a codepoint neither has. The last is the one
 * that shows up in front of a user, so it gets the most attention.
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

#include <string.h>

#include "../src/glyph_source.h"
#include "../src/font8x8.h" /* defines the built-in face for this binary */

static GlyphSet build_set(GlyphSetEntry *entries, unsigned count, uint8_t *storage,
                          size_t storage_size) {
    GlyphSet set;
    uint8_t provenance[SHA256_DIGEST_SIZE];
    size_t length = 0U;
    memset(provenance, 0x5a, sizeof(provenance));
    assert_true(glyph_set_write(entries, count, provenance, storage, storage_size, &length));
    assert_int_equal(glyph_set_from_bytes(storage, length, &set, NULL), GLYPH_SET_OK);
    return set;
}

static void test_set_wins_over_the_builtin_face(void **state) {
    GlyphSetEntry entries[2];
    GlyphSet set;
    GlyphSource source;
    GlyphSourceGlyph glyph;
    uint8_t storage[128];
    (void)state;
    /* Two entries deliberately unlike the built-in glyphs, so "which answered" is visible in the
       pixels and not only in the kind. */
    memset(entries, 0, sizeof(entries));
    entries[0].codepoint = 0x0041U; /* 'A', which the built-in face also knows */
    entries[0].rows[0] = 0xFFU;
    entries[1].codepoint = 0x3042U; /* あ, which it does not */
    entries[1].mode = 1U;
    entries[1].rows[3] = 0x3CU;
    set = build_set(entries, 2U, storage, sizeof(storage));
    glyph_source_init(&source, &set);

    glyph = glyph_source_resolve(&source, 0x0041U);
    assert_int_equal(glyph.kind, GLYPH_SOURCE_SET);
    assert_int_equal(glyph.rows[0], 0xFFU);
    assert_int_equal(glyph.codepoint, 0x0041U);

    glyph = glyph_source_resolve(&source, 0x3042U);
    assert_int_equal(glyph.kind, GLYPH_SOURCE_SET);
    assert_int_equal(glyph.rows[3], 0x3CU);
    assert_int_equal(glyph.mode, 1U);
}

static void test_builtin_answers_ascii_when_the_set_lacks_it(void **state) {
    GlyphSetEntry entries[1];
    GlyphSet set;
    GlyphSource source;
    GlyphSource empty_source;
    GlyphSourceGlyph glyph;
    GlyphSourceGlyph no_set;
    uint8_t storage[128];
    (void)state;
    memset(entries, 0, sizeof(entries));
    entries[0].codepoint = 0x3042U;
    entries[0].rows[1] = 0x18U;
    set = build_set(entries, 1U, storage, sizeof(storage));
    glyph_source_init(&source, &set);

    /* 'A' is absent from the set but present in the application's own face, so the chrome keeps
       rendering even when a set covers only CJK. */
    glyph = glyph_source_resolve(&source, 0x0041U);
    assert_int_equal(glyph.kind, GLYPH_SOURCE_BUILTIN);
    assert_memory_equal(glyph.rows, font8x8_basic[0x41], GLYPH_SOURCE_CELL_SIZE);

    /* And with no set at all the answer is the same, not an error. */
    glyph_source_init(&empty_source, NULL);
    no_set = glyph_source_resolve(&empty_source, 0x0041U);
    assert_int_equal(no_set.kind, GLYPH_SOURCE_BUILTIN);
    assert_memory_equal(no_set.rows, glyph.rows, GLYPH_SOURCE_CELL_SIZE);
}

static void test_missing_returns_the_sentinel_and_says_so(void **state) {
    GlyphSource source;
    GlyphSourceGlyph glyph;
    (void)state;
    glyph_source_init(&source, NULL);
    glyph = glyph_source_resolve(&source, 0x3042U);
    /* The pixels are the built-in 'X' — derived, never stored — and the kind is what lets the
       caller paint it as a diagnostic instead of as a character. */
    assert_int_equal(glyph.kind, GLYPH_SOURCE_MISSING);
    assert_true(glyph_source_is_missing(&glyph));
    assert_memory_equal(glyph.rows, font8x8_basic[GLYPH_SOURCE_SENTINEL_CODEPOINT],
                        GLYPH_SOURCE_CELL_SIZE);
    assert_int_equal(glyph.codepoint, 0x3042U);
    assert_int_equal(glyph.mode, 0U);
}

static void test_sentinel_colours_are_the_recorded_palette_values(void **state) {
    (void)state;
    /* Pinned because D8 names them and the renderer will paint them: magenta foreground on red
       background, both quoted from the palette rather than invented. If these change, the record
       changes first. */
    assert_int_equal(GLYPH_SOURCE_SENTINEL_FG_R, 0xFFU);
    assert_int_equal(GLYPH_SOURCE_SENTINEL_FG_G, 0x2BU);
    assert_int_equal(GLYPH_SOURCE_SENTINEL_FG_B, 0xD6U);
    assert_int_equal(GLYPH_SOURCE_SENTINEL_BG_R, 0xFFU);
    assert_int_equal(GLYPH_SOURCE_SENTINEL_BG_G, 0x2BU);
    assert_int_equal(GLYPH_SOURCE_SENTINEL_BG_B, 0x2BU);
    /* A sentinel whose two colours matched would be a mark nobody notices, which defeats it. */
    assert_true(GLYPH_SOURCE_SENTINEL_FG_R != GLYPH_SOURCE_SENTINEL_BG_R ||
                GLYPH_SOURCE_SENTINEL_FG_G != GLYPH_SOURCE_SENTINEL_BG_G ||
                GLYPH_SOURCE_SENTINEL_FG_B != GLYPH_SOURCE_SENTINEL_BG_B);
}

static void test_resolution_is_total_and_pure(void **state) {
    GlyphSource source;
    GlyphSourceGlyph first;
    GlyphSourceGlyph second;
    GlyphSourceGlyph null_source;
    static const uint32_t PROBES[] = {0x00U, 0x20U, 0x41U, 0x7FU, 0x80U, 0x3042U, 0x10FFFFU};
    size_t index;
    (void)state;
    glyph_source_init(&source, NULL);
    for (index = 0U; index < sizeof(PROBES) / sizeof(PROBES[0]); index++) {
        first = glyph_source_resolve(&source, PROBES[index]);
        second = glyph_source_resolve(&source, PROBES[index]);
        /* No codepoint is an error and the answer does not change between calls: a resolver that
           could fail would push failure handling into the per-cell draw path. */
        assert_memory_equal(&first, &second, sizeof(first));
        assert_int_equal(first.codepoint, PROBES[index]);
    }
    /* NULL arguments are tolerated rather than crashing a draw loop. */
    null_source = glyph_source_resolve(NULL, 0x41U);
    assert_int_equal(null_source.kind, GLYPH_SOURCE_BUILTIN);
    assert_false(glyph_source_is_missing(NULL));
    glyph_source_init(NULL, NULL);
    assert_string_equal(glyph_source_kind_name(GLYPH_SOURCE_SET), "set");
    assert_string_equal(glyph_source_kind_name(GLYPH_SOURCE_BUILTIN), "builtin");
    assert_string_equal(glyph_source_kind_name(GLYPH_SOURCE_MISSING), "missing");
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_set_wins_over_the_builtin_face),
        cmocka_unit_test(test_builtin_answers_ascii_when_the_set_lacks_it),
        cmocka_unit_test(test_missing_returns_the_sentinel_and_says_so),
        cmocka_unit_test(test_sentinel_colours_are_the_recorded_palette_values),
        cmocka_unit_test(test_resolution_is_total_and_pure)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}

