/**
 * scene_diagnostic.h — Structured, bounded scene diagnostics
 *
 * Records are plain values: no heap ownership and no global last-error state.
 */

#ifndef SCENE_DIAGNOSTIC_H
#define SCENE_DIAGNOSTIC_H

#include "scene_types.h"

#include <stddef.h>

#define SCENE_DIAGNOSTIC_PATH_MAX 1024U
#define SCENE_DIAGNOSTIC_SECTION_MAX 64U
#define SCENE_DIAGNOSTIC_FIELD_MAX 64U
#define SCENE_DIAGNOSTIC_DETAIL_MAX 192U

typedef enum {
    SCENE_DIAGNOSTIC_NONE = 0,
    SCENE_DIAGNOSTIC_INPUT_SYNTAX,
    SCENE_DIAGNOSTIC_INPUT_REQUIRED_MISSING,
    SCENE_DIAGNOSTIC_INPUT_DUPLICATE,
    SCENE_DIAGNOSTIC_INPUT_UNKNOWN,
    SCENE_DIAGNOSTIC_INPUT_UNSUPPORTED_VERSION,
    SCENE_DIAGNOSTIC_INPUT_DIMENSIONS,
    SCENE_DIAGNOSTIC_INPUT_NUMERIC,
    SCENE_DIAGNOSTIC_INPUT_INSTANCE_ID,
    SCENE_DIAGNOSTIC_INPUT_HIGH_WATER,
    SCENE_DIAGNOSTIC_INPUT_INSTANCE_REFERENCE,
    SCENE_DIAGNOSTIC_INPUT_ASSET_MISSING,
    SCENE_DIAGNOSTIC_INPUT_SAVE_REPAIR_BLOCKED,
    SCENE_DIAGNOSTIC_INPUT_LEGACY_IMPORT,
    SCENE_DIAGNOSTIC_INPUT_HEIGHT_RELATION,
    SCENE_DIAGNOSTIC_INPUT_HEIGHT_RANGE,
    SCENE_DIAGNOSTIC_INPUT_MOVEMENT,
    SCENE_DIAGNOSTIC_INPUT_SURFACE_PRESENCE,
    SCENE_DIAGNOSTIC_INPUT_GRAVITY,
    SCENE_DIAGNOSTIC_ENV_ALLOCATION,
    SCENE_DIAGNOSTIC_ENV_SOURCE_READ,
    SCENE_DIAGNOSTIC_ENV_TEMP_CREATE,
    SCENE_DIAGNOSTIC_ENV_WRITE,
    SCENE_DIAGNOSTIC_ENV_REPLACE,
    SCENE_DIAGNOSTIC_ENV_ID_EXHAUSTED,
    SCENE_DIAGNOSTIC_ENV_DIRECTORY_SYNC,
    SCENE_DIAGNOSTIC_BUG_PREMATURE_COMMIT,
    SCENE_DIAGNOSTIC_BUG_LOAD_MUTATED_LIVE,
    SCENE_DIAGNOSTIC_BUG_SAVE_MUTATED_IDENTITY,
    SCENE_DIAGNOSTIC_BUG_RUNTIME_MUTATED_AUTHORED,
    SCENE_DIAGNOSTIC_BUG_ID_REUSE
} SceneDiagnosticCode;

typedef enum {
    SCENE_DIAGNOSTIC_CATEGORY_NONE = 0,
    SCENE_DIAGNOSTIC_CATEGORY_INPUT,
    SCENE_DIAGNOSTIC_CATEGORY_ENV,
    SCENE_DIAGNOSTIC_CATEGORY_BUG
} SceneDiagnosticCategory;

typedef enum {
    SCENE_DIAGNOSTIC_SEVERITY_NONE = 0,
    SCENE_DIAGNOSTIC_SEVERITY_WARNING,
    SCENE_DIAGNOSTIC_SEVERITY_ERROR,
    SCENE_DIAGNOSTIC_SEVERITY_FATAL
} SceneDiagnosticSeverity;

typedef struct {
    SceneDiagnosticCode code;
    SceneDiagnosticCategory category;
    SceneDiagnosticSeverity severity;
    char path[SCENE_DIAGNOSTIC_PATH_MAX + 1U];
    char section[SCENE_DIAGNOSTIC_SECTION_MAX + 1U];
    char field[SCENE_DIAGNOSTIC_FIELD_MAX + 1U];
    char detail[SCENE_DIAGNOSTIC_DETAIL_MAX + 1U];
    size_t line;
    size_t column;
    SceneInstanceId instance_id;
    int system_error;
} SceneDiagnostic;

void scene_diagnostic_reset(SceneDiagnostic *diagnostic);
const char *scene_diagnostic_id(SceneDiagnosticCode code);
SceneDiagnosticCategory scene_diagnostic_category(SceneDiagnosticCode code);

void scene_diagnostic_set(
    SceneDiagnostic *diagnostic,
    SceneDiagnosticCode code,
    SceneDiagnosticSeverity severity,
    const char *path,
    const char *section,
    const char *field,
    const char *detail
);

#endif /* SCENE_DIAGNOSTIC_H */
