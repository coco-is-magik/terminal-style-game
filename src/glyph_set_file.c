/**
 * glyph_set_file.c — path to validated set, with the four outcomes kept distinct
 *
 * Read, bound, validate, return. Deliberately no caching and no search path: a caller that wants
 * to look in several places does that, because only it knows what the places are.
 */

#include "glyph_set_file.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The largest legal set is a full glyph table; anything longer is refused from its length alone,
   before a byte is allocated. */
static size_t maximum_file_size(void) {
    size_t encoded = glyph_set_encoded_size(GLYPH_SET_MAX_GLYPHS);
    return encoded > 0U ? encoded : 0U;
}

const char *glyph_set_file_result_name(GlyphSetFileResult result) {
    switch (result) {
    case GLYPH_SET_FILE_OK: return "OK";
    case GLYPH_SET_FILE_MISSING: return "MISSING";
    case GLYPH_SET_FILE_UNREADABLE: return "UNREADABLE";
    case GLYPH_SET_FILE_TOO_LARGE: return "TOO_LARGE";
    case GLYPH_SET_FILE_INVALID: return "INVALID";
    case GLYPH_SET_FILE_INVALID_ARGUMENT: return "INVALID_ARGUMENT";
    default: return "UNKNOWN_RESULT";
    }
}

void glyph_set_file_free(uint8_t *bytes) {
    free(bytes);
}

static GlyphSetFileResult fail(GlyphSetFileDiagnostic *diagnostic, GlyphSetFileResult result) {
    if (diagnostic) {
        diagnostic->result = result;
        diagnostic->set_result = GLYPH_SET_OK;
        diagnostic->set_error.result = GLYPH_SET_OK;
        diagnostic->set_error.index = 0U;
        diagnostic->set_error.codepoint = 0U;
    }
    return result;
}

GlyphSetFileResult glyph_set_file_load(const char *path, uint8_t **out_bytes, GlyphSet *out_set,
                                       GlyphSetFileDiagnostic *out_diagnostic) {
    FILE *file;
    long length;
    uint8_t *bytes;
    GlyphSet set;
    GlyphSetError set_error;
    GlyphSetResult parsed;
    GlyphSetFileResult result;

    if (out_diagnostic) {
        out_diagnostic->result = GLYPH_SET_FILE_OK;
        out_diagnostic->set_result = GLYPH_SET_OK;
        out_diagnostic->set_error.result = GLYPH_SET_OK;
        out_diagnostic->set_error.index = 0U;
        out_diagnostic->set_error.codepoint = 0U;
    }
    if (out_bytes) *out_bytes = NULL;
    if (out_set) memset(out_set, 0, sizeof(*out_set));
    if (!path || !out_bytes || !out_set) return fail(out_diagnostic, GLYPH_SET_FILE_INVALID_ARGUMENT);

    file = fopen(path, "rb");
    if (!file) {
        /* A missing set is an ordinary state — the built-in face is used — so it is a distinct
           result rather than a failure. A directory or a permission problem is not ordinary. */
        return fail(out_diagnostic,
                    errno == ENOENT ? GLYPH_SET_FILE_MISSING : GLYPH_SET_FILE_UNREADABLE);
    }
    if (fseek(file, 0L, SEEK_END) != 0 || (length = ftell(file)) < 0) {
        fclose(file);
        return fail(out_diagnostic, GLYPH_SET_FILE_UNREADABLE);
    }
    if ((size_t)length > maximum_file_size()) {
        fclose(file);
        return fail(out_diagnostic, GLYPH_SET_FILE_TOO_LARGE);
    }
    rewind(file);
    if (length == 0) {
        /* An empty file cannot be a set; the format's own answer for "shorter than a header" is
           the honest explanation, so report it rather than leaving the reason unset. */
        fclose(file);
        result = fail(out_diagnostic, GLYPH_SET_FILE_INVALID);
        if (out_diagnostic) out_diagnostic->set_result = GLYPH_SET_TOO_SHORT;
        return result;
    }
    bytes = malloc((size_t)length);
    if (!bytes) {
        fclose(file);
        return fail(out_diagnostic, GLYPH_SET_FILE_UNREADABLE);
    }
    if (fread(bytes, 1U, (size_t)length, file) != (size_t)length) {
        free(bytes);
        fclose(file);
        return fail(out_diagnostic, GLYPH_SET_FILE_UNREADABLE);
    }
    fclose(file);

    parsed = glyph_set_from_bytes(bytes, (size_t)length, &set, &set_error);
    if (parsed != GLYPH_SET_OK) {
        free(bytes);
        result = fail(out_diagnostic, GLYPH_SET_FILE_INVALID);
        if (out_diagnostic) {
            out_diagnostic->set_result = parsed;
            out_diagnostic->set_error = set_error;
        }
        return result;
    }

    *out_bytes = bytes;
    *out_set = set;
    return fail(out_diagnostic, GLYPH_SET_FILE_OK);
}
