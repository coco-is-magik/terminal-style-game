/**
 * test_glyph_set.c — the set format, both directions
 *
 * The reader is validated input handling, so most of this file is rejections: a glyph set is a
 * file from a build step, and it can be truncated, corrupt, stale or from another font. Each of
 * those has a name, and none of them may be read as though it were fine.
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

#include <string.h>

#include "../src/glyph_set.h"

/* A buffer smaller than any real set, for the too-small write path. */
#define SMALL_BUFFER 64U

static const uint8_t PROVENANCE[SHA256_DIGEST_SIZE] = {
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e,
    0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c,
    0x1d, 0x1e, 0x1f, 0x20
};

static GlyphSetEntry make_entry(uint32_t codepoint, uint8_t first_row) {
    GlyphSetEntry entry;
    memset(&entry, 0, sizeof(entry));
    entry.codepoint = codepoint;
    entry.mode = 0U;
    entry.rows[0] = first_row;
    return entry;
}

/* Three entries, ascending, distinguishable by their first row. */
static unsigned build_three(GlyphSetEntry *entries) {
    entries[0] = make_entry(0x0021U, 0x01U);
    entries[1] = make_entry(0x0041U, 0x0FU);
    entries[2] = make_entry(0x3042U, 0xF0U);
    entries[1].mode = 2U; /* one non-default mode, to prove the field survives */
    return 3U;
}

static void test_round_trip_preserves_entries_and_provenance(void **state) {
    GlyphSetEntry entries[3];
    GlyphSetEntry found;
    GlyphSet set;
    uint8_t buffer[256];
    size_t length = 0U;
    unsigned count = build_three(entries);
    (void)state;

    assert_int_equal(glyph_set_encoded_size(count), GLYPH_SET_HEADER_SIZE +
                                                        count * GLYPH_SET_ENTRY_SIZE);
    assert_true(glyph_set_write(entries, count, PROVENANCE, buffer, sizeof(buffer), &length));
    assert_int_equal(length, glyph_set_encoded_size(count));
    assert_memory_equal(buffer, "TSGFONT1", GLYPH_SET_MAGIC_SIZE);

    assert_int_equal(glyph_set_from_bytes(buffer, length, &set, NULL), GLYPH_SET_OK);
    assert_int_equal(set.glyph_count, count);
    assert_memory_equal(set.provenance, PROVENANCE, SHA256_DIGEST_SIZE);

    assert_true(glyph_set_lookup(&set, 0x0021U, &found));
    assert_int_equal(found.codepoint, 0x0021U);
    assert_int_equal(found.rows[0], 0x01U);
    assert_int_equal(found.mode, 0U);
    assert_true(glyph_set_lookup(&set, 0x0041U, &found));
    assert_int_equal(found.rows[0], 0x0FU);
    assert_int_equal(found.mode, 2U);
    assert_true(glyph_set_lookup(&set, 0x3042U, &found));
    assert_int_equal(found.rows[0], 0xF0U);
    /* Absent codepoints are an ordinary answer, not an error: the document may use a character
       this face lacks, and the renderer's missing state is how that is shown. */
    assert_false(glyph_set_lookup(&set, 0x0022U, &found));
    assert_false(glyph_set_lookup(&set, 0x0000U, &found));
    assert_false(glyph_set_lookup(&set, 0x10FFFFU, &found));
}

static void test_empty_set_is_valid_and_finds_nothing(void **state) {
    GlyphSet set;
    GlyphSetEntry found;
    uint8_t buffer[SMALL_BUFFER];
    size_t length = 0U;
    (void)state;
    assert_true(glyph_set_write(NULL, 0U, PROVENANCE, buffer, sizeof(buffer), &length));
    assert_int_equal(length, GLYPH_SET_HEADER_SIZE);
    assert_int_equal(glyph_set_from_bytes(buffer, length, &set, NULL), GLYPH_SET_OK);
    assert_int_equal(set.glyph_count, 0U);
    assert_false(glyph_set_lookup(&set, 0x0041U, &found));
}

static void test_writer_refuses_bad_arguments(void **state) {
    GlyphSetEntry entries[3];
    uint8_t small[SMALL_BUFFER];
    uint8_t buffer[256];
    unsigned count = build_three(entries);
    (void)state;
    memset(small, 0x5a, sizeof(small));
    assert_false(glyph_set_write(entries, count, PROVENANCE, small, sizeof(small), NULL));
    /* A refused write leaves the buffer alone rather than half-filling it. */
    assert_int_equal(small[0], 0x5a);
    assert_false(glyph_set_write(NULL, count, PROVENANCE, buffer, sizeof(buffer), NULL));
    assert_false(glyph_set_write(entries, count, NULL, buffer, sizeof(buffer), NULL));
    assert_false(glyph_set_write(entries, count, PROVENANCE, NULL, sizeof(buffer), NULL));
    /* A mode the reader would reject is not emitted in the first place. */
    entries[1].mode = 9U;
    assert_false(glyph_set_write(entries, count, PROVENANCE, buffer, sizeof(buffer), NULL));
    /* And the ceiling is refused rather than computed as an enormous size. */
    assert_int_equal(glyph_set_encoded_size(GLYPH_SET_MAX_GLYPHS + 1U), 0U);
}

/* Every rejection gets its own named result, because they call for different repairs: a truncated
   file means copy it again, a bad magic means it is not this kind of file, an unsupported cell
   means it came from a different product. */
static void test_structural_rejections_are_named(void **state) {
    GlyphSetEntry entries[3];
    GlyphSet set;
    GlyphSet before;
    GlyphSetError error;
    uint8_t buffer[256];
    unsigned count = build_three(entries);
    size_t length = 0U;
    (void)state;
    assert_true(glyph_set_write(entries, count, PROVENANCE, buffer, sizeof(buffer), &length));
    assert_int_equal(glyph_set_from_bytes(buffer, length, &set, NULL), GLYPH_SET_OK);
    before = set;

    assert_int_equal(glyph_set_from_bytes(NULL, length, &set, &error),
                     GLYPH_SET_INVALID_ARGUMENT);
    assert_int_equal(glyph_set_from_bytes(buffer, length, NULL, &error),
                     GLYPH_SET_INVALID_ARGUMENT);
    assert_int_equal(glyph_set_from_bytes(buffer, GLYPH_SET_HEADER_SIZE - 1U, &set, &error),
                     GLYPH_SET_TOO_SHORT);

    buffer[0] = 'X';
    assert_int_equal(glyph_set_from_bytes(buffer, length, &set, &error), GLYPH_SET_BAD_MAGIC);
    buffer[0] = 'T';

    /* A 16x16 set is a different product, not a bigger version of this one. */
    buffer[8] = 16U;
    assert_int_equal(glyph_set_from_bytes(buffer, length, &set, &error),
                     GLYPH_SET_UNSUPPORTED_CELL);
    buffer[8] = GLYPH_SET_CELL_SIZE;

    /* One byte short: the last entry's trailing padding is missing. */
    assert_int_equal(glyph_set_from_bytes(buffer, length - 1U, &set, &error),
                     GLYPH_SET_TRUNCATED);
    /* One byte long: trailing data means these are not the bytes the digest described. */
    {
        uint8_t padded[257];
        memcpy(padded, buffer, length);
        padded[length] = 0xFFU;
        assert_int_equal(glyph_set_from_bytes(padded, length + 1U, &set, &error),
                         GLYPH_SET_TRUNCATED);
    }
    /* An absurd count is refused before any size arithmetic depends on it. */
    {
        uint8_t header[GLYPH_SET_HEADER_SIZE];
        memcpy(header, buffer, sizeof(header));
        header[12] = 0x01U;
        header[13] = 0x00U;
        header[14] = 0x01U; /* 65537 */
        header[15] = 0x00U;
        assert_int_equal(glyph_set_from_bytes(header, sizeof(header), &set, &error),
                         GLYPH_SET_TOO_MANY_GLYPHS);
    }

    /* No rejection may have written through the caller's pointer. */
    assert_int_equal(set.glyph_count, before.glyph_count);
    assert_memory_equal(set.provenance, before.provenance, SHA256_DIGEST_SIZE);
}


static void test_entry_rejections_name_the_offending_entry(void **state) {
    GlyphSetEntry entries[3];
    GlyphSet set;
    GlyphSetError error;
    uint8_t buffer[256];
    unsigned count = build_three(entries);
    size_t length = 0U;
    size_t entry_offset = GLYPH_SET_HEADER_SIZE + GLYPH_SET_ENTRY_SIZE;
    (void)state;
    assert_true(glyph_set_write(entries, count, PROVENANCE, buffer, sizeof(buffer), &length));

    /* A mode value the fit policy never produces. */
    buffer[entry_offset + 4U] = 9U;
    assert_int_equal(glyph_set_from_bytes(buffer, length, &set, &error), GLYPH_SET_BAD_MODE);
    assert_int_equal(error.index, 1U);
    assert_int_equal(error.codepoint, 0x0041U);
    buffer[entry_offset + 4U] = 2U;

    /* Out of order: the order is what makes lookup a binary search, so it is not negotiable. */
    buffer[entry_offset] = 0x20U; /* 0x0020 sorts before 0x0021 */
    assert_int_equal(glyph_set_from_bytes(buffer, length, &set, &error), GLYPH_SET_UNSORTED);
    assert_int_equal(error.codepoint, 0x0020U);
    buffer[entry_offset] = 0x41U;

    /* Duplicate: two entries for one codepoint means one of them is unreachable. */
    buffer[entry_offset + GLYPH_SET_ENTRY_SIZE] = 0x41U;
    buffer[entry_offset + GLYPH_SET_ENTRY_SIZE + 1U] = 0x00U;
    assert_int_equal(glyph_set_from_bytes(buffer, length, &set, &error),
                     GLYPH_SET_DUPLICATE_CODEPOINT);
    assert_int_equal(error.codepoint, 0x0041U);
}

static void test_lookup_handles_the_edges_of_the_search(void **state) {
    static GlyphSetEntry entries[9];
    GlyphSet set;
    uint8_t buffer[512];
    size_t length = 0U;
    unsigned index;
    (void)state;
    for (index = 0U; index < 9U; index++) {
        entries[index] = make_entry(0x100U + index * 2U, (uint8_t)index);
    }
    assert_true(glyph_set_write(entries, 9U, PROVENANCE, buffer, sizeof(buffer), &length));
    assert_int_equal(glyph_set_from_bytes(buffer, length, &set, NULL), GLYPH_SET_OK);
    /* First, last, middle and the gaps between them: an off-by-one in a binary search shows up
       at exactly these and nowhere else. */
    for (index = 0U; index < 9U; index++) {
        GlyphSetEntry found;
        assert_true(glyph_set_lookup(&set, 0x100U + index * 2U, &found));
        assert_int_equal(found.rows[0], (uint8_t)index);
        assert_false(glyph_set_lookup(&set, 0x101U + index * 2U, &found));
    }
    assert_false(glyph_set_lookup(&set, 0x0FFU, NULL));
    assert_false(glyph_set_lookup(&set, 0x200U, NULL));
    assert_false(glyph_set_lookup(NULL, 0x100U, NULL));
}

static void test_result_names_are_stable(void **state) {
    (void)state;
    assert_string_equal(glyph_set_result_name(GLYPH_SET_OK), "OK");
    assert_string_equal(glyph_set_result_name(GLYPH_SET_BAD_MAGIC), "BAD_MAGIC");
    assert_string_equal(glyph_set_result_name(GLYPH_SET_UNSUPPORTED_CELL), "UNSUPPORTED_CELL");
    assert_string_equal(glyph_set_result_name(GLYPH_SET_TRUNCATED), "TRUNCATED");
    assert_string_equal(glyph_set_result_name(GLYPH_SET_UNSORTED), "UNSORTED");
    assert_string_equal(glyph_set_result_name(GLYPH_SET_DUPLICATE_CODEPOINT),
                        "DUPLICATE_CODEPOINT");
    assert_string_equal(glyph_set_result_name(GLYPH_SET_BAD_MODE), "BAD_MODE");
}


/* The cell holds a 16-bit codepoint (V1_5_CELL_IDENTITY_DECISION_2026-11-22, D1). The set format can
   physically store a u32, so the ceiling has to be enforced deliberately in both directions: the
   writer refuses to emit one, and the reader rejects a file that contains one — because such a file
   cannot have come from this pipeline, and reading it as ordinary data would mean a glyph sitting in
   a set that the renderer can never reach. */
static void test_the_codepoint_ceiling_is_enforced_both_ways(void **state) {
    GlyphSetEntry entries[3];
    GlyphSet set;
    GlyphSetError error;
    uint8_t buffer[256];
    size_t length = 0U;
    unsigned count = build_three(entries);
    (void)state;

    assert_true(glyph_set_codepoint_is_addressable(0x0000U));
    assert_true(glyph_set_codepoint_is_addressable(0x0041U));
    assert_true(glyph_set_codepoint_is_addressable(0x3042U));
    assert_true(glyph_set_codepoint_is_addressable(GLYPH_SET_MAX_CODEPOINT));
    assert_false(glyph_set_codepoint_is_addressable(GLYPH_SET_MAX_CODEPOINT + 1U));
    assert_false(glyph_set_codepoint_is_addressable(0x1F600U));
    assert_false(glyph_set_codepoint_is_addressable(0x10FFFFU));

    /* Writer: an unaddressable entry is a refusal, not a silently dropped row. */
    assert_true(glyph_set_write(entries, count, PROVENANCE, buffer, sizeof(buffer), &length));
    entries[2].codepoint = 0x1F600U;
    assert_false(glyph_set_write(entries, count, PROVENANCE, buffer, sizeof(buffer), NULL));
    entries[2].codepoint = 0x3042U;

    /* Reader: craft a set whose last entry is astral, in ascending order so nothing else fires
       first, and require the named rejection with the entry identified. */
    assert_true(glyph_set_write(entries, count, PROVENANCE, buffer, sizeof(buffer), &length));
    {
        size_t last = GLYPH_SET_HEADER_SIZE + 2U * GLYPH_SET_ENTRY_SIZE;
        buffer[last + 0U] = 0x00U;
        buffer[last + 1U] = 0xF6U;
        buffer[last + 2U] = 0x01U;
        buffer[last + 3U] = 0x00U; /* U+0001F600 */
    }
    assert_int_equal(glyph_set_from_bytes(buffer, length, &set, &error),
                     GLYPH_SET_CODEPOINT_TOO_WIDE);
    assert_int_equal(error.index, 2U);
    assert_int_equal(error.codepoint, 0x1F600U);
    assert_string_equal(glyph_set_result_name(GLYPH_SET_CODEPOINT_TOO_WIDE),
                        "CODEPOINT_TOO_WIDE");
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_round_trip_preserves_entries_and_provenance),
        cmocka_unit_test(test_empty_set_is_valid_and_finds_nothing),
        cmocka_unit_test(test_writer_refuses_bad_arguments),
        cmocka_unit_test(test_structural_rejections_are_named),
        cmocka_unit_test(test_entry_rejections_name_the_offending_entry),
        cmocka_unit_test(test_lookup_handles_the_edges_of_the_search),
        cmocka_unit_test(test_the_codepoint_ceiling_is_enforced_both_ways),
        cmocka_unit_test(test_result_names_are_stable)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}

