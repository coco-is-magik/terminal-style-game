/**
 * glyph_set_file.h — reading a converted glyph set from disk
 *
 * `glyph_set` is deliberately pure: bytes in, bytes out, no file access. That is what makes it
 * testable and what keeps filesystem effects out of the render path. This module is the one place
 * that turns a path into those bytes, so the decision about what a *missing* set means lives once.
 *
 * Missing is not an error. The application renders its built-in ASCII face with no set loaded —
 * that is the state the project ships in today — so "no set configured" and "the set is corrupt"
 * must be different answers. A corrupt set, by contrast, is refused rather than partially used:
 * half a font produces text with holes nobody can explain.
 *
 * Bounded before allocating: a file longer than any legal set is rejected from its length, so a
 * corrupt or hostile file cannot ask for an allocation it will never justify.
 */

#ifndef GLYPH_SET_FILE_H
#define GLYPH_SET_FILE_H

#include "glyph_set.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    GLYPH_SET_FILE_OK = 0,
    /* No file at that path. Ordinary: the built-in face is used instead. */
    GLYPH_SET_FILE_MISSING,
    /* The file exists but could not be read (permissions, I/O, a directory). */
    GLYPH_SET_FILE_UNREADABLE,
    /* Larger than any legal set. Refused before allocating. */
    GLYPH_SET_FILE_TOO_LARGE,
    /* Read fine, rejected by the format. `set_result` says why and `set_error` says where. */
    GLYPH_SET_FILE_INVALID,
    GLYPH_SET_FILE_INVALID_ARGUMENT
} GlyphSetFileResult;

typedef struct {
    GlyphSetFileResult result;
    GlyphSetResult set_result;  /* meaningful when result == GLYPH_SET_FILE_INVALID */
    GlyphSetError set_error;    /* entry index and codepoint, when the format said so */
} GlyphSetFileDiagnostic;

/* Read and validate. On GLYPH_SET_FILE_OK the caller owns `*out_bytes` and must release it with
   glyph_set_file_free; `*out_set` borrows from it and is invalid afterwards. On any failure both
   outputs are NULL/zeroed, so a caller cannot accidentally use a refused set. */
GlyphSetFileResult glyph_set_file_load(const char *path, uint8_t **out_bytes, GlyphSet *out_set,
                                       GlyphSetFileDiagnostic *out_diagnostic);

/* Releases bytes returned by glyph_set_file_load. Tolerates NULL. */
void glyph_set_file_free(uint8_t *bytes);

/* Stable short name, for tool output and test failure messages. */
const char *glyph_set_file_result_name(GlyphSetFileResult result);

#endif /* GLYPH_SET_FILE_H */
