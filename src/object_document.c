#define _POSIX_C_SOURCE 200809L

#include "object_document.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static bool valid_name(const char *name) {
    size_t length;
    if (!name || name[0] == '\0') return false;
    length = strlen(name);
    if (length >= OBJECT_NAME_CAPACITY) return false;
    for (size_t i = 0U; i < length; i++) {
        unsigned char ch = (unsigned char)name[i];
        if (!(isalnum(ch) || ch == '_' || ch == '-')) return false;
    }
    return true;
}

bool object_asset_name_has_prefix(const char *name, const char *prefix) {
    if (!name || !prefix) return false;
    while (*prefix) {
        unsigned char a = (unsigned char)*name++;
        unsigned char b = (unsigned char)*prefix++;
        if (a == '\0' || tolower(a) != tolower(b)) return false;
    }
    return true;
}

int object_asset_find_by_name(const AssetRegistry *assets, const char *name) {
    if (!assets || !name) return -1;
    for (uint16_t id = 1U; id < OBJECT_ID_CAPACITY; id++)
        if (assets->objects[id].loaded &&
            strcmp(assets->objects[id].name, name) == 0) return (int)id;
    return -1;
}

uint16_t object_asset_allocate_id(const AssetRegistry *assets) {
    if (!assets) return 0U;
    for (uint16_t id = 1U; id < OBJECT_ID_CAPACITY; id++)
        if (!assets->objects[id].loaded) return id;
    return 0U;
}

ObjectDocumentResult object_document_create_atomic(
    const AssetRegistry *assets, const char *object_directory,
    const char *name, uint16_t sprite_id, double front_direction,
    uint16_t *out_id
) {
    char path[1200];
    char temporary[1240];
    uint16_t id;
    int fd;
    FILE *file;
    bool failed = false;
    if (out_id) *out_id = 0U;
    if (!assets || !object_directory || !out_id || !isfinite(front_direction) ||
        front_direction < 0.0 || front_direction >= SCENE_LIGHT_DIRECTION_MAX)
        return OBJECT_DOCUMENT_INVALID_ARGUMENT;
    if (!valid_name(name)) return OBJECT_DOCUMENT_INVALID_NAME;
    if (object_asset_find_by_name(assets, name) >= 0)
        return OBJECT_DOCUMENT_DUPLICATE_NAME;
    if (sprite_id == 0U || !sprite_id_is_loaded(assets, sprite_id))
        return OBJECT_DOCUMENT_INVALID_SPRITE;
    id = object_asset_allocate_id(assets);
    if (!id) return OBJECT_DOCUMENT_FULL;
    if (snprintf(path, sizeof(path), "%s/%u.txt", object_directory,
                 (unsigned)id) >= (int)sizeof(path) ||
        snprintf(temporary, sizeof(temporary), "%s.tmp.XXXXXX", path) >=
            (int)sizeof(temporary)) return OBJECT_DOCUMENT_IO_ERROR;
    fd = mkstemp(temporary);
    if (fd < 0) return OBJECT_DOCUMENT_IO_ERROR;
    file = fdopen(fd, "w");
    if (!file) {
        (void)close(fd); (void)unlink(temporary);
        return OBJECT_DOCUMENT_IO_ERROR;
    }
    if (fprintf(file, "name=%s\nsprite_id=%u\nfront_direction=%.17g\n"
                      "attributes=simple\n", name, (unsigned)sprite_id,
                front_direction) < 0) failed = true;
    if (!failed && fflush(file) != 0) failed = true;
    if (!failed && fsync(fd) != 0) failed = true;
    if (fclose(file) != 0) failed = true;
    if (!failed && rename(temporary, path) != 0) failed = true;
    if (failed) { (void)unlink(temporary); return OBJECT_DOCUMENT_IO_ERROR; }
    *out_id = id;
    return OBJECT_DOCUMENT_OK;
}