#define _GNU_SOURCE

#include "scene_format.h"

#include "checked_size.h"
#include "scene_block_codec.h"

#include <float.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <locale.h>
#include <math.h>
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
    SECTION_FLOOR_HEIGHTS,
    SECTION_CEILING_HEIGHTS,
    SECTION_FLOOR_PRESENCE,
    SECTION_CEILING_PRESENCE,
    SECTION_GRAVITY_SCALES,
    SECTION_GRAVITY_ORIENTATIONS,
    SECTION_MOVEMENT,
    SECTION_LIGHT,
    SECTION_DECAL,
    SECTION_SPRITE,
    SECTION_TRIGGER,
    SECTION_OPTICAL_MATERIAL,
    SECTION_OPTICAL_CELL
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
static int compare_optical_cell_overrides(const void *left, const void *right);

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
    META_LEGACY_PATH = 1U << 10,
    META_EAST_GROWTH = 1U << 11,
    META_SOUTH_GROWTH = 1U << 12
};

#define META_REQUIRED (META_SCENE_TYPE | META_VERSION | META_NAME | META_WIDTH | \
                       META_HEIGHT | META_ORIGIN_X | META_ORIGIN_Y | META_NEXT_ID | \
                       META_AMBIENT | META_SPAWN)
#define META_V3_REQUIRED (META_REQUIRED | META_EAST_GROWTH | META_SOUTH_GROWTH)

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
    candidate->movement = scene_movement_parameters_default();
}

void scene_format_candidate_destroy(SceneFormatCandidate *candidate) {
    if (!candidate) return;
    free(candidate->map.cells);
    free(candidate->map.light_map);
    free(candidate->authored_cells);
    free(candidate->lights);
    free(candidate->decals);
    free(candidate->sprites);
    free(candidate->triggers);
    free(candidate->optical_material_defaults);
    free(candidate->optical_cell_overrides);
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
    if (strcmp(inside, "floor_heights") == 0) {
        header->kind = SECTION_FLOOR_HEIGHTS; header->id = 0U; return true;
    }
    if (strcmp(inside, "ceiling_heights") == 0) {
        header->kind = SECTION_CEILING_HEIGHTS; header->id = 0U; return true;
    }
    if (strcmp(inside, "floor_presence") == 0) {
        header->kind = SECTION_FLOOR_PRESENCE; header->id = 0U; return true;
    }
    if (strcmp(inside, "ceiling_presence") == 0) {
        header->kind = SECTION_CEILING_PRESENCE; header->id = 0U; return true;
    }
    if (strcmp(inside, "gravity_scales") == 0) {
        header->kind = SECTION_GRAVITY_SCALES; header->id = 0U; return true;
    }
    if (strcmp(inside, "gravity_orientations") == 0) {
        header->kind = SECTION_GRAVITY_ORIENTATIONS; header->id = 0U; return true;
    }
    if (strcmp(inside, "movement") == 0) {
        header->kind = SECTION_MOVEMENT; header->id = 0U; return true;
    }
    space = strchr(inside, ' ');
    if (!space || strchr(space + 1, ' ')) return false;
    *space++ = '\0';
    if (!parse_u64(space, &header->id)) return false;
    if (strcmp(inside, "light") == 0) header->kind = SECTION_LIGHT;
    else if (strcmp(inside, "decal_instance") == 0) header->kind = SECTION_DECAL;
    else if (strcmp(inside, "sprite_instance") == 0) header->kind = SECTION_SPRITE;
    else if (strcmp(inside, "trigger") == 0) header->kind = SECTION_TRIGGER;
    else if (strcmp(inside, "optical_material") == 0)
        header->kind = SECTION_OPTICAL_MATERIAL;
    else if (strcmp(inside, "optical_cell") == 0)
        header->kind = SECTION_OPTICAL_CELL;
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
    if (candidate->source_version >= SCENE_VERSION_V2 &&
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
    for (i = 0U; i < candidate->sprite_count; i++) {
        if (candidate->sprites[i].id == id) return true;
    }
    for (i = 0U; i < candidate->trigger_count; i++)
        if (candidate->triggers[i].id == id) return true;
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
    if (default_material < 1U || default_material > UINT16_MAX) {
        return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_NUMERIC, NULL, NULL,
                      "default_material", "default material outside 1..65535",
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
        cells[i].wall_material = (uint16_t)(material == 0
            ? default_material
            : (unsigned int)material);
        cells[i].floor_material = (uint16_t)default_material;
        cells[i].ceiling_material = (uint16_t)default_material;
        cells[i].floor_height_step = SCENE_DEFAULT_FLOOR_HEIGHT_STEP;
        cells[i].ceiling_height_step = SCENE_DEFAULT_CEILING_HEIGHT_STEP;
        cells[i].floor_present = true;
        cells[i].ceiling_present = true;
    }
    candidate->authored_cells = cells;
    candidate->authored_cell_count = count;
    candidate->source_version = SCENE_VERSION_V2;
    return SCENE_FORMAT_OK;
}

SceneFormatResult scene_format_migrate_to_v5(
    SceneFormatCandidate *candidate, SceneDiagnostic *diagnostic
) {
    size_t i;
    SceneFormatResult result;
    if (diagnostic) scene_diagnostic_reset(diagnostic);
    if (!candidate || candidate->source_version < SCENE_VERSION_V2 ||
        candidate->source_version >= SCENE_VERSION_V5 ||
        !candidate->authored_cells) {
        return SCENE_FORMAT_INVALID_ARGUMENT;
    }
    result = scene_format_validate(candidate, NULL, diagnostic);
    if (result != SCENE_FORMAT_OK) return result;
    for (i = 0U; i < candidate->authored_cell_count; i++) {
        SceneAuthoredCell *cell = &candidate->authored_cells[i];
        cell->floor_height_step = SCENE_DEFAULT_FLOOR_HEIGHT_STEP;
        cell->ceiling_height_step = SCENE_DEFAULT_CEILING_HEIGHT_STEP;
        cell->gravity_scale_step = 0U;
        cell->gravity_orientation = SCENE_GRAVITY_INHERIT;
        cell->floor_present = true;
        cell->ceiling_present = true;
    }
    candidate->movement = scene_movement_parameters_default();
    candidate->source_version = SCENE_VERSION_V5;
    return SCENE_FORMAT_OK;
}

SceneFormatResult scene_format_migrate_v5_to_v6(
    SceneFormatCandidate *candidate, SceneDiagnostic *diagnostic
) {
    SceneFormatResult result;
    if (diagnostic) scene_diagnostic_reset(diagnostic);
    if (!candidate || candidate->source_version != SCENE_VERSION_V5 ||
        candidate->optical_material_defaults ||
        candidate->optical_material_capacity != 0U ||
        candidate->optical_cell_overrides ||
        candidate->optical_cell_override_count != 0U) {
        return SCENE_FORMAT_INVALID_ARGUMENT;
    }
    result = scene_format_validate(candidate, NULL, diagnostic);
    if (result != SCENE_FORMAT_OK) return result;
    candidate->source_version = SCENE_VERSION_V6;
    return SCENE_FORMAT_OK;
}

SceneFormatResult scene_format_migrate_v6_to_v7(
    SceneFormatCandidate *candidate, SceneDiagnostic *diagnostic
) {
    SceneFormatResult result;
    size_t i;
    if (diagnostic) scene_diagnostic_reset(diagnostic);
    if (!candidate || candidate->source_version != SCENE_VERSION_V6)
        return SCENE_FORMAT_INVALID_ARGUMENT;
    result = scene_format_validate(candidate, NULL, diagnostic);
    if (result != SCENE_FORMAT_OK) return result;
    for (i = 0U; i < candidate->light_count; i++)
        scene_light_set_point_defaults(&candidate->lights[i]);
    candidate->source_version = SCENE_VERSION_V7;
    return SCENE_FORMAT_OK;
}

SceneFormatResult scene_format_migrate_v7_to_v8(
    SceneFormatCandidate *candidate, SceneDiagnostic *diagnostic
) {
    SceneFormatResult result;
    if (diagnostic) scene_diagnostic_reset(diagnostic);
    if (!candidate || candidate->source_version != SCENE_VERSION_V7)
        return SCENE_FORMAT_INVALID_ARGUMENT;
    result = scene_format_validate(candidate, NULL, diagnostic);
    if (result != SCENE_FORMAT_OK) return result;
    candidate->source_version = SCENE_VERSION_V8;
    return SCENE_FORMAT_OK;
}

SceneFormatResult scene_format_migrate_v8_to_v9(
    SceneFormatCandidate *candidate, SceneDiagnostic *diagnostic
) {
    SceneFormatResult result;
    if (diagnostic) scene_diagnostic_reset(diagnostic);
    if (!candidate || candidate->source_version != SCENE_VERSION_V8 ||
        candidate->triggers || candidate->trigger_count != 0U)
        return SCENE_FORMAT_INVALID_ARGUMENT;
    result = scene_format_validate(candidate, NULL, diagnostic);
    if (result != SCENE_FORMAT_OK) return result;
    candidate->source_version = SCENE_VERSION_V9;
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
            candidate->map.cells[i].material_id > (int)UINT16_MAX) {
            return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_NUMERIC, path,
                          "cells", NULL, "material ID outside 0000..FFFF", 0U, 0U);
        }
    }
    if (candidate->source_version >= SCENE_VERSION_V2) {
        if (!candidate->authored_cells || candidate->authored_cell_count != count) {
            return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_DIMENSIONS, path,
                          NULL, NULL, "invalid v2 authored-cell count", 0U, 0U);
        }
        for (i = 0U; i < count; i++) {
            const SceneAuthoredCell *cell = &candidate->authored_cells[i];
            bool invalid_null = candidate->source_version < SCENE_VERSION_V4
                ? cell->wall_material == 0U || cell->floor_material == 0U ||
                  cell->ceiling_material == 0U
                : cell->occupancy == SCENE_CELL_OCCUPANCY_WALL &&
                  cell->wall_material == 0U;
            if ((cell->occupancy != SCENE_CELL_OCCUPANCY_EMPTY &&
                 cell->occupancy != SCENE_CELL_OCCUPANCY_WALL) || invalid_null) {
                return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_NUMERIC, path,
                              "authored_cells", NULL,
                              "invalid v2 occupancy or material reference", 0U, 0U);
            }
        }
    }
    if (candidate->source_version >= SCENE_VERSION_V5) {
        const SceneMovementParameters *movement = &candidate->movement;
        if (!isfinite(movement->gravity_magnitude) ||
            !isfinite(movement->step_height) ||
            !isfinite(movement->jump_impulse) ||
            !isfinite(movement->air_control_scale) ||
            !isfinite(movement->eye_height) ||
            !isfinite(movement->head_clearance) ||
            movement->gravity_magnitude <= 0.0 ||
            movement->gravity_magnitude > 256.0 ||
            movement->gravity_orientation < SCENE_GRAVITY_DOWN ||
            movement->gravity_orientation > SCENE_GRAVITY_WEST ||
            movement->step_height < 0.0625 || movement->step_height > 1.0 ||
            movement->jump_impulse <= 0.0 || movement->jump_impulse > 16.0 ||
            movement->air_control_scale < 0.0 || movement->air_control_scale > 1.0 ||
            movement->eye_height <= 0.0 || movement->eye_height > 8.0 ||
            movement->head_clearance < 0.25 || movement->head_clearance > 8.0) {
            return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_MOVEMENT, path,
                          "movement", NULL,
                          "movement parameter is non-finite or out of range", 0U, 0U);
        }
        for (i = 0U; i < count; i++) {
            const SceneAuthoredCell *cell = &candidate->authored_cells[i];
            if (cell->floor_height_step < SCENE_HEIGHT_MIN_STEP ||
                cell->floor_height_step > SCENE_HEIGHT_MAX_STEP ||
                cell->ceiling_height_step < SCENE_HEIGHT_MIN_STEP ||
                cell->ceiling_height_step > SCENE_HEIGHT_MAX_STEP) {
                return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_HEIGHT_RANGE,
                              path, "authored_cells", NULL,
                              "height outside signed -0800..0800", 0U, 0U);
            }
            if (cell->floor_present && cell->ceiling_present &&
                (cell->ceiling_height_step <= cell->floor_height_step ||
                 cell->ceiling_height_step - cell->floor_height_step <
                     SCENE_MIN_CLEARANCE_STEP)) {
                return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_HEIGHT_RELATION,
                              path, "authored_cells", NULL,
                              "floor/ceiling relation violates minimum clearance",
                              0U, 0U);
            }
            if (cell->gravity_orientation > SCENE_GRAVITY_WEST) {
                return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_GRAVITY,
                              path, "gravity_orientations", NULL,
                              "gravity orientation outside 0..6", 0U, 0U);
            }
            if (cell->gravity_scale_step != 0U &&
                movement->gravity_magnitude *
                    ((double)cell->gravity_scale_step /
                     (double)SCENE_HEIGHT_STEPS_PER_UNIT) > 256.0) {
                return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_GRAVITY,
                              path, "gravity_scales", NULL,
                              "effective gravity exceeds 256", 0U, 0U);
            }
        }
    }
    if (candidate->source_version >= SCENE_VERSION_V3) {
        for (i = 0U; i < candidate->east_growth_count; i++) {
            if (candidate->east_growth[i] < 0 ||
                candidate->east_growth[i] >= candidate->map.height)
                return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_NUMERIC, path,
                              NULL, "east_growth", "trigger row is out of bounds",
                              0U, 0U);
        }
        for (i = 0U; i < candidate->south_growth_count; i++) {
            if (candidate->south_growth[i] < 0 ||
                candidate->south_growth[i] >= candidate->map.width)
                return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_NUMERIC, path,
                              NULL, "south_growth", "trigger column is out of bounds",
                              0U, 0U);
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
        candidate->sprite_count > SCENE_MAX_SPRITES ||
        candidate->trigger_count > SCENE_MAX_TRIGGERS ||
        (candidate->light_count && !candidate->lights) ||
        (candidate->decal_count && !candidate->decals) ||
        (candidate->sprite_count && !candidate->sprites) ||
        (candidate->trigger_count && !candidate->triggers)) {
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
            light->radius <= 0.0 ||
            (candidate->source_version >= SCENE_VERSION_V7 &&
             (light->type < SCENE_LIGHT_POINT || light->type > SCENE_LIGHT_SPOT ||
              !isfinite(light->direction) ||
              light->direction < SCENE_LIGHT_DIRECTION_MIN ||
              light->direction >= SCENE_LIGHT_DIRECTION_MAX ||
              !isfinite(light->cone) || light->cone < SCENE_LIGHT_CONE_MIN ||
              light->cone > SCENE_LIGHT_CONE_MAX || !isfinite(light->falloff) ||
              light->falloff < SCENE_LIGHT_FALLOFF_MIN ||
              light->falloff > SCENE_LIGHT_FALLOFF_MAX))) {
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
            decal->asset.id < 1U ||
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
    for (i = 0U; i < candidate->sprite_count; i++) {
        const SceneSpriteInstance *sprite = &candidate->sprites[i];
        size_t j;
        if (sprite->id == 0U ||
            sprite->asset.kind != SCENE_ASSET_KIND_SPRITE_PATTERN ||
            sprite->asset.id == 0U || sprite->asset.id >= SPRITE_ID_CAPACITY ||
            !isfinite(sprite->x) || !isfinite(sprite->y) ||
            sprite->x < 0.0 || sprite->x >= candidate->map.width ||
            sprite->y < 0.0 || sprite->y >= candidate->map.height) {
            return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_NUMERIC, path,
                          "sprite_instance", NULL, "invalid sprite value", 0U,
                          sprite->id);
        }
        for (j = 0U; j < candidate->light_count; j++)
            if (candidate->lights[j].id == sprite->id) goto duplicate_sprite;
        for (j = 0U; j < candidate->decal_count; j++)
            if (candidate->decals[j].id == sprite->id) goto duplicate_sprite;
        for (j = 0U; j < i; j++)
            if (candidate->sprites[j].id == sprite->id) goto duplicate_sprite;
        if (sprite->id > maximum_id) maximum_id = sprite->id;
        continue;
duplicate_sprite:
        return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_INSTANCE_ID, path,
                      "sprite_instance", NULL, "duplicate instance ID", 0U,
                      sprite->id);
    }
    for (i = 0U; i < candidate->trigger_count; i++) {
        const SceneTrigger *trigger = &candidate->triggers[i];
        size_t j;
        bool target_found = trigger->action != SCENE_TRIGGER_ACTION_TOGGLE_LIGHT;
        if (trigger->id == 0U || !isfinite(trigger->min_x) ||
            !isfinite(trigger->min_y) || !isfinite(trigger->max_x) ||
            !isfinite(trigger->max_y) || trigger->min_x < 0.0 ||
            trigger->min_y < 0.0 || trigger->max_x > candidate->map.width ||
            trigger->max_y > candidate->map.height ||
            trigger->min_x >= trigger->max_x || trigger->min_y >= trigger->max_y ||
            trigger->condition != SCENE_TRIGGER_CONDITION_ENTER_REGION ||
            trigger->action < SCENE_TRIGGER_ACTION_SET_FLAG ||
            trigger->action > SCENE_TRIGGER_ACTION_TOGGLE_LIGHT ||
            (trigger->action == SCENE_TRIGGER_ACTION_SET_FLAG &&
             (trigger->flag_id < 1U || trigger->flag_id > SCENE_TRIGGER_FLAG_CAPACITY ||
              trigger->target_id != 0U)) ||
            (trigger->action == SCENE_TRIGGER_ACTION_TELEPORT_TO_SPAWN &&
             (trigger->flag_id != 0U || trigger->flag_value || trigger->target_id != 0U)) ||
            (trigger->action == SCENE_TRIGGER_ACTION_TOGGLE_LIGHT &&
             (trigger->flag_id != 0U || trigger->flag_value || trigger->target_id == 0U)))
            return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_NUMERIC, path,
                          "trigger", NULL, "invalid trigger value", 0U, trigger->id);
        for (j = 0U; j < candidate->light_count; j++) {
            if (candidate->lights[j].id == trigger->id) goto duplicate_trigger;
            if (candidate->lights[j].id == trigger->target_id) target_found = true;
        }
        for (j = 0U; j < candidate->decal_count; j++)
            if (candidate->decals[j].id == trigger->id) goto duplicate_trigger;
        for (j = 0U; j < candidate->sprite_count; j++)
            if (candidate->sprites[j].id == trigger->id) goto duplicate_trigger;
        for (j = 0U; j < i; j++)
            if (candidate->triggers[j].id == trigger->id) goto duplicate_trigger;
        if (!target_found)
            return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_INSTANCE_REFERENCE, path,
                          "trigger", "target_id", "dangling light target", 0U,
                          trigger->id);
        if (trigger->id > maximum_id) maximum_id = trigger->id;
        continue;
duplicate_trigger:
        return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_INSTANCE_ID, path,
                      "trigger", NULL, "duplicate instance ID", 0U, trigger->id);
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
                                                    size_t sprites, size_t triggers,
                                                   size_t optical_material_capacity,
                                                   size_t optical_cells,
                                                   const char *path,
                                                   SceneDiagnostic *diagnostic) {
    size_t count;
    if (!checked_size_2d(candidate->map.width, candidate->map.height, &count))
        return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_DIMENSIONS, path, NULL,
                      NULL, "invalid dimensions", 0U, 0U);
    candidate->map.cells = calloc(count, sizeof(*candidate->map.cells));
    if (!candidate->map.cells) goto allocation_failed;
    /* light_map must be allocated here, not just in map_create().  The
     * renderer and lighting_update_optical both check map->light_map != NULL and
     * silently fall back to full-brightness if it is missing.  Without this
     * allocation, native-loaded scenes appear fully lit with no point-light
     * or ambient effect. */
    candidate->map.light_map = calloc(count, sizeof(*candidate->map.light_map));
    if (!candidate->map.light_map) goto allocation_failed;
    if (candidate->source_version >= SCENE_VERSION_V2) {
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
    if (sprites) {
        candidate->sprites = calloc(sprites, sizeof(*candidate->sprites));
        if (!candidate->sprites) goto allocation_failed;
    }
    if (triggers) {
        candidate->triggers = calloc(triggers, sizeof(*candidate->triggers));
        if (!candidate->triggers) goto allocation_failed;
    }
    if (optical_material_capacity) {
        candidate->optical_material_defaults = calloc(
            optical_material_capacity,
            sizeof(*candidate->optical_material_defaults));
        if (!candidate->optical_material_defaults) goto allocation_failed;
        candidate->optical_material_capacity = optical_material_capacity;
    }
    if (optical_cells) {
        candidate->optical_cell_overrides = calloc(
            optical_cells, sizeof(*candidate->optical_cell_overrides));
        if (!candidate->optical_cell_overrides) goto allocation_failed;
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
    if (strcmp(key, "east_growth") == 0) return META_EAST_GROWTH;
    if (strcmp(key, "south_growth") == 0) return META_SOUTH_GROWTH;
    return 0U;
}

static bool parse_growth_list(char *value, int *items, size_t capacity,
                              size_t *out_count) {
    char *cursor = value;
    size_t count = 0U;
    if (strcmp(value, "-") == 0) { *out_count = 0U; return true; }
    while (*cursor) {
        char *end;
        long parsed;
        if (count >= capacity) return false;
        errno = 0;
        parsed = strtol(cursor, &end, 10);
        if (errno || end == cursor || parsed < 0 || parsed > INT_MAX) return false;
        items[count++] = (int)parsed;
        if (*end == '\0') break;
        if (*end != ',') return false;
        cursor = end + 1;
        if (*cursor == '\0') return false;
    }
    *out_count = count;
    return true;
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
                 (parsed != SCENE_VERSION_V1 && parsed != SCENE_VERSION_V2 &&
                   parsed != SCENE_VERSION_V3 && parsed != SCENE_VERSION_V4 &&
                   parsed != SCENE_VERSION_V5 && parsed != SCENE_VERSION_V6 &&
                    parsed != SCENE_VERSION_V7 && parsed != SCENE_VERSION_V8 &&
                    parsed != SCENE_VERSION_V9))
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
        case META_EAST_GROWTH:
            if (!parse_growth_list(value, candidate->east_growth, SCENE_MAX_WIDTH,
                                   &candidate->east_growth_count))
                return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_NUMERIC, path, NULL,
                              "east_growth", "invalid east growth list", line, 0U);
            break;
        case META_SOUTH_GROWTH:
            if (!parse_growth_list(value, candidate->south_growth, SCENE_MAX_HEIGHT,
                                   &candidate->south_growth_count))
                return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_NUMERIC, path, NULL,
                              "south_growth", "invalid south growth list", line, 0U);
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
            if (candidate->source_version >= SCENE_VERSION_V4) {
                uint16_t block_value;
                char block[SCENE_BLOCK_TEXT_SIZE];
                if (strlen(cursor) < SCENE_BLOCK_HEX_DIGITS ||
                    (cursor[SCENE_BLOCK_HEX_DIGITS] != '\0' &&
                     cursor[SCENE_BLOCK_HEX_DIGITS] != ' ' &&
                     cursor[SCENE_BLOCK_HEX_DIGITS] != '\t')) {
                    return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_SYNTAX, path,
                                  NULL, NULL, "v4 material requires XXXX block",
                                  line, 0U);
                }
                memcpy(block, cursor, SCENE_BLOCK_HEX_DIGITS);
                block[SCENE_BLOCK_HEX_DIGITS] = '\0';
                if (!scene_block_parse(block, &block_value)) {
                    return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_SYNTAX, path,
                                  NULL, NULL, "invalid v4 hexadecimal material block",
                                  line, 0U);
                }
                value = block_value;
                cursor += SCENE_BLOCK_HEX_DIGITS;
            } else if (strlen(cursor) < 3U || cursor[0] < '0' || cursor[0] > '9' ||
                       cursor[1] < '0' || cursor[1] > '9' || cursor[2] < '0' ||
                       cursor[2] > '9' ||
                       (cursor[3] != '\0' && cursor[3] != ' ' && cursor[3] != '\t')) {
                return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_SYNTAX, path,
                              section == SECTION_WALL_MATERIALS ? "wall_materials" :
                              section == SECTION_FLOOR_MATERIALS ? "floor_materials" :
                              "ceiling_materials", NULL,
                              "material grids require three-digit tokens", line, 0U);
            } else {
                memcpy(token, cursor, 3U);
                token[3] = '\0';
                if (!parse_uint_range(token, 255U, &value) || value == 0U) {
                    return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_NUMERIC, path,
                                  NULL, NULL, "v2 material outside 001..255", line, 0U);
                }
                cursor += 3;
            }
        }
        if (column >= (size_t)candidate->map.width) {
            return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_DIMENSIONS, path,
                          NULL, NULL, "too many v2 grid cells in row", line, 0U);
        }
        index = row * (size_t)candidate->map.width + column++;
        if (section == SECTION_OCCUPANCY)
            candidate->authored_cells[index].occupancy = (SceneCellOccupancy)value;
        else if (section == SECTION_WALL_MATERIALS)
            candidate->authored_cells[index].wall_material = (uint16_t)value;
        else if (section == SECTION_FLOOR_MATERIALS)
            candidate->authored_cells[index].floor_material = (uint16_t)value;
        else
            candidate->authored_cells[index].ceiling_material = (uint16_t)value;
    }
    if (column != (size_t)candidate->map.width) {
        return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_DIMENSIONS, path, NULL,
                      NULL, "wrong number of v2 grid cells in row", line, 0U);
    }
    return SCENE_FORMAT_OK;
}

static const char *section_name(SectionKind section) {
    switch (section) {
        case SECTION_FLOOR_HEIGHTS: return "floor_heights";
        case SECTION_CEILING_HEIGHTS: return "ceiling_heights";
        case SECTION_FLOOR_PRESENCE: return "floor_presence";
        case SECTION_CEILING_PRESENCE: return "ceiling_presence";
        case SECTION_GRAVITY_SCALES: return "gravity_scales";
        case SECTION_GRAVITY_ORIENTATIONS: return "gravity_orientations";
        default: return "";
    }
}

static SceneFormatResult parse_v5_row(SceneFormatCandidate *candidate,
                                      SectionKind section, char *text, size_t row,
                                      const char *path, size_t line,
                                      SceneDiagnostic *diagnostic) {
    size_t column = 0U;
    char *cursor = text;
    while (*cursor) {
        unsigned value;
        size_t index;
        while (*cursor == ' ' || *cursor == '\t') cursor++;
        if (*cursor == '\0') break;
        if (section == SECTION_FLOOR_HEIGHTS ||
            section == SECTION_CEILING_HEIGHTS ||
            section == SECTION_GRAVITY_SCALES) {
            char block[SCENE_BLOCK_TEXT_SIZE];
            uint16_t parsed;
            if (strlen(cursor) < SCENE_BLOCK_HEX_DIGITS ||
                (cursor[SCENE_BLOCK_HEX_DIGITS] != '\0' &&
                 cursor[SCENE_BLOCK_HEX_DIGITS] != ' ' &&
                 cursor[SCENE_BLOCK_HEX_DIGITS] != '\t')) {
                return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_SYNTAX, path,
                              section_name(section), NULL,
                              "v5 grid requires XXXX block", line, 0U);
            }
            memcpy(block, cursor, SCENE_BLOCK_HEX_DIGITS);
            block[SCENE_BLOCK_HEX_DIGITS] = '\0';
            if (!scene_block_parse(block, &parsed)) {
                return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_SYNTAX, path,
                              section_name(section), NULL,
                              "invalid v5 hexadecimal block", line, 0U);
            }
            value = parsed;
            cursor += SCENE_BLOCK_HEX_DIGITS;
        } else {
            size_t length = strcspn(cursor, " \t");
            char token[4];
            unsigned maximum = (section == SECTION_FLOOR_PRESENCE ||
                                section == SECTION_CEILING_PRESENCE) ? 1U : 6U;
            if (length == 0U || length >= sizeof(token)) {
                return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_SYNTAX, path,
                              section_name(section), NULL,
                              "invalid v5 decimal grid token", line, 0U);
            }
            memcpy(token, cursor, length); token[length] = '\0';
            if (!parse_uint_range(token, maximum, &value)) {
                return reject(diagnostic,
                              (section == SECTION_FLOOR_PRESENCE ||
                               section == SECTION_CEILING_PRESENCE)
                                  ? SCENE_DIAGNOSTIC_INPUT_NUMERIC
                                  : SCENE_DIAGNOSTIC_INPUT_GRAVITY,
                              path, section_name(section), NULL,
                              "v5 grid value is out of range", line, 0U);
            }
            cursor += length;
        }
        if (column >= (size_t)candidate->map.width) {
            return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_DIMENSIONS, path,
                          section_name(section), NULL,
                          "too many v5 grid cells in row", line, 0U);
        }
        index = row * (size_t)candidate->map.width + column++;
        if (section == SECTION_FLOOR_HEIGHTS) {
            int16_t height = (int16_t)(uint16_t)value;
            if (height < SCENE_HEIGHT_MIN_STEP || height > SCENE_HEIGHT_MAX_STEP)
                return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_HEIGHT_RANGE,
                              path, section_name(section), NULL,
                              "height outside signed -0800..0800", line, 0U);
            candidate->authored_cells[index].floor_height_step = height;
        } else if (section == SECTION_CEILING_HEIGHTS) {
            int16_t height = (int16_t)(uint16_t)value;
            if (height < SCENE_HEIGHT_MIN_STEP || height > SCENE_HEIGHT_MAX_STEP)
                return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_HEIGHT_RANGE,
                              path, section_name(section), NULL,
                              "height outside signed -0800..0800", line, 0U);
            candidate->authored_cells[index].ceiling_height_step = height;
        } else if (section == SECTION_FLOOR_PRESENCE) {
            candidate->authored_cells[index].floor_present = value != 0U;
        } else if (section == SECTION_CEILING_PRESENCE) {
            candidate->authored_cells[index].ceiling_present = value != 0U;
        } else if (section == SECTION_GRAVITY_SCALES) {
            candidate->authored_cells[index].gravity_scale_step = (uint16_t)value;
        } else {
            candidate->authored_cells[index].gravity_orientation = (uint8_t)value;
        }
    }
    if (column != (size_t)candidate->map.width) {
        return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_DIMENSIONS, path,
                      section_name(section), NULL,
                      "wrong v5 grid cell count", line, 0U);
    }
    return SCENE_FORMAT_OK;
}

static SceneGravityOrientation parse_gravity_orientation(const char *value) {
    if (strcmp(value, "down") == 0) return SCENE_GRAVITY_DOWN;
    if (strcmp(value, "up") == 0) return SCENE_GRAVITY_UP;
    if (strcmp(value, "north") == 0) return SCENE_GRAVITY_NORTH;
    if (strcmp(value, "south") == 0) return SCENE_GRAVITY_SOUTH;
    if (strcmp(value, "east") == 0) return SCENE_GRAVITY_EAST;
    if (strcmp(value, "west") == 0) return SCENE_GRAVITY_WEST;
    return SCENE_GRAVITY_INHERIT;
}

static SceneFormatResult parse_movement_field(SceneMovementParameters *movement,
                                              unsigned *seen, const char *key,
                                              const char *value, const char *path,
                                              size_t line,
                                              SceneDiagnostic *diagnostic) {
    static const char *const keys[] = {
        "gravity_magnitude", "gravity_orientation", "step_height",
        "jump_impulse", "air_control_scale", "eye_height", "head_clearance"
    };
    size_t i;
    double parsed;
    for (i = 0U; i < sizeof(keys) / sizeof(keys[0]); i++)
        if (strcmp(key, keys[i]) == 0) break;
    if (i == sizeof(keys) / sizeof(keys[0]))
        return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_MOVEMENT, path,
                      "movement", key, "unknown movement field", line, 0U);
    if ((*seen & (1U << i)) != 0U)
        return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_MOVEMENT, path,
                      "movement", key, "duplicate movement field", line, 0U);
    *seen |= 1U << i;
    if (i == 1U) {
        movement->gravity_orientation = parse_gravity_orientation(value);
        if (movement->gravity_orientation == SCENE_GRAVITY_INHERIT) goto invalid;
        return SCENE_FORMAT_OK;
    }
    if (!parse_double_c(value, &parsed)) goto invalid;
    switch (i) {
        case 0U: movement->gravity_magnitude = parsed; break;
        case 2U: movement->step_height = parsed; break;
        case 3U: movement->jump_impulse = parsed; break;
        case 4U: movement->air_control_scale = parsed; break;
        case 5U: movement->eye_height = parsed; break;
        case 6U: movement->head_clearance = parsed; break;
        default: goto invalid;
    }
    return SCENE_FORMAT_OK;
invalid:
    return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_MOVEMENT, path,
                  "movement", key, "invalid movement field value", line, 0U);
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
    else if (strcmp(key, "type") == 0) bit = 16U;
    else if (strcmp(key, "direction") == 0) bit = 32U;
    else if (strcmp(key, "cone") == 0) bit = 64U;
    else if (strcmp(key, "falloff") == 0) bit = 128U;
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
    } else if (bit == 8U) {
        if (!parse_double_c(value, &light->radius)) goto numeric;
    } else if (bit == 16U) {
        if (strcmp(value, "point") == 0) light->type = SCENE_LIGHT_POINT;
        else if (strcmp(value, "spot") == 0) light->type = SCENE_LIGHT_SPOT;
        else goto numeric;
    } else if (bit == 32U) {
        if (!parse_double_c(value, &light->direction)) goto numeric;
    } else if (bit == 64U) {
        if (!parse_double_c(value, &light->cone)) goto numeric;
    } else if (!parse_double_c(value, &light->falloff)) goto numeric;
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
            if (!parse_uint_range(value, ASSET_ID_MAX, &number) || number == 0U) goto numeric;
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

static SceneFormatResult parse_sprite_field(
    SceneSpriteInstance *sprite, unsigned *seen, char *key, char *value,
    const char *path, size_t line, SceneDiagnostic *diagnostic
) {
    unsigned bit;
    unsigned number;
    double tuple[2];
    if (strcmp(key, "asset_kind") == 0) bit = 1U;
    else if (strcmp(key, "asset_id") == 0) bit = 2U;
    else if (strcmp(key, "position") == 0) bit = 4U;
    else return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_UNKNOWN, path,
                       "sprite_instance", key, "unknown sprite field", line,
                       sprite->id);
    if ((*seen & bit) != 0U)
        return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_DUPLICATE, path,
                      "sprite_instance", key, "duplicate sprite field", line,
                      sprite->id);
    *seen |= bit;
    if (bit == 1U) {
        if (strcmp(value, "sprite_pattern") != 0) goto numeric;
        sprite->asset.kind = SCENE_ASSET_KIND_SPRITE_PATTERN;
    } else if (bit == 2U) {
        if (!parse_uint_range(value, SPRITE_ID_CAPACITY - 1U, &number) ||
            number == 0U) goto numeric;
        sprite->asset.id = (uint16_t)number;
    } else {
        if (!parse_double_tuple(value, tuple, 2U)) goto numeric;
        sprite->x = tuple[0];
        sprite->y = tuple[1];
    }
    return SCENE_FORMAT_OK;
numeric:
    return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_NUMERIC, path,
                  "sprite_instance", key, "invalid sprite field value", line,
                  sprite->id);
}

static SceneFormatResult parse_trigger_field(
    SceneTrigger *trigger, unsigned *seen, char *key, char *value,
    const char *path, size_t line, SceneDiagnostic *diagnostic
) {
    unsigned bit, number;
    if (strcmp(key, "min_x") == 0) bit = 1U;
    else if (strcmp(key, "min_y") == 0) bit = 2U;
    else if (strcmp(key, "max_x") == 0) bit = 4U;
    else if (strcmp(key, "max_y") == 0) bit = 8U;
    else if (strcmp(key, "condition") == 0) bit = 16U;
    else if (strcmp(key, "action") == 0) bit = 32U;
    else if (strcmp(key, "flag_id") == 0) bit = 64U;
    else if (strcmp(key, "flag_value") == 0) bit = 128U;
    else if (strcmp(key, "target_id") == 0) bit = 256U;
    else return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_UNKNOWN, path,
                       "trigger", key, "unknown trigger field", line, trigger->id);
    if ((*seen & bit) != 0U)
        return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_DUPLICATE, path,
                      "trigger", key, "duplicate trigger field", line, trigger->id);
    *seen |= bit;
    if (bit <= 8U) {
        double *out = bit == 1U ? &trigger->min_x : bit == 2U ? &trigger->min_y :
                      bit == 4U ? &trigger->max_x : &trigger->max_y;
        if (!parse_double_c(value, out)) goto numeric;
    } else if (bit == 16U) {
        if (strcmp(value, "enter_region") != 0) goto numeric;
        trigger->condition = SCENE_TRIGGER_CONDITION_ENTER_REGION;
    } else if (bit == 32U) {
        if (strcmp(value, "set_flag") == 0) trigger->action = SCENE_TRIGGER_ACTION_SET_FLAG;
        else if (strcmp(value, "teleport_to_spawn") == 0)
            trigger->action = SCENE_TRIGGER_ACTION_TELEPORT_TO_SPAWN;
        else if (strcmp(value, "toggle_light") == 0)
            trigger->action = SCENE_TRIGGER_ACTION_TOGGLE_LIGHT;
        else goto numeric;
    } else if (bit == 64U) {
        if (!parse_uint_range(value, SCENE_TRIGGER_FLAG_CAPACITY, &number) || !number)
            goto numeric;
        trigger->flag_id = (uint8_t)number;
    } else if (bit == 128U) {
        if (!parse_uint_range(value, 1U, &number)) goto numeric;
        trigger->flag_value = number != 0U;
    } else if (!parse_u64(value, &trigger->target_id) || trigger->target_id == 0U)
        goto numeric;
    return SCENE_FORMAT_OK;
numeric:
    return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_NUMERIC, path, "trigger",
                  key, "invalid trigger field value", line, trigger->id);
}

static SceneFormatResult parse_optical_field(
    OpticalExtension *optical, unsigned *seen, char *key, char *value,
    const char *path, const char *section, size_t line, SceneInstanceId id,
    SceneDiagnostic *diagnostic
) {
    unsigned bit;
    unsigned number;
    uint8_t *target;
    unsigned maximum = 255U;
    if (strcmp(key, "player_blocks") == 0) {
        bit = OPTICAL_OVERRIDE_PLAYER_BLOCKS;
        target = &optical->player_blocks;
        maximum = 1U;
    } else if (strcmp(key, "ray_blocks") == 0) {
        bit = OPTICAL_OVERRIDE_RAY_BLOCKS;
        target = &optical->ray_blocks;
        maximum = 1U;
    } else if (strcmp(key, "light_blocks") == 0) {
        bit = OPTICAL_OVERRIDE_LIGHT_BLOCKS;
        target = &optical->light_blocks;
        maximum = 1U;
    } else if (strcmp(key, "opacity") == 0) {
        bit = OPTICAL_OVERRIDE_OPACITY;
        target = &optical->opacity;
    } else if (strcmp(key, "transmission") == 0) {
        bit = OPTICAL_OVERRIDE_TRANSMISSION;
        target = &optical->transmission;
    } else if (strcmp(key, "reflectivity") == 0) {
        bit = OPTICAL_OVERRIDE_REFLECTIVITY;
        target = &optical->reflectivity;
    } else {
        return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_UNKNOWN, path,
                      section, key, "unknown optical field", line, id);
    }
    if ((*seen & bit) != 0U) {
        return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_DUPLICATE, path,
                      section, key, "duplicate optical field", line, id);
    }
    if (!parse_uint_range(value, maximum, &number)) {
        return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_NUMERIC, path,
                      section, key, "invalid optical field value", line, id);
    }
    *seen |= bit;
    optical->override_mask = (uint8_t)(optical->override_mask | bit);
    *target = (uint8_t)number;
    return SCENE_FORMAT_OK;
}

static SceneFormatResult finalize_section(SectionHeader header, unsigned seen,
                                          SceneDecalSurface decal_surface,
                                           SceneTriggerActionType trigger_action,
                                          unsigned int source_version,
                                          const char *path, size_t line,
                                          SceneDiagnostic *diagnostic) {
    unsigned required;
    if (header.kind == SECTION_MOVEMENT) required = 127U;
    else if (header.kind == SECTION_LIGHT) required = source_version >= SCENE_VERSION_V7
        ? 255U : 15U;
    else if (header.kind == SECTION_SPRITE) required = 7U;
    else if (header.kind == SECTION_TRIGGER) {
        required = 63U;
        if (trigger_action == SCENE_TRIGGER_ACTION_SET_FLAG) required |= 64U | 128U;
        else if (trigger_action == SCENE_TRIGGER_ACTION_TOGGLE_LIGHT) required |= 256U;
        if (seen != required)
            return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_REQUIRED_MISSING, path,
                          "trigger", NULL, "missing or inapplicable trigger field",
                          line, header.id);
    }
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
    } else if (header.kind == SECTION_OPTICAL_MATERIAL ||
               header.kind == SECTION_OPTICAL_CELL) {
        if (seen == 0U) {
            return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_REQUIRED_MISSING,
                          path, header.kind == SECTION_OPTICAL_MATERIAL
                              ? "optical_material" : "optical_cell",
                          NULL, "optical block has no fields", line, header.id);
        }
        return SCENE_FORMAT_OK;
    } else return SCENE_FORMAT_OK;
    if ((seen & required) != required)
        return reject(diagnostic,
                      header.kind == SECTION_MOVEMENT
                          ? SCENE_DIAGNOSTIC_INPUT_MOVEMENT
                          : SCENE_DIAGNOSTIC_INPUT_REQUIRED_MISSING, path,
                      header.kind == SECTION_MOVEMENT ? "movement" :
                       header.kind == SECTION_LIGHT ? "light" :
                       header.kind == SECTION_SPRITE ? "sprite_instance" :
                       header.kind == SECTION_TRIGGER ? "trigger" :
                       "decal_instance",
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
    size_t light_count = 0U, decal_count = 0U, sprite_count = 0U, trigger_count = 0U;
    size_t cells_sections = 0U;
    size_t occupancy_sections = 0U, wall_sections = 0U;
    size_t floor_sections = 0U, ceiling_sections = 0U;
    size_t floor_height_sections = 0U, ceiling_height_sections = 0U;
    size_t floor_presence_sections = 0U, ceiling_presence_sections = 0U;
    size_t gravity_scale_sections = 0U;
    size_t gravity_orientation_sections = 0U, movement_sections = 0U;
    size_t optical_cell_count = 0U;
    size_t optical_material_capacity = 0U;
    SceneFormatResult result = SCENE_FORMAT_OK;
    if (diagnostic) scene_diagnostic_reset(diagnostic);
    if (!source || !out_candidate) return SCENE_FORMAT_INVALID_ARGUMENT;
    if (source_size > SCENE_FILE_MAX_BYTES)
        return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_DIMENSIONS, path, NULL,
                      NULL, "scene file exceeds 8 MiB", 0U, 0U);
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
            else if (current == SECTION_FLOOR_HEIGHTS) floor_height_sections++;
            else if (current == SECTION_CEILING_HEIGHTS) ceiling_height_sections++;
            else if (current == SECTION_FLOOR_PRESENCE) floor_presence_sections++;
            else if (current == SECTION_CEILING_PRESENCE) ceiling_presence_sections++;
            else if (current == SECTION_GRAVITY_SCALES) gravity_scale_sections++;
            else if (current == SECTION_GRAVITY_ORIENTATIONS) gravity_orientation_sections++;
            else if (current == SECTION_MOVEMENT) movement_sections++;
            else if (current == SECTION_LIGHT) light_count++;
            else if (current == SECTION_DECAL) decal_count++;
            else if (current == SECTION_SPRITE) sprite_count++;
            else if (current == SECTION_TRIGGER) trigger_count++;
            else if (current == SECTION_OPTICAL_MATERIAL) {
                if (header.id >= OPTICAL_MATERIAL_CAPACITY_MAX) {
                    result = reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_NUMERIC,
                                    path, "optical_material", NULL,
                                    "material ID exceeds 65535", line.number,
                                    header.id);
                    goto done;
                }
                if ((size_t)header.id + 1U > optical_material_capacity)
                    optical_material_capacity = (size_t)header.id + 1U;
            } else if (current == SECTION_OPTICAL_CELL) optical_cell_count++;
            if (cells_sections > 1U || occupancy_sections > 1U || wall_sections > 1U ||
                floor_sections > 1U || ceiling_sections > 1U ||
                floor_height_sections > 1U || ceiling_height_sections > 1U ||
                floor_presence_sections > 1U || ceiling_presence_sections > 1U ||
                gravity_scale_sections > 1U ||
                gravity_orientation_sections > 1U || movement_sections > 1U) {
                result = reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_DUPLICATE, path,
                                NULL, NULL, "duplicate grid section", line.number, 0U);
                goto done;
            }
            if (light_count > SCENE_MAX_LIGHTS || decal_count > SCENE_MAX_DECALS ||
                sprite_count > SCENE_MAX_SPRITES || trigger_count > SCENE_MAX_TRIGGERS) {
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
    if (temporary.source_version >= SCENE_VERSION_V5 && movement_sections != 1U) {
        result = reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_MOVEMENT, path,
                        "movement", NULL, "required movement section is missing",
                        0U, 0U);
        goto done;
    }
    if (temporary.source_version < SCENE_VERSION_V6 &&
        (optical_material_capacity != 0U || optical_cell_count != 0U)) {
        result = reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_UNSUPPORTED_VERSION,
                        path, NULL, "scene_version",
                        "optical blocks require scene version 6", 0U, 0U);
        goto done;
    }
    if (temporary.source_version < SCENE_VERSION_V8 && sprite_count != 0U) {
        result = reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_UNSUPPORTED_VERSION,
                        path, NULL, "scene_version",
                        "sprite blocks require scene version 8", 0U, 0U);
        goto done;
    }
    if (temporary.source_version < SCENE_VERSION_V9 && trigger_count != 0U) {
        result = reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_UNSUPPORTED_VERSION,
                        path, NULL, "scene_version",
                        "trigger blocks require scene version 9", 0U, 0U);
        goto done;
    }
    if ((metadata_seen & (temporary.source_version >= SCENE_VERSION_V3
                              ? META_V3_REQUIRED : META_REQUIRED)) !=
            (temporary.source_version >= SCENE_VERSION_V3
                 ? META_V3_REQUIRED : META_REQUIRED) ||
        (temporary.source_version < SCENE_VERSION_V3 &&
         (metadata_seen & (META_EAST_GROWTH | META_SOUTH_GROWTH))) ||
        (temporary.source_version == SCENE_VERSION_V1 &&
         (cells_sections != 1U || occupancy_sections || wall_sections ||
          floor_sections || ceiling_sections)) ||
        (temporary.source_version >= SCENE_VERSION_V2 &&
         (cells_sections || occupancy_sections != 1U || wall_sections != 1U ||
           floor_sections != 1U || ceiling_sections != 1U)) ||
        (temporary.source_version < SCENE_VERSION_V5 &&
          (floor_height_sections || ceiling_height_sections || floor_presence_sections ||
           ceiling_presence_sections ||
          gravity_scale_sections || gravity_orientation_sections || movement_sections)) ||
        (temporary.source_version >= SCENE_VERSION_V5 &&
         (floor_height_sections != 1U || ceiling_height_sections != 1U ||
           floor_presence_sections != 1U || ceiling_presence_sections != 1U ||
           gravity_scale_sections != 1U ||
          gravity_orientation_sections != 1U || movement_sections != 1U))) {
        result = reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_REQUIRED_MISSING, path, NULL,
                        NULL, "required metadata or versioned grids are missing", 0U, 0U);
        goto done;
    }
    result = allocate_candidate_arrays(
        &temporary, light_count, decal_count, sprite_count, trigger_count,
        optical_material_capacity,
        optical_cell_count, path, diagnostic);
    if (result != SCENE_FORMAT_OK) goto done;

    memcpy(storage, source, source_size); storage[source_size] = '\0';
    reader = (LineReader){storage, source_size, 0U, 0U};
    current = SECTION_NONE;
    {
        SectionHeader header = {SECTION_NONE, 0U};
        unsigned fields_seen = 0U;
        size_t rows[11] = {0U};
        size_t light_index = 0U, decal_index = 0U, sprite_index = 0U, trigger_index = 0U;
        size_t optical_cell_index = 0U;
        while (line_reader_next(&reader, &line)) {
            char *text = trim(line.text);
            if (*text == '\0' || *text == '#') continue;
            if (*text == '[') {
                SceneDecalSurface surface = SCENE_DECAL_SURFACE_WALL;
                if (header.kind == SECTION_DECAL && decal_index > 0U)
                    surface = temporary.decals[decal_index - 1U].surface;
                result = finalize_section(header, fields_seen, surface,
                                          header.kind == SECTION_TRIGGER && trigger_index
                                              ? temporary.triggers[trigger_index - 1U].action
                                              : SCENE_TRIGGER_ACTION_SET_FLAG,
                                          temporary.source_version,
                                          path, line.number,
                                          diagnostic);
                if (result != SCENE_FORMAT_OK) goto done;
                if (!parse_header(text, &header)) { result = SCENE_FORMAT_REJECTED; goto done; }
                current = header.kind;
                fields_seen = 0U;
                if ((current == SECTION_LIGHT || current == SECTION_DECAL ||
                     current == SECTION_SPRITE || current == SECTION_TRIGGER) &&
                    (header.id == 0U || candidate_has_id(&temporary, header.id))) {
                    result = reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_INSTANCE_ID,
                                    path, current == SECTION_LIGHT ? "light" :
                                    current == SECTION_SPRITE ? "sprite_instance" :
                                    current == SECTION_TRIGGER ? "trigger" :
                                    "decal_instance",
                                    NULL, "zero or duplicate instance ID", line.number,
                                    header.id); goto done;
                }
                if (current == SECTION_LIGHT) {
                    temporary.lights[light_index].id = header.id;
                    scene_light_set_point_defaults(&temporary.lights[light_index]);
                    temporary.light_count = ++light_index;
                } else if (current == SECTION_DECAL) {
                    temporary.decals[decal_index].id = header.id;
                    temporary.decal_count = ++decal_index;
                } else if (current == SECTION_SPRITE) {
                    temporary.sprites[sprite_index].id = header.id;
                    temporary.sprite_count = ++sprite_index;
                } else if (current == SECTION_TRIGGER) {
                    temporary.triggers[trigger_index].id = header.id;
                    temporary.trigger_count = ++trigger_index;
                } else if (current == SECTION_OPTICAL_MATERIAL) {
                    OpticalExtension *extension =
                        &temporary.optical_material_defaults[header.id];
                    if (extension->override_mask != 0U) {
                        result = reject(diagnostic,
                                        SCENE_DIAGNOSTIC_INPUT_DUPLICATE, path,
                                        "optical_material", NULL,
                                        "duplicate optical material block",
                                        line.number, header.id);
                        goto done;
                    }
                } else if (current == SECTION_OPTICAL_CELL) {
                    size_t cell_count;
                    if (!checked_size_2d(temporary.map.width,
                                         temporary.map.height, &cell_count) ||
                        header.id >= cell_count) {
                        result = reject(diagnostic,
                                        SCENE_DIAGNOSTIC_INPUT_NUMERIC, path,
                                        "optical_cell", NULL,
                                        "optical cell index is out of range",
                                        line.number, header.id);
                        goto done;
                    }
                    temporary.optical_cell_overrides[optical_cell_index].cell_index =
                        (uint32_t)header.id;
                    temporary.optical_cell_override_count = ++optical_cell_index;
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
            } else if (current >= SECTION_FLOOR_HEIGHTS &&
                       current <= SECTION_GRAVITY_ORIENTATIONS) {
                size_t row_index = (size_t)(current - SECTION_FLOOR_HEIGHTS) + 5U;
                if (rows[row_index] >= (size_t)temporary.map.height) {
                    result = reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_DIMENSIONS,
                                    path, section_name(current), NULL,
                                    "too many v5 grid rows", line.number, 0U);
                    goto done;
                }
                result = parse_v5_row(&temporary, current, text,
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
                if (current == SECTION_MOVEMENT)
                    result = parse_movement_field(&temporary.movement, &fields_seen,
                                                  key, value, path, line.number,
                                                  diagnostic);
                else if (current == SECTION_LIGHT)
                    result = parse_light_field(&temporary.lights[light_index - 1U],
                                               &fields_seen, key, value, path,
                                               line.number, diagnostic);
                else if (current == SECTION_DECAL)
                    result = parse_decal_field(&temporary.decals[decal_index - 1U],
                                               &fields_seen, key, value, path,
                                               line.number, diagnostic);
                else if (current == SECTION_SPRITE)
                    result = parse_sprite_field(
                        &temporary.sprites[sprite_index - 1U], &fields_seen,
                        key, value, path, line.number, diagnostic);
                else if (current == SECTION_TRIGGER)
                    result = parse_trigger_field(
                        &temporary.triggers[trigger_index - 1U], &fields_seen,
                        key, value, path, line.number, diagnostic);
                else if (current == SECTION_OPTICAL_MATERIAL)
                    result = parse_optical_field(
                        &temporary.optical_material_defaults[header.id],
                        &fields_seen, key, value, path, "optical_material",
                        line.number, header.id, diagnostic);
                else if (current == SECTION_OPTICAL_CELL)
                    result = parse_optical_field(
                        &temporary.optical_cell_overrides[
                            optical_cell_index - 1U].optical,
                        &fields_seen, key, value, path, "optical_cell",
                        line.number, header.id, diagnostic);
                else result = reject(
                    diagnostic, SCENE_DIAGNOSTIC_INPUT_UNKNOWN, path, NULL,
                    key, "property is not valid in this section",
                    line.number, header.id);
            }
            if (result != SCENE_FORMAT_OK) goto done;
        }
        result = finalize_section(
            header, fields_seen,
            header.kind == SECTION_DECAL && decal_index > 0U
                ? temporary.decals[decal_index - 1U].surface
                : SCENE_DECAL_SURFACE_WALL,
            header.kind == SECTION_TRIGGER && trigger_index
                ? temporary.triggers[trigger_index - 1U].action
                : SCENE_TRIGGER_ACTION_SET_FLAG,
            temporary.source_version,
            path, reader.line_number,
                                  diagnostic);
        if (result != SCENE_FORMAT_OK) goto done;
        if ((temporary.source_version == SCENE_VERSION_V1 &&
             rows[0] != (size_t)temporary.map.height) ||
            (temporary.source_version >= SCENE_VERSION_V2 &&
             (rows[1] != (size_t)temporary.map.height ||
              rows[2] != (size_t)temporary.map.height ||
              rows[3] != (size_t)temporary.map.height ||
               rows[4] != (size_t)temporary.map.height)) ||
            (temporary.source_version >= SCENE_VERSION_V5 &&
             (rows[5] != (size_t)temporary.map.height ||
              rows[6] != (size_t)temporary.map.height ||
              rows[7] != (size_t)temporary.map.height ||
              rows[8] != (size_t)temporary.map.height ||
               rows[9] != (size_t)temporary.map.height ||
               rows[10] != (size_t)temporary.map.height))) {
            result = reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_DIMENSIONS, path,
                            NULL, NULL, "wrong number of versioned grid rows", 0U, 0U);
            goto done;
        }
    }
    if (temporary.source_version >= SCENE_VERSION_V2) {
        size_t i;
        for (i = 0U; i < temporary.authored_cell_count; i++) {
            const SceneAuthoredCell *cell = &temporary.authored_cells[i];
            temporary.map.cells[i].material_id =
                cell->occupancy == SCENE_CELL_OCCUPANCY_WALL
                    ? (int)cell->wall_material : 0;
        }
    }
    if (temporary.optical_cell_override_count > 1U) {
        qsort(temporary.optical_cell_overrides,
              temporary.optical_cell_override_count,
              sizeof(*temporary.optical_cell_overrides),
              compare_optical_cell_overrides);
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

static int compare_sprite_ptrs(const void *left, const void *right) {
    const SceneSpriteInstance *a = *(const SceneSpriteInstance *const *)left;
    const SceneSpriteInstance *b = *(const SceneSpriteInstance *const *)right;
    return a->id < b->id ? -1 : a->id > b->id;
}

static int compare_trigger_ptrs(const void *left, const void *right) {
    const SceneTrigger *a = *(const SceneTrigger *const *)left;
    const SceneTrigger *b = *(const SceneTrigger *const *)right;
    return a->id < b->id ? -1 : a->id > b->id;
}

static int compare_optical_cell_overrides(const void *left, const void *right) {
    const OpticalCellOverride *a = left;
    const OpticalCellOverride *b = right;
    return a->cell_index < b->cell_index ? -1 : a->cell_index > b->cell_index;
}

static const char *gravity_orientation_name(SceneGravityOrientation orientation) {
    switch (orientation) {
        case SCENE_GRAVITY_DOWN: return "down";
        case SCENE_GRAVITY_UP: return "up";
        case SCENE_GRAVITY_NORTH: return "north";
        case SCENE_GRAVITY_SOUTH: return "south";
        case SCENE_GRAVITY_EAST: return "east";
        case SCENE_GRAVITY_WEST: return "west";
        default: return "";
    }
}

static bool writer_optical_fields(TextWriter *writer,
                                  const OpticalExtension *optical) {
    if ((optical->override_mask & OPTICAL_OVERRIDE_PLAYER_BLOCKS) != 0U &&
        !writer_printf(writer, "player_blocks = %u\n", optical->player_blocks))
        return false;
    if ((optical->override_mask & OPTICAL_OVERRIDE_RAY_BLOCKS) != 0U &&
        !writer_printf(writer, "ray_blocks = %u\n", optical->ray_blocks))
        return false;
    if ((optical->override_mask & OPTICAL_OVERRIDE_LIGHT_BLOCKS) != 0U &&
        !writer_printf(writer, "light_blocks = %u\n", optical->light_blocks))
        return false;
    if ((optical->override_mask & OPTICAL_OVERRIDE_OPACITY) != 0U &&
        !writer_printf(writer, "opacity = %u\n", optical->opacity))
        return false;
    if ((optical->override_mask & OPTICAL_OVERRIDE_TRANSMISSION) != 0U &&
        !writer_printf(writer, "transmission = %u\n", optical->transmission))
        return false;
    if ((optical->override_mask & OPTICAL_OVERRIDE_REFLECTIVITY) != 0U &&
        !writer_printf(writer, "reflectivity = %u\n", optical->reflectivity))
        return false;
    return true;
}

SceneFormatResult scene_format_serialize(const SceneFormatCandidate *candidate,
                                         SceneFormatBuffer *out,
                                         SceneDiagnostic *diagnostic) {
    TextWriter writer = {0};
    const SceneLight *lights[SCENE_MAX_LIGHTS];
    const SceneDecalInstance *decals[SCENE_MAX_DECALS];
    const SceneSpriteInstance *sprites[SCENE_MAX_SPRITES];
    const SceneTrigger *triggers[SCENE_MAX_TRIGGERS];
    size_t i, x, y;
    locale_t c_locale;
    locale_t previous;
    SceneFormatResult result;
    unsigned int output_version;
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
    output_version = candidate->source_version == 0U
        ? SCENE_VERSION_V1 : candidate->source_version;
    for (i = 0U; i < candidate->light_count; i++) lights[i] = &candidate->lights[i];
    for (i = 0U; i < candidate->decal_count; i++) decals[i] = &candidate->decals[i];
    for (i = 0U; i < candidate->sprite_count; i++) sprites[i] = &candidate->sprites[i];
    for (i = 0U; i < candidate->trigger_count; i++) triggers[i] = &candidate->triggers[i];
    qsort(lights, candidate->light_count, sizeof(lights[0]), compare_light_ptrs);
    qsort(decals, candidate->decal_count, sizeof(decals[0]), compare_decal_ptrs);
    qsort(sprites, candidate->sprite_count, sizeof(sprites[0]), compare_sprite_ptrs);
    qsort(triggers, candidate->trigger_count, sizeof(triggers[0]), compare_trigger_ptrs);
#define APPEND(expression) do { if (!(expression)) goto allocation_failed; } while (0)
    APPEND(writer_printf(&writer, "scene_type = terminal_scene\nscene_version = %u\nname = ",
                         output_version));
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
    if (output_version >= SCENE_VERSION_V3) {
        APPEND(writer_append(&writer, "east_growth = "));
        if (candidate->east_growth_count == 0U) APPEND(writer_append(&writer, "-"));
        for (i = 0U; i < candidate->east_growth_count; i++)
            APPEND(writer_printf(&writer, "%s%d", i ? "," : "", candidate->east_growth[i]));
        APPEND(writer_append(&writer, "\nsouth_growth = "));
        if (candidate->south_growth_count == 0U) APPEND(writer_append(&writer, "-"));
        for (i = 0U; i < candidate->south_growth_count; i++)
            APPEND(writer_printf(&writer, "%s%d", i ? "," : "", candidate->south_growth[i]));
        APPEND(writer_append(&writer, "\n"));
    }
    if (candidate->legacy_source_path) {
        APPEND(writer_append(&writer, "legacy_source_path = "));
        APPEND(writer_quoted(&writer, candidate->legacy_source_path));
        APPEND(writer_append(&writer, "\n"));
    }
    if (output_version >= SCENE_VERSION_V5) {
        const SceneMovementParameters *movement = &candidate->movement;
        APPEND(writer_append(&writer, "\n[movement]\ngravity_magnitude = "));
        APPEND(writer_double(&writer, movement->gravity_magnitude));
        APPEND(writer_printf(&writer, "\ngravity_orientation = %s\nstep_height = ",
                             gravity_orientation_name(movement->gravity_orientation)));
        APPEND(writer_double(&writer, movement->step_height));
        APPEND(writer_append(&writer, "\njump_impulse = "));
        APPEND(writer_double(&writer, movement->jump_impulse));
        APPEND(writer_append(&writer, "\nair_control_scale = "));
        APPEND(writer_double(&writer, movement->air_control_scale));
        APPEND(writer_append(&writer, "\neye_height = "));
        APPEND(writer_double(&writer, movement->eye_height));
        APPEND(writer_append(&writer, "\nhead_clearance = "));
        APPEND(writer_double(&writer, movement->head_clearance));
        APPEND(writer_append(&writer, "\n"));
    }
    if (output_version >= SCENE_VERSION_V2) {
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
                    if (sections[section_index] == SECTION_OCCUPANCY) {
                        APPEND(writer_printf(&writer, x ? " %u" : "%u", value));
                    } else if (output_version >= SCENE_VERSION_V4) {
                        char block[SCENE_BLOCK_TEXT_SIZE];
                        APPEND(scene_block_format((uint16_t)value, block));
                        APPEND(writer_printf(&writer, x ? " %s" : "%s", block));
                    } else {
                        APPEND(writer_printf(&writer, x ? " %03u" : "%03u", value));
                    }
                }
                APPEND(writer_append(&writer, "\n"));
            }
        }
        if (output_version >= SCENE_VERSION_V5) {
            const SectionKind sections[] = {
                SECTION_FLOOR_HEIGHTS, SECTION_CEILING_HEIGHTS,
                SECTION_FLOOR_PRESENCE, SECTION_CEILING_PRESENCE,
                SECTION_GRAVITY_SCALES,
                SECTION_GRAVITY_ORIENTATIONS
            };
            const char *names[] = {
                "floor_heights", "ceiling_heights", "floor_presence",
                "ceiling_presence",
                "gravity_scales", "gravity_orientations"
            };
            size_t section_index;
            for (section_index = 0U; section_index < 6U; section_index++) {
                APPEND(writer_printf(&writer, "\n[%s]\n", names[section_index]));
                for (y = 0U; y < (size_t)candidate->map.height; y++) {
                    for (x = 0U; x < (size_t)candidate->map.width; x++) {
                        const SceneAuthoredCell *cell = &candidate->authored_cells[
                            y * (size_t)candidate->map.width + x];
                        unsigned value;
                        if (sections[section_index] == SECTION_FLOOR_HEIGHTS)
                            value = cell->floor_height_step;
                        else if (sections[section_index] == SECTION_CEILING_HEIGHTS)
                            value = cell->ceiling_height_step;
                        else if (sections[section_index] == SECTION_FLOOR_PRESENCE)
                            value = cell->floor_present ? 1U : 0U;
                        else if (sections[section_index] == SECTION_CEILING_PRESENCE)
                            value = cell->ceiling_present ? 1U : 0U;
                        else if (sections[section_index] == SECTION_GRAVITY_SCALES)
                            value = cell->gravity_scale_step;
                        else value = cell->gravity_orientation;
                        if (sections[section_index] == SECTION_FLOOR_HEIGHTS ||
                            sections[section_index] == SECTION_CEILING_HEIGHTS ||
                            sections[section_index] == SECTION_GRAVITY_SCALES) {
                            char block[SCENE_BLOCK_TEXT_SIZE];
                            APPEND(scene_block_format((uint16_t)value, block));
                            APPEND(writer_printf(&writer, x ? " %s" : "%s", block));
                        } else {
                            APPEND(writer_printf(&writer, x ? " %u" : "%u", value));
                        }
                    }
                    APPEND(writer_append(&writer, "\n"));
                }
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
        APPEND(writer_double(&writer, light->radius));
        if (output_version >= SCENE_VERSION_V7) {
            APPEND(writer_printf(&writer, "\ntype = %s\ndirection = ",
                                 light->type == SCENE_LIGHT_SPOT ? "spot" : "point"));
            APPEND(writer_double(&writer, light->direction));
            APPEND(writer_append(&writer, "\ncone = "));
            APPEND(writer_double(&writer, light->cone));
            APPEND(writer_append(&writer, "\nfalloff = "));
            APPEND(writer_double(&writer, light->falloff));
        }
        APPEND(writer_append(&writer, "\n"));
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
    if (output_version >= SCENE_VERSION_V8) {
        for (i = 0U; i < candidate->sprite_count; i++) {
            const SceneSpriteInstance *sprite = sprites[i];
            APPEND(writer_printf(
                &writer, "\n[sprite_instance %" PRIu64 "]\n"
                "asset_kind = sprite_pattern\nasset_id = %u\nposition = ",
                sprite->id, sprite->asset.id));
            APPEND(writer_double(&writer, sprite->x));
            APPEND(writer_append(&writer, ","));
            APPEND(writer_double(&writer, sprite->y));
            APPEND(writer_append(&writer, "\n"));
        }
    }
    if (output_version >= SCENE_VERSION_V9) {
        for (i = 0U; i < candidate->trigger_count; i++) {
            const SceneTrigger *trigger = triggers[i];
            const char *action = trigger->action == SCENE_TRIGGER_ACTION_SET_FLAG
                ? "set_flag" : trigger->action == SCENE_TRIGGER_ACTION_TELEPORT_TO_SPAWN
                ? "teleport_to_spawn" : "toggle_light";
            APPEND(writer_printf(&writer, "\n[trigger %" PRIu64 "]\nmin_x = ",
                                 trigger->id));
            APPEND(writer_double(&writer, trigger->min_x));
            APPEND(writer_append(&writer, "\nmin_y = "));
            APPEND(writer_double(&writer, trigger->min_y));
            APPEND(writer_append(&writer, "\nmax_x = "));
            APPEND(writer_double(&writer, trigger->max_x));
            APPEND(writer_append(&writer, "\nmax_y = "));
            APPEND(writer_double(&writer, trigger->max_y));
            APPEND(writer_printf(&writer,
                "\ncondition = enter_region\naction = %s\n", action));
            if (trigger->action == SCENE_TRIGGER_ACTION_SET_FLAG)
                APPEND(writer_printf(&writer, "flag_id = %u\nflag_value = %u\n",
                    trigger->flag_id, trigger->flag_value ? 1U : 0U));
            else if (trigger->action == SCENE_TRIGGER_ACTION_TOGGLE_LIGHT)
                APPEND(writer_printf(&writer, "target_id = %" PRIu64 "\n",
                                     trigger->target_id));
        }
    }
    if (output_version >= SCENE_VERSION_V6) {
        for (i = 0U; i < candidate->optical_material_capacity; i++) {
            const OpticalExtension *optical =
                &candidate->optical_material_defaults[i];
            if (optical->override_mask == 0U) continue;
            APPEND(writer_printf(&writer, "\n[optical_material %zu]\n", i));
            APPEND(writer_optical_fields(&writer, optical));
        }
        for (i = 0U; i < candidate->optical_cell_override_count; i++) {
            const OpticalCellOverride *override =
                &candidate->optical_cell_overrides[i];
            APPEND(writer_printf(&writer, "\n[optical_cell %u]\n",
                                 override->cell_index));
            APPEND(writer_optical_fields(&writer, &override->optical));
        }
    }
#undef APPEND
    uselocale(previous); freelocale(c_locale);
    if (writer.size > SCENE_FILE_MAX_BYTES) {
        free(writer.data);
        return reject(diagnostic, SCENE_DIAGNOSTIC_INPUT_DIMENSIONS, NULL, NULL,
                      NULL, "canonical scene exceeds 8 MiB", 0U, 0U);
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
