#define _POSIX_C_SOURCE 200809L

#include "ui_workbench_store.h"

#include "platform_fs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const char *type_name(UiElementType type) {
    switch (type) {
        case UI_ELE_CONTAINER: return "container";
        case UI_ELE_TEXT: return "text";
        case UI_ELE_BUTTON: return "button";
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
        ui_ele_focus_effect_is_valid(element->focus_effect);
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
    if (fprintf(file, "style=%s\ntransition=%s\nfocus_effect=%s\ncontent=%s\n",
                element->style, element->transition, element->focus_effect,
                element->content ? element->content : "") < 0) return false;
    return true;
}

UiWorkbenchStoreResult ui_workbench_store_element(const UiElement *element,
                                                   const char *path) {
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
    if (result != PLATFORM_FS_OK || !metadata.is_regular_file ||
        metadata.is_link_or_reparse ||
        platform_fs_apply_metadata(file, &metadata, &error) != PLATFORM_FS_OK)
        failed = true;
    if (!failed && !write_element(file, element)) failed = true;
    if (!failed && fflush(file) != 0) failed = true;
    if (!failed && platform_fs_sync_file(file, &error) != PLATFORM_FS_OK) failed = true;
    if (fclose(file) != 0) failed = true;
    if (failed) {
        (void)unlink(temporary);
        return UI_WORKBENCH_STORE_IO_ERROR;
    }
    replacement = platform_fs_replace(temporary, path, true);
    if (replacement.commit_state == PLATFORM_COMMIT_NOT_COMMITTED) {
        (void)unlink(temporary);
        return UI_WORKBENCH_STORE_IO_ERROR;
    }
    return replacement.commit_state == PLATFORM_COMMIT_COMMITTED_DURABILITY_WARNING
        ? UI_WORKBENCH_STORE_OK_DURABILITY_WARNING : UI_WORKBENCH_STORE_OK;
}