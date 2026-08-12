#define _POSIX_C_SOURCE 200809L

#include "material_document.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char *duplicate_string(const char *text) {
    size_t length;
    char *copy;
    if (!text) return NULL;
    length = strlen(text);
    copy = malloc(length + 1U);
    if (copy) memcpy(copy, text, length + 1U);
    return copy;
}

static bool valid_name(const char *name) {
    size_t i;
    size_t length;
    if (!name || name[0] == '\0') return false;
    length = strlen(name);
    if (length >= MATERIAL_NAME_CAPACITY) return false;
    for (i = 0U; i < length; i++) {
        unsigned char ch = (unsigned char)name[i];
        if (!(isalnum(ch) || ch == '_' || ch == '-')) return false;
    }
    return true;
}

static bool name_available(const AssetRegistry *assets, const char *name,
                           uint16_t own_id) {
    int found;
    if (!assets || !name) return false;
    found = material_find_by_name(assets, name);
    return found < 0 || found == (int)own_id;
}

static MaterialDocumentResult validate_value(const MaterialDocumentValue *value,
                                             const AssetRegistry *assets) {
    if (!value || !assets || value->id == 0U) {
        return MATERIAL_DOCUMENT_INVALID_ARGUMENT;
    }
    if (!valid_name(value->name)) return MATERIAL_DOCUMENT_INVALID_NAME;
    if (!name_available(assets, value->name, value->id)) {
        return MATERIAL_DOCUMENT_DUPLICATE_NAME;
    }
    if (value->palette_id == 0U || !assets->palettes) {
        return MATERIAL_DOCUMENT_INVALID_PALETTE;
    }
    return MATERIAL_DOCUMENT_OK;
}

static MaterialDocumentResult push_change(MaterialDocument *document,
                                          MaterialDocumentValue value) {
    MaterialDocumentChange *grown;
    MaterialDocumentChange *change;
    size_t capacity;
    if (memcmp(&document->value, &value, sizeof(value)) == 0) {
        return MATERIAL_DOCUMENT_NO_CHANGE;
    }
    if (document->state.next_state == 0U ||
        document->state.next_state == UINT64_MAX) {
        return MATERIAL_DOCUMENT_STATE_ID_EXHAUSTED;
    }
    if (document->change_cursor == document->change_capacity) {
        capacity = document->change_capacity == 0U ? 8U :
                   document->change_capacity * 2U;
        grown = realloc(document->changes, capacity * sizeof(*grown));
        if (!grown) return MATERIAL_DOCUMENT_OUT_OF_MEMORY;
        document->changes = grown;
        document->change_capacity = capacity;
    }
    document->change_count = document->change_cursor;
    change = &document->changes[document->change_cursor];
    change->before = document->value;
    change->after = value;
    change->before_state = document->state.current_state;
    if (!asset_document_state_advance(&document->state)) {
        return MATERIAL_DOCUMENT_STATE_ID_EXHAUSTED;
    }
    change->after_state = document->state.current_state;
    document->value = value;
    document->change_cursor++;
    document->change_count = document->change_cursor;
    return MATERIAL_DOCUMENT_OK;
}

void material_document_init(MaterialDocument *document) {
    if (!document) return;
    memset(document, 0, sizeof(*document));
    asset_document_state_init(&document->state);
}

void material_document_destroy(MaterialDocument *document) {
    if (!document) return;
    free(document->changes);
    free(document->path);
    material_document_init(document);
}

MaterialDocumentResult material_document_create(MaterialDocument *document,
                                                const AssetRegistry *assets,
                                                const char *name,
                                                uint16_t palette_id,
                                                const char glyphs[4]) {
    MaterialDocumentValue value;
    MaterialDocumentResult result;
    uint16_t id;
    if (!document || !assets || !glyphs) return MATERIAL_DOCUMENT_INVALID_ARGUMENT;
    id = asset_registry_allocate_material_id(assets);
    memset(&value, 0, sizeof(value));
    value.id = id;
    value.palette_id = palette_id;
    if (name) snprintf(value.name, sizeof(value.name), "%s", name);
    memcpy(value.glyphs, glyphs, 4U);
    result = validate_value(&value, assets);
    if (result != MATERIAL_DOCUMENT_OK) return result;
    material_document_destroy(document);
    document->value = value;
    document->saved_value = value;
    document->state.saved_state = 0U;
    return MATERIAL_DOCUMENT_OK;
}

MaterialDocumentResult material_document_open(MaterialDocument *document,
                                              const AssetRegistry *assets,
                                              uint16_t id,
                                              const char *material_directory) {
    MaterialDocumentValue value;
    char path[1024];
    char *owned_path;
    if (!document || !assets || !material_directory ||
        !material_id_is_loaded(assets, id)) {
        return MATERIAL_DOCUMENT_INVALID_ARGUMENT;
    }
    memset(&value, 0, sizeof(value));
    value.id = id;
    value.palette_id = (uint16_t)assets->materials[id].palette_id;
    memcpy(value.glyphs, assets->materials[id].glyphs, 4U);
    snprintf(value.name, sizeof(value.name), "%s", material_name_by_id(assets, id));
    if (snprintf(path, sizeof(path), "%s/%s.txt", material_directory,
                 value.name) >= (int)sizeof(path)) return MATERIAL_DOCUMENT_IO_ERROR;
    owned_path = duplicate_string(path);
    if (!owned_path) return MATERIAL_DOCUMENT_OUT_OF_MEMORY;
    material_document_destroy(document);
    document->value = value;
    document->saved_value = value;
    document->path = owned_path;
    return MATERIAL_DOCUMENT_OK;
}

MaterialDocumentResult material_document_set_name(MaterialDocument *document,
                                                  const AssetRegistry *assets,
                                                  const char *name) {
    MaterialDocumentValue value;
    if (!document || !assets || !valid_name(name)) return MATERIAL_DOCUMENT_INVALID_NAME;
    if (!name_available(assets, name, document->value.id)) {
        return MATERIAL_DOCUMENT_DUPLICATE_NAME;
    }
    value = document->value;
    snprintf(value.name, sizeof(value.name), "%s", name);
    return push_change(document, value);
}

MaterialDocumentResult material_document_set_palette(MaterialDocument *document,
                                                     const AssetRegistry *assets,
                                                     uint16_t palette_id) {
    MaterialDocumentValue value;
    if (!document || !assets || palette_id == 0U || !assets->palettes) {
        return MATERIAL_DOCUMENT_INVALID_PALETTE;
    }
    value = document->value;
    value.palette_id = palette_id;
    return push_change(document, value);
}

MaterialDocumentResult material_document_set_glyphs(MaterialDocument *document,
                                                    const char glyphs[4]) {
    MaterialDocumentValue value;
    if (!document || !glyphs) return MATERIAL_DOCUMENT_INVALID_ARGUMENT;
    value = document->value;
    memcpy(value.glyphs, glyphs, 4U);
    return push_change(document, value);
}

MaterialDocumentResult material_document_undo(MaterialDocument *document) {
    MaterialDocumentChange *change;
    if (!document) return MATERIAL_DOCUMENT_INVALID_ARGUMENT;
    if (document->change_cursor == 0U) return MATERIAL_DOCUMENT_NOTHING_TO_UNDO;
    change = &document->changes[--document->change_cursor];
    document->value = change->before;
    asset_document_state_restore(&document->state, change->before_state);
    return MATERIAL_DOCUMENT_OK;
}

MaterialDocumentResult material_document_redo(MaterialDocument *document) {
    MaterialDocumentChange *change;
    if (!document) return MATERIAL_DOCUMENT_INVALID_ARGUMENT;
    if (document->change_cursor >= document->change_count) {
        return MATERIAL_DOCUMENT_NOTHING_TO_REDO;
    }
    change = &document->changes[document->change_cursor++];
    document->value = change->after;
    asset_document_state_restore(&document->state, change->after_state);
    return MATERIAL_DOCUMENT_OK;
}

void material_document_discard(MaterialDocument *document) {
    if (!document) return;
    document->value = document->saved_value;
    document->change_count = 0U;
    document->change_cursor = 0U;
    asset_document_state_restore(&document->state, document->state.saved_state);
}

bool material_document_is_dirty(const MaterialDocument *document) {
    return document && asset_document_state_is_dirty(&document->state);
}

Material material_document_preview(const MaterialDocument *document) {
    Material material;
    memset(&material, 0, sizeof(material));
    if (!document) return material;
    material.id = document->value.id;
    material.palette_id = document->value.palette_id;
    memcpy(material.glyphs, document->value.glyphs, 4U);
    return material;
}

static MaterialDocumentResult atomic_write(const MaterialDocument *document,
                                           const char *path) {
    char temp_path[1200];
    int fd;
    FILE *file;
    bool failed = false;
    if (snprintf(temp_path, sizeof(temp_path), "%s.tmp.XXXXXX", path) >=
        (int)sizeof(temp_path)) return MATERIAL_DOCUMENT_IO_ERROR;
    fd = mkstemp(temp_path);
    if (fd < 0) return MATERIAL_DOCUMENT_IO_ERROR;
    file = fdopen(fd, "w");
    if (!file) {
        close(fd);
        unlink(temp_path);
        return MATERIAL_DOCUMENT_IO_ERROR;
    }
    if (fprintf(file, "id=%u\npalette=%u\nglyphs=%c%c%c%c\n",
                (unsigned)document->value.id,
                (unsigned)document->value.palette_id,
                document->value.glyphs[0], document->value.glyphs[1],
                document->value.glyphs[2], document->value.glyphs[3]) < 0) failed = true;
    if (!failed && fflush(file) != 0) failed = true;
    if (!failed && fsync(fd) != 0) failed = true;
    if (fclose(file) != 0) failed = true;
    if (!failed && rename(temp_path, path) != 0) failed = true;
    if (failed) {
        unlink(temp_path);
        return MATERIAL_DOCUMENT_IO_ERROR;
    }
    return MATERIAL_DOCUMENT_OK;
}

MaterialDocumentResult material_document_save(MaterialDocument *document) {
    MaterialDocumentResult result;
    if (!document) return MATERIAL_DOCUMENT_INVALID_ARGUMENT;
    if (!document->path) return MATERIAL_DOCUMENT_NO_PATH;
    result = atomic_write(document, document->path);
    if (result == MATERIAL_DOCUMENT_OK) {
        document->saved_value = document->value;
        asset_document_state_mark_saved(&document->state);
    }
    return result;
}

MaterialDocumentResult material_document_save_as(MaterialDocument *document,
                                                 const char *material_directory) {
    char path[1024];
    char *owned;
    MaterialDocumentResult result;
    if (!document || !material_directory || !valid_name(document->value.name)) {
        return MATERIAL_DOCUMENT_INVALID_ARGUMENT;
    }
    if (snprintf(path, sizeof(path), "%s/%s.txt", material_directory,
                 document->value.name) >= (int)sizeof(path)) {
        return MATERIAL_DOCUMENT_IO_ERROR;
    }
    owned = duplicate_string(path);
    if (!owned) return MATERIAL_DOCUMENT_OUT_OF_MEMORY;
    result = atomic_write(document, path);
    if (result != MATERIAL_DOCUMENT_OK) {
        free(owned);
        return result;
    }
    free(document->path);
    document->path = owned;
    document->saved_value = document->value;
    asset_document_state_mark_saved(&document->state);
    return MATERIAL_DOCUMENT_OK;
}

MaterialDocumentResult material_document_commit_to_registry(
    const MaterialDocument *document, AssetRegistry *assets) {
    char glyphs[5];
    bool was_loaded;
    if (!document || !assets || !document->path) {
        return MATERIAL_DOCUMENT_INVALID_ARGUMENT;
    }
    memcpy(glyphs, document->value.glyphs, 4U);
    glyphs[4] = '\0';
    was_loaded = material_id_is_loaded(assets, document->value.id);
    asset_registry_set_material(assets, document->value.id,
                                document->value.palette_id, glyphs);
    snprintf(assets->material_names[document->value.id], MATERIAL_NAME_CAPACITY,
             "%s", document->value.name);
    if (!was_loaded) assets->material_count++;
    (void)asset_registry_bump_generation(assets);
    return MATERIAL_DOCUMENT_OK;
}