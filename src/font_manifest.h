/**
 * font_manifest.h — the font contract: who this face is, and which bytes it must be
 *
 * Glyph data is not committed to this repository (V1_4_FONT_DEPENDENCY_DECISION_2026-11-06.md,
 * D2). The user supplies font files; the converter turns them into 8x8 glyphs; and *this* file is
 * what makes two installs comparable: a face is identified by its name, the SHA-256 of every file
 * it ships, and a URL that still exists in a year. A filename is not an identity and a font family
 * name is not an identity — both are editable, both are routinely wrong.
 *
 * Pure text in, pure text out, like mapgen_seed: no file access and no allocation, because the
 * same codec has to serve the converter now, the game's load-time verification later, and a test
 * today. The caller owns the file and the diagnostics boundary.
 *
 * Strictness is the point. Unknown keys, duplicate keys, unpaired `file`/`sha256` lines, a
 * missing `source`, or a `cell` that is not 8x8 are rejections with a line number and the
 * offending key, because a manifest typo that is silently tolerated becomes a font that converts
 * differently on someone else's machine.
 */

#ifndef FONT_MANIFEST_H
#define FONT_MANIFEST_H

#include "sha256.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* A manifest larger than this is refused rather than truncated. */
#define FONT_MANIFEST_TEXT_MAX 32768U

#define FONT_MANIFEST_MAX_FACES 8U
#define FONT_MANIFEST_MAX_FILES 4U

#define FONT_MANIFEST_ID_SIZE 32U
#define FONT_MANIFEST_NAME_SIZE 64U
#define FONT_MANIFEST_PATH_SIZE 128U
#define FONT_MANIFEST_SOURCE_SIZE 192U
#define FONT_MANIFEST_LABEL_SIZE 64U

/* The renderer's cell is 8x8 and is not negotiable by a font (D6): the converter fits the font to
   the cell, and a font that will not fit is reported, not accommodated. */
#define FONT_MANIFEST_CELL_SIZE 8U

typedef enum {
    FONT_MANIFEST_OK = 0,
    FONT_MANIFEST_INVALID_ARGUMENT,
    /* Structure. */
    FONT_MANIFEST_MALFORMED,
    FONT_MANIFEST_UNKNOWN_KEY,
    FONT_MANIFEST_DUPLICATE_KEY,
    FONT_MANIFEST_MISSING_KEY,
    FONT_MANIFEST_VALUE_TOO_LONG,
    FONT_MANIFEST_INVALID_VALUE,
    FONT_MANIFEST_INVALID_CELL,
    FONT_MANIFEST_UNPAIRED_FILE,
    FONT_MANIFEST_DUPLICATE_FACE,
    FONT_MANIFEST_TOO_MANY_FACES,
    FONT_MANIFEST_TOO_MANY_FILES,
    /* Output. */
    FONT_MANIFEST_BUFFER_TOO_SMALL
} FontManifestResult;

/* `key` points at a static literal, never at the parsed text: a diagnostic outlives the buffer it
   was produced from, and must not dangle. `line` is one-based, or 0 when the rejection is not
   about a particular line. */
typedef struct {
    FontManifestResult result;
    unsigned line;
    const char *key;
} FontManifestDiagnostic;

typedef struct {
    char file[FONT_MANIFEST_PATH_SIZE];
    uint8_t sha256[SHA256_DIGEST_SIZE];
} FontManifestFile;

/* A face is the set of files named here, all verified or none. `version` and `coverage` are
   optional declarations; when present the converter checks them against the file rather than
   trusting them. */
typedef struct {
    char id[FONT_MANIFEST_ID_SIZE];
    char name[FONT_MANIFEST_NAME_SIZE];
    char source[FONT_MANIFEST_SOURCE_SIZE];
    char licence[FONT_MANIFEST_LABEL_SIZE];
    char version[FONT_MANIFEST_LABEL_SIZE];
    char coverage[FONT_MANIFEST_LABEL_SIZE];
    unsigned file_count;
    FontManifestFile files[FONT_MANIFEST_MAX_FILES];
} FontManifestFace;

typedef struct {
    unsigned face_count;
    FontManifestFace faces[FONT_MANIFEST_MAX_FACES];
} FontManifest;

/* Total: zeroes the structure so a partially-filled manifest is never mistaken for content. */
void font_manifest_init(FontManifest *manifest);

/* Parse manifest text. On any rejection `out_manifest` is untouched, so a caller keeps the
   manifest it already had. `out_diagnostic` may be NULL.
 *
 * Format, one `key=value` per line, `#` starts a whole-line comment, blank lines ignored:
 *
 *     font=<id>        starts a face; required, unique
 *     name=<text>      required
 *     file=<path>      required, at least one; relative to the fonts directory
 *     sha256=<64 hex>  required, exactly one per file, in any order after it
 *     source=<url>     required: where to obtain the file without guessing
 *     licence=<text>   required
 *     version=<text>   optional
 *     coverage=<text>  optional
 *     cell=8x8         required
 */
FontManifestResult font_manifest_read_text(const char *text, FontManifest *out_manifest,
                                           FontManifestDiagnostic *out_diagnostic);

/* Canonical text: fixed key order, one `key=value` per line, faces in declaration order, trailing
   newline. The same manifest always produces the same bytes, so it can be diffed and hashed. */
FontManifestResult font_manifest_write_text(const FontManifest *manifest, char *buffer,
                                            size_t buffer_size, size_t *out_length);

/* Case-sensitive lookup by id; NULL when absent. */
const FontManifestFace *font_manifest_find_face(const FontManifest *manifest, const char *id);

/* Stable short name for a result, for tool output and test failure messages. */
const char *font_manifest_result_name(FontManifestResult result);

#endif /* FONT_MANIFEST_H */
