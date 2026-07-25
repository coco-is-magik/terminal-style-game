/**
 * scene_document.c — SceneDocument lifecycle, load, save, and accessors
 *
 * Ownership model:
 *   - SceneDocument embeds Map by value. Heap Map* from map_create /
 *     map_load_from_string is moved in (fields copied, source pointer
 *     nulled before free of the shell) so destroy never double-frees.
 *   - path is owned heap storage (strdup).
 *   - light_map is never serialized.
 *
 * Save is atomic: write temp in destination directory, fflush, fclose,
 * rename over destination. Failures before rename leave the destination
 * untouched and remove the temp file.
 */

#define _POSIX_C_SOURCE 200809L

#include "scene_document.h"

#include "scene_document_internal.h"
#include "map_loader.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>   /* close(), mkstemp() */


/* ===================================================================
 *  Internal helpers
 * =================================================================== */

static void map_clear_owned(Map *map) {
    if (!map) return;
    free(map->cells);
    free(map->light_map);
    map->cells = NULL;
    map->light_map = NULL;
    map->width = 0;
    map->height = 0;
}

/* Move heap Map* contents into an embedded Map and free the shell. */
static void map_move_from_heap(Map *dst, Map *src_heap) {
    map_clear_owned(dst);
    if (!src_heap) return;
    *dst = *src_heap;
    src_heap->cells = NULL;
    src_heap->light_map = NULL;
    src_heap->width = 0;
    src_heap->height = 0;
    free(src_heap);
}

static char *duplicate_path(const char *path) {
    if (!path) return NULL;
    size_t len = strlen(path);
    char *copy = malloc(len + 1);
    if (!copy) return NULL;
    memcpy(copy, path, len + 1);
    return copy;
}

static bool map_has_positive_dims(const Map *map) {
    return map && map->width > 0 && map->height > 0 && map->cells != NULL;
}

static bool map_materials_serializable(const Map *map) {
    if (!map_has_positive_dims(map)) return false;
    size_t n = (size_t)map->width * (size_t)map->height;
    for (size_t i = 0; i < n; i++) {
        int id = map->cells[i].material_id;
        if (id < 0 || id > 9) return false;
    }
    return true;
}

/* Read entire file into a NUL-terminated heap buffer. Caller frees. */
static char *read_file_text(const char *path, SceneLoadResult *err_out) {
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        if (err_out) {
            *err_out = (errno == ENOENT) ? SCENE_LOAD_FILE_NOT_FOUND
                                         : SCENE_LOAD_PARSE_ERROR;
        }
        return NULL;
    }

    if (fseek(fp, 0, SEEK_END) != 0) {
        fclose(fp);
        if (err_out) *err_out = SCENE_LOAD_PARSE_ERROR;
        return NULL;
    }

    long size = ftell(fp);
    if (size < 0) {
        fclose(fp);
        if (err_out) *err_out = SCENE_LOAD_PARSE_ERROR;
        return NULL;
    }

    if (fseek(fp, 0, SEEK_SET) != 0) {
        fclose(fp);
        if (err_out) *err_out = SCENE_LOAD_PARSE_ERROR;
        return NULL;
    }

    char *buf = malloc((size_t)size + 1);
    if (!buf) {
        fclose(fp);
        if (err_out) *err_out = SCENE_LOAD_OUT_OF_MEMORY;
        return NULL;
    }

    size_t nread = fread(buf, 1, (size_t)size, fp);
    if (nread != (size_t)size) {
        free(buf);
        fclose(fp);
        if (err_out) *err_out = SCENE_LOAD_PARSE_ERROR;
        return NULL;
    }
    buf[size] = '\0';
    fclose(fp);
    if (err_out) *err_out = SCENE_LOAD_OK;
    return buf;
}

/*
 * Build temp path beside destination: "<dir>/.scene_doc_XXXXXX" via mkstemp.
 * out_path must hold at least path_len + 32 bytes; returns open fd or -1.
 */
static int create_temp_beside(const char *dest_path, char *out_path, size_t out_sz) {
    if (!dest_path || !out_path || out_sz < 8) return -1;

    const char *slash = strrchr(dest_path, '/');
    if (slash) {
        size_t dir_len = (size_t)(slash - dest_path);
        if (dir_len + 1 + 20 >= out_sz) return -1;
        memcpy(out_path, dest_path, dir_len);
        out_path[dir_len] = '/';
        memcpy(out_path + dir_len + 1, ".scene_doc_XXXXXX", 18);
    } else {
        if (20 >= out_sz) return -1;
        memcpy(out_path, ".scene_doc_XXXXXX", 18);
    }

    int fd = mkstemp(out_path);
    return fd;
}

static SceneSaveResult write_map_digits(FILE *fp, const Map *map) {
    for (int y = 0; y < map->height; y++) {
        for (int x = 0; x < map->width; x++) {
            int id = map->cells[y * map->width + x].material_id;
            if (id < 0 || id > 9) return SCENE_SAVE_UNREPRESENTABLE_MATERIAL;
            if (fputc('0' + id, fp) == EOF) return SCENE_SAVE_WRITE_FAILED;
        }
        if (fputc('\n', fp) == EOF) return SCENE_SAVE_WRITE_FAILED;
    }
    return SCENE_SAVE_OK;
}

/* ===================================================================
 *  Public lifecycle
 * =================================================================== */

void scene_document_init(SceneDocument *document) {
    if (!document) return;
    document->map.width = 0;
    document->map.height = 0;
    document->map.cells = NULL;
    document->map.light_map = NULL;
    document->current_state = 0;
    document->saved_state = 0;
    document->path = NULL;
}

void scene_document_destroy(SceneDocument *document) {
    if (!document) return;
    map_clear_owned(&document->map);
    free(document->path);
    document->path = NULL;
    document->current_state = 0;
    document->saved_state = 0;
}

/* ===================================================================
 *  Load (transactional)
 * =================================================================== */

SceneLoadResult scene_document_load(SceneDocument *document, const char *path) {
    if (!document || !path || path[0] == '\0') {
        return SCENE_LOAD_VALIDATION_FAILED;
    }

    SceneLoadResult read_err = SCENE_LOAD_OK;
    char *text = read_file_text(path, &read_err);
    if (!text) return read_err;

    Map *temp = map_load_from_string(text);
    free(text);
    if (!temp) return SCENE_LOAD_PARSE_ERROR;

    if (!map_has_positive_dims(temp)) {
        map_destroy(temp);
        return SCENE_LOAD_VALIDATION_FAILED;
    }

    char *new_path = duplicate_path(path);
    if (!new_path) {
        map_destroy(temp);
        return SCENE_LOAD_OUT_OF_MEMORY;
    }

    /* Commit: replace old ownership only after all fallible work. */
    map_move_from_heap(&document->map, temp);
    free(document->path);
    document->path = new_path;
    document->current_state = 1;
    document->saved_state = 1;
    return SCENE_LOAD_OK;
}

/* ===================================================================
 *  Save (atomic)
 * =================================================================== */

SceneSaveResult scene_document_validate_for_save(const SceneDocument *document) {
    if (!document) return SCENE_SAVE_INVALID_DOCUMENT;
    if (!document->path || document->path[0] == '\0') return SCENE_SAVE_NO_PATH;
    if (!map_has_positive_dims(&document->map)) return SCENE_SAVE_INVALID_DOCUMENT;
    if (!map_materials_serializable(&document->map)) {
        return SCENE_SAVE_UNREPRESENTABLE_MATERIAL;
    }
    return SCENE_SAVE_OK;
}

SceneSaveResult scene_document_save(SceneDocument *document) {
    SceneSaveResult v = scene_document_validate_for_save(document);
    if (v != SCENE_SAVE_OK) return v;

    char temp_path[4096];
    int fd = create_temp_beside(document->path, temp_path, sizeof(temp_path));
    if (fd < 0) return SCENE_SAVE_TEMP_CREATE_FAILED;

    FILE *fp = fdopen(fd, "wb");
    if (!fp) {
        close(fd);
        remove(temp_path);
        return SCENE_SAVE_TEMP_CREATE_FAILED;
    }

    SceneSaveResult wr = write_map_digits(fp, &document->map);
    if (wr != SCENE_SAVE_OK) {
        fclose(fp);
        remove(temp_path);
        return wr;
    }

    if (fflush(fp) != 0) {
        fclose(fp);
        remove(temp_path);
        return SCENE_SAVE_FLUSH_FAILED;
    }

    if (fclose(fp) != 0) {
        remove(temp_path);
        return SCENE_SAVE_CLOSE_FAILED;
    }

    if (rename(temp_path, document->path) != 0) {
        remove(temp_path);
        return SCENE_SAVE_REPLACE_FAILED;
    }

    document->saved_state = document->current_state;
    return SCENE_SAVE_OK;
}

/* ===================================================================
 *  Accessors
 * =================================================================== */

const Map *scene_document_get_map(const SceneDocument *document) {
    if (!document) return NULL;
    return &document->map;
}

Map *scene_document_get_map_for_runtime(SceneDocument *document) {
    if (!document) return NULL;
    return &document->map;
}

bool scene_document_get_wall_material(
    const SceneDocument *document,
    WallMaterialRef ref,
    MaterialId *out_material
) {
    if (!document || !out_material) return false;
    /* map_in_bounds / map_get take non-const Map*; cast is safe for read. */
    Map *map = (Map *)&document->map;
    if (!map_in_bounds(map, ref.map_x, ref.map_y)) return false;
    MapCell *cell = map_get(map, ref.map_x, ref.map_y);
    if (!cell) return false;
    *out_material = cell->material_id;
    return true;
}

bool scene_document_is_dirty(const SceneDocument *document) {
    if (!document) return false;
    return document->current_state != document->saved_state;
}

/* ===================================================================
 *  Internal mutations (command system only)
 * =================================================================== */

bool scene_document_internal_set_wall_material(
    SceneDocument *document,
    WallMaterialRef ref,
    MaterialId material
) {
    if (!document) return false;
    if (!map_in_bounds(&document->map, ref.map_x, ref.map_y)) return false;
    map_set(&document->map, ref.map_x, ref.map_y, material);
    return true;
}

void scene_document_internal_set_current_state(
    SceneDocument *document,
    DocumentStateId state
) {
    if (!document) return;
    document->current_state = state;
}
