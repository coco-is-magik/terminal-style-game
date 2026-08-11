#define _GNU_SOURCE

#include "scene_format.h"

#include "checked_size.h"

#include <float.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <locale.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *text;
    size_t number;
} SceneLine;

typedef struct {
    char *storage;
    size_t size;
    size_t offset;
    size_t line_number;
} LineReader;

typedef enum {
    SECTION_NONE = 0,
    SECTION_CELLS,
    SECTION_OCCUPANCY,
    SECTION_WALL_MATERIALS,
    SECTION_FLOOR_MATERIALS,
    SECTION_CEILING_MATERIALS,
    SECTION_LIGHT,
    SECTION_DECAL
} SectionKind;

typedef struct {
    SectionKind kind;
    SceneInstanceId id;
} SectionHeader;

typedef struct {
    char *data;
    size_t size;
    size_t capacity;
} TextWriter;

typedef void *(*SceneCallocFn)(size_t count, size_t size);
static SceneCallocFn g_scene_calloc = calloc;

void scene_format_set_allocator_for_test(
    void *(*calloc_fn)(size_t count, size_t size)
) {
    g_scene_calloc = calloc_fn ? calloc_fn : calloc;
}

void scene_format_reset_allocator_for_test(void) {
    g_scene_calloc = calloc;
}

enum {
    META_SCENE_TYPE = 1U << 0,
    META_VERSION = 1U << 1,
    META_NAME = 1U << 2,
    META_WIDTH = 1U << 3,
    META_HEIGHT = 1U << 4,
    META_ORIGIN_X = 1U << 5,
    META_ORIGIN_Y = 1U << 6,
    META_NEXT_ID = 1U << 7,
    META_AMBIENT = 1U << 8,
    META_SPAWN = 1U << 9,
    META_LEGACY_PATH = 1U << 10
};

#define META_REQUIRED (META_SCENE_TYPE | META_VERSION | META_NAME | META_WIDTH | \
                       META_HEIGHT | META_ORIGIN_X | META_ORIGIN_Y | META_NEXT_ID | \
                       META_AMBIENT | META_SPAWN)

static void set_error(SceneDiagnostic *diagnostic, SceneDiagnosticCode code,
                      const char *path, const char *section, const char *field,
                      const char *detail, size_t line, SceneInstanceId id) {
    scene_diagnostic_set(diagnostic, code, SCENE_DIAGNOSTIC_SEVERITY_ERROR,
                         path, section, field, detail);
    if (diagnostic) {
        diagnostic->line = line;
        diagnostic->instance_id = id;
    }
}

static SceneFormatResult reject(SceneDiagnostic *diagnostic,
                                SceneDiagnosticCode code, const char *path,
                                const char *section, const char *field,
                                const char *detail, size_t line,
                                SceneInstanceId id) {
    set_error(diagnostic, code, path, section, field, detail, line, id);
    return SCENE_FORMAT_REJECTED;
}

void scene_format_candidate_init(SceneFormatCandidate *candidate) {
    if (!candidate) return;
    memset(candidate, 0, sizeof(*candidate));
    candidate->next_instance_id = 1U;
}

void scene_format_candidate_destroy(SceneFormatCandidate *candidate) {
    if (!candidate) return;
    free(candidate->map.cells);
    free(candidate->map.light_map);
    free(candidate->authored_cells);
    free(candidate->lights);
    free(candidate->decals);
    free(candidate->legacy_source_path);
    scene_format_candidate_init(candidate);
}

void scene_format_buffer_destroy(SceneFormatBuffer *buffer) {
    if (!buffer) return;
    free(buffer->data);
    buffer->data = NULL;
    buffer->size = 0U;
}

static char *trim(char *text) {
    char *end;
    while (*text == ' ' || *text == '\t') text++;
    end = text + strlen(text);
    while (end > text && (end[-1] == ' ' || end[-1] == '\t')) end--;
    *end = '\0';
    return text;
}

static bool line_reader_next(LineReader *reader, SceneLine *line) {
    size_t start;
    size_t end;
    if (reader->offset >= reader->size) return false;
    start = reader->offset;
    end = start;
    while (end < reader->size && reader->storage[end] != '\n' &&
           reader->storage[end] != '\r') {
        end++;
    }
    reader->line_number++;
    line->number = reader->line_number;
    line->text = reader->storage + start;
    if (end < reader->size) {
        char terminator = reader->storage[end];
        reader->storage[end++] = '\0';
        if (terminator == '\r') {
            if (end >= reader->size || reader->storage[end] != '\n') {
                reader->offset = reader->size;
                line->text = NULL;
                return true;
            }
            reader->storage[end++] = '\0';
        }
    }
    reader->offset = end;
    return true;
}

static bool valid_scene_name(const char *name) {
    size_t i;
    size_t length;
    if (!name) return false;
    length = strlen(name);
    if (length == 0U || length > SCENE_NAME_MAX) return false;
    for (i = 0U; i < length; i++) {
        unsigned char c = (unsigned char)name[i];
        if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-')) return false;
    }
    return true;
}

static bool valid_quoted_bytes(const char *text, size_t maximum) {
    size_t i;
    size_t length;
    if (!text) return false;
    length = strlen(text);
    if (length > maximum) return false;
    for (i = 0U; i < length; i++) {
        unsigned char c = (unsigned char)text[i];
        if (c < 0x20U || c == 0x7fU) return false;
    }
    return true;
}

static bool split_property(char *text, char **key, char **value) {
    char *equals = strchr(text, '=');
    if (!equals || strchr(equals + 1, '=')) return false;
    *equals = '\0';
    *key = trim(text);
    *value = trim(equals + 1);
    if (**key == '\0' || **value == '\0') return false;
    return true;
}

static bool parse_u64(const char *text, SceneInstanceId *value) {
    uint64_t result = 0U;
    const unsigned char *cursor = (const unsigned char *)text;
    if (!text || !*text) return false;
    while (*cursor) {
        unsigned digit;
        if (*cursor < '0' || *cursor > '9') return false;
        digit = (unsigned)(*cursor - '0');
        if (result > (UINT64_MAX - digit) / 10U) return false;
        result = result * 10U + digit;
        cursor++;
    }
    *value = result;
    return true;
}

static bool parse_uint_range(const char *text, unsigned maximum, unsigned *value) {
    SceneInstanceId parsed;
    if (!parse_u64(text, &parsed) || parsed > maximum) return false;
    *value = (unsigned)parsed;
    return true;
}

static bool float_grammar(const char *text) {
    const unsigned char *p = (const unsigned char *)text;
    bool before = false;
    bool after = false;
    if (*p == '+' || *p == '-') p++;
    while (*p >= '0' && *p <= '9') { before = true; p++; }
    if (*p == '.') {
        p++;
        while (*p >= '0' && *p <= '9') { after = true; p++; }
    }
    if (!before && !after) return false;
    if (*p == 'e' || *p == 'E') {
        bool exponent = false;
        p++;
        if (*p == '+' || *p == '-') p++;
        while (*p >= '0' && *p <= '9') { exponent = true; p++; }
        if (!exponent) return false;
    }
    return *p == '\0';
}

static bool parse_double_c(const char *text, double *value) {
    locale_t c_locale;
    char *end = NULL;
    double parsed;
    if (!float_grammar(text)) return false;
    c_locale = newlocale(LC_NUMERIC_MASK, "C", (locale_t)0);
    if (c_locale == (locale_t)0) return false;
    errno = 0;
    parsed = strtod_l(text, &end, c_locale);
    freelocale(c_locale);
    if (errno == ERANGE || !end || *end != '\0' || parsed != parsed ||
        parsed > DBL_MAX || parsed < -DBL_MAX) return false;
    *value = parsed;
    return true;
}

static bool split_tuple(char *text, char **parts, size_t count) {
    size_t i;
    char *cursor = text;
    for (i = 0U; i < count; i++) {
        char *comma;
        parts[i] = trim(cursor);
        if (*parts[i] == '\0') return false;
        comma = strchr(cursor, ',');
        if (i + 1U == count) return comma == NULL;
        if (!comma) return false;
        *comma = '\0';
        cursor = comma + 1;
    }
    return true;
}

static bool parse_double_tuple(char *text, double *values, size_t count) {
    char *parts[4];
    size_t i;
    if (count > sizeof(parts) / sizeof(parts[0]) ||
        !split_tuple(text, parts, count)) return false;
    for (i = 0U; i < count; i++) {
        if (!parse_double_c(parts[i], &values[i])) return false;
    }
    return true;
}

static bool parse_quoted(const char *text, char *output, size_t capacity) {
    size_t used = 0U;
    const unsigned char *p = (const unsigned char *)text;
    if (!text || *p++ != '"' || capacity == 0U) return false;
    while (*p && *p != '"') {
        unsigned char c = *p++;
        if (c == '\\') {
            c = *p++;
            if (c != '\\' && c != '"') return false;
        }
        if (c < 0x20U || c == 0x7fU || used + 1U >= capacity) return false;
        output[used++] = (char)c;
    }
    if (*p++ != '"' || *p != '\0') return false;
    output[used] = '\0';
    return true;
}

static bool parse_header(char *text, SectionHeader *header) {
    char *inside;
    char *space;
    size_t length = strlen(text);
    if (length < 3U || text[0] != '[' || text[length - 1U] != ']') return false;
    text[length - 1U] = '\0';
    inside = text + 1;
    if (strcmp(inside, "cells") == 0) {
        header->kind = SECTION_CELLS;
        header->id = 0U;
        return true;
    }
    if (strcmp(inside, "occupancy") == 0) {
        header->kind = SECTION_OCCUPANCY;
        header->id = 0U;
        return true;
    }
    if (strcmp(inside, "wall_materials") == 0) {
        header->kind = SECTION_WALL_MATERIALS;
        header->id = 0U;
        return true;
    }
    if (strcmp(inside, "floor_materials") == 0) {
        header->kind = SECTION_FLOOR_MATERIALS;
        header->id = 0U;
        return true;
    }
    if (strcmp(inside, "ceiling_materials") == 0) {
        header->kind = SECTION_CEILING_MATERIALS;
        header->id = 0U;
        return true;
    }
    space = strchr(inside, ' ');
    if (!space || strchr(space + 1, ' ')) return false;
    *space++ = '\0';
    if (!parse_u64(space, &header->id)) return false;
    if (strcmp(inside, "light") == 0) header->kind = SECTION_LIGHT;
    else if (strcmp(inside, "decal_instance") == 0) header->kind = SECTION_DECAL;
    else return false;
    return true;
}

static bool passable_spawn(const SceneFormatCandidate *candidate) {
    int x;
    int y;
    size_t index;
    if (candidate->spawn_x < 0.0 || candidate->spawn_y < 0.0 ||
        candidate->spawn_x >= (double)candidate->map.width ||
        candidate->spawn_y >= (double)candidate->map.height) return false;
    x = (int)candidate->spawn_x;
    y = (int)candidate->spawn_y;
    index = (size_t)y * (size_t)candidate->map.width + (size_t)x;
    if (candidate->source_version == SCENE_VERSION_V2 &&
        candidate->authored_cells && index < candidate->authored_cell_count) {
        return candidate->authored_cells[index].occupancy ==
               SCENE_CELL_OCCUPANCY_EMPTY;
    }
    return candidate->map.cells[index].material_id == 0;
}

static bool candidate_has_id(const SceneFormatCandidate *candidate,
                             SceneInstanceId id) {
    size_t i;
    for (i = 0U; i < candidate->light_count; i++) {
        if (candidate->lights[i].id == id) return true;
    }
    for (i = 0U; i < candidate->decal_count; i++) {
        if (candidate->decals[i].id == id) return true;
    }
    return false;
}

SceneFormatResult scene_format_migrate_v1_to_v2(
    SceneFormatCandidate *candidate,
    unsigned int default_material,
    SceneDiagnostic *diagnostic
) {
    SceneAuthoredCell *cells;
    size_t count;
    size_t bytes;
    size_t i;
    SceneFormatResult result;

    if (diagnostic) scene_diagnostic_reset(diagnostic);
    if (!candidate) return SCENE_FORMAT_INVALID_ARGUMENT;
    if (candidate->source_version != SCENE_VERSION_V1 ||
        candidate->authored_cells || candidate->authored_cell_count != 0U) {
        return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_UNSUPPORTED_VERSION,
                      NULL, NULL, "scene_version",
                      "candidate is not an unmigrated v1 scene", 0U, 0U);
    }
    if (default_material < 1U || default_material > 255U) {
        return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_NUMERIC, NULL, NULL,
                      "default_material", "default material outside 1..255",
                      0U, 0U);
    }
    result = scene_format_validate(candidate, NULL, diagnostic);
    if (result != SCENE_FORMAT_OK) return result;
    if (!checked_size_2d(candidate->map.width, candidate->map.height, &count) ||
        !checked_size_bytes(count, sizeof(*cells), &bytes)) {
        return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_DIMENSIONS, NULL, NULL,
                      NULL, "invalid authored-cell allocation size", 0U, 0U);
    }
    (void)bytes;
    cells = g_scene_calloc(count, sizeof(*cells));
    if (!cells) {
        set_error(diagnostic, SCENE_DIAGNOSTIC_ENV_ALLOCATION, NULL, NULL, NULL,
                  "authored-cell migration allocation failed", 0U, 0U);
        return SCENE_FORMAT_OUT_OF_MEMORY;
    }
    for (i = 0U; i < count; i++) {
        int material = candidate->map.cells[i].material_id;
        cells[i].occupancy = material == 0
            ? SCENE_CELL_OCCUPANCY_EMPTY
            : SCENE_CELL_OCCUPANCY_WALL;
        cells[i].wall_material = (uint8_t)(material == 0
            ? default_material
            : (unsigned int)material);
        cells[i].floor_material = (uint8_t)default_material;
        cells[i].ceiling_material = (uint8_t)default_material;
    }
    candidate->authored_cells = cells;
    candidate->authored_cell_count = count;
    candidate->source_version = SCENE_VERSION_V2;
    return SCENE_FORMAT_OK;
}

SceneFormatResult scene_format_validate(const SceneFormatCandidate *candidate,
                                        const char *path,
                                        SceneDiagnostic *diagnostic) {
    size_t count;
    size_t i;
    SceneInstanceId maximum_id = 0U;
    if (diagnostic) scene_diagnostic_reset(diagnostic);
    if (!candidate) return SCENE_FORMAT_INVALID_ARGUMENT;
    if (!valid_scene_name(candidate->name))
        return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_SYNTAX, path, NULL,
                      "name", "invalid scene name", 0U, 0U);
    if (candidate->map.width < 1 || candidate->map.width > SCENE_MAX_WIDTH ||
        candidate->map.height < 1 || candidate->map.height > SCENE_MAX_HEIGHT ||
        !checked_size_2d(candidate->map.width, candidate->map.height, &count) ||
        !candidate->map.cells) {
        return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_DIMENSIONS, path, NULL,
                      NULL, "invalid scene dimensions or cells", 0U, 0U);
    }
    for (i = 0U; i < count; i++) {
        if (candidate->map.cells[i].material_id < 0 ||
            candidate->map.cells[i].material_id > 255) {
            return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_NUMERIC, path,
                          "cells", NULL, "material ID outside 000..255", 0U, 0U);
        }
    }
    if (candidate->source_version == SCENE_VERSION_V2) {
        if (!candidate->authored_cells || candidate->authored_cell_count != count) {
            return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_DIMENSIONS, path,
                          NULL, NULL, "invalid v2 authored-cell count", 0U, 0U);
        }
        for (i = 0U; i < count; i++) {
            const SceneAuthoredCell *cell = &candidate->authored_cells[i];
            if ((cell->occupancy != SCENE_CELL_OCCUPANCY_EMPTY &&
                 cell->occupancy != SCENE_CELL_OCCUPANCY_WALL) ||
                cell->wall_material == 0U || cell->floor_material == 0U ||
                cell->ceiling_material == 0U) {
                return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_NUMERIC, path,
                              "authored_cells", NULL,
                              "invalid v2 occupancy or material reference", 0U, 0U);
            }
        }
    }
    if (candidate->ambient_intensity != candidate->ambient_intensity ||
        candidate->ambient_intensity < 0.0 || candidate->ambient_intensity > 1.0 ||
        candidate->spawn_x != candidate->spawn_x ||
        candidate->spawn_y != candidate->spawn_y ||
        candidate->spawn_angle != candidate->spawn_angle ||
        candidate->spawn_angle > DBL_MAX || candidate->spawn_angle < -DBL_MAX ||
        !passable_spawn(candidate)) {
        return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_NUMERIC, path, NULL,
                      "spawn", "ambient or spawn is invalid", 0U, 0U);
    }
    if (candidate->light_count > SCENE_MAX_LIGHTS ||
        candidate->decal_count > SCENE_MAX_DECALS ||
        (candidate->light_count && !candidate->lights) ||
        (candidate->decal_count && !candidate->decals)) {
        return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_DIMENSIONS, path, NULL,
                      NULL, "instance count exceeds v1 limit", 0U, 0U);
    }
    if (candidate->legacy_source_path &&
        !valid_quoted_bytes(candidate->legacy_source_path, SCENE_PATH_MAX)) {
        return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_SYNTAX, path, NULL,
                      "legacy_source_path", "provenance path is too long", 0U, 0U);
    }
    for (i = 0U; i < candidate->light_count; i++) {
        const SceneLight *light = &candidate->lights[i];
        size_t j;
        if (light->id == 0U) {
            return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_INSTANCE_ID, path,
                          "light", NULL, "zero or duplicate instance ID", 0U, light->id);
        }
        for (j = 0U; j < i; j++) {
            if (candidate->lights[j].id == light->id)
                return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_INSTANCE_ID, path,
                              "light", NULL, "duplicate instance ID", 0U, light->id);
        }
        if (light->x != light->x || light->y != light->y || light->x < 0.0 ||
            light->y < 0.0 || light->x >= candidate->map.width ||
            light->y >= candidate->map.height || light->intensity != light->intensity ||
            light->intensity > DBL_MAX || light->intensity < -DBL_MAX ||
            light->radius != light->radius || light->radius > DBL_MAX ||
            light->radius <= 0.0) {
            return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_NUMERIC, path,
                          "light", NULL, "invalid light value", 0U, light->id);
        }
        if (light->id > maximum_id) maximum_id = light->id;
    }
    for (i = 0U; i < candidate->decal_count; i++) {
        const SceneDecalInstance *decal = &candidate->decals[i];
        size_t j;
        if (decal->id == 0U) {
            return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_INSTANCE_ID, path,
                          "decal_instance", NULL, "zero instance ID", 0U, decal->id);
        }
        for (j = 0U; j < candidate->light_count; j++) {
            if (candidate->lights[j].id == decal->id)
                return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_INSTANCE_ID, path,
                              "decal_instance", NULL, "duplicate instance ID", 0U,
                              decal->id);
        }
        for (j = 0U; j < i; j++) {
            if (candidate->decals[j].id == decal->id)
                return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_INSTANCE_ID, path,
                              "decal_instance", NULL, "duplicate instance ID", 0U,
                              decal->id);
        }
        if (decal->asset.kind != SCENE_ASSET_KIND_DECAL_PATTERN ||
            decal->asset.id < 1U || decal->asset.id > 255U ||
            decal->surface < SCENE_DECAL_SURFACE_WALL ||
            decal->surface > SCENE_DECAL_SURFACE_CEILING ||
            decal->width != decal->width || decal->width > DBL_MAX || decal->width <= 0.0 ||
            decal->height != decal->height || decal->height > DBL_MAX || decal->height <= 0.0 ||
            decal->glyph_step_u != decal->glyph_step_u || decal->glyph_step_u > DBL_MAX ||
            decal->glyph_step_u < 0.0 || decal->glyph_step_v != decal->glyph_step_v ||
            decal->glyph_step_v > DBL_MAX || decal->glyph_step_v < 0.0 ||
            decal->depth != decal->depth || decal->depth > DBL_MAX || decal->depth < 0.0 ||
            decal->rotation != decal->rotation || decal->rotation > DBL_MAX ||
            decal->rotation < -DBL_MAX) {
            return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_NUMERIC, path,
                          "decal_instance", NULL, "invalid decal value", 0U, decal->id);
        }
        if (decal->surface == SCENE_DECAL_SURFACE_WALL) {
            if (decal->map_x < 0 || decal->map_x >= candidate->map.width ||
                decal->map_y < 0 || decal->map_y >= candidate->map.height ||
                (decal->side != 0 && decal->side != 1) || decal->u != decal->u ||
                decal->u < 0.0 || decal->u > 1.0 || decal->v != decal->v ||
                decal->v < 0.0 || decal->v > 1.0) {
                return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_NUMERIC, path,
                              "decal_instance", NULL, "invalid wall placement", 0U,
                              decal->id);
            }
        } else if (decal->x != decal->x || decal->y != decal->y || decal->z != decal->z ||
                   decal->z > DBL_MAX || decal->z < -DBL_MAX ||
                   decal->x < 0.0 || decal->x >= candidate->map.width ||
                   decal->y < 0.0 || decal->y >= candidate->map.height) {
            return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_NUMERIC, path,
                          "decal_instance", NULL, "invalid plane placement", 0U,
                          decal->id);
        }
        if (decal->id > maximum_id) maximum_id = decal->id;
    }
    if (candidate->next_instance_id == 0U ||
        candidate->next_instance_id <= maximum_id) {
        return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_HIGH_WATER, path, NULL,
                      "next_instance_id", "ID high-water mark is invalid", 0U, 0U);
    }
    return SCENE_FORMAT_OK;
}

static SceneFormatResult allocate_candidate_arrays(SceneFormatCandidate *candidate,
                                                   size_t lights, size_t decals,
                                                   const char *path,
                                                   SceneDiagnostic *diagnostic) {
    size_t count;
    if (!checked_size_2d(candidate->map.width, candidate->map.height, &count))
        return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_DIMENSIONS, path, NULL,
                      NULL, "invalid dimensions", 0U, 0U);
    candidate->map.cells = calloc(count, sizeof(*candidate->map.cells));
    if (!candidate->map.cells) goto allocation_failed;
    /* light_map must be allocated here, not just in map_create().  The
     * renderer and lighting_update both check map->light_map != NULL and
     * silently fall back to full-brightness if it is missing.  Without this
     * allocation, native-loaded scenes appear fully lit with no point-light
     * or ambient effect. */
    candidate->map.light_map = calloc(count, sizeof(*candidate->map.light_map));
    if (!candidate->map.light_map) goto allocation_failed;
    if (candidate->source_version == SCENE_VERSION_V2) {
        candidate->authored_cells = calloc(count, sizeof(*candidate->authored_cells));
        if (!candidate->authored_cells) goto allocation_failed;
        candidate->authored_cell_count = count;
    }
    if (lights) {
        candidate->lights = calloc(lights, sizeof(*candidate->lights));
        if (!candidate->lights) goto allocation_failed;
    }
    if (decals) {
        candidate->decals = calloc(decals, sizeof(*candidate->decals));
        if (!candidate->decals) goto allocation_failed;
    }
    return SCENE_FORMAT_OK;
allocation_failed:
    set_error(diagnostic, SCENE_DIAGNOSTIC_ENV_ALLOCATION, path, NULL, NULL,
              "scene candidate allocation failed", 0U, 0U);
    return SCENE_FORMAT_OUT_OF_MEMORY;
}

static unsigned metadata_bit(const char *key) {
    if (strcmp(key, "scene_type") == 0) return META_SCENE_TYPE;
    if (strcmp(key, "scene_version") == 0) return META_VERSION;
    if (strcmp(key, "name") == 0) return META_NAME;
    if (strcmp(key, "width") == 0) return META_WIDTH;
    if (strcmp(key, "height") == 0) return META_HEIGHT;
    if (strcmp(key, "origin_x") == 0) return META_ORIGIN_X;
    if (strcmp(key, "origin_y") == 0) return META_ORIGIN_Y;
    if (strcmp(key, "next_instance_id") == 0) return META_NEXT_ID;
    if (strcmp(key, "ambient_intensity") == 0) return META_AMBIENT;
    if (strcmp(key, "spawn") == 0) return META_SPAWN;
    if (strcmp(key, "legacy_source_path") == 0) return META_LEGACY_PATH;
    return 0U;
}

static unsigned metadata_bit_from_property(const char *text) {
    const char *equals = strchr(text, '=');
    const char *start = text;
    const char *end;
    char key[32];
    size_t length;
    if (!equals || strchr(equals + 1, '=')) return 0U;
    while (*start == ' ' || *start == '\t') start++;
    end = equals;
    while (end > start && (end[-1] == ' ' || end[-1] == '\t')) end--;
    length = (size_t)(end - start);
    if (length == 0U || length >= sizeof(key)) return 0U;
    memcpy(key, start, length);
    key[length] = '\0';
    return metadata_bit(key);
}

static SceneFormatResult parse_metadata_value(SceneFormatCandidate *candidate,
                                              unsigned bit, char *value,
                                              const char *path, size_t line,
                                              SceneDiagnostic *diagnostic) {
    unsigned parsed;
    double tuple[3];
    char decoded[SCENE_PATH_MAX + 1U];
    switch (bit) {
        case META_SCENE_TYPE:
            if (strcmp(value, "terminal_scene") != 0)
                return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_UNSUPPORTED_VERSION,
                              path, NULL, "scene_type", "unsupported scene type", line, 0U);
            break;
        case META_VERSION:
            if (!parse_uint_range(value, UINT_MAX, &parsed) ||
                (parsed != SCENE_VERSION_V1 && parsed != SCENE_VERSION_V2))
                return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_UNSUPPORTED_VERSION,
                              path, NULL, "scene_version", "unsupported scene version",
                              line, 0U);
            candidate->source_version = parsed;
            break;
        case META_NAME:
            if (!parse_quoted(value, candidate->name, sizeof(candidate->name)) ||
                !valid_scene_name(candidate->name))
                return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_SYNTAX, path, NULL,
                              "name", "invalid quoted scene name", line, 0U);
            break;
        case META_WIDTH:
        case META_HEIGHT:
            if (!parse_uint_range(value, bit == META_WIDTH ? SCENE_MAX_WIDTH : SCENE_MAX_HEIGHT,
                                  &parsed) || parsed == 0U)
                return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_DIMENSIONS, path, NULL,
                              bit == META_WIDTH ? "width" : "height",
                              "dimension outside v1 bounds", line, 0U);
            if (bit == META_WIDTH) candidate->map.width = (int)parsed;
            else candidate->map.height = (int)parsed;
            break;
        case META_ORIGIN_X:
        case META_ORIGIN_Y:
            if (!parse_uint_range(value, 0U, &parsed))
                return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_DIMENSIONS, path, NULL,
                              bit == META_ORIGIN_X ? "origin_x" : "origin_y",
                              "v1 origin must be zero", line, 0U);
            break;
        case META_NEXT_ID:
            if (!parse_u64(value, &candidate->next_instance_id))
                return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_NUMERIC, path, NULL,
                              "next_instance_id", "invalid decimal instance ID", line, 0U);
            break;
        case META_AMBIENT:
            if (!parse_double_c(value, &candidate->ambient_intensity))
                return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_NUMERIC, path, NULL,
                              "ambient_intensity", "invalid decimal float", line, 0U);
            break;
        case META_SPAWN:
            if (!parse_double_tuple(value, tuple, 3U))
                return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_NUMERIC, path, NULL,
                              "spawn", "invalid spawn tuple", line, 0U);
            candidate->spawn_x = tuple[0];
            candidate->spawn_y = tuple[1];
            candidate->spawn_angle = tuple[2];
            break;
        case META_LEGACY_PATH:
            if (!parse_quoted(value, decoded, sizeof(decoded)))
                return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_SYNTAX, path, NULL,
                              "legacy_source_path", "invalid quoted provenance", line, 0U);
            candidate->legacy_source_path = malloc(strlen(decoded) + 1U);
            if (!candidate->legacy_source_path) {
                set_error(diagnostic, SCENE_DIAGNOSTIC_ENV_ALLOCATION, path, NULL,
                          "legacy_source_path", "provenance allocation failed", line, 0U);
                return SCENE_FORMAT_OUT_OF_MEMORY;
            }
            memcpy(candidate->legacy_source_path, decoded, strlen(decoded) + 1U);
            break;
        default: return SCENE_FORMAT_REJECTED;
    }
    return SCENE_FORMAT_OK;
}

static SceneFormatResult parse_cells_row(SceneFormatCandidate *candidate, char *text,
                                         size_t row, const char *path, size_t line,
                                         SceneDiagnostic *diagnostic) {
    size_t column = 0U;
    char *cursor = text;
    while (*cursor) {
        unsigned value;
        char token[4];
        while (*cursor == ' ' || *cursor == '\t') cursor++;
        if (*cursor == '\0') break;
        if (strlen(cursor) < 3U || cursor[0] < '0' || cursor[0] > '9' ||
            cursor[1] < '0' || cursor[1] > '9' || cursor[2] < '0' || cursor[2] > '9' ||
            (cursor[3] != '\0' && cursor[3] != ' ' && cursor[3] != '\t')) {
            return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_SYNTAX, path, "cells",
                          NULL, "cells require three-digit tokens", line, 0U);
        }
        if (column >= (size_t)candidate->map.width)
            return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_DIMENSIONS, path, "cells",
                          NULL, "too many cells in row", line, 0U);
        memcpy(token, cursor, 3U); token[3] = '\0';
        if (!parse_uint_range(token, 255U, &value))
            return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_NUMERIC, path, "cells",
                          NULL, "material ID outside 000..255", line, 0U);
        candidate->map.cells[row * (size_t)candidate->map.width + column].material_id =
            (int)value;
        column++;
        cursor += 3;
    }
    if (column != (size_t)candidate->map.width)
        return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_DIMENSIONS, path, "cells",
                      NULL, "wrong cell count in row", line, 0U);
    return SCENE_FORMAT_OK;
}

static SceneFormatResult parse_v2_row(SceneFormatCandidate *candidate,
                                      SectionKind section, char *text, size_t row,
                                      const char *path, size_t line,
                                      SceneDiagnostic *diagnostic) {
    size_t column = 0U;
    char *cursor = text;
    while (*cursor) {
        unsigned value;
        char token[4];
        size_t index;
        while (*cursor == ' ' || *cursor == '\t') cursor++;
        if (*cursor == '\0') break;
        if (section == SECTION_OCCUPANCY) {
            if ((cursor[0] != '0' && cursor[0] != '1') ||
                (cursor[1] != '\0' && cursor[1] != ' ' && cursor[1] != '\t')) {
                return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_SYNTAX, path,
                              "occupancy", NULL, "occupancy requires 0 or 1 tokens",
                              line, 0U);
            }
            value = (unsigned)(cursor[0] - '0');
            cursor += 1;
        } else {
            if (strlen(cursor) < 3U || cursor[0] < '0' || cursor[0] > '9' ||
                cursor[1] < '0' || cursor[1] > '9' || cursor[2] < '0' ||
                cursor[2] > '9' ||
                (cursor[3] != '\0' && cursor[3] != ' ' && cursor[3] != '\t')) {
                return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_SYNTAX, path,
                              section == SECTION_WALL_MATERIALS ? "wall_materials" :
                              section == SECTION_FLOOR_MATERIALS ? "floor_materials" :
                              "ceiling_materials", NULL,
                              "material grids require three-digit tokens", line, 0U);
            }
            memcpy(token, cursor, 3U);
            token[3] = '\0';
            if (!parse_uint_range(token, 255U, &value) || value == 0U) {
                return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_NUMERIC, path,
                              NULL, NULL, "v2 material outside 001..255", line, 0U);
            }
            cursor += 3;
        }
        if (column >= (size_t)candidate->map.width) {
            return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_DIMENSIONS, path,
                          NULL, NULL, "too many v2 grid cells in row", line, 0U);
        }
        index = row * (size_t)candidate->map.width + column++;
        if (section == SECTION_OCCUPANCY)
            candidate->authored_cells[index].occupancy = (SceneCellOccupancy)value;
        else if (section == SECTION_WALL_MATERIALS)
            candidate->authored_cells[index].wall_material = (uint8_t)value;
        else if (section == SECTION_FLOOR_MATERIALS)
            candidate->authored_cells[index].floor_material = (uint8_t)value;
        else
            candidate->authored_cells[index].ceiling_material = (uint8_t)value;
    }
    if (column != (size_t)candidate->map.width) {
        return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_DIMENSIONS, path, NULL,
                      NULL, "wrong number of v2 grid cells in row", line, 0U);
    }
    return SCENE_FORMAT_OK;
}

static SceneFormatResult parse_light_field(SceneLight *light, unsigned *seen,
                                           char *key, char *value, const char *path,
                                           size_t line, SceneDiagnostic *diagnostic) {
    unsigned bit = 0U;
    double tuple[2];
    char *parts[4];
    unsigned channels[4];
    size_t i;
    if (strcmp(key, "position") == 0) bit = 1U;
    else if (strcmp(key, "color") == 0) bit = 2U;
    else if (strcmp(key, "intensity") == 0) bit = 4U;
    else if (strcmp(key, "radius") == 0) bit = 8U;
    else return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_UNKNOWN, path, "light",
                       key, "unknown light field", line, light->id);
    if (*seen & bit) return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_DUPLICATE, path,
                                  "light", key, "duplicate light field", line, light->id);
    *seen |= bit;
    if (bit == 1U) {
        if (!parse_double_tuple(value, tuple, 2U)) goto numeric;
        light->x = tuple[0]; light->y = tuple[1];
    } else if (bit == 2U) {
        if (!split_tuple(value, parts, 4U)) goto numeric;
        for (i = 0U; i < 4U; i++) if (!parse_uint_range(parts[i], 255U, &channels[i])) goto numeric;
        light->red = (uint8_t)channels[0]; light->green = (uint8_t)channels[1];
        light->blue = (uint8_t)channels[2]; light->alpha = (uint8_t)channels[3];
    } else if (bit == 4U) {
        if (!parse_double_c(value, &light->intensity)) goto numeric;
    } else if (!parse_double_c(value, &light->radius)) goto numeric;
    return SCENE_FORMAT_OK;
numeric:
    return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_NUMERIC, path, "light", key,
                  "invalid light field value", line, light->id);
}

static SceneFormatResult parse_decal_field(SceneDecalInstance *decal, unsigned *seen,
                                           char *key, char *value, const char *path,
                                           size_t line, SceneDiagnostic *diagnostic) {
    unsigned bit = 0U;
    double tuple[3];
    unsigned number;
    if (strcmp(key, "asset_kind") == 0) bit = 1U;
    else if (strcmp(key, "asset_id") == 0) bit = 2U;
    else if (strcmp(key, "surface") == 0) bit = 4U;
    else if (strcmp(key, "anchor") == 0) bit = 8U;
    else if (strcmp(key, "uv") == 0) bit = 16U;
    else if (strcmp(key, "position") == 0) bit = 32U;
    else if (strcmp(key, "size") == 0) bit = 64U;
    else if (strcmp(key, "glyph_step") == 0) bit = 128U;
    else if (strcmp(key, "depth") == 0) bit = 256U;
    else if (strcmp(key, "rotation") == 0) bit = 512U;
    else return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_UNKNOWN, path,
                       "decal_instance", key, "unknown decal field", line, decal->id);
    if (*seen & bit) return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_DUPLICATE, path,
                                  "decal_instance", key, "duplicate decal field", line,
                                  decal->id);
    *seen |= bit;
    switch (bit) {
        case 1U:
            if (strcmp(value, "decal_pattern") != 0) goto numeric;
            decal->asset.kind = SCENE_ASSET_KIND_DECAL_PATTERN; break;
        case 2U:
            if (!parse_uint_range(value, 255U, &number) || number == 0U) goto numeric;
            decal->asset.id = (uint16_t)number; break;
        case 4U:
            if (strcmp(value, "wall") == 0) decal->surface = SCENE_DECAL_SURFACE_WALL;
            else if (strcmp(value, "floor") == 0) decal->surface = SCENE_DECAL_SURFACE_FLOOR;
            else if (strcmp(value, "ceiling") == 0) decal->surface = SCENE_DECAL_SURFACE_CEILING;
            else goto numeric;
            break;
        case 8U:
            {
                char *parts[3];
                if (!split_tuple(value, parts, 3U) ||
                    !parse_uint_range(parts[0], (unsigned)SCENE_MAX_WIDTH, &number)) goto numeric;
                decal->map_x = (int)number;
                if (!parse_uint_range(parts[1], (unsigned)SCENE_MAX_HEIGHT, &number)) goto numeric;
                decal->map_y = (int)number;
                if (!parse_uint_range(parts[2], 1U, &number)) goto numeric;
                decal->side = (int)number;
            }
            break;
        case 16U:
            if (!parse_double_tuple(value, tuple, 2U)) goto numeric;
            decal->u = tuple[0]; decal->v = tuple[1]; break;
        case 32U:
            if (!parse_double_tuple(value, tuple, 3U)) goto numeric;
            decal->x = tuple[0]; decal->y = tuple[1]; decal->z = tuple[2]; break;
        case 64U:
            if (!parse_double_tuple(value, tuple, 2U)) goto numeric;
            decal->width = tuple[0]; decal->height = tuple[1]; break;
        case 128U:
            if (!parse_double_tuple(value, tuple, 2U)) goto numeric;
            decal->glyph_step_u = tuple[0]; decal->glyph_step_v = tuple[1]; break;
        case 256U: if (!parse_double_c(value, &decal->depth)) goto numeric; break;
        case 512U: if (!parse_double_c(value, &decal->rotation)) goto numeric; break;
        default: goto numeric;
    }
    return SCENE_FORMAT_OK;
numeric:
    return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_NUMERIC, path, "decal_instance",
                  key, "invalid decal field value", line, decal->id);
}

static SceneFormatResult finalize_section(SectionHeader header, unsigned seen,
                                          SceneDecalSurface decal_surface,
                                          const char *path, size_t line,
                                          SceneDiagnostic *diagnostic) {
    unsigned required;
    if (header.kind == SECTION_LIGHT) required = 15U;
    else if (header.kind == SECTION_DECAL) {
        bool wall = decal_surface == SCENE_DECAL_SURFACE_WALL;
        unsigned placement = seen & (8U | 16U | 32U);
        required = wall ? (1U | 2U | 4U | 8U | 16U | 64U | 128U | 256U | 512U)
                        : (1U | 2U | 4U | 32U | 64U | 128U | 256U | 512U);
        if (placement != (required & (8U | 16U | 32U)))
            return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_REQUIRED_MISSING, path,
                          "decal_instance", NULL,
                          "missing or surface-inapplicable placement field", line,
                          header.id);
    } else return SCENE_FORMAT_OK;
    if ((seen & required) != required)
        return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_REQUIRED_MISSING, path,
                      header.kind == SECTION_LIGHT ? "light" : "decal_instance",
                      NULL, "required section field is missing", line, header.id);
    return SCENE_FORMAT_OK;
}

SceneFormatResult scene_format_parse(const char *source, size_t source_size,
                                     const char *path,
                                     SceneFormatCandidate *out_candidate,
                                     SceneDiagnostic *diagnostic) {
    SceneFormatCandidate temporary;
    char *storage;
    LineReader reader;
    SceneLine line;
    SectionKind current = SECTION_NONE;
    unsigned metadata_seen = 0U;
    size_t light_count = 0U, decal_count = 0U, cells_sections = 0U;
    size_t occupancy_sections = 0U, wall_sections = 0U;
    size_t floor_sections = 0U, ceiling_sections = 0U;
    SceneFormatResult result = SCENE_FORMAT_OK;
    if (diagnostic) scene_diagnostic_reset(diagnostic);
    if (!source || !out_candidate) return SCENE_FORMAT_INVALID_ARGUMENT;
    if (source_size > SCENE_FILE_MAX_BYTES)
        return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_DIMENSIONS, path, NULL,
                      NULL, "scene file exceeds 2 MiB", 0U, 0U);
    if (memchr(source, '\0', source_size))
        return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_SYNTAX, path, NULL,
                      NULL, "embedded NUL byte", 0U, 0U);
    storage = malloc(source_size + 1U);
    if (!storage) {
        set_error(diagnostic, SCENE_DIAGNOSTIC_ENV_ALLOCATION, path, NULL, NULL,
                  "parser input allocation failed", 0U, 0U);
        return SCENE_FORMAT_OUT_OF_MEMORY;
    }
    memcpy(storage, source, source_size); storage[source_size] = '\0';
    scene_format_candidate_init(&temporary);
    reader = (LineReader){storage, source_size, 0U, 0U};
    while (line_reader_next(&reader, &line)) {
        char *text;
        size_t raw_length;
        if (!line.text) { result = reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_SYNTAX,
                                          path, NULL, NULL, "bare CR line ending",
                                          line.number, 0U); goto done; }
        raw_length = strlen(line.text);
        if (raw_length > SCENE_LINE_MAX_BYTES) {
            result = reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_DIMENSIONS, path, NULL,
                            NULL, "physical line exceeds 4096 bytes", line.number, 0U);
            goto done;
        }
        text = trim(line.text);
        if (*text == '\0' || *text == '#') continue;
        if (*text == '[') {
            SectionHeader header;
            if (!parse_header(text, &header)) {
                result = reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_UNKNOWN, path, NULL,
                                NULL, "unknown or malformed section", line.number, 0U);
                goto done;
            }
            current = header.kind;
            if (current == SECTION_CELLS) cells_sections++;
            else if (current == SECTION_OCCUPANCY) occupancy_sections++;
            else if (current == SECTION_WALL_MATERIALS) wall_sections++;
            else if (current == SECTION_FLOOR_MATERIALS) floor_sections++;
            else if (current == SECTION_CEILING_MATERIALS) ceiling_sections++;
            else if (current == SECTION_LIGHT) light_count++;
            else if (current == SECTION_DECAL) decal_count++;
            if (cells_sections > 1U || occupancy_sections > 1U || wall_sections > 1U ||
                floor_sections > 1U || ceiling_sections > 1U) {
                result = reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_DUPLICATE, path,
                                NULL, NULL, "duplicate grid section", line.number, 0U);
                goto done;
            }
            if (light_count > SCENE_MAX_LIGHTS || decal_count > SCENE_MAX_DECALS) {
                result = reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_DIMENSIONS, path,
                                NULL, NULL, "instance count exceeds v1 limit",
                                line.number, header.id); goto done;
            }
            continue;
        }
        {
            char *key, *value;
            unsigned bit;
            bool property = split_property(text, &key, &value);
            bit = property ? metadata_bit(key) : 0U;
            if (!property && current == SECTION_NONE) {
                result = reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_SYNTAX, path, NULL,
                                NULL, "malformed metadata property", line.number, 0U);
                goto done;
            }
            if (!bit && current == SECTION_NONE) {
                result = reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_UNKNOWN,
                                path, NULL, key, "unknown metadata field",
                                line.number, 0U); goto done;
            }
            if (!bit) continue;
            if ((metadata_seen & bit) != 0U) {
                result = reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_DUPLICATE, path,
                                NULL, key, "duplicate metadata field", line.number, 0U);
                goto done;
            }
            metadata_seen |= bit;
            result = parse_metadata_value(&temporary, bit, value, path, line.number,
                                          diagnostic);
            if (result != SCENE_FORMAT_OK) goto done;
        }
    }
    if ((metadata_seen & META_REQUIRED) != META_REQUIRED ||
        (temporary.source_version == SCENE_VERSION_V1 &&
         (cells_sections != 1U || occupancy_sections || wall_sections ||
          floor_sections || ceiling_sections)) ||
        (temporary.source_version == SCENE_VERSION_V2 &&
         (cells_sections || occupancy_sections != 1U || wall_sections != 1U ||
          floor_sections != 1U || ceiling_sections != 1U))) {
        result = reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_REQUIRED_MISSING, path, NULL,
                        NULL, "required metadata or versioned grids are missing", 0U, 0U);
        goto done;
    }
    result = allocate_candidate_arrays(&temporary, light_count, decal_count, path,
                                       diagnostic);
    if (result != SCENE_FORMAT_OK) goto done;

    memcpy(storage, source, source_size); storage[source_size] = '\0';
    reader = (LineReader){storage, source_size, 0U, 0U};
    current = SECTION_NONE;
    {
        SectionHeader header = {SECTION_NONE, 0U};
        unsigned fields_seen = 0U;
        size_t rows[5] = {0U, 0U, 0U, 0U, 0U};
        size_t light_index = 0U, decal_index = 0U;
        while (line_reader_next(&reader, &line)) {
            char *text = trim(line.text);
            if (*text == '\0' || *text == '#') continue;
            if (*text == '[') {
                SceneDecalSurface surface = SCENE_DECAL_SURFACE_WALL;
                if (header.kind == SECTION_DECAL && decal_index > 0U)
                    surface = temporary.decals[decal_index - 1U].surface;
                result = finalize_section(header, fields_seen, surface,
                                          path, line.number,
                                          diagnostic);
                if (result != SCENE_FORMAT_OK) goto done;
                if (!parse_header(text, &header)) { result = SCENE_FORMAT_REJECTED; goto done; }
                current = header.kind;
                fields_seen = 0U;
                if ((current == SECTION_LIGHT || current == SECTION_DECAL) &&
                    (header.id == 0U || candidate_has_id(&temporary, header.id))) {
                    result = reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_INSTANCE_ID,
                                    path, current == SECTION_LIGHT ? "light" : "decal_instance",
                                    NULL, "zero or duplicate instance ID", line.number,
                                    header.id); goto done;
                }
                if (current == SECTION_LIGHT) {
                    temporary.lights[light_index].id = header.id;
                    temporary.light_count = ++light_index;
                } else if (current == SECTION_DECAL) {
                    temporary.decals[decal_index].id = header.id;
                    temporary.decal_count = ++decal_index;
                }
                continue;
            }
            {
                if (metadata_bit_from_property(text) != 0U)
                    continue;
            }
            if (current == SECTION_NONE) continue;
            if (current == SECTION_CELLS) {
                if (rows[0] >= (size_t)temporary.map.height) {
                    result = reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_DIMENSIONS, path,
                                    "cells", NULL, "too many cell rows", line.number, 0U);
                    goto done;
                }
                result = parse_cells_row(&temporary, text, rows[0]++, path, line.number,
                                         diagnostic);
            } else if (current >= SECTION_OCCUPANCY &&
                       current <= SECTION_CEILING_MATERIALS) {
                size_t row_index = (size_t)(current - SECTION_OCCUPANCY) + 1U;
                if (rows[row_index] >= (size_t)temporary.map.height) {
                    result = reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_DIMENSIONS,
                                    path, NULL, NULL, "too many v2 grid rows",
                                    line.number, 0U);
                    goto done;
                }
                result = parse_v2_row(&temporary, current, text,
                                      rows[row_index]++, path, line.number,
                                      diagnostic);
            } else {
                char *key, *value;
                if (!split_property(text, &key, &value)) {
                    result = reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_SYNTAX, path,
                                    current == SECTION_LIGHT ? "light" : "decal_instance",
                                    NULL, "malformed section property", line.number,
                                    header.id); goto done;
                }
                if (current == SECTION_LIGHT)
                    result = parse_light_field(&temporary.lights[light_index - 1U],
                                               &fields_seen, key, value, path,
                                               line.number, diagnostic);
                else result = parse_decal_field(&temporary.decals[decal_index - 1U],
                                                &fields_seen, key, value, path,
                                                line.number, diagnostic);
            }
            if (result != SCENE_FORMAT_OK) goto done;
        }
        result = finalize_section(
            header, fields_seen,
            header.kind == SECTION_DECAL && decal_index > 0U
                ? temporary.decals[decal_index - 1U].surface
                : SCENE_DECAL_SURFACE_WALL,
            path, reader.line_number,
                                  diagnostic);
        if (result != SCENE_FORMAT_OK) goto done;
        if ((temporary.source_version == SCENE_VERSION_V1 &&
             rows[0] != (size_t)temporary.map.height) ||
            (temporary.source_version == SCENE_VERSION_V2 &&
             (rows[1] != (size_t)temporary.map.height ||
              rows[2] != (size_t)temporary.map.height ||
              rows[3] != (size_t)temporary.map.height ||
              rows[4] != (size_t)temporary.map.height))) {
            result = reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_DIMENSIONS, path,
                            NULL, NULL, "wrong number of versioned grid rows", 0U, 0U);
            goto done;
        }
    }
    if (temporary.source_version == SCENE_VERSION_V2) {
        size_t i;
        for (i = 0U; i < temporary.authored_cell_count; i++) {
            const SceneAuthoredCell *cell = &temporary.authored_cells[i];
            temporary.map.cells[i].material_id =
                cell->occupancy == SCENE_CELL_OCCUPANCY_WALL
                    ? (int)cell->wall_material : 0;
        }
    }
    result = scene_format_validate(&temporary, path, diagnostic);
    if (result == SCENE_FORMAT_OK) {
        scene_format_candidate_destroy(out_candidate);
        *out_candidate = temporary;
        scene_format_candidate_init(&temporary);
    }
done:
    free(storage);
    scene_format_candidate_destroy(&temporary);
    return result;
}

static bool writer_reserve(TextWriter *writer, size_t additional) {
    size_t needed;
    size_t capacity;
    char *grown;
    if (additional > SIZE_MAX - writer->size - 1U) return false;
    needed = writer->size + additional + 1U;
    if (needed <= writer->capacity) return true;
    capacity = writer->capacity ? writer->capacity : 256U;
    while (capacity < needed) {
        if (capacity > SIZE_MAX / 2U) { capacity = needed; break; }
        capacity *= 2U;
    }
    grown = realloc(writer->data, capacity);
    if (!grown) return false;
    writer->data = grown; writer->capacity = capacity;
    return true;
}

static bool writer_append(TextWriter *writer, const char *text) {
    size_t length = strlen(text);
    if (!writer_reserve(writer, length)) return false;
    memcpy(writer->data + writer->size, text, length);
    writer->size += length; writer->data[writer->size] = '\0';
    return true;
}

static bool writer_printf(TextWriter *writer, const char *format, ...) {
    va_list args;
    va_list copy;
    int length;
    va_start(args, format); va_copy(copy, args);
    length = vsnprintf(NULL, 0U, format, copy); va_end(copy);
    if (length < 0 || !writer_reserve(writer, (size_t)length)) { va_end(args); return false; }
    vsnprintf(writer->data + writer->size, (size_t)length + 1U, format, args);
    va_end(args); writer->size += (size_t)length;
    return true;
}

static bool writer_double(TextWriter *writer, double value) {
    locale_t c_locale = newlocale(LC_NUMERIC_MASK, "C", (locale_t)0);
    locale_t previous;
    bool ok;
    if (c_locale == (locale_t)0) return false;
    if (value == 0.0) value = 0.0;
    previous = uselocale(c_locale);
    ok = writer_printf(writer, "%.17g", value);
    uselocale(previous); freelocale(c_locale);
    return ok;
}

static bool writer_quoted(TextWriter *writer, const char *text) {
    const unsigned char *p = (const unsigned char *)text;
    if (!writer_append(writer, "\"")) return false;
    while (*p) {
        char bytes[3] = {(char)*p, '\0', '\0'};
        if (*p == '\\' || *p == '"') { bytes[0] = '\\'; bytes[1] = (char)*p; }
        if (!writer_append(writer, bytes)) return false;
        p++;
    }
    return writer_append(writer, "\"");
}

static int compare_light_ptrs(const void *left, const void *right) {
    const SceneLight *a = *(const SceneLight *const *)left;
    const SceneLight *b = *(const SceneLight *const *)right;
    return a->id < b->id ? -1 : a->id > b->id;
}

static int compare_decal_ptrs(const void *left, const void *right) {
    const SceneDecalInstance *a = *(const SceneDecalInstance *const *)left;
    const SceneDecalInstance *b = *(const SceneDecalInstance *const *)right;
    return a->id < b->id ? -1 : a->id > b->id;
}

SceneFormatResult scene_format_serialize(const SceneFormatCandidate *candidate,
                                         SceneFormatBuffer *out,
                                         SceneDiagnostic *diagnostic) {
    TextWriter writer = {0};
    const SceneLight *lights[SCENE_MAX_LIGHTS];
    const SceneDecalInstance *decals[SCENE_MAX_DECALS];
    size_t i, x, y;
    locale_t c_locale;
    locale_t previous;
    SceneFormatResult result;
    if (!candidate || !out) return SCENE_FORMAT_INVALID_ARGUMENT;
    if (diagnostic) scene_diagnostic_reset(diagnostic);
    result = scene_format_validate(candidate, NULL, diagnostic);
    if (result != SCENE_FORMAT_OK) return result;
    c_locale = newlocale(LC_NUMERIC_MASK, "C", (locale_t)0);
    if (c_locale == (locale_t)0) {
        set_error(diagnostic, SCENE_DIAGNOSTIC_ENV_ALLOCATION, NULL, NULL, NULL,
                  "C numeric locale creation failed", 0U, 0U);
        return SCENE_FORMAT_OUT_OF_MEMORY;
    }
    previous = uselocale(c_locale);
    for (i = 0U; i < candidate->light_count; i++) lights[i] = &candidate->lights[i];
    for (i = 0U; i < candidate->decal_count; i++) decals[i] = &candidate->decals[i];
    qsort(lights, candidate->light_count, sizeof(lights[0]), compare_light_ptrs);
    qsort(decals, candidate->decal_count, sizeof(decals[0]), compare_decal_ptrs);
#define APPEND(expression) do { if (!(expression)) goto allocation_failed; } while (0)
    APPEND(writer_printf(&writer, "scene_type = terminal_scene\nscene_version = %u\nname = ",
                         candidate->source_version == SCENE_VERSION_V2
                             ? SCENE_VERSION_V2 : SCENE_VERSION_V1));
    APPEND(writer_quoted(&writer, candidate->name));
    APPEND(writer_printf(&writer, "\nwidth = %d\nheight = %d\norigin_x = 0\norigin_y = 0\n"
                        "next_instance_id = %" PRIu64 "\nambient_intensity = ",
                        candidate->map.width, candidate->map.height,
                        candidate->next_instance_id));
    APPEND(writer_double(&writer, candidate->ambient_intensity));
    APPEND(writer_append(&writer, "\nspawn = "));
    APPEND(writer_double(&writer, candidate->spawn_x)); APPEND(writer_append(&writer, ","));
    APPEND(writer_double(&writer, candidate->spawn_y)); APPEND(writer_append(&writer, ","));
    APPEND(writer_double(&writer, candidate->spawn_angle)); APPEND(writer_append(&writer, "\n"));
    if (candidate->legacy_source_path) {
        APPEND(writer_append(&writer, "legacy_source_path = "));
        APPEND(writer_quoted(&writer, candidate->legacy_source_path));
        APPEND(writer_append(&writer, "\n"));
    }
    if (candidate->source_version == SCENE_VERSION_V2) {
        const SectionKind sections[] = {
            SECTION_OCCUPANCY, SECTION_WALL_MATERIALS,
            SECTION_FLOOR_MATERIALS, SECTION_CEILING_MATERIALS
        };
        const char *names[] = {
            "occupancy", "wall_materials", "floor_materials", "ceiling_materials"
        };
        size_t section_index;
        for (section_index = 0U; section_index < 4U; section_index++) {
            APPEND(writer_printf(&writer, "\n[%s]\n", names[section_index]));
            for (y = 0U; y < (size_t)candidate->map.height; y++) {
                for (x = 0U; x < (size_t)candidate->map.width; x++) {
                    const SceneAuthoredCell *cell = &candidate->authored_cells[
                        y * (size_t)candidate->map.width + x];
                    unsigned value;
                    if (sections[section_index] == SECTION_OCCUPANCY)
                        value = (unsigned)cell->occupancy;
                    else if (sections[section_index] == SECTION_WALL_MATERIALS)
                        value = cell->wall_material;
                    else if (sections[section_index] == SECTION_FLOOR_MATERIALS)
                        value = cell->floor_material;
                    else value = cell->ceiling_material;
                    APPEND(writer_printf(&writer,
                        sections[section_index] == SECTION_OCCUPANCY
                            ? (x ? " %u" : "%u")
                            : (x ? " %03u" : "%03u"), value));
                }
                APPEND(writer_append(&writer, "\n"));
            }
        }
    } else {
        APPEND(writer_append(&writer, "\n[cells]\n"));
        for (y = 0U; y < (size_t)candidate->map.height; y++) {
            for (x = 0U; x < (size_t)candidate->map.width; x++) {
                int material = candidate->map.cells[
                    y * (size_t)candidate->map.width + x].material_id;
                APPEND(writer_printf(&writer, x ? " %03d" : "%03d", material));
            }
            APPEND(writer_append(&writer, "\n"));
        }
    }
    for (i = 0U; i < candidate->light_count; i++) {
        const SceneLight *light = lights[i];
        APPEND(writer_printf(&writer, "\n[light %" PRIu64 "]\nposition = ", light->id));
        APPEND(writer_double(&writer, light->x)); APPEND(writer_append(&writer, ","));
        APPEND(writer_double(&writer, light->y));
        APPEND(writer_printf(&writer, "\ncolor = %u,%u,%u,%u\nintensity = ",
                            light->red, light->green, light->blue, light->alpha));
        APPEND(writer_double(&writer, light->intensity)); APPEND(writer_append(&writer, "\nradius = "));
        APPEND(writer_double(&writer, light->radius)); APPEND(writer_append(&writer, "\n"));
    }
    for (i = 0U; i < candidate->decal_count; i++) {
        const SceneDecalInstance *decal = decals[i];
        const char *surface = decal->surface == SCENE_DECAL_SURFACE_WALL ? "wall" :
                              decal->surface == SCENE_DECAL_SURFACE_FLOOR ? "floor" : "ceiling";
        APPEND(writer_printf(&writer, "\n[decal_instance %" PRIu64 "]\n"
                            "asset_kind = decal_pattern\nasset_id = %u\nsurface = %s\n",
                            decal->id, decal->asset.id, surface));
        if (decal->surface == SCENE_DECAL_SURFACE_WALL) {
            APPEND(writer_printf(&writer, "anchor = %d,%d,%d\nuv = ", decal->map_x,
                                decal->map_y, decal->side));
            APPEND(writer_double(&writer, decal->u)); APPEND(writer_append(&writer, ","));
            APPEND(writer_double(&writer, decal->v)); APPEND(writer_append(&writer, "\n"));
        } else {
            APPEND(writer_append(&writer, "position = "));
            APPEND(writer_double(&writer, decal->x)); APPEND(writer_append(&writer, ","));
            APPEND(writer_double(&writer, decal->y)); APPEND(writer_append(&writer, ","));
            APPEND(writer_double(&writer, decal->z)); APPEND(writer_append(&writer, "\n"));
        }
        APPEND(writer_append(&writer, "size = "));
        APPEND(writer_double(&writer, decal->width)); APPEND(writer_append(&writer, ","));
        APPEND(writer_double(&writer, decal->height)); APPEND(writer_append(&writer, "\nglyph_step = "));
        APPEND(writer_double(&writer, decal->glyph_step_u)); APPEND(writer_append(&writer, ","));
        APPEND(writer_double(&writer, decal->glyph_step_v)); APPEND(writer_append(&writer, "\ndepth = "));
        APPEND(writer_double(&writer, decal->depth)); APPEND(writer_append(&writer, "\nrotation = "));
        APPEND(writer_double(&writer, decal->rotation)); APPEND(writer_append(&writer, "\n"));
    }
#undef APPEND
    uselocale(previous); freelocale(c_locale);
    if (writer.size > SCENE_FILE_MAX_BYTES) {
        free(writer.data);
        return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_DIMENSIONS, NULL, NULL,
                      NULL, "canonical scene exceeds 2 MiB", 0U, 0U);
    }
    scene_format_buffer_destroy(out);
    out->data = writer.data; out->size = writer.size;
    return SCENE_FORMAT_OK;
allocation_failed:
    uselocale(previous); freelocale(c_locale); free(writer.data);
    set_error(diagnostic, SCENE_DIAGNOSTIC_ENV_ALLOCATION, NULL, NULL, NULL,
              "serialization buffer allocation failed", 0U, 0U);
    return SCENE_FORMAT_OUT_OF_MEMORY;
}
