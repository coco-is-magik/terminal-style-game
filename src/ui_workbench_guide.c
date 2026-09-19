#include "ui_workbench_guide.h"

#include "checked_size.h"
#include "grid.h"
#include "ui_workbench_chrome.h"

#include <stdio.h>
#include <string.h>

const char *ui_workbench_guide_result_string(UiWorkbenchGuideResult result) {
    switch (result) {
        case UI_WORKBENCH_GUIDE_OK: return "ok";
        case UI_WORKBENCH_GUIDE_INVALID_ARGUMENT: return "invalid argument";
        case UI_WORKBENCH_GUIDE_LOAD_FAILED: return "load failed";
        default: return "unknown result";
    }
}

static void guide_clear(UiWorkbenchGuide *guide) {
    size_t i;
    if (!guide) return;
    for (i = 0U; i < UI_WORKBENCH_GUIDE_ENTRY_MAX; i++) {
        guide->entries[i].key[0] = '\0';
        guide->entries[i].text[0] = '\0';
    }
    guide->count = 0U;
    guide->loaded = false;
}

static bool guide_append(UiWorkbenchGuide *guide, const char *key,
                         const char *text) {
    UiWorkbenchGuideEntry *entry;
    if (!guide || !key || !text || key[0] == '\0' || text[0] == '\0' ||
        guide->count >= UI_WORKBENCH_GUIDE_ENTRY_MAX) return false;
    entry = &guide->entries[guide->count];
    if (snprintf(entry->key, sizeof(entry->key), "%s", key) >=
        (int)sizeof(entry->key)) return false;
    if (snprintf(entry->text, sizeof(entry->text), "%s", text) >=
        (int)sizeof(entry->text)) return false;
    guide->count++;
    return true;
}

static void strip_trailing(char *line) {
    size_t length = strlen(line);
    while (length > 0U &&
           (line[length - 1U] == '\n' || line[length - 1U] == '\r')) {
        length--;
        line[length] = '\0';
    }
}

UiWorkbenchGuideResult ui_workbench_guide_load(UiWorkbenchGuide *guide,
                                               const char *path) {
    FILE *file;
    char line[512];
    if (!guide || !path || path[0] == '\0')
        return UI_WORKBENCH_GUIDE_INVALID_ARGUMENT;
    guide_clear(guide);
    file = fopen(path, "r");
    if (!file) return UI_WORKBENCH_GUIDE_LOAD_FAILED;
    while (fgets(line, sizeof(line), file)) {
        char *divider;
        strip_trailing(line);
        if (line[0] == '\0' || line[0] == '#') continue;
        divider = strchr(line, '=');
        if (!divider || divider == line || divider[1] == '\0') continue;
        *divider = '\0';
        if (!guide_append(guide, line, divider + 1)) {
            (void)fclose(file);
            guide_clear(guide);
            return UI_WORKBENCH_GUIDE_LOAD_FAILED;
        }
    }
    if (ferror(file)) {
        (void)fclose(file);
        guide_clear(guide);
        return UI_WORKBENCH_GUIDE_LOAD_FAILED;
    }
    (void)fclose(file);
    guide->loaded = true;
    return UI_WORKBENCH_GUIDE_OK;
}

void ui_workbench_guide_destroy(UiWorkbenchGuide *guide) {
    guide_clear(guide);
}

static const char *guide_lookup(const UiWorkbenchGuide *guide,
                                const char *key) {
    size_t i;
    if (!guide || !guide->loaded || !key) return NULL;
    for (i = 0U; i < guide->count; i++) {
        if (strcmp(guide->entries[i].key, key) == 0)
            return guide->entries[i].text;
    }
    return NULL;
}

static const char *property_key(UiWorkbenchProperty property) {
    switch (property) {
        case UI_WORKBENCH_PROPERTY_PARENT: return "ui_property_parent";
        case UI_WORKBENCH_PROPERTY_STYLE: return "ui_property_style";
        case UI_WORKBENCH_PROPERTY_TRANSITION:
            return "ui_property_transition";
        case UI_WORKBENCH_PROPERTY_FOCUS_EFFECT:
            return "ui_property_focus_effect";
        case UI_WORKBENCH_PROPERTY_PRESET: return "ui_property_preset";
        case UI_WORKBENCH_PROPERTY_TARGET: return "ui_property_target";
        case UI_WORKBENCH_PROPERTY_TRIGGER: return "ui_property_trigger";
        case UI_WORKBENCH_PROPERTY_ORIENTATION:
            return "ui_property_orientation";
        case UI_WORKBENCH_PROPERTY_LOOP: return "ui_property_loop";
        case UI_WORKBENCH_PROPERTY_RANDOMIZE: return "ui_property_randomize";
        case UI_WORKBENCH_PROPERTY_WIDTH: return "ui_property_width";
        case UI_WORKBENCH_PROPERTY_HEIGHT: return "ui_property_height";
        default: return NULL;
    }
}

const char *ui_workbench_guide_tooltip(const UiWorkbenchGuide *guide,
                                       const UiWorkbench *workbench) {
    const char *key = NULL;
    const char *text;
    if (!guide || !guide->loaded || !workbench) return NULL;
    if (workbench->mode == UI_WORKBENCH_MODE_ADD) {
        key = "ui_mode_add";
    } else if (workbench->mode == UI_WORKBENCH_MODE_REMOVE_CONFIRM) {
        key = "ui_mode_remove_confirm";
    } else if (workbench->editing) {
        key = property_key(workbench->property);
    } else {
        key = "ui_mode_browse";
    }
    if (!key) return NULL;
    text = guide_lookup(guide, key);
    if (text) return text;
    if (workbench->mode != UI_WORKBENCH_MODE_BROWSE || workbench->editing)
        return guide_lookup(guide, "ui_editor_help");
    return NULL;
}

static bool guide_key_present(const UiWorkbenchGuide *guide, const char *key) {
    return guide_lookup(guide, key) != NULL;
}

size_t ui_workbench_guide_missing_count(const UiWorkbenchGuide *guide,
                                        const UiWorkbench *workbench) {
    static const char *const required[] = {
        "ui_property_parent", "ui_property_style", "ui_property_transition",
        "ui_property_focus_effect", "ui_property_preset", "ui_property_target",
        "ui_property_trigger", "ui_property_orientation", "ui_property_loop",
        "ui_property_randomize", "ui_property_width", "ui_property_height",
        "ui_context_main", "ui_context_pause", "ui_context_settings",
        "ui_context_confirm", "ui_mode_browse", "ui_mode_add",
        "ui_mode_remove_confirm", "ui_action_activate",
        "ui_action_add_confirm", "ui_action_remove_confirm",
        "ui_action_scale", "ui_editor_save_failed", "ui_editor_load_failed",
        "ui_editor_help"
    };
    size_t missing = 0U;
    size_t i;
    (void)workbench;
    if (!guide || !guide->loaded) return sizeof(required) / sizeof(required[0]);
    for (i = 0U; i < sizeof(required) / sizeof(required[0]); i++) {
        if (!guide_key_present(guide, required[i])) missing++;
    }
    return missing;
}

bool ui_workbench_guide_footer_stable(const Grid *grid, int grid_columns,
                                      int grid_rows) {
    int footer_first;
    int status_glyph;
    int controls_glyph;
    int diagnostic_glyph;
    if (!grid || !grid->cells ||
        grid_columns != UI_WORKBENCH_CHROME_COLUMNS ||
        grid_rows != UI_WORKBENCH_CHROME_ROWS) return false;
    footer_first = ui_workbench_chrome_footer_first_row(grid_rows);
    if (footer_first < 0) return false;
    status_glyph =
        (int)grid->cells[(size_t)footer_first * (size_t)grid_columns + 1U]
            .glyph;
    controls_glyph =
        (int)grid->cells[(size_t)(footer_first + 1) * (size_t)grid_columns +
                         1U]
            .glyph;
    diagnostic_glyph =
        (int)grid->cells[(size_t)(footer_first + 2) * (size_t)grid_columns +
                         1U]
            .glyph;
    if (status_glyph == 0 || controls_glyph == 0 || diagnostic_glyph == 0)
        return false;
    return true;
}