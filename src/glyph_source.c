/**
 * glyph_source.c — the precedence rule, in one place
 *
 * Deliberately small. Everything interesting about a glyph happens elsewhere: conversion decides
 * the pixels (font_fit), the file format decides their storage (glyph_set), and this decides only
 * which of two faces answers, or that neither does.
 */

#include "glyph_source.h"

#include <string.h>

/* The built-in face is a definition in font8x8.h; declaring it extern here (as renderer.c does)
   keeps this module from dragging a 1 KB array into every translation unit that asks a question
   about a glyph. */
extern unsigned char font8x8_basic[128][8];

/* The single process-lifetime selection. Reset by passing NULL, which puts the application back on
   the built-in face — the same state as a fresh process with no set configured. */
static GlyphSource g_active_source = {NULL};

void glyph_source_set_active(const GlyphSet *set) {
    g_active_source.set = set;
}

const GlyphSource *glyph_source_active(void) {
    return &g_active_source;
}

void glyph_source_init(GlyphSource *source, const GlyphSet *set) {
    if (!source) return;
    source->set = set;
}

bool glyph_source_is_missing(const GlyphSourceGlyph *glyph) {
    return glyph && glyph->kind == GLYPH_SOURCE_MISSING;
}

const char *glyph_source_kind_name(GlyphSourceKind kind) {
    switch (kind) {
    case GLYPH_SOURCE_SET: return "set";
    case GLYPH_SOURCE_BUILTIN: return "builtin";
    case GLYPH_SOURCE_MISSING: return "missing";
    default: return "unknown";
    }
}

static void builtin_rows(uint32_t codepoint, uint8_t rows[GLYPH_SOURCE_CELL_SIZE]) {
    unsigned row;
    for (row = 0U; row < GLYPH_SOURCE_CELL_SIZE; row++) {
        rows[row] = font8x8_basic[codepoint & 0x7FU][row];
    }
}

GlyphSourceGlyph glyph_source_resolve(const GlyphSource *source, uint32_t codepoint) {
    GlyphSourceGlyph glyph;
    GlyphSetEntry entry;
    memset(&glyph, 0, sizeof(glyph));
    glyph.codepoint = codepoint;

    /* The set first: it is the face the user chose, and it may deliberately overrule the built-in
       ASCII look. Only if it has nothing to say does the application fall back. */
    if (source && source->set && glyph_set_lookup(source->set, codepoint, &entry)) {
        memcpy(glyph.rows, entry.rows, GLYPH_SOURCE_CELL_SIZE);
        glyph.kind = GLYPH_SOURCE_SET;
        glyph.mode = entry.mode;
        return glyph;
    }
    if (codepoint < 0x80U) {
        builtin_rows(codepoint, glyph.rows);
        glyph.kind = GLYPH_SOURCE_BUILTIN;
        return glyph;
    }
    /* Missing: the caller gets pixels *and* knows they are a sentinel. Returning blank here, or a
       question mark, would make an absent glyph indistinguishable from a drawn one. */
    builtin_rows(GLYPH_SOURCE_SENTINEL_CODEPOINT, glyph.rows);
    glyph.kind = GLYPH_SOURCE_MISSING;
    return glyph;
}
