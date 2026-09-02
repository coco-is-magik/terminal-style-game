#define _POSIX_C_SOURCE 200809L

#include "sprite_document.h"

#include "checked_size.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static bool valid_dimensions(size_t cols, size_t rows, size_t *out_count) {
    if (cols == 0U || rows == 0U || cols > SPRITE_PATTERN_MAX_COLS ||
        rows > SPRITE_PATTERN_MAX_ROWS || cols > SIZE_MAX / rows) return false;
    if (out_count) *out_count = cols * rows;
    return true;
}

static char *duplicate_string(const char *text) {
    size_t length;
    char *copy;
    if (!text) return NULL;
    length = strlen(text);
    copy = malloc(length + 1U);
    if (copy) memcpy(copy, text, length + 1U);
    return copy;
}

void sprite_document_init(SpriteDocument *document) {
    if (document) memset(document, 0, sizeof(*document));
}

void sprite_document_destroy(SpriteDocument *document) {
    if (!document) return;
    free(document->cells);
    free(document->path);
    sprite_document_init(document);
}

SpriteDocumentResult sprite_document_create(SpriteDocument *document,
                                            const AssetRegistry *assets,
                                            size_t cols, size_t rows) {
    PatternCell *cells;
    size_t count;
    uint16_t id;
    if (!document || !assets || !valid_dimensions(cols, rows, &count))
        return SPRITE_DOCUMENT_INVALID_DIMENSIONS;
    id = asset_registry_allocate_sprite_id(assets);
    if (id == 0U) return SPRITE_DOCUMENT_INVALID_ARGUMENT;
    cells = calloc(count, sizeof(*cells));
    if (!cells) return SPRITE_DOCUMENT_OUT_OF_MEMORY;
    sprite_document_destroy(document);
    document->id = id;
    document->cols = cols;
    document->rows = rows;
    document->cells = cells;
    document->dirty = true;
    return SPRITE_DOCUMENT_OK;
}

SpriteDocumentResult sprite_document_open_loaded(SpriteDocument *document,
                                                 const AssetRegistry *assets,
                                                 uint16_t id,
                                                 const char *sprite_directory) {
    const SpriteAsset *asset;
    PatternCell *cells;
    char path[1024];
    char *owned_path;
    size_t count;
    size_t bytes;
    if (!document || !assets || !sprite_directory || sprite_directory[0] == '\0')
        return SPRITE_DOCUMENT_INVALID_ARGUMENT;
    asset = asset_registry_get_sprite(assets, (int)id);
    if (!asset || !valid_dimensions((size_t)asset->cols, (size_t)asset->rows, &count) ||
        !checked_size_bytes(count, sizeof(*cells), &bytes))
        return SPRITE_DOCUMENT_INVALID_ARGUMENT;
    if (snprintf(path, sizeof(path), "%s/%u.txt", sprite_directory,
                 (unsigned)id) < 0 || strlen(path) >= sizeof(path))
        return SPRITE_DOCUMENT_IO_ERROR;
    owned_path = duplicate_string(path);
    if (!owned_path) return SPRITE_DOCUMENT_OUT_OF_MEMORY;
    cells = malloc(bytes);
    if (!cells) {
        free(owned_path);
        return SPRITE_DOCUMENT_OUT_OF_MEMORY;
    }
    memcpy(cells, asset->pattern, bytes);
    sprite_document_destroy(document);
    document->id = id;
    document->cols = (size_t)asset->cols;
    document->rows = (size_t)asset->rows;
    document->cells = cells;
    document->path = owned_path;
    return SPRITE_DOCUMENT_OK;
}

SpriteDocumentResult sprite_document_paint_cell(SpriteDocument *document,
                                                const AssetRegistry *assets,
                                                size_t x, size_t y,
                                                PatternCell cell) {
    PatternCell *current;
    if (!document || !assets || !document->cells || x >= document->cols ||
        y >= document->rows) return SPRITE_DOCUMENT_OUT_OF_BOUNDS;
    if (cell.glyph != 0U && cell.glyph != (uint8_t)' ' &&
        !material_id_is_loaded(assets, (int)cell.material_id))
        return SPRITE_DOCUMENT_INVALID_MATERIAL;
    current = &document->cells[y * document->cols + x];
    if (current->glyph == cell.glyph && current->material_id == cell.material_id)
        return SPRITE_DOCUMENT_NO_CHANGE;
    *current = cell;
    document->dirty = true;
    return SPRITE_DOCUMENT_OK;
}

SpriteDocumentResult sprite_document_erase_cell(SpriteDocument *document,
                                                size_t x, size_t y) {
    PatternCell empty = {0U, 0U};
    PatternCell *current;
    if (!document || !document->cells || x >= document->cols || y >= document->rows)
        return SPRITE_DOCUMENT_OUT_OF_BOUNDS;
    current = &document->cells[y * document->cols + x];
    if (current->glyph == 0U && current->material_id == 0U)
        return SPRITE_DOCUMENT_NO_CHANGE;
    *current = empty;
    document->dirty = true;
    return SPRITE_DOCUMENT_OK;
}

static SpriteDocumentResult write_file(const SpriteDocument *document,
                                       const char *path) {
    char temp_path[1060];
    FILE *file;
    int fd;
    size_t y;
    size_t x;
    if (snprintf(temp_path, sizeof(temp_path), "%s.tmpXXXXXX", path) < 0 ||
        strlen(temp_path) >= sizeof(temp_path)) return SPRITE_DOCUMENT_IO_ERROR;
    fd = mkstemp(temp_path);
    if (fd < 0) return SPRITE_DOCUMENT_IO_ERROR;
    file = fdopen(fd, "wb");
    if (!file) {
        close(fd);
        unlink(temp_path);
        return SPRITE_DOCUMENT_IO_ERROR;
    }
    if (fprintf(file, "cols=%zu\nrows=%zu\ndefault_material=1\n",
                document->cols, document->rows) < 0) goto fail;
    for (y = 0U; y < document->rows; y++) {
        if (fprintf(file, "pattern_%zu=", y) < 0) goto fail;
        for (x = 0U; x < document->cols; x++) {
            uint8_t glyph = document->cells[y * document->cols + x].glyph;
            if (fputc(glyph == 0U ? ' ' : (int)glyph, file) == EOF) goto fail;
        }
        if (fputc('\n', file) == EOF || fprintf(file, "material_%zu=", y) < 0) goto fail;
        for (x = 0U; x < document->cols; x++) {
            if (x > 0U && fputc(',', file) == EOF) goto fail;
            if (fprintf(file, "%u", (unsigned)document->cells[
                    y * document->cols + x].material_id) < 0) goto fail;
        }
        if (fputc('\n', file) == EOF) goto fail;
    }
    if (fflush(file) != 0 || fsync(fd) != 0) goto fail;
    if (fclose(file) != 0) {
        unlink(temp_path);
        return SPRITE_DOCUMENT_IO_ERROR;
    }
    file = NULL;
    if (rename(temp_path, path) != 0) {
        unlink(temp_path);
        return SPRITE_DOCUMENT_IO_ERROR;
    }
    return SPRITE_DOCUMENT_OK;
fail:
    if (file) (void)fclose(file);
    unlink(temp_path);
    return SPRITE_DOCUMENT_IO_ERROR;
}

SpriteDocumentResult sprite_document_save(SpriteDocument *document,
                                          const AssetRegistry *assets,
                                          const char *sprite_directory) {
    char path[1024];
    char *owned_path = NULL;
    size_t i;
    size_t count;
    SpriteDocumentResult result;
    if (!document || !assets || !document->cells || !sprite_directory ||
        !valid_dimensions(document->cols, document->rows, &count))
        return SPRITE_DOCUMENT_INVALID_ARGUMENT;
    for (i = 0U; i < count; i++) {
        if (document->cells[i].glyph != 0U && document->cells[i].glyph != (uint8_t)' ' &&
            !material_id_is_loaded(assets, (int)document->cells[i].material_id))
            return SPRITE_DOCUMENT_INVALID_MATERIAL;
    }
    if (snprintf(path, sizeof(path), "%s/%u.txt", sprite_directory,
                 (unsigned)document->id) < 0 || strlen(path) >= sizeof(path))
        return SPRITE_DOCUMENT_IO_ERROR;
    if (!document->path) {
        owned_path = duplicate_string(path);
        if (!owned_path) return SPRITE_DOCUMENT_OUT_OF_MEMORY;
    }
    result = write_file(document, path);
    if (result != SPRITE_DOCUMENT_OK) {
        free(owned_path);
        return result;
    }
    if (owned_path) document->path = owned_path;
    document->dirty = false;
    return SPRITE_DOCUMENT_OK;
}

SpriteDocumentResult sprite_document_commit_to_registry(
    const SpriteDocument *document, AssetRegistry *assets) {
    if (!document || !assets || !document->cells ||
        !asset_registry_set_sprite(assets, document->id, (int)document->cols,
                                   (int)document->rows, document->cells))
        return SPRITE_DOCUMENT_INVALID_ARGUMENT;
    (void)asset_registry_bump_generation(assets);
    return SPRITE_DOCUMENT_OK;
}