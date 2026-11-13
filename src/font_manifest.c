/**
 * font_manifest.c — strict codec for the font contract
 *
 * Parsing rules enforced rather than tolerated: known keys only, each singleton key at most once,
 * every `file` paired with exactly one `sha256`, all required keys present, and `cell=8x8` because
 * that is the renderer's answer and a manifest is not allowed to have a different one. No
 * defaulting: a missing `source` is a broken record, not a request for silence, and a face whose
 * URL is absent is a face nobody can reproduce next year.
 */

#include "font_manifest.h"

#include <stdio.h>
#include <string.h>

typedef enum {
    FONT_KEY_FONT = 0,
    FONT_KEY_NAME,
    FONT_KEY_FILE,
    FONT_KEY_SHA256,
    FONT_KEY_SOURCE,
    FONT_KEY_LICENCE,
    FONT_KEY_VERSION,
    FONT_KEY_COVERAGE,
    FONT_KEY_CELL,
    FONT_KEY_COUNT
} FontKey;

static const char *const FONT_KEY_NAMES[FONT_KEY_COUNT] = {
    "font", "name", "file", "sha256", "source", "licence", "version", "coverage", "cell"
};

/* Bit per singleton key, so "seen twice" is one test rather than nine flags. `file` and `sha256`
   are excluded: they repeat, paired. */
static const unsigned FONT_KEY_SINGLETON_MASK = (1U << FONT_KEY_FONT) | (1U << FONT_KEY_NAME) |
                                                (1U << FONT_KEY_SOURCE) |
                                                (1U << FONT_KEY_LICENCE) |
                                                (1U << FONT_KEY_VERSION) |
                                                (1U << FONT_KEY_COVERAGE) |
                                                (1U << FONT_KEY_CELL);

typedef struct {
    unsigned seen_mask;
    unsigned start_line;
} FaceProgress;

static FontManifestResult reject(FontManifestDiagnostic *diagnostic, FontManifestResult result,
                                 unsigned line, const char *key) {
    if (diagnostic) {
        diagnostic->result = result;
        diagnostic->line = line;
        diagnostic->key = key;
    }
    return result;
}

void font_manifest_init(FontManifest *manifest) {
    if (!manifest) return;
    memset(manifest, 0, sizeof(*manifest));
}

static bool trimmed_equals(const char *begin, size_t length, const char *literal) {
    size_t literal_length = strlen(literal);
    if (length != literal_length) return false;
    return memcmp(begin, literal, length) == 0;
}

static void trim(const char **begin, const char **end) {
    while (*begin < *end && (**begin == ' ' || **begin == '\t')) (*begin)++;
    while (*end > *begin && ((*end)[-1] == ' ' || (*end)[-1] == '\t' || (*end)[-1] == '\r'))
        (*end)--;
}

/* Copy a trimmed value into a fixed buffer. Over-length is a rejection, never a truncation:
   a truncated path or URL is a different font or a broken link. */
static FontManifestResult copy_value(char *destination, size_t capacity, const char *begin,
                                     const char *end, unsigned line, const char *key,
                                     FontManifestDiagnostic *diagnostic) {
    size_t length = (size_t)(end - begin);
    if (length == 0U) {
        return reject(diagnostic, FONT_MANIFEST_INVALID_VALUE, line, key);
    }
    if (length >= capacity) {
        return reject(diagnostic, FONT_MANIFEST_VALUE_TOO_LONG, line, key);
    }
    memcpy(destination, begin, length);
    destination[length] = '\0';
    return FONT_MANIFEST_OK;
}

static bool id_is_well_formed(const char *id) {
    size_t index;
    if (id[0] == '\0') return false;
    for (index = 0U; id[index] != '\0'; index++) {
        char character = id[index];
        bool lower = character >= 'a' && character <= 'z';
        bool digit = character >= '0' && character <= '9';
        if (!lower && !digit && character != '_' && character != '-') return false;
    }
    return true;
}

static bool span_is_cell_8x8(const char *begin, const char *end) {
    return trimmed_equals(begin, (size_t)(end - begin), "8x8");
}

/* Values are matched against the same fixed table for every key so a typo in a *key* is reported
   as an unknown key rather than silently landing in the wrong field. */
static int lookup_key(const char *begin, const char *end) {
    int index;
    for (index = 0; index < (int)FONT_KEY_COUNT; index++) {
        if (trimmed_equals(begin, (size_t)(end - begin), FONT_KEY_NAMES[index])) return index;
    }
    return -1;
}

/* Validate the face that was just parsed. Required keys are checked together so a rejection can
   name the face's own start line rather than the line that happened to close it. */
static FontManifestResult finish_face(FontManifest *manifest, const FaceProgress *progress,
                                      int pending_file, FontManifestDiagnostic *diagnostic) {
    const FontManifestFace *face = &manifest->faces[manifest->face_count - 1U];
    unsigned required = (1U << FONT_KEY_FONT) | (1U << FONT_KEY_NAME) | (1U << FONT_KEY_SOURCE) |
                        (1U << FONT_KEY_LICENCE) | (1U << FONT_KEY_CELL);
    unsigned line = progress->start_line;
    if ((progress->seen_mask & required) != required) {
        const char *missing = "font";
        if (!(progress->seen_mask & (1U << FONT_KEY_NAME))) missing = "name";
        else if (!(progress->seen_mask & (1U << FONT_KEY_SOURCE))) missing = "source";
        else if (!(progress->seen_mask & (1U << FONT_KEY_LICENCE))) missing = "licence";
        else if (!(progress->seen_mask & (1U << FONT_KEY_CELL))) missing = "cell";
        return reject(diagnostic, FONT_MANIFEST_MISSING_KEY, line, missing);
    }
    if (face->file_count == 0U) {
        return reject(diagnostic, FONT_MANIFEST_MISSING_KEY, line, "file");
    }
    if (pending_file >= 0) {
        return reject(diagnostic, FONT_MANIFEST_UNPAIRED_FILE, line, "sha256");
    }
    return FONT_MANIFEST_OK;
}

FontManifestResult font_manifest_read_text(const char *text, FontManifest *out_manifest,
                                           FontManifestDiagnostic *out_diagnostic) {
    FontManifest manifest;
    FaceProgress progress[FONT_MANIFEST_MAX_FACES];
    unsigned line = 0U;
    int pending_file = -1;
    const char *cursor;
    FontManifestResult result;

    if (out_diagnostic) {
        out_diagnostic->result = FONT_MANIFEST_OK;
        out_diagnostic->line = 0U;
        out_diagnostic->key = NULL;
    }
    if (!text || !out_manifest) {
        return reject(out_diagnostic, FONT_MANIFEST_INVALID_ARGUMENT, 0U, NULL);
    }
    if (strlen(text) >= (size_t)FONT_MANIFEST_TEXT_MAX) {
        return reject(out_diagnostic, FONT_MANIFEST_MALFORMED, 0U, NULL);
    }

    font_manifest_init(&manifest);
    memset(progress, 0, sizeof(progress));

    cursor = text;
    while (*cursor != '\0') {
        const char *line_end = strchr(cursor, '\n');
        const char *line_begin = cursor;
        const char *equals = NULL;
        const char *scan;
        const char *key_begin;
        const char *key_end;
        const char *value_begin;
        const char *value_end;
        FontManifestFace *face;
        FontManifestResult step = FONT_MANIFEST_OK;
        int key;

        line++;
        /* A final line without a terminating newline is accepted: this file is hand-edited, and
           insisting on a trailing newline would turn an editor quirk into a rejection. */
        if (!line_end) line_end = cursor + strlen(cursor);

        key_begin = line_begin;
        key_end = line_end;
        trim(&key_begin, &key_end);
        if (key_end == key_begin || *key_begin == '#') {
            cursor = (*line_end == '\n') ? line_end + 1 : line_end;
            continue;
        }

        /* The first '=' separates key from value; later ones belong to the value, so a URL with a
           query string is not a syntax error. */
        for (scan = line_begin; scan < line_end; scan++) {
            if (*scan == '=') {
                equals = scan;
                break;
            }
        }
        if (!equals) {
            return reject(out_diagnostic, FONT_MANIFEST_MALFORMED, line, NULL);
        }
        key_begin = line_begin;
        key_end = equals;
        value_begin = equals + 1;
        value_end = line_end;
        trim(&key_begin, &key_end);
        trim(&value_begin, &value_end);
        key = lookup_key(key_begin, key_end);
        if (key < 0) {
            return reject(out_diagnostic, FONT_MANIFEST_UNKNOWN_KEY, line, NULL);
        }
        if (manifest.face_count == 0U && key != (int)FONT_KEY_FONT) {
            /* Keys outside a face have no owner: a manifest whose first line is `name=` was
               written by someone guessing at the format. */
            return reject(out_diagnostic, FONT_MANIFEST_MISSING_KEY, line, "font");
        }

        if (key == (int)FONT_KEY_FONT) {
            if (manifest.face_count > 0U) {
                step = finish_face(&manifest, &progress[manifest.face_count - 1U], pending_file,
                                   out_diagnostic);
                if (step != FONT_MANIFEST_OK) return step;
            }
            if (manifest.face_count == FONT_MANIFEST_MAX_FACES) {
                return reject(out_diagnostic, FONT_MANIFEST_TOO_MANY_FACES, line, "font");
            }
            face = &manifest.faces[manifest.face_count];
            step = copy_value(face->id, sizeof(face->id), value_begin, value_end, line, "font",
                              out_diagnostic);
            if (step != FONT_MANIFEST_OK) return step;
            if (!id_is_well_formed(face->id)) {
                return reject(out_diagnostic, FONT_MANIFEST_INVALID_VALUE, line, "font");
            }
            if (font_manifest_find_face(&manifest, face->id)) {
                return reject(out_diagnostic, FONT_MANIFEST_DUPLICATE_FACE, line, "font");
            }
            manifest.face_count++;
            memset(&progress[manifest.face_count - 1U], 0, sizeof(FaceProgress));
            progress[manifest.face_count - 1U].seen_mask = 1U << FONT_KEY_FONT;
            progress[manifest.face_count - 1U].start_line = line;
            pending_file = -1;
            cursor = (*line_end == '\n') ? line_end + 1 : line_end;
            continue;
        }

        face = &manifest.faces[manifest.face_count - 1U];
        if ((FONT_KEY_SINGLETON_MASK & (1U << (unsigned)key)) != 0U) {
            if ((progress[manifest.face_count - 1U].seen_mask & (1U << (unsigned)key)) != 0U) {
                return reject(out_diagnostic, FONT_MANIFEST_DUPLICATE_KEY, line,
                              FONT_KEY_NAMES[key]);
            }
            progress[manifest.face_count - 1U].seen_mask |= 1U << (unsigned)key;
        }

        switch (key) {
        case FONT_KEY_NAME:
            step = copy_value(face->name, sizeof(face->name), value_begin, value_end, line,
                              "name", out_diagnostic);
            break;
        case FONT_KEY_SOURCE:
            step = copy_value(face->source, sizeof(face->source), value_begin, value_end, line,
                              "source", out_diagnostic);
            break;
        case FONT_KEY_LICENCE:
            step = copy_value(face->licence, sizeof(face->licence), value_begin, value_end, line,
                              "licence", out_diagnostic);
            break;
        case FONT_KEY_VERSION:
            step = copy_value(face->version, sizeof(face->version), value_begin, value_end, line,
                              "version", out_diagnostic);
            break;
        case FONT_KEY_COVERAGE:
            step = copy_value(face->coverage, sizeof(face->coverage), value_begin, value_end,
                              line, "coverage", out_diagnostic);
            break;
        case FONT_KEY_FILE:
            if (pending_file >= 0) {
                return reject(out_diagnostic, FONT_MANIFEST_UNPAIRED_FILE, line, "file");
            }
            if (face->file_count == FONT_MANIFEST_MAX_FILES) {
                return reject(out_diagnostic, FONT_MANIFEST_TOO_MANY_FILES, line, "file");
            }
            {
                unsigned slot = face->file_count;
                size_t other;
                step = copy_value(face->files[slot].file, sizeof(face->files[slot].file),
                                  value_begin, value_end, line, "file", out_diagnostic);
                if (step != FONT_MANIFEST_OK) break;
                /* The same file twice in one face means one of the hashes describes a file that
                   is not there, so it is a rejection rather than a silent no-op. */
                for (other = 0U; other < (size_t)slot; other++) {
                    if (strcmp(face->files[other].file, face->files[slot].file) == 0) {
                        return reject(out_diagnostic, FONT_MANIFEST_DUPLICATE_KEY, line, "file");
                    }
                }
                face->file_count++;
                pending_file = (int)slot;
            }
            break;
        case FONT_KEY_SHA256:
            if (pending_file < 0) {
                return reject(out_diagnostic, FONT_MANIFEST_UNPAIRED_FILE, line, "sha256");
            }
            {
                char hex[SHA256_HEX_SIZE];
                step = copy_value(hex, sizeof(hex), value_begin, value_end, line, "sha256",
                                  out_diagnostic);
                if (step != FONT_MANIFEST_OK) break;
                if (!sha256_from_hex(hex, face->files[pending_file].sha256)) {
                    return reject(out_diagnostic, FONT_MANIFEST_INVALID_VALUE, line, "sha256");
                }
                pending_file = -1;
            }
            break;
        case FONT_KEY_CELL:
            if (!span_is_cell_8x8(value_begin, value_end)) {
                return reject(out_diagnostic, FONT_MANIFEST_INVALID_CELL, line, "cell");
            }
            break;
        /* CASE_TAIL */
        default:
            return reject(out_diagnostic, FONT_MANIFEST_UNKNOWN_KEY, line, NULL);
        }
        if (step != FONT_MANIFEST_OK) return step;
        cursor = (*line_end == '\n') ? line_end + 1 : line_end;
    }

    if (manifest.face_count == 0U) {
        return reject(out_diagnostic, FONT_MANIFEST_MISSING_KEY, 0U, "font");
    }
    result = finish_face(&manifest, &progress[manifest.face_count - 1U], pending_file,
                         out_diagnostic);
    if (result != FONT_MANIFEST_OK) return result;

    *out_manifest = manifest;
    return FONT_MANIFEST_OK;
}

const FontManifestFace *font_manifest_find_face(const FontManifest *manifest, const char *id) {
    unsigned index;
    if (!manifest || !id) return NULL;
    for (index = 0U; index < manifest->face_count; index++) {
        if (strcmp(manifest->faces[index].id, id) == 0) return &manifest->faces[index];
    }
    return NULL;
}

const char *font_manifest_result_name(FontManifestResult result) {
    switch (result) {
    case FONT_MANIFEST_OK: return "OK";
    case FONT_MANIFEST_INVALID_ARGUMENT: return "INVALID_ARGUMENT";
    case FONT_MANIFEST_MALFORMED: return "MALFORMED";
    case FONT_MANIFEST_UNKNOWN_KEY: return "UNKNOWN_KEY";
    case FONT_MANIFEST_DUPLICATE_KEY: return "DUPLICATE_KEY";
    case FONT_MANIFEST_MISSING_KEY: return "MISSING_KEY";
    case FONT_MANIFEST_VALUE_TOO_LONG: return "VALUE_TOO_LONG";
    case FONT_MANIFEST_INVALID_VALUE: return "INVALID_VALUE";
    case FONT_MANIFEST_INVALID_CELL: return "INVALID_CELL";
    case FONT_MANIFEST_UNPAIRED_FILE: return "UNPAIRED_FILE";
    case FONT_MANIFEST_DUPLICATE_FACE: return "DUPLICATE_FACE";
    case FONT_MANIFEST_TOO_MANY_FACES: return "TOO_MANY_FACES";
    case FONT_MANIFEST_TOO_MANY_FILES: return "TOO_MANY_FILES";
    case FONT_MANIFEST_BUFFER_TOO_SMALL: return "BUFFER_TOO_SMALL";
    default: return "UNKNOWN_RESULT";
    }
}

/* Append one canonical line. Returns false when the buffer cannot hold it, which the caller turns
   into BUFFER_TOO_SMALL: a half-written manifest is worse than none. */
static bool append_line(char *buffer, size_t buffer_size, size_t *length, const char *key,
                        const char *value) {
    int written = snprintf(buffer + *length, buffer_size - *length, "%s=%s\n", key, value);
    if (written < 0 || (size_t)written >= buffer_size - *length) return false;
    *length += (size_t)written;
    return true;
}

FontManifestResult font_manifest_write_text(const FontManifest *manifest, char *buffer,
                                            size_t buffer_size, size_t *out_length) {
    static const char *const CELL_TEXT = "8x8";
    unsigned face_index;
    size_t length = 0U;
    if (!manifest || !buffer || buffer_size == 0U) return FONT_MANIFEST_INVALID_ARGUMENT;
    for (face_index = 0U; face_index < manifest->face_count; face_index++) {
        const FontManifestFace *face = &manifest->faces[face_index];
        unsigned file_index;
        if (face->file_count == 0U) return FONT_MANIFEST_MISSING_KEY;
        if (face->source[0] == '\0' || face->licence[0] == '\0' || face->name[0] == '\0' ||
            face->id[0] == '\0')
            return FONT_MANIFEST_MISSING_KEY;
        if (!append_line(buffer, buffer_size, &length, "font", face->id)) goto too_small;
        if (!append_line(buffer, buffer_size, &length, "name", face->name)) goto too_small;
        for (file_index = 0U; file_index < face->file_count; file_index++) {
            char hex[SHA256_HEX_SIZE];
            if (!append_line(buffer, buffer_size, &length, "file",
                             face->files[file_index].file))
                goto too_small;
            sha256_to_hex(face->files[file_index].sha256, hex);
            if (!append_line(buffer, buffer_size, &length, "sha256", hex)) goto too_small;
        }
        if (!append_line(buffer, buffer_size, &length, "source", face->source)) goto too_small;
        if (!append_line(buffer, buffer_size, &length, "licence", face->licence)) goto too_small;
        if (face->version[0] != '\0' &&
            !append_line(buffer, buffer_size, &length, "version", face->version))
            goto too_small;
        if (face->coverage[0] != '\0' &&
            !append_line(buffer, buffer_size, &length, "coverage", face->coverage))
            goto too_small;
        if (!append_line(buffer, buffer_size, &length, "cell", CELL_TEXT)) goto too_small;
    }
    if (out_length) *out_length = length;
    return FONT_MANIFEST_OK;
too_small:
    buffer[0] = '\0';
    return FONT_MANIFEST_BUFFER_TOO_SMALL;
}
