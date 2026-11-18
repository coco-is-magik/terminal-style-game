/**
 * glyph_set.h — the converted glyph set, and the only definition of its byte layout
 *
 * `fontconv --convert` writes this; the runtime will read it. Both go through this header, so the
 * format cannot drift between producer and consumer — the same reasoning as mapgen_seed, and the
 * same strictness: a glyph set is a file, files are untrusted, and a malformed one must be refused
 * with a reason rather than read as though it were fine.
 *
 * Layout, little-endian and fixed width so two machines can compare bytes:
 *
 *     offset  size  field
 *     0       8     magic "TSGFONT1"
 *     8       4     cell size (must be 8)
 *     12      4     glyph count
 *     16      32    provenance: sha256 over the face's declared file digests, in manifest order
 *     48      16*n  entries, ascending codepoint:
 *                     0  4  codepoint (u32)
 *                     4  1  fit mode that produced the cell (FontFitMode)
 *                     5  8  cell rows, bit 0 leftmost — the renderer's convention
 *                     13 3  reserved, zero
 *
 * The provenance field is what makes a stale set detectable: it changes exactly when the bytes
 * behind the manifest change, so a set built from one font cannot be mistaken for another's.
 */

#ifndef GLYPH_SET_H
#define GLYPH_SET_H

#include "sha256.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define GLYPH_SET_MAGIC_SIZE 8U
#define GLYPH_SET_HEADER_SIZE 48U
#define GLYPH_SET_ENTRY_SIZE 16U
#define GLYPH_SET_CELL_SIZE 8U

/* A bounded ceiling, not a design target: the largest face considered so far is ~7,200 glyphs,
   and a bound keeps a corrupt count from asking for an unbounded allocation. */
#define GLYPH_SET_MAX_GLYPHS 65536U

typedef enum {
    GLYPH_SET_OK = 0,
    GLYPH_SET_INVALID_ARGUMENT,
    GLYPH_SET_TOO_SHORT,
    GLYPH_SET_BAD_MAGIC,
    GLYPH_SET_UNSUPPORTED_CELL,
    GLYPH_SET_TRUNCATED,
    GLYPH_SET_TOO_MANY_GLYPHS,
    GLYPH_SET_UNSORTED,
    GLYPH_SET_DUPLICATE_CODEPOINT,
    GLYPH_SET_BAD_MODE,
    GLYPH_SET_BUFFER_TOO_SMALL
} GlyphSetResult;

/* Where a rejection happened. `codepoint` and `index` name the offending entry; both are 0 for
   structural failures that have no entry. */
typedef struct {
    GlyphSetResult result;
    unsigned index;
    uint32_t codepoint;
} GlyphSetError;

/* Borrowed view over a validated buffer. The caller owns the bytes and must keep them alive; no
   copy is made, so a 114 KB set costs one structure. */
typedef struct {
    const uint8_t *data;
    size_t size;
    unsigned glyph_count;
    uint8_t provenance[SHA256_DIGEST_SIZE];
} GlyphSet;

/* One cell, as produced by the converter. `rows` uses bit 0 = leftmost, matching renderer.c. */
typedef struct {
    uint32_t codepoint;
    uint8_t mode;
    uint8_t rows[GLYPH_SET_CELL_SIZE];
} GlyphSetEntry;

/* Bytes needed to encode `count` entries. 0 when count exceeds the ceiling, so a caller cannot
   compute a size it will then fail to honour. */
size_t glyph_set_encoded_size(unsigned count);

/* Encode. Entries are written in the order given; the writer does not sort, because silently
   reordering a caller's data hides the caller's bug. Returns false when the buffer is too small
   or an entry is out of range, and writes nothing in that case. */
bool glyph_set_write(const GlyphSetEntry *entries, unsigned count,
                     const uint8_t provenance[SHA256_DIGEST_SIZE], uint8_t *buffer,
                     size_t buffer_size, size_t *out_length);

/* Validate untrusted bytes and take a view. On any rejection `out_set` is untouched. */
GlyphSetResult glyph_set_from_bytes(const uint8_t *data, size_t size, GlyphSet *out_set,
                                    GlyphSetError *out_error);

/* Binary search (the format guarantees ascending order). Returns false when the codepoint is
   absent, which is a legitimate answer: the document may use a character this face lacks, and the
   renderer's missing-glyph state is how that is shown. */
bool glyph_set_lookup(const GlyphSet *set, uint32_t codepoint, GlyphSetEntry *out_entry);

/* Stable short name for a result, for tool output and test failure messages. */
const char *glyph_set_result_name(GlyphSetResult result);

#endif /* GLYPH_SET_H */
