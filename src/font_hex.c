/**
 * font_hex.c — see font_hex.h for the contract and for why the bits are reversed.
 */

#include "font_hex.h"

#include "font_fit.h"

#include <stdlib.h>
#include <string.h>

static uint8_t reverse_bits(uint8_t value) {
    uint8_t reversed = 0U;
    unsigned bit;
    for (bit = 0U; bit < 8U; bit++) {
        reversed = (uint8_t)((reversed << 1) | (uint8_t)((value >> bit) & 1U));
    }
    return reversed;
}

static int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static bool line_space(char c) {
    return c == ' ' || c == '\t' || c == '\r';
}

static FontHexResult refuse(FontHexError *out_error, FontHexResult result, size_t line,
                            unsigned long codepoint) {
    if (out_error) {
        out_error->result = result;
        out_error->line = line;
        out_error->codepoint = codepoint;
    }
    return result;
}

static int compare_by_codepoint(const void *left, const void *right) {
    const GlyphSetEntry *a = left;
    const GlyphSetEntry *b = right;
    if (a->codepoint < b->codepoint) return -1;
    if (a->codepoint > b->codepoint) return 1;
    return 0;
}

FontHexResult font_hex_parse(const char *text, size_t text_size, GlyphSetEntry *entries,
                             size_t capacity, size_t *out_count, FontHexError *out_error) {
    size_t offset = 0U;
    size_t line_number = 0U;
    size_t count = 0U;
    size_t i;

    if (out_count) *out_count = 0U;
    /* The size bound is a caller error rather than a font defect: a reader told to parse more text
       than the format could possibly address is being misused. */
    if (!text || !entries || !out_count || capacity == 0U ||
        text_size > FONT_HEX_MAX_TEXT_BYTES) {
        return refuse(out_error, FONT_HEX_INVALID_ARGUMENT, 0U, 0UL);
    }

    while (offset < text_size) {
        size_t line_start = offset;
        size_t line_end;
        size_t cursor;
        size_t digits = 0U;
        unsigned long codepoint = 0UL;
        bool saw_digit = false;
        uint8_t rows[GLYPH_SET_CELL_SIZE];
        unsigned row;

        while (offset < text_size && text[offset] != '\n') offset++;
        line_end = offset;
        if (offset < text_size) offset++; /* consume the newline */
        line_number++;

        while (line_end > line_start && line_space(text[line_end - 1U])) line_end--;
        while (line_start < line_end && line_space(text[line_start])) line_start++;

        if (line_start == line_end) continue;    /* blank */
        if (text[line_start] == '#') continue;   /* comment */

        cursor = line_start;
        while (cursor < line_end && text[cursor] != ':') {
            int value = hex_value(text[cursor]);
            if (value < 0 || digits >= 6U) {
                return refuse(out_error, FONT_HEX_LINE_SYNTAX, line_number, 0UL);
            }
            codepoint = (codepoint << 4) | (unsigned long)value;
            digits++;
            saw_digit = true;
            cursor++;
        }
        if (!saw_digit || cursor >= line_end) {
            return refuse(out_error, FONT_HEX_LINE_SYNTAX, line_number, 0UL);
        }
        cursor++; /* the colon */

        for (row = 0U; row < GLYPH_SET_CELL_SIZE; row++) {
            int high;
            int low;
            if (cursor + 1U >= line_end) {
                return refuse(out_error, FONT_HEX_LINE_WIDTH, line_number, codepoint);
            }
            high = hex_value(text[cursor]);
            low = hex_value(text[cursor + 1U]);
            if (high < 0 || low < 0) {
                return refuse(out_error, FONT_HEX_LINE_WIDTH, line_number, codepoint);
            }
            rows[row] = reverse_bits((uint8_t)((high << 4) | low));
            cursor += 2U;
        }
        if (cursor != line_end) { /* trailing junk after the cell */
            return refuse(out_error, FONT_HEX_LINE_SYNTAX, line_number, codepoint);
        }

        if (codepoint > (unsigned long)GLYPH_SET_MAX_CODEPOINT) {
            return refuse(out_error, FONT_HEX_CODEPOINT_RANGE, line_number, codepoint);
        }
        if (count >= capacity) {
            return refuse(out_error, FONT_HEX_TOO_MANY, line_number, codepoint);
        }
        entries[count].codepoint = (uint32_t)codepoint;
        entries[count].mode = (uint8_t)FONT_FIT_MODE_NONE;
        memcpy(entries[count].rows, rows, GLYPH_SET_CELL_SIZE);
        count++;
    }

    if (count == 0U) return refuse(out_error, FONT_HEX_EMPTY, 0U, 0UL);

    /* The set format is ascending and unique, and the file is not. Sorting here means no caller has
       to remember; refusing duplicates means the format's invariant is enforced where it is created
       rather than where it is validated, so a duplicate cannot reach a writer. */
    qsort(entries, count, sizeof(entries[0]), compare_by_codepoint);
    for (i = 1U; i < count; i++) {
        if (entries[i].codepoint == entries[i - 1U].codepoint) {
            return refuse(out_error, FONT_HEX_DUPLICATE_CODEPOINT, 0U, entries[i].codepoint);
        }
    }

    *out_count = count;
    return FONT_HEX_OK;
}

const char *font_hex_result_name(FontHexResult result) {
    switch (result) {
    case FONT_HEX_OK: return "OK";
    case FONT_HEX_INVALID_ARGUMENT: return "INVALID_ARGUMENT";
    case FONT_HEX_EMPTY: return "EMPTY";
    case FONT_HEX_LINE_SYNTAX: return "LINE_SYNTAX";
    case FONT_HEX_LINE_WIDTH: return "LINE_WIDTH";
    case FONT_HEX_CODEPOINT_RANGE: return "CODEPOINT_RANGE";
    case FONT_HEX_DUPLICATE_CODEPOINT: return "DUPLICATE_CODEPOINT";
    case FONT_HEX_TOO_MANY: return "TOO_MANY";
    }
    return "UNKNOWN";
}
