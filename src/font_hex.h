/**
 * font_hex.h — the Unifont-style hexdump reader
 *
 * A `.hex` font is text: one glyph per line, `CODEPOINT:HEXDATA`, no header, no index. unscii ships
 * its real bitmaps in this form and its vectorised TTF is lossy at 8 px, so this is the artefact a
 * bitmap-native face actually has to be read from. Reading it needs no rasteriser and no FreeType,
 * which is the point: conversion stops depending on a host library for faces that already describe
 * themselves as bits.
 *
 * **Bit order is the trap.** Unifont hex is MSB-first (bit 7 is the leftmost pixel); this project's
 * cells are LSB-first (bit 0 is leftmost, `renderer.c`). Every byte is therefore bit-reversed on the
 * way in. The same class of mistake as the mirrored-glyph bug, so it is pinned by test rather than
 * assumed: `U+0045` is `7E60607C60607E00`, and its second row must land as `0x06`, not `0x60`.
 *
 * Strict by design, and it names the line when it refuses. A hexdump is authored data: a truncated
 * row or a stray character is a defect in the font, not something to skip past.
 */

#ifndef FONT_HEX_H
#define FONT_HEX_H

#include "glyph_set.h"

#include <stdbool.h>
#include <stddef.h>

/* The largest text this reader will look at: one line per addressable codepoint, with room for the
   longest line the format allows. A bound, not a target. */
#define FONT_HEX_MAX_TEXT_BYTES ((size_t)GLYPH_SET_MAX_GLYPHS * 32U)

/* 8x8 cells are exactly this many hex digits. */
#define FONT_HEX_CELL_HEX_DIGITS 16U

typedef enum {
    FONT_HEX_OK = 0,
    /* A caller-supplied argument was unusable: no buffer, no output count, zero capacity. */
    FONT_HEX_INVALID_ARGUMENT,
    /* There was nothing to read: no non-blank, non-comment line. */
    FONT_HEX_EMPTY,
    /* A line was not `CODEPOINT:HEXDATA`, or carried trailing junk. `line` names it. */
    FONT_HEX_LINE_SYNTAX,
    /* The data was not exactly FONT_HEX_CELL_HEX_DIGITS hexadecimal digits. `line` names it. */
    FONT_HEX_LINE_WIDTH,
    /* The codepoint is above what a cell can address (GLYPH_SET_MAX_CODEPOINT). `codepoint` names
       it, and `line` the entry that carried it. */
    FONT_HEX_CODEPOINT_RANGE,
    /* The same codepoint appeared twice. Refused rather than resolved: choosing one would silently
       pick one drawing over another, which is the rule the PUA mapping question already settled. */
    FONT_HEX_DUPLICATE_CODEPOINT,
    /* More glyphs than the caller's buffer can hold. */
    FONT_HEX_TOO_MANY
} FontHexResult;

/* Where a refusal happened. `line` is 1-based and 0 when the failure has no line; `codepoint` is 0
   when the failure is structural. */
typedef struct {
    FontHexResult result;
    size_t line;
    unsigned long codepoint;
} FontHexError;

/**
 * Parse `text_size` bytes of hexdump into `entries`.
 *
 * On FONT_HEX_OK the entries are **ascending by codepoint and unique** — the reader sorts in place,
 * because the set format requires that order and sorting a caller's array here is cheaper and less
 * error-prone than making every caller remember. `mode` is FONT_FIT_MODE_NONE for every entry: the
 * file states the cell directly, so nothing was fitted.
 *
 * On any refusal `*out_count` is 0 and the contents of `entries` are unspecified; the caller writes
 * nothing. `text` need not be NUL-terminated.
 */
FontHexResult font_hex_parse(const char *text, size_t text_size, GlyphSetEntry *entries,
                             size_t capacity, size_t *out_count, FontHexError *out_error);

/* Stable short name for a result, for tool output and test failure messages. */
const char *font_hex_result_name(FontHexResult result);

#endif /* FONT_HEX_H */
