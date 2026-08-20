#include "scene_diagnostic.h"

#include <string.h>

static void copy_bounded(char *destination, size_t capacity, const char *source) {
    size_t length;
    if (!destination || capacity == 0U) return;
    if (!source) {
        destination[0] = '\0';
        return;
    }
    length = strlen(source);
    if (length >= capacity) length = capacity - 1U;
    memcpy(destination, source, length);
    destination[length] = '\0';
}

void scene_diagnostic_reset(SceneDiagnostic *diagnostic) {
    if (!diagnostic) return;
    memset(diagnostic, 0, sizeof(*diagnostic));
}

const char *scene_diagnostic_id(SceneDiagnosticCode code) {
    static const char *const ids[] = {
        "",
        "TSG-SCENE-INPUT-0001", "TSG-SCENE-INPUT-0002",
        "TSG-SCENE-INPUT-0003", "TSG-SCENE-INPUT-0004",
        "TSG-SCENE-INPUT-0005", "TSG-SCENE-INPUT-0006",
        "TSG-SCENE-INPUT-0007", "TSG-SCENE-INPUT-0008",
        "TSG-SCENE-INPUT-0009", "TSG-SCENE-INPUT-0010",
        "TSG-SCENE-INPUT-0011", "TSG-SCENE-INPUT-0012",
        "TSG-SCENE-INPUT-0013", "TSG-SCENE-INPUT-0014",
        "TSG-SCENE-INPUT-0015", "TSG-SCENE-INPUT-0016",
        "TSG-SCENE-INPUT-0017", "TSG-SCENE-INPUT-0018",
        "TSG-SCENE-ENV-0001", "TSG-SCENE-ENV-0002",
        "TSG-SCENE-ENV-0003", "TSG-SCENE-ENV-0004",
        "TSG-SCENE-ENV-0005", "TSG-SCENE-ENV-0006",
        "TSG-SCENE-ENV-0007",
        "TSG-SCENE-BUG-0001", "TSG-SCENE-BUG-0002",
        "TSG-SCENE-BUG-0003", "TSG-SCENE-BUG-0004",
        "TSG-SCENE-BUG-0005"
    };
    size_t index = (size_t)code;
    if (index >= sizeof(ids) / sizeof(ids[0])) return "";
    return ids[index];
}

SceneDiagnosticCategory scene_diagnostic_category(SceneDiagnosticCode code) {
    if (code >= SCENE_DIAGNOSTIC_INPUT_SYNTAX &&
        code <= SCENE_DIAGNOSTIC_INPUT_GRAVITY) {
        return SCENE_DIAGNOSTIC_CATEGORY_INPUT;
    }
    if (code >= SCENE_DIAGNOSTIC_ENV_ALLOCATION &&
        code <= SCENE_DIAGNOSTIC_ENV_DIRECTORY_SYNC) {
        return SCENE_DIAGNOSTIC_CATEGORY_ENV;
    }
    if (code >= SCENE_DIAGNOSTIC_BUG_PREMATURE_COMMIT &&
        code <= SCENE_DIAGNOSTIC_BUG_ID_REUSE) {
        return SCENE_DIAGNOSTIC_CATEGORY_BUG;
    }
    return SCENE_DIAGNOSTIC_CATEGORY_NONE;
}

void scene_diagnostic_set(
    SceneDiagnostic *diagnostic,
    SceneDiagnosticCode code,
    SceneDiagnosticSeverity severity,
    const char *path,
    const char *section,
    const char *field,
    const char *detail
) {
    if (!diagnostic) return;
    scene_diagnostic_reset(diagnostic);
    diagnostic->code = code;
    diagnostic->category = scene_diagnostic_category(code);
    diagnostic->severity = severity;
    copy_bounded(diagnostic->path, sizeof(diagnostic->path), path);
    copy_bounded(diagnostic->section, sizeof(diagnostic->section), section);
    copy_bounded(diagnostic->field, sizeof(diagnostic->field), field);
    copy_bounded(diagnostic->detail, sizeof(diagnostic->detail), detail);
}
