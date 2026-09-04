#define _POSIX_C_SOURCE 200809L

#include "sprite_document.h"

#include "checked_size.h"

#include <dirent.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
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

static void select_alias(SpriteDocument *document) {
    SpriteDocumentFrame *frame;
    if (!document || !document->frames || document->frame_count == 0U) return;
    if (document->selected_frame >= document->frame_count) document->selected_frame = 0U;
    frame = &document->frames[document->selected_frame];
    document->cols = frame->cols;
    document->rows = frame->rows;
    document->cells = frame->cells;
}

void sprite_document_init(SpriteDocument *document) {
    if (document) memset(document, 0, sizeof(*document));
}

void sprite_document_destroy(SpriteDocument *document) {
    if (!document) return;
    for (size_t i = 0U; i < document->frame_count; i++)
        free(document->frames[i].cells);
    free(document->frames);
    free(document->path);
    sprite_document_init(document);
}

SpriteDocumentResult sprite_document_create(SpriteDocument *document,
                                             const AssetRegistry *assets,
                                             size_t cols, size_t rows) {
    SpriteDocumentFrame *frames;
    size_t count;
    uint16_t id;
    if (!document || !assets || !valid_dimensions(cols, rows, &count))
        return SPRITE_DOCUMENT_INVALID_DIMENSIONS;
    id = asset_registry_allocate_sprite_id(assets);
    if (id == 0U) return SPRITE_DOCUMENT_INVALID_ARGUMENT;
    frames = calloc(1U, sizeof(*frames));
    if (!frames) return SPRITE_DOCUMENT_OUT_OF_MEMORY;
    frames[0].cells = calloc(count, sizeof(*frames[0].cells));
    if (!frames[0].cells) { free(frames); return SPRITE_DOCUMENT_OUT_OF_MEMORY; }
    frames[0].cols = cols;
    frames[0].rows = rows;
    sprite_document_destroy(document);
    document->id = id;
    document->frames = frames;
    document->frame_count = 1U;
    document->frames_per_second = 4.0;
    document->loop = true;
    document->dirty = true;
    select_alias(document);
    return SPRITE_DOCUMENT_OK;
}

static bool copy_asset_frame(SpriteDocumentFrame *out, const SpriteAsset *asset) {
    size_t count;
    size_t bytes;
    if (!out || !asset ||
        !valid_dimensions((size_t)asset->cols, (size_t)asset->rows, &count) ||
        !checked_size_bytes(count, sizeof(*out->cells), &bytes) || !asset->pattern)
        return false;
    out->cells = malloc(bytes);
    if (!out->cells) return false;
    memcpy(out->cells, asset->pattern, bytes);
    out->cols = (size_t)asset->cols;
    out->rows = (size_t)asset->rows;
    return true;
}

SpriteDocumentResult sprite_document_open_loaded(SpriteDocument *document,
                                                  const AssetRegistry *assets,
                                                  uint16_t id,
                                                  const char *sprite_directory) {
    const SpriteAnimationAsset *animation;
    SpriteDocument candidate;
    char path[1024];
    int written;
    if (!document || !assets || !sprite_directory || sprite_directory[0] == '\0')
        return SPRITE_DOCUMENT_INVALID_ARGUMENT;
    sprite_document_init(&candidate);
    animation = asset_registry_get_sprite_animation(assets, (int)id);
    candidate.frame_count = animation ? animation->frame_count : 1U;
    candidate.frames = calloc(candidate.frame_count, sizeof(*candidate.frames));
    if (!candidate.frames) return SPRITE_DOCUMENT_OUT_OF_MEMORY;
    for (size_t i = 0U; i < candidate.frame_count; i++) {
        const SpriteAsset *asset = asset_registry_get_sprite_frame(assets, (int)id, i);
        if (!copy_asset_frame(&candidate.frames[i], asset)) {
            sprite_document_destroy(&candidate);
            return asset ? SPRITE_DOCUMENT_OUT_OF_MEMORY : SPRITE_DOCUMENT_INVALID_ARGUMENT;
        }
    }
    written = snprintf(path, sizeof(path), "%s/%u", sprite_directory, (unsigned)id);
    if (written < 0 || (size_t)written >= sizeof(path)) {
        sprite_document_destroy(&candidate);
        return SPRITE_DOCUMENT_IO_ERROR;
    }
    candidate.path = duplicate_string(path);
    if (!candidate.path) {
        sprite_document_destroy(&candidate);
        return SPRITE_DOCUMENT_OUT_OF_MEMORY;
    }
    candidate.id = id;
    candidate.frames_per_second = animation ? animation->frames_per_second : 4.0;
    candidate.loop = animation ? animation->loop : true;
    select_alias(&candidate);
    sprite_document_destroy(document);
    *document = candidate;
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

SpriteDocumentResult sprite_document_select_frame(SpriteDocument *document,
                                                   size_t frame_index) {
    if (!document || !document->frames || frame_index >= document->frame_count)
        return SPRITE_DOCUMENT_OUT_OF_BOUNDS;
    if (frame_index == document->selected_frame) return SPRITE_DOCUMENT_NO_CHANGE;
    document->selected_frame = frame_index;
    select_alias(document);
    return SPRITE_DOCUMENT_OK;
}

SpriteDocumentResult sprite_document_select_previous_frame(SpriteDocument *document) {
    if (!document || document->frame_count == 0U) return SPRITE_DOCUMENT_INVALID_ARGUMENT;
    return sprite_document_select_frame(document, document->selected_frame == 0U
        ? document->frame_count - 1U : document->selected_frame - 1U);
}

SpriteDocumentResult sprite_document_select_next_frame(SpriteDocument *document) {
    if (!document || document->frame_count == 0U) return SPRITE_DOCUMENT_INVALID_ARGUMENT;
    return sprite_document_select_frame(
        document, (document->selected_frame + 1U) % document->frame_count);
}

SpriteDocumentResult sprite_document_add_frame_after_selected(SpriteDocument *document) {
    SpriteDocumentFrame *grown;
    SpriteDocumentFrame added = {0};
    size_t count;
    size_t bytes;
    size_t insert;
    if (!document || !document->frames || document->frame_count == 0U)
        return SPRITE_DOCUMENT_INVALID_ARGUMENT;
    if (document->frame_count >= SPRITE_ANIMATION_MAX_FRAMES)
        return SPRITE_DOCUMENT_OUT_OF_BOUNDS;
    if (!valid_dimensions(document->cols, document->rows, &count) ||
        !checked_size_bytes(count, sizeof(*added.cells), &bytes))
        return SPRITE_DOCUMENT_INVALID_DIMENSIONS;
    added.cells = calloc(1U, bytes);
    if (!added.cells) return SPRITE_DOCUMENT_OUT_OF_MEMORY;
    added.cols = document->cols;
    added.rows = document->rows;
    grown = realloc(document->frames,
                    (document->frame_count + 1U) * sizeof(*grown));
    if (!grown) { free(added.cells); return SPRITE_DOCUMENT_OUT_OF_MEMORY; }
    document->frames = grown;
    insert = document->selected_frame + 1U;
    memmove(&grown[insert + 1U], &grown[insert],
            (document->frame_count - insert) * sizeof(*grown));
    grown[insert] = added;
    document->frame_count++;
    document->selected_frame = insert;
    document->dirty = true;
    select_alias(document);
    return SPRITE_DOCUMENT_OK;
}

SpriteDocumentResult sprite_document_remove_selected_frame(SpriteDocument *document) {
    size_t removed;
    if (!document || !document->frames || document->frame_count <= 1U)
        return SPRITE_DOCUMENT_INVALID_ARGUMENT;
    removed = document->selected_frame;
    free(document->frames[removed].cells);
    memmove(&document->frames[removed], &document->frames[removed + 1U],
            (document->frame_count - removed - 1U) * sizeof(*document->frames));
    document->frame_count--;
    document->selected_frame = removed == 0U ? document->frame_count - 1U : removed - 1U;
    document->dirty = true;
    select_alias(document);
    return SPRITE_DOCUMENT_OK;
}

SpriteDocumentResult sprite_document_set_frames_per_second(SpriteDocument *document,
                                                            double fps) {
    if (!document || !isfinite(fps) || fps < 0.1 || fps > 120.0)
        return SPRITE_DOCUMENT_INVALID_ARGUMENT;
    if (document->frames_per_second == fps) return SPRITE_DOCUMENT_NO_CHANGE;
    document->frames_per_second = fps;
    document->dirty = true;
    return SPRITE_DOCUMENT_OK;
}

SpriteDocumentResult sprite_document_set_loop(SpriteDocument *document, bool loop) {
    if (!document) return SPRITE_DOCUMENT_INVALID_ARGUMENT;
    if (document->loop == loop) return SPRITE_DOCUMENT_NO_CHANGE;
    document->loop = loop;
    document->dirty = true;
    return SPRITE_DOCUMENT_OK;
}

const SpriteDocumentFrame *sprite_document_previous_frame(const SpriteDocument *document) {
    if (!document || document->frame_count < 2U) return NULL;
    return &document->frames[document->selected_frame == 0U
        ? document->frame_count - 1U : document->selected_frame - 1U];
}

const SpriteDocumentFrame *sprite_document_next_frame(const SpriteDocument *document) {
    if (!document || document->frame_count < 2U) return NULL;
    return &document->frames[(document->selected_frame + 1U) % document->frame_count];
}

static SpriteDocumentResult write_pattern_file(const SpriteDocumentFrame *frame,
                                                const char *path) {
    FILE *file = fopen(path, "wb");
    int fd;
    if (!file) return SPRITE_DOCUMENT_IO_ERROR;
    if (fprintf(file, "cols=%zu\nrows=%zu\ndefault_material=1\n",
                frame->cols, frame->rows) < 0) goto fail;
    for (size_t y = 0U; y < frame->rows; y++) {
        if (fprintf(file, "pattern_%zu=", y) < 0) goto fail;
        for (size_t x = 0U; x < frame->cols; x++) {
            uint8_t glyph = frame->cells[y * frame->cols + x].glyph;
            if (fputc(glyph == 0U ? ' ' : (int)glyph, file) == EOF) goto fail;
        }
        if (fputc('\n', file) == EOF || fprintf(file, "material_%zu=", y) < 0) goto fail;
        for (size_t x = 0U; x < frame->cols; x++) {
            if (x > 0U && fputc(',', file) == EOF) goto fail;
            if (fprintf(file, "%u", (unsigned)frame->cells[
                    y * frame->cols + x].material_id) < 0) goto fail;
        }
        if (fputc('\n', file) == EOF) goto fail;
    }
    if (fflush(file) != 0) goto fail;
    fd = fileno(file);
    if (fd < 0 || fsync(fd) != 0 || fclose(file) != 0) return SPRITE_DOCUMENT_IO_ERROR;
    return SPRITE_DOCUMENT_OK;
fail:
    (void)fclose(file);
    return SPRITE_DOCUMENT_IO_ERROR;
}

static bool remove_folder(const char *path) {
    DIR *directory = opendir(path);
    struct dirent *entry;
    bool ok = true;
    if (!directory) return false;
    while ((entry = readdir(directory)) != NULL) {
        char child[1200];
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        if (snprintf(child, sizeof(child), "%s/%s", path, entry->d_name) < 0 ||
            strlen(child) >= sizeof(child) || unlink(child) != 0) ok = false;
    }
    if (closedir(directory) != 0) ok = false;
    return rmdir(path) == 0 && ok;
}

static SpriteDocumentResult write_candidate_folder(const SpriteDocument *document,
                                                    const char *directory) {
    char path[1200];
    FILE *manifest;
    int fd;
    if (snprintf(path, sizeof(path), "%s/animation.txt", directory) < 0 ||
        strlen(path) >= sizeof(path)) return SPRITE_DOCUMENT_IO_ERROR;
    manifest = fopen(path, "wb");
    if (!manifest) return SPRITE_DOCUMENT_IO_ERROR;
    if (document->frame_count == 1U) {
        if (fputs("static\n", manifest) < 0) goto manifest_fail;
    } else {
        if (fprintf(manifest, "fps=%.17g\nloop=%s\n",
                    document->frames_per_second, document->loop ? "true" : "false") < 0)
            goto manifest_fail;
        for (size_t i = 0U; i < document->frame_count; i++)
            if (fprintf(manifest, "frame=frame_%03zu.txt\n", i) < 0)
                goto manifest_fail;
    }
    if (fflush(manifest) != 0 || (fd = fileno(manifest)) < 0 ||
        fsync(fd) != 0 || fclose(manifest) != 0) return SPRITE_DOCUMENT_IO_ERROR;
    for (size_t i = 0U; i < document->frame_count; i++) {
        if (snprintf(path, sizeof(path), "%s/frame_%03zu.txt", directory, i) < 0 ||
            strlen(path) >= sizeof(path) ||
            write_pattern_file(&document->frames[i], path) != SPRITE_DOCUMENT_OK)
            return SPRITE_DOCUMENT_IO_ERROR;
    }
    return SPRITE_DOCUMENT_OK;
manifest_fail:
    (void)fclose(manifest);
    return SPRITE_DOCUMENT_IO_ERROR;
}

SpriteDocumentResult sprite_document_save(SpriteDocument *document,
                                           const AssetRegistry *assets,
                                           const char *sprite_directory) {
    char target[1024];
    char temporary[1060];
    char backup[1060];
    char *owned_path = NULL;
    struct stat info;
    bool had_target;
    int written;
    if (!document || !assets || !document->frames || document->frame_count == 0U ||
        document->frame_count > SPRITE_ANIMATION_MAX_FRAMES || !sprite_directory ||
        (document->frame_count > 1U && (!isfinite(document->frames_per_second) ||
         document->frames_per_second < 0.1 || document->frames_per_second > 120.0)))
        return SPRITE_DOCUMENT_INVALID_ARGUMENT;
    for (size_t frame = 0U; frame < document->frame_count; frame++) {
        size_t count;
        if (!valid_dimensions(document->frames[frame].cols,
                              document->frames[frame].rows, &count))
            return SPRITE_DOCUMENT_INVALID_DIMENSIONS;
        for (size_t i = 0U; i < count; i++) {
            PatternCell cell = document->frames[frame].cells[i];
            if (cell.glyph != 0U && cell.glyph != (uint8_t)' ' &&
                !material_id_is_loaded(assets, (int)cell.material_id))
                return SPRITE_DOCUMENT_INVALID_MATERIAL;
        }
    }
    written = snprintf(target, sizeof(target), "%s/%u", sprite_directory,
                       (unsigned)document->id);
    if (written < 0 || (size_t)written >= sizeof(target)) return SPRITE_DOCUMENT_IO_ERROR;
    written = snprintf(temporary, sizeof(temporary), "%s.tmpXXXXXX", target);
    if (written < 0 || (size_t)written >= sizeof(temporary) || !mkdtemp(temporary))
        return SPRITE_DOCUMENT_IO_ERROR;
    if (write_candidate_folder(document, temporary) != SPRITE_DOCUMENT_OK) {
        (void)remove_folder(temporary);
        return SPRITE_DOCUMENT_IO_ERROR;
    }
    written = snprintf(backup, sizeof(backup), "%s.backup", target);
    if (written < 0 || (size_t)written >= sizeof(backup)) {
        (void)remove_folder(temporary);
        return SPRITE_DOCUMENT_IO_ERROR;
    }
    had_target = stat(target, &info) == 0;
    if (had_target && (!S_ISDIR(info.st_mode) || stat(backup, &info) == 0 ||
                       rename(target, backup) != 0)) {
        (void)remove_folder(temporary);
        return SPRITE_DOCUMENT_IO_ERROR;
    }
    if (rename(temporary, target) != 0) {
        if (had_target) (void)rename(backup, target);
        (void)remove_folder(temporary);
        return SPRITE_DOCUMENT_IO_ERROR;
    }
    if (!document->path) {
        owned_path = duplicate_string(target);
        if (!owned_path) {
            (void)remove_folder(target);
            if (had_target) (void)rename(backup, target);
            return SPRITE_DOCUMENT_OUT_OF_MEMORY;
        }
    }
    if (had_target && !remove_folder(backup)) {
        free(owned_path);
        return SPRITE_DOCUMENT_IO_ERROR;
    }
    if (owned_path) document->path = owned_path;
    document->dirty = false;
    return SPRITE_DOCUMENT_OK;
}

SpriteDocumentResult sprite_document_delete_saved(const SpriteDocument *document) {
    if (!document || !document->path) return SPRITE_DOCUMENT_INVALID_ARGUMENT;
    return remove_folder(document->path) ? SPRITE_DOCUMENT_OK : SPRITE_DOCUMENT_IO_ERROR;
}

SpriteDocumentResult sprite_document_commit_to_registry(
    const SpriteDocument *document, AssetRegistry *assets) {
    bool ok;
    if (!document || !assets || !document->frames || document->frame_count == 0U)
        return SPRITE_DOCUMENT_INVALID_ARGUMENT;
    if (document->frame_count == 1U) {
        ok = asset_registry_set_sprite(
            assets, document->id, (int)document->frames[0].cols,
            (int)document->frames[0].rows, document->frames[0].cells);
    } else {
        SpriteAsset frames[SPRITE_ANIMATION_MAX_FRAMES];
        for (size_t i = 0U; i < document->frame_count; i++) {
            frames[i].cols = (int)document->frames[i].cols;
            frames[i].rows = (int)document->frames[i].rows;
            frames[i].pattern = document->frames[i].cells;
        }
        ok = asset_registry_set_sprite_animation(
            assets, document->id, frames, document->frame_count,
            document->frames_per_second, document->loop);
    }
    if (!ok) return SPRITE_DOCUMENT_INVALID_ARGUMENT;
    (void)asset_registry_bump_generation(assets);
    return SPRITE_DOCUMENT_OK;
}