/**
 * mapgen_seed.c — sidecar text codec for generation parameters
 *
 * Parsing rules, all enforced rather than tolerated: one `key = value` per line, known keys
 * only, each key at most once, all required keys present, values validated by
 * mapgen_parameters_valid. No silent defaulting -- a sidecar with a missing key is a broken
 * record, not a request for a default, because defaulting would silently change the map a user
 * asked to reproduce.
 */

#include "mapgen_seed.h"

#include "scene_types.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *key;
    bool seen;
    uint64_t value;
} SeedField;

/* Exact, case-sensitive key compare over a trimmed span. */
static bool trimmed_equals(const char *begin, size_t length, const char *literal) {
    size_t literal_length = strlen(literal);
    if (length != literal_length) return false;
    return memcmp(begin, literal, length) == 0;
}

static void trim(const char **begin, const char **end) {
    while (*begin < *end && (**begin == ' ' || **begin == '\t')) (*begin)++;
    while (*end > *begin && ((*end)[-1] == ' ' || (*end)[-1] == '\t' ||
                            (*end)[-1] == '\r'))
        (*end)--;
}

MapgenSeedResult mapgen_seed_write_text(const MapgenParameters *parameters,
                                        char *buffer, size_t buffer_size,
                                        size_t *out_length) {
    int written;
    if (!parameters || !buffer) return MAPGEN_SEED_INVALID_ARGUMENT;
    if (!mapgen_parameters_valid(parameters)) return MAPGEN_SEED_INVALID_VALUE;
    written = snprintf(buffer, buffer_size,
                       "mapgen_version = %d\n"
                       "seed = %llu\n"
                       "width = %d\n"
                       "height = %d\n"
                       "solid_material = %d\n"
                       "room_attempts = %d\n"
                       "room_min_size = %d\n"
                       "room_max_size = %d\n",
                       MAPGEN_SEED_VERSION,
                       (unsigned long long)parameters->seed, parameters->width,
                       parameters->height, parameters->solid_material,
                       parameters->room_attempts, parameters->room_min_size,
                       parameters->room_max_size);
    if (written < 0 || (size_t)written >= buffer_size)
        return MAPGEN_SEED_BUFFER_TOO_SMALL;
    if (out_length) *out_length = (size_t)written;
    return MAPGEN_SEED_OK;
}

static bool parse_unsigned_value(const char *begin, const char *end, uint64_t *out) {
    char digits[32];
    size_t length = (size_t)(end - begin);
    char *parse_end = NULL;
    unsigned long long value;
    if (length == 0U || length >= sizeof(digits)) return false;
    if (begin[0] == '-') return false;
    memcpy(digits, begin, length);
    digits[length] = '\0';
    errno = 0;
    value = strtoull(digits, &parse_end, 10);
    if (errno != 0 || !parse_end || *parse_end != '\0') return false;
    *out = (uint64_t)value;
    return true;
}

MapgenSeedResult mapgen_seed_read_text(const char *text,
                                       MapgenParameters *out_parameters) {
    SeedField fields[] = {
        {"mapgen_version", false, 0U}, {"seed", false, 0U},
        {"width", false, 0U},          {"height", false, 0U},
        {"solid_material", false, 0U}, {"room_attempts", false, 0U},
        {"room_min_size", false, 0U},  {"room_max_size", false, 0U}
    };
    const size_t field_count = sizeof(fields) / sizeof(fields[0]);
    MapgenParameters parsed = mapgen_parameters_default();
    const char *cursor;
    size_t index;
    if (!text || !out_parameters) return MAPGEN_SEED_INVALID_ARGUMENT;
    if (strlen(text) >= MAPGEN_SEED_TEXT_MAX) return MAPGEN_SEED_MALFORMED;

    cursor = text;
    while (*cursor != '\0') {
        const char *line_end = strchr(cursor, '\n');
        const char *line_begin = cursor;
        const char *equals = NULL;
        const char *scan;
        bool matched = false;
        if (!line_end) return MAPGEN_SEED_MALFORMED;
        /* Find the first '=' and reject a second one: a value containing '=' is ambiguous. */
        for (scan = line_begin; scan < line_end; scan++) {
            if (*scan == '=') {
                if (equals) return MAPGEN_SEED_MALFORMED;
                equals = scan;
            }
        }
        if (equals) {
            const char *key_begin = line_begin;
            const char *key_end = equals;
            const char *value_begin = equals + 1;
            const char *value_end = line_end;
            uint64_t value = 0U;
            trim(&key_begin, &key_end);
            trim(&value_begin, &value_end);
            if (key_end == key_begin) return MAPGEN_SEED_MALFORMED;
            if (!parse_unsigned_value(value_begin, value_end, &value))
                return MAPGEN_SEED_MALFORMED;
            for (index = 0U; index < field_count; index++) {
                if (!trimmed_equals(key_begin, (size_t)(key_end - key_begin),
                                    fields[index].key))
                    continue;
                if (fields[index].seen) return MAPGEN_SEED_MALFORMED; /* duplicate */
                fields[index].seen = true;
                fields[index].value = value;
                matched = true;
                break;
            }
            if (!matched) return MAPGEN_SEED_MALFORMED; /* unknown key */
        } else {
            /* Tolerate blank lines, reject anything else without '='. */
            const char *blank_begin = line_begin;
            const char *blank_end = line_end;
            trim(&blank_begin, &blank_end);
            if (blank_end != blank_begin) return MAPGEN_SEED_MALFORMED;
        }
        cursor = line_end + 1;
    }

    for (index = 0U; index < field_count; index++) {
        if (!fields[index].seen) return MAPGEN_SEED_MALFORMED; /* missing key */
    }
    if (fields[0].value != (uint64_t)MAPGEN_SEED_VERSION)
        return MAPGEN_SEED_UNSUPPORTED_VERSION;

    /* Range-check before narrowing to int: a value that does not fit would otherwise truncate
       into a plausible-looking map. */
    if (fields[2].value > (uint64_t)SCENE_MAX_WIDTH ||
        fields[3].value > (uint64_t)SCENE_MAX_HEIGHT ||
        fields[4].value > (uint64_t)ASSET_ID_MAX ||
        fields[5].value > (uint64_t)INT_MAX ||
        fields[6].value > (uint64_t)INT_MAX ||
        fields[7].value > (uint64_t)INT_MAX)
        return MAPGEN_SEED_INVALID_VALUE;

    parsed.seed = fields[1].value;
    parsed.width = (int)fields[2].value;
    parsed.height = (int)fields[3].value;
    parsed.solid_material = (int)fields[4].value;
    parsed.room_attempts = (int)fields[5].value;
    parsed.room_min_size = (int)fields[6].value;
    parsed.room_max_size = (int)fields[7].value;
    /* The generator's own validator is the last word, so a sidecar cannot smuggle in something
       generation would refuse. */
    if (!mapgen_parameters_valid(&parsed)) return MAPGEN_SEED_INVALID_VALUE;
    *out_parameters = parsed;
    return MAPGEN_SEED_OK;
}

