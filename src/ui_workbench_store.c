#define _POSIX_C_SOURCE 200809L

#include "ui_workbench_store.h"

#include "platform_fs.h"
#include "number_parse.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const char *type_name(UiElementType type) {
    switch (type) {
        case UI_ELE_CONTAINER: return "container";
        case UI_ELE_TEXT: return "text";
        case UI_ELE_BUTTON: return "button";
        case UI_ELE_ANIMATION: return "animation";
        default: return NULL;
    }
}

static const char *coords_name(UiCoordMode mode) {
    return mode == UI_COORD_RELATIVE ? "relative" : "absolute";
}

static const char *align_name(UiAlign align) {
    switch (align) {
        case UI_ALIGN_LEFT: return "left";
        case UI_ALIGN_CENTER: return "center";
        case UI_ALIGN_RIGHT: return "right";
        default: return NULL;
    }
}

static bool safe_text(const char *text) {
    return text && strchr(text, '\n') == NULL && strchr(text, '\r') == NULL;
}

bool ui_workbench_element_is_valid(const UiElement *element) {
    const char *type;
    const char *align;
    if (!element || element->name[0] == '\0' || !safe_text(element->name) ||
        !safe_text(element->parent_name) || !safe_text(element->action) ||
        !safe_text(element->content ? element->content : "") ||
        element->layout.width < 0 || element->layout.height < 0 ||
        element->visible < 0 || element->visible > 1) return false;
    if (element->parent_name[0] != '\0' &&
        strcmp(element->name, element->parent_name) == 0) return false;
    type = type_name(element->type);
    align = align_name(element->align);
    return type && align && ui_ele_style_is_valid(element->type, element->style) &&
        ui_ele_transition_is_valid(element->transition) &&
        ui_ele_focus_effect_is_valid(element->focus_effect) &&
        ui_ele_animation_is_valid(element);
}

static bool write_element(FILE *file, const UiElement *element) {
    int i;
    if (fprintf(file, "name=%s\ntype=%s\n", element->name,
                type_name(element->type)) < 0) return false;
    if (element->parent_name[0] != '\0' &&
        fprintf(file, "parent=%s\n", element->parent_name) < 0) return false;
    if (fprintf(file, "x=%d\ny=%d\ncoords=%s\nwidth=%d\nheight=%d\n",
                element->layout.x, element->layout.y,
                coords_name(element->layout.coords_mode), element->layout.width,
                element->layout.height) < 0) return false;
    if (element->child_count > 0) {
        if (fputs("children=", file) < 0) return false;
        for (i = 0; i < element->child_count; i++) {
            if (i > 0 && fputc(',', file) == EOF) return false;
            if (fputs(element->child_names[i], file) < 0) return false;
        }
        if (fputc('\n', file) == EOF) return false;
    }
    if (fprintf(file, "visible=%d\nz_index=%d\nalign=%s\n",
                element->visible, element->z_index, align_name(element->align)) < 0)
        return false;
    if (element->has_fg && fprintf(file, "fg=%u,%u,%u,%u\n", element->fg.r,
                                   element->fg.g, element->fg.b, element->fg.a) < 0)
        return false;
    if (element->has_bg && fprintf(file, "bg=%u,%u,%u,%u\n", element->bg.r,
                                   element->bg.g, element->bg.b, element->bg.a) < 0)
        return false;
    if (element->action[0] != '\0' &&
        fprintf(file, "action=%s\n", element->action) < 0) return false;
    if (element->type == UI_ELE_ANIMATION &&
        fprintf(file, "preset=%s\ntarget=%s\ntrigger=%s\norientation=%s\n"
                "loop=%d\nrandomize=%d\n", element->preset, element->target,
                element->trigger, element->orientation, element->loop,
                element->randomize) < 0) return false;
    if (fprintf(file, "style=%s\ntransition=%s\nfocus_effect=%s\ncontent=%s\n",
                element->style, element->transition, element->focus_effect,
                element->content ? element->content : "") < 0) return false;
    return true;
}

static UiWorkbenchStoreResult store_element(const UiElement *element,
                                             const char *path,
                                             bool destination_exists) {
    char temporary[UI_ELE_PATH_MAX + 32U];
    int descriptor;
    FILE *file;
    bool failed = false;
    PlatformFileMetadata metadata;
    PlatformNativeError error;
    PlatformFsResult result;
    PlatformReplaceResult replacement;
    if (!path || path[0] == '\0') return UI_WORKBENCH_STORE_INVALID_ARGUMENT;
    if (!ui_workbench_element_is_valid(element)) return UI_WORKBENCH_STORE_INVALID_ELEMENT;
    if (snprintf(temporary, sizeof(temporary), "%s.tmp.XXXXXX", path) >=
        (int)sizeof(temporary)) return UI_WORKBENCH_STORE_INVALID_ARGUMENT;
    descriptor = mkstemp(temporary);
    if (descriptor < 0) return UI_WORKBENCH_STORE_IO_ERROR;
    file = fdopen(descriptor, "w");
    if (!file) {
        (void)close(descriptor);
        (void)unlink(temporary);
        return UI_WORKBENCH_STORE_IO_ERROR;
    }
    result = platform_fs_inspect_nofollow(path, &metadata, &error);
    if (destination_exists) {
        if (result != PLATFORM_FS_OK || !metadata.is_regular_file ||
            metadata.is_link_or_reparse ||
            platform_fs_apply_metadata(file, &metadata, &error) != PLATFORM_FS_OK)
            failed = true;
    } else if (result != PLATFORM_FS_NOT_FOUND) failed = true;
    if (!failed && !write_element(file, element)) failed = true;
    if (!failed && fflush(file) != 0) failed = true;
    if (!failed && platform_fs_sync_file(file, &error) != PLATFORM_FS_OK) failed = true;
    if (fclose(file) != 0) failed = true;
    if (failed) {
        (void)unlink(temporary);
        return UI_WORKBENCH_STORE_IO_ERROR;
    }
    replacement = platform_fs_replace(temporary, path, destination_exists);
    if (replacement.commit_state == PLATFORM_COMMIT_NOT_COMMITTED) {
        (void)unlink(temporary);
        return UI_WORKBENCH_STORE_IO_ERROR;
    }
    return replacement.commit_state == PLATFORM_COMMIT_COMMITTED_DURABILITY_WARNING
        ? UI_WORKBENCH_STORE_OK_DURABILITY_WARNING : UI_WORKBENCH_STORE_OK;
}

UiWorkbenchStoreResult ui_workbench_store_element(const UiElement *element,
                                                   const char *path) {
    return store_element(element, path, true);
}

UiWorkbenchStoreResult ui_workbench_create_element(const UiElement *element,
                                                    const char *path) {
    return store_element(element, path, false);
}

static char *read_all(const char *path, size_t *out_size) {
    FILE *file;
    long length;
    char *text;
    if (!path || !out_size || !(file = fopen(path, "rb"))) return NULL;
    if (fseek(file, 0, SEEK_END) != 0 || (length = ftell(file)) < 0 || length > 16777216L ||
        fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }
    text = malloc((size_t)length + 1U);
    if (!text) { fclose(file); return NULL; }
    if (fread(text, 1U, (size_t)length, file) != (size_t)length) {
        (void)fclose(file);
        free(text);
        return NULL;
    }
    if (fclose(file) != 0) {
        free(text);
        return NULL;
    }
    text[length] = '\0';
    *out_size = (size_t)length;
    return text;
}

static bool name_in_csv(const char *csv, const char *name) {
    size_t length = strlen(name);
    const char *cursor = csv;
    while ((cursor = strstr(cursor, name)) != NULL) {
        bool left = cursor == csv || cursor[-1] == ',';
        bool right = cursor[length] == '\0' || cursor[length] == '\n' ||
                     cursor[length] == '\r' || cursor[length] == ',';
        if (left && right) return true;
        cursor += length;
    }
    return false;
}

static char *rewrite_csv_line(const char *text, const char *prefix,
                              const char *name, bool add) {
    const char *line = strstr(text, prefix);
    const char *value;
    const char *end;
    char csv[4096] = {0};
    char rebuilt[4096] = {0};
    char *result;
    size_t before;
    size_t after;
    if (!line || (line != text && line[-1] != '\n')) return NULL;
    value = line + strlen(prefix);
    end = strchr(value, '\n');
    if (!end) end = value + strlen(value);
    if ((size_t)(end - value) >= sizeof(csv)) return NULL;
    memcpy(csv, value, (size_t)(end - value));
    if (add) {
        if (name_in_csv(csv, name)) return NULL;
        if (snprintf(rebuilt, sizeof(rebuilt), "%s%s%s", csv,
                     csv[0] ? "," : "", name) >= (int)sizeof(rebuilt)) return NULL;
    } else {
        char copy[4096];
        char *token;
        bool found = false;
        (void)snprintf(copy, sizeof(copy), "%s", csv);
        token = strtok(copy, ",");
        while (token) {
            if (strcmp(token, name) == 0) found = true;
            else if (snprintf(rebuilt + strlen(rebuilt),
                              sizeof(rebuilt) - strlen(rebuilt), "%s%s",
                              rebuilt[0] ? "," : "", token) >=
                     (int)(sizeof(rebuilt) - strlen(rebuilt))) return NULL;
            token = strtok(NULL, ",");
        }
        if (!found) return NULL;
    }
    before = (size_t)(value - text);
    after = strlen(end);
    result = malloc(before + strlen(rebuilt) + after + 1U);
    if (!result) return NULL;
    memcpy(result, text, before);
    memcpy(result + before, rebuilt, strlen(rebuilt));
    memcpy(result + before + strlen(rebuilt), end, after + 1U);
    return result;
}

static UiWorkbenchStoreResult prepare_text(const char *path, const char *text,
                                           char *temporary, size_t capacity) {
    int descriptor;
    FILE *file;
    PlatformFileMetadata metadata;
    PlatformNativeError error;
    bool failed = false;
    int written = snprintf(temporary, capacity, "%s.tmp.XXXXXX", path);
    if (written < 0 || (size_t)written >= capacity ||
        platform_fs_inspect_nofollow(path, &metadata, &error) != PLATFORM_FS_OK ||
        !metadata.is_regular_file || metadata.is_link_or_reparse) {
        temporary[0] = '\0';
        return UI_WORKBENCH_STORE_IO_ERROR;
    }
    descriptor = mkstemp(temporary);
    if (descriptor < 0) {
        temporary[0] = '\0';
        return UI_WORKBENCH_STORE_IO_ERROR;
    }
    file = fdopen(descriptor, "w");
    if (!file) {
        (void)close(descriptor);
        (void)unlink(temporary);
        temporary[0] = '\0';
        return UI_WORKBENCH_STORE_IO_ERROR;
    }
    if (platform_fs_apply_metadata(file, &metadata, &error) != PLATFORM_FS_OK)
        failed = true;
    if (!failed && fputs(text, file) < 0) failed = true;
    if (!failed && fflush(file) != 0) failed = true;
    if (!failed && platform_fs_sync_file(file, &error) != PLATFORM_FS_OK) failed = true;
    if (fclose(file) != 0) failed = true;
    if (failed) {
        unlink(temporary);
        temporary[0] = '\0';
        return UI_WORKBENCH_STORE_IO_ERROR;
    }
    return UI_WORKBENCH_STORE_OK;
}

static bool recovery_path(const char *layout_path, char *path, size_t capacity) {
    return layout_path && layout_path[0] &&
        snprintf(path, capacity, "%s.membership-recovery", layout_path) < (int)capacity;
}

UiWorkbenchStoreResult ui_workbench_recover_membership(const char *layout_path,
                                                       const char *master_path) {
    char path[UI_ELE_PATH_MAX + 32U];
    char temporary[UI_ELE_PATH_MAX + 32U] = {0};
    PlatformFileMetadata metadata;
    PlatformNativeError error;
    PlatformFsResult inspected;
    char *record = NULL;
    char *master = NULL;
    char *line;
    char *payload;
    size_t size;
    size_t ignored;
    int lengths[3];
    size_t total = 0;
    UiWorkbenchStoreResult result = UI_WORKBENCH_STORE_ROLLBACK_FAILED;
    if (!master_path || !recovery_path(layout_path, path, sizeof(path)))
        return UI_WORKBENCH_STORE_INVALID_ARGUMENT;
    inspected = platform_fs_inspect_nofollow(path, &metadata, &error);
    if (inspected == PLATFORM_FS_NOT_FOUND) return UI_WORKBENCH_STORE_OK;
    if (inspected != PLATFORM_FS_OK || !metadata.is_regular_file || metadata.is_link_or_reparse)
        return result;
    record = read_all(path, &size);
    master = read_all(master_path, &ignored);
    if (!record || !master || strlen(record) != size) goto done;
    line = record;
    for (size_t i = 0; i < 3; i++) {
        char *end = strchr(line, '\n');
        if (!end) goto done;
        *end = '\0';
        if (!number_parse_int(line, 1, 4194304, &lengths[i])) goto done;
        total += (size_t)lengths[i];
        line = end + 1;
    }
    payload = line;
    if ((size_t)(payload - record) > size || size - (size_t)(payload - record) != total)
        goto done;
    /* The master replacement is the commit decision. Unknown external edits are
       never overwritten. Repeating recovery after interruption is idempotent. */
    if (strlen(master) == (size_t)lengths[2] &&
        memcmp(master, payload + lengths[0] + lengths[1], (size_t)lengths[2]) == 0) {
        result = unlink(path) == 0 ? UI_WORKBENCH_STORE_OK : UI_WORKBENCH_STORE_ROLLBACK_FAILED;
        goto done;
    }
    if (strlen(master) != (size_t)lengths[1] ||
        memcmp(master, payload + lengths[0], (size_t)lengths[1]) != 0) goto done;
    payload[lengths[0]] = '\0';
    if (prepare_text(layout_path, payload, temporary, sizeof(temporary)) != UI_WORKBENCH_STORE_OK)
        goto done;
    if (platform_fs_replace(temporary, layout_path, true).commit_state == PLATFORM_COMMIT_NOT_COMMITTED)
        goto done;
    result = unlink(path) == 0 ? UI_WORKBENCH_STORE_OK : UI_WORKBENCH_STORE_ROLLBACK_FAILED;
done:
    if (temporary[0]) (void)unlink(temporary);
    free(master);
    free(record);
    return result;
}

static bool publish_recovery(const char *layout_path, const char *layout,
                             const char *master, const char *new_master,
                             char *path, size_t capacity) {
    char temporary[UI_ELE_PATH_MAX + 32U] = {0};
    char *record;
    size_t size = strlen(layout) + strlen(master) + strlen(new_master) + 96U;
    bool success = false;
    if (strlen(layout) > 4194304U || strlen(master) > 4194304U ||
        strlen(new_master) > 4194304U || !recovery_path(layout_path, path, capacity)) return false;
    record = malloc(size);
    if (!record) return false;
    if (snprintf(record, size, "%zu\n%zu\n%zu\n%s%s%s", strlen(layout), strlen(master),
                 strlen(new_master), layout, master, new_master) < (int)size &&
        prepare_text(layout_path, record, temporary, sizeof(temporary)) == UI_WORKBENCH_STORE_OK) {
        PlatformMoveResult moved = platform_fs_move(temporary, path);
        success = moved.commit_state == PLATFORM_COMMIT_COMMITTED;
        if (!success && moved.commit_state != PLATFORM_COMMIT_NOT_COMMITTED) (void)unlink(path);
    }
    if (temporary[0]) (void)unlink(temporary);
    free(record);
    return success;
}

UiWorkbenchStoreResult ui_workbench_store_membership(const char *layout_path,
                                                      const char *master_path,
                                                      const char *layout_name,
                                                      const char *element_name,
                                                      bool add) {
    size_t ignored;
    char *layout;
    char *master;
    char *new_layout;
    char *new_master = NULL;
    char marker[UI_ELE_NAME_MAX + 16U];
    char *section;
    char staged_layout[UI_ELE_PATH_MAX + 32U] = {0};
    char staged_master[UI_ELE_PATH_MAX + 32U] = {0};
    char rollback[UI_ELE_PATH_MAX + 32U] = {0};
    char journal[UI_ELE_PATH_MAX + 32U] = {0};
    bool keep_journal = false;
    UiWorkbenchStoreResult result = UI_WORKBENCH_STORE_IO_ERROR;
    if (!layout_path || !master_path || !layout_name || !element_name ||
        !layout_name[0] || !element_name[0] || !safe_text(layout_name) ||
        !safe_text(element_name) || strchr(element_name, ',') ||
        strchr(layout_name, '=') || strcmp(layout_path, master_path) == 0)
        return UI_WORKBENCH_STORE_INVALID_ARGUMENT;
    if (ui_workbench_recover_membership(layout_path, master_path) != UI_WORKBENCH_STORE_OK)
        return UI_WORKBENCH_STORE_ROLLBACK_FAILED;
    layout = read_all(layout_path, &ignored);
    master = read_all(master_path, &ignored);
    if (!layout || !master || !layout_name || !element_name ||
        snprintf(marker, sizeof(marker), "layout=%s\n", layout_name) >= (int)sizeof(marker))
        goto done;
    new_layout = rewrite_csv_line(layout, "elements=", element_name, add);
    section = strstr(master, marker);
    if (!new_layout || !section || (section != master && section[-1] != '\n')) {
        free(new_layout);
        goto done;
    }
    {
        const char *next_section = strstr(section + strlen(marker), "\nlayout=");
        const char *cache = strstr(section, "\ncache_next=");
        if (!cache || (next_section && cache >= next_section)) {
            free(new_layout);
            goto done;
        }
        char *section_rewritten = rewrite_csv_line(section, "cache_next=", element_name, add);
        size_t prefix = (size_t)(section - master);
        if (section_rewritten) {
            new_master = malloc(prefix + strlen(section_rewritten) + 1U);
            if (new_master) {
                memcpy(new_master, master, prefix);
                memcpy(new_master + prefix, section_rewritten,
                       strlen(section_rewritten) + 1U);
            }
            free(section_rewritten);
        }
    }
    /* Prepare both replacements and compensation before touching either original.
       A second-write failure must not require fresh allocation or free disk space. */
    if (!new_master ||
        prepare_text(layout_path, layout, rollback, sizeof(rollback)) != UI_WORKBENCH_STORE_OK ||
        prepare_text(layout_path, new_layout, staged_layout, sizeof(staged_layout)) != UI_WORKBENCH_STORE_OK ||
        prepare_text(master_path, new_master, staged_master, sizeof(staged_master)) != UI_WORKBENCH_STORE_OK) {
        free(new_layout); goto done;
    }
    if (!publish_recovery(layout_path, layout, master, new_master, journal, sizeof(journal))) {
        journal[0] = '\0';
        free(new_layout); goto done;
    }
    if (platform_fs_replace(staged_layout, layout_path, true).commit_state == PLATFORM_COMMIT_NOT_COMMITTED) {
        free(new_layout); goto done;
    }
    if (platform_fs_replace(staged_master, master_path, true).commit_state == PLATFORM_COMMIT_NOT_COMMITTED) {
        if (platform_fs_replace(rollback, layout_path, true).commit_state == PLATFORM_COMMIT_NOT_COMMITTED) {
            result = UI_WORKBENCH_STORE_ROLLBACK_FAILED;
            keep_journal = true;
            /* Preserve the synchronized original for recovery, never delete it. */
            fprintf(stderr, "UI membership rollback failed: original layout preserved at %s; destination %s\n",
                    rollback, layout_path);
            rollback[0] = '\0';
        }
        free(new_layout);
        goto done;
    }
    free(new_layout);
    result = UI_WORKBENCH_STORE_OK;
done:
    if (journal[0] && !keep_journal) (void)unlink(journal);
    if (staged_layout[0]) (void)unlink(staged_layout);
    if (staged_master[0]) (void)unlink(staged_master);
    if (rollback[0]) (void)unlink(rollback);
    free(new_master);
    free(master);
    free(layout);
    return result;
}