#define _POSIX_C_SOURCE 200809L

#include "decal_document.h"

#include "decal_io.h"
#include "decal_painter.h"

#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

static bool valid_dimensions(size_t cols, size_t rows, size_t *out_count) {
    if (cols == 0U || rows == 0U || cols > DECAL_PATTERN_MAX_COLS ||
        rows > DECAL_PATTERN_MAX_ROWS || cols > SIZE_MAX / rows) return false;
    if (out_count) *out_count = cols * rows;
    return true;
}

static void value_clear(DecalDocumentValue *value) {
    if (!value) return;
    free(value->cells);
    memset(value, 0, sizeof(*value));
}

static bool value_copy(DecalDocumentValue *out,
                       const DecalDocumentValue *source) {
    size_t count;
    PatternCell *cells;
    if (!out || !source || !source->cells ||
        !valid_dimensions(source->cols, source->rows, &count)) return false;
    cells = malloc(count * sizeof(*cells));
    if (!cells) return false;
    memcpy(cells, source->cells, count * sizeof(*cells));
    value_clear(out);
    *out = *source;
    out->cells = cells;
    return true;
}

static bool values_equal(const DecalDocumentValue *a,
                         const DecalDocumentValue *b) {
    size_t count;
    if (!a || !b || a->id != b->id || a->cols != b->cols ||
        a->rows != b->rows || !a->cells || !b->cells ||
        !valid_dimensions(a->cols, a->rows, &count)) return false;
    return memcmp(a->cells, b->cells, count * sizeof(*a->cells)) == 0;
}

static void change_clear(DecalDocumentChange *change) {
    if (!change) return;
    value_clear(&change->before);
    value_clear(&change->after);
    memset(change, 0, sizeof(*change));
}

static void truncate_changes(DecalDocument *document, size_t from) {
    size_t i;
    for (i = from; i < document->change_count; i++) {
        change_clear(&document->changes[i]);
    }
    document->change_count = from;
}

static DecalDocumentResult push_change(DecalDocument *document,
                                       const DecalDocumentValue *value) {
    DecalDocumentChange *grown;
    DecalDocumentChange change;
    DecalDocumentValue replacement;
    size_t capacity;
    if (!document || !value) return DECAL_DOCUMENT_INVALID_ARGUMENT;
    if (values_equal(&document->value, value)) return DECAL_DOCUMENT_NO_CHANGE;
    if (document->state.next_state == 0U ||
        document->state.next_state == UINT64_MAX) {
        return DECAL_DOCUMENT_STATE_ID_EXHAUSTED;
    }
    memset(&change, 0, sizeof(change));
    memset(&replacement, 0, sizeof(replacement));
    if (!value_copy(&change.before, &document->value) ||
        !value_copy(&change.after, value) ||
        !value_copy(&replacement, value)) {
        change_clear(&change);
        value_clear(&replacement);
        return DECAL_DOCUMENT_OUT_OF_MEMORY;
    }
    if (document->change_cursor == document->change_capacity) {
        capacity = document->change_capacity == 0U ? 8U :
                   document->change_capacity * 2U;
        grown = realloc(document->changes, capacity * sizeof(*grown));
        if (!grown) {
            change_clear(&change);
            value_clear(&replacement);
            return DECAL_DOCUMENT_OUT_OF_MEMORY;
        }
        memset(grown + document->change_capacity, 0,
               (capacity - document->change_capacity) * sizeof(*grown));
        document->changes = grown;
        document->change_capacity = capacity;
    }
    truncate_changes(document, document->change_cursor);
    change.before_state = document->state.current_state;
    if (!asset_document_state_advance(&document->state)) {
        change_clear(&change);
        value_clear(&replacement);
        return DECAL_DOCUMENT_STATE_ID_EXHAUSTED;
    }
    change.after_state = document->state.current_state;
    document->changes[document->change_cursor] = change;
    value_clear(&document->value);
    document->value = replacement;
    document->change_cursor++;
    document->change_count = document->change_cursor;
    return DECAL_DOCUMENT_OK;
}

void decal_document_init(DecalDocument *document) {
    if (!document) return;
    memset(document, 0, sizeof(*document));
    asset_document_state_init(&document->state);
}

void decal_document_destroy(DecalDocument *document) {
    size_t i;
    if (!document) return;
    value_clear(&document->value);
    value_clear(&document->saved_value);
    for (i = 0U; i < document->change_count; i++) {
        change_clear(&document->changes[i]);
    }
    free(document->changes);
    free(document->path);
    decal_document_init(document);
}

DecalDocumentResult decal_document_create(DecalDocument *document,
                                          const AssetRegistry *assets,
                                          size_t cols, size_t rows) {
    DecalDocumentValue value;
    size_t count;
    if (!document || !assets ||
        !valid_dimensions(cols, rows, &count)) {
        return DECAL_DOCUMENT_INVALID_DIMENSIONS;
    }
    memset(&value, 0, sizeof(value));
    value.id = asset_registry_allocate_decal_pattern_id(assets);
    if (value.id == 0U) return DECAL_DOCUMENT_INVALID_ARGUMENT;
    value.cols = cols;
    value.rows = rows;
    value.cells = calloc(count, sizeof(*value.cells));
    if (!value.cells) return DECAL_DOCUMENT_OUT_OF_MEMORY;
    decal_document_destroy(document);
    document->value = value;
    if (!value_copy(&document->saved_value, &value)) {
        decal_document_destroy(document);
        return DECAL_DOCUMENT_OUT_OF_MEMORY;
    }
    document->state.saved_state = 0U;
    return DECAL_DOCUMENT_OK;
}

DecalDocumentResult decal_document_open(DecalDocument *document,
                                        const AssetRegistry *assets,
                                        uint16_t id,
                                        const char *path) {
    Decal *decal;
    DecalDocumentValue value;
    char *owned_path;
    DecalDocumentResult validation;
    if (!document || !assets || id == 0U || !path || path[0] == '\0') {
        return DECAL_DOCUMENT_INVALID_ARGUMENT;
    }
    decal = decal_load_from_file(path);
    if (!decal || !valid_dimensions((size_t)decal->pattern_cols,
                                    (size_t)decal->pattern_rows, NULL)) {
        decal_free(decal);
        return DECAL_DOCUMENT_IO_ERROR;
    }
    owned_path = duplicate_string(path);
    if (!owned_path) {
        decal_free(decal);
        return DECAL_DOCUMENT_OUT_OF_MEMORY;
    }
    memset(&value, 0, sizeof(value));
    value.id = id;
    value.cols = (size_t)decal->pattern_cols;
    value.rows = (size_t)decal->pattern_rows;
    value.cells = decal->pattern;
    decal_document_destroy(document);
    if (!value_copy(&document->value, &value) ||
        !value_copy(&document->saved_value, &value)) {
        free(owned_path);
        decal_free(decal);
        decal_document_destroy(document);
        return DECAL_DOCUMENT_OUT_OF_MEMORY;
    }
    decal_free(decal);
    document->path = owned_path;
    validation = decal_document_validate(document, assets);
    if (validation != DECAL_DOCUMENT_OK) {
        decal_document_destroy(document);
        return validation;
    }
    return DECAL_DOCUMENT_OK;
}

DecalDocumentResult decal_document_validate(const DecalDocument *document,
                                            const AssetRegistry *assets) {
    size_t count;
    size_t i;
    if (!document || !assets || document->value.id == 0U ||
        !document->value.cells) return DECAL_DOCUMENT_INVALID_ARGUMENT;
    if (!valid_dimensions(document->value.cols, document->value.rows, &count)) {
        return DECAL_DOCUMENT_INVALID_DIMENSIONS;
    }
    for (i = 0U; i < count; i++) {
        const PatternCell cell = document->value.cells[i];
        bool visible = cell.glyph != 0U && cell.glyph != (uint8_t)' ';
        if ((cell.material_id != 0U &&
             !material_id_is_loaded(assets, cell.material_id)) ||
            (visible && cell.material_id == 0U)) {
            return DECAL_DOCUMENT_INVALID_MATERIAL;
        }
    }
    return DECAL_DOCUMENT_OK;
}

static DecalDocumentResult painter_result(DecalPainterResult result) {
    switch (result) {
        case DECAL_PAINTER_OK: return DECAL_DOCUMENT_OK;
        case DECAL_PAINTER_NO_CHANGE: return DECAL_DOCUMENT_NO_CHANGE;
        case DECAL_PAINTER_OUT_OF_BOUNDS: return DECAL_DOCUMENT_OUT_OF_BOUNDS;
        case DECAL_PAINTER_OUT_OF_MEMORY: return DECAL_DOCUMENT_OUT_OF_MEMORY;
        case DECAL_PAINTER_INVALID_DIMENSIONS:
        case DECAL_PAINTER_SIZE_OVERFLOW: return DECAL_DOCUMENT_INVALID_DIMENSIONS;
        default: return DECAL_DOCUMENT_INVALID_ARGUMENT;
    }
}

typedef DecalPainterResult (*PainterMutation)(DecalPainter *, PatternCell);

static DecalDocumentResult mutate_all(DecalDocument *document,
                                      PatternCell cell,
                                      PainterMutation mutation) {
    DecalDocumentValue candidate;
    DecalPainter painter;
    DecalPainterResult result;
    memset(&candidate, 0, sizeof(candidate));
    if (!document || !mutation || !value_copy(&candidate, &document->value)) {
        return DECAL_DOCUMENT_OUT_OF_MEMORY;
    }
    decal_painter_init(&painter);
    result = decal_painter_attach(&painter, candidate.cells,
                                  candidate.cols, candidate.rows);
    if (result == DECAL_PAINTER_OK) result = mutation(&painter, cell);
    decal_painter_destroy(&painter);
    if (result == DECAL_PAINTER_OK) {
        DecalDocumentResult pushed = push_change(document, &candidate);
        value_clear(&candidate);
        return pushed;
    }
    value_clear(&candidate);
    return painter_result(result);
}

DecalDocumentResult decal_document_paint_cell(DecalDocument *document,
                                              const AssetRegistry *assets,
                                              size_t x, size_t y,
                                              PatternCell cell) {
    DecalDocumentValue candidate;
    DecalPainter painter;
    DecalPainterResult result;
    if (!document || !assets) return DECAL_DOCUMENT_INVALID_ARGUMENT;
    if ((cell.material_id != 0U &&
         !material_id_is_loaded(assets, cell.material_id)) ||
        (cell.glyph != 0U && cell.glyph != (uint8_t)' ' &&
         cell.material_id == 0U)) {
        return DECAL_DOCUMENT_INVALID_MATERIAL;
    }
    memset(&candidate, 0, sizeof(candidate));
    if (!value_copy(&candidate, &document->value)) {
        return DECAL_DOCUMENT_OUT_OF_MEMORY;
    }
    decal_painter_init(&painter);
    result = decal_painter_attach(&painter, candidate.cells,
                                  candidate.cols, candidate.rows);
    if (result == DECAL_PAINTER_OK) {
        result = decal_painter_paint_cell(&painter, x, y, cell);
    }
    decal_painter_destroy(&painter);
    if (result == DECAL_PAINTER_OK) {
        DecalDocumentResult pushed = push_change(document, &candidate);
        value_clear(&candidate);
        return pushed;
    }
    value_clear(&candidate);
    return painter_result(result);
}

static DecalPainterResult erase_adapter(DecalPainter *painter,
                                        PatternCell ignored) {
    (void)ignored;
    return decal_painter_clear(painter);
}

DecalDocumentResult decal_document_erase_cell(DecalDocument *document,
                                              size_t x, size_t y) {
    const PatternCell empty = {0U, 0U};
    DecalDocumentValue candidate;
    DecalPainter painter;
    DecalPainterResult result;
    memset(&candidate, 0, sizeof(candidate));
    if (!document || !value_copy(&candidate, &document->value)) {
        return DECAL_DOCUMENT_INVALID_ARGUMENT;
    }
    decal_painter_init(&painter);
    result = decal_painter_attach(&painter, candidate.cells,
                                  candidate.cols, candidate.rows);
    if (result == DECAL_PAINTER_OK) {
        result = decal_painter_erase_cell(&painter, x, y);
    }
    decal_painter_destroy(&painter);
    if (result == DECAL_PAINTER_OK) {
        DecalDocumentResult pushed = push_change(document, &candidate);
        value_clear(&candidate);
        return pushed;
    }
    value_clear(&candidate);
    (void)empty;
    return painter_result(result);
}

DecalDocumentResult decal_document_fill(DecalDocument *document,
                                        const AssetRegistry *assets,
                                        PatternCell cell) {
    if (!document || !assets) return DECAL_DOCUMENT_INVALID_ARGUMENT;
    if ((cell.material_id != 0U &&
         !material_id_is_loaded(assets, cell.material_id)) ||
        (cell.glyph != 0U && cell.glyph != (uint8_t)' ' &&
         cell.material_id == 0U)) {
        return DECAL_DOCUMENT_INVALID_MATERIAL;
    }
    return mutate_all(document, cell, decal_painter_fill);
}

DecalDocumentResult decal_document_clear(DecalDocument *document) {
    const PatternCell empty = {0U, 0U};
    return mutate_all(document, empty, erase_adapter);
}

DecalDocumentResult decal_document_resize(DecalDocument *document,
                                          size_t cols, size_t rows) {
    DecalDocumentValue candidate;
    size_t count;
    size_t copy_cols;
    size_t copy_rows;
    size_t y;
    if (!document || !valid_dimensions(cols, rows, &count)) {
        return DECAL_DOCUMENT_INVALID_DIMENSIONS;
    }
    if (document->value.cols == cols && document->value.rows == rows) {
        return DECAL_DOCUMENT_NO_CHANGE;
    }
    memset(&candidate, 0, sizeof(candidate));
    candidate.id = document->value.id;
    candidate.cols = cols;
    candidate.rows = rows;
    candidate.cells = calloc(count, sizeof(*candidate.cells));
    if (!candidate.cells) return DECAL_DOCUMENT_OUT_OF_MEMORY;
    copy_cols = cols < document->value.cols ? cols : document->value.cols;
    copy_rows = rows < document->value.rows ? rows : document->value.rows;
    for (y = 0U; y < copy_rows; y++) {
        memcpy(candidate.cells + y * cols,
               document->value.cells + y * document->value.cols,
               copy_cols * sizeof(*candidate.cells));
    }
    DecalDocumentResult result = push_change(document, &candidate);
    value_clear(&candidate);
    return result;
}

DecalDocumentResult decal_document_undo(DecalDocument *document) {
    DecalDocumentChange *change;
    DecalDocumentValue replacement;
    if (!document) return DECAL_DOCUMENT_INVALID_ARGUMENT;
    if (document->change_cursor == 0U) return DECAL_DOCUMENT_NOTHING_TO_UNDO;
    change = &document->changes[document->change_cursor - 1U];
    memset(&replacement, 0, sizeof(replacement));
    if (!value_copy(&replacement, &change->before)) {
        return DECAL_DOCUMENT_OUT_OF_MEMORY;
    }
    value_clear(&document->value);
    document->value = replacement;
    document->change_cursor--;
    asset_document_state_restore(&document->state, change->before_state);
    return DECAL_DOCUMENT_OK;
}

DecalDocumentResult decal_document_redo(DecalDocument *document) {
    DecalDocumentChange *change;
    DecalDocumentValue replacement;
    if (!document) return DECAL_DOCUMENT_INVALID_ARGUMENT;
    if (document->change_cursor >= document->change_count) {
        return DECAL_DOCUMENT_NOTHING_TO_REDO;
    }
    change = &document->changes[document->change_cursor];
    memset(&replacement, 0, sizeof(replacement));
    if (!value_copy(&replacement, &change->after)) {
        return DECAL_DOCUMENT_OUT_OF_MEMORY;
    }
    value_clear(&document->value);
    document->value = replacement;
    document->change_cursor++;
    asset_document_state_restore(&document->state, change->after_state);
    return DECAL_DOCUMENT_OK;
}

void decal_document_discard(DecalDocument *document) {
    DecalDocumentValue replacement;
    if (!document || !document->saved_value.cells) return;
    memset(&replacement, 0, sizeof(replacement));
    if (!value_copy(&replacement, &document->saved_value)) return;
    value_clear(&document->value);
    document->value = replacement;
    document->change_cursor = 0U;
    truncate_changes(document, 0U);
    asset_document_state_restore(&document->state, document->state.saved_state);
}

bool decal_document_is_dirty(const DecalDocument *document) {
    return document && asset_document_state_is_dirty(&document->state);
}

DecalPatternAsset decal_document_preview(const DecalDocument *document) {
    DecalPatternAsset preview;
    memset(&preview, 0, sizeof(preview));
    if (!document || document->value.cols > (size_t)INT_MAX ||
        document->value.rows > (size_t)INT_MAX) return preview;
    preview.cols = (int)document->value.cols;
    preview.rows = (int)document->value.rows;
    preview.pattern = document->value.cells;
    return preview;
}

static DecalDocumentResult atomic_write(const DecalDocument *document,
                                        const char *path) {
    char temp_path[1200];
    Decal decal;
    int fd;
    if (snprintf(temp_path, sizeof(temp_path), "%s.tmp.XXXXXX", path) >=
        (int)sizeof(temp_path)) return DECAL_DOCUMENT_IO_ERROR;
    fd = mkstemp(temp_path);
    if (fd < 0) return DECAL_DOCUMENT_IO_ERROR;
    if (close(fd) != 0) {
        unlink(temp_path);
        return DECAL_DOCUMENT_IO_ERROR;
    }
    memset(&decal, 0, sizeof(decal));
    decal.width = 1.0;
    decal.height = 1.0;
    decal.pattern_cols = (int)document->value.cols;
    decal.pattern_rows = (int)document->value.rows;
    decal.pattern = document->value.cells;
    if (decal_save_to_file(temp_path, &decal) != 0) {
        unlink(temp_path);
        return DECAL_DOCUMENT_IO_ERROR;
    }
    fd = open(temp_path, O_RDONLY);
    if (fd < 0) {
        unlink(temp_path);
        return DECAL_DOCUMENT_IO_ERROR;
    }
    bool sync_failed = fsync(fd) != 0;
    if (close(fd) != 0) sync_failed = true;
    if (sync_failed) {
        unlink(temp_path);
        return DECAL_DOCUMENT_IO_ERROR;
    }
    if (rename(temp_path, path) != 0) {
        unlink(temp_path);
        return DECAL_DOCUMENT_IO_ERROR;
    }
    return DECAL_DOCUMENT_OK;
}

DecalDocumentResult decal_document_save(DecalDocument *document,
                                        const AssetRegistry *assets) {
    DecalDocumentValue saved;
    DecalDocumentResult result;
    if (!document) return DECAL_DOCUMENT_INVALID_ARGUMENT;
    if (!document->path) return DECAL_DOCUMENT_NO_PATH;
    result = decal_document_validate(document, assets);
    if (result != DECAL_DOCUMENT_OK) return result;
    memset(&saved, 0, sizeof(saved));
    if (!value_copy(&saved, &document->value)) {
        return DECAL_DOCUMENT_OUT_OF_MEMORY;
    }
    result = atomic_write(document, document->path);
    if (result == DECAL_DOCUMENT_OK) {
        value_clear(&document->saved_value);
        document->saved_value = saved;
        asset_document_state_mark_saved(&document->state);
    } else {
        value_clear(&saved);
    }
    return result;
}

DecalDocumentResult decal_document_save_as(DecalDocument *document,
                                           const AssetRegistry *assets,
                                           const char *decal_directory) {
    char path[1024];
    char *owned;
    DecalDocumentValue saved;
    DecalDocumentResult result;
    if (!document || !decal_directory) return DECAL_DOCUMENT_INVALID_ARGUMENT;
    result = decal_document_validate(document, assets);
    if (result != DECAL_DOCUMENT_OK) return result;
    if (snprintf(path, sizeof(path), "%s/%u.txt", decal_directory,
                 (unsigned)document->value.id) >= (int)sizeof(path)) {
        return DECAL_DOCUMENT_IO_ERROR;
    }
    owned = duplicate_string(path);
    if (!owned) return DECAL_DOCUMENT_OUT_OF_MEMORY;
    memset(&saved, 0, sizeof(saved));
    if (!value_copy(&saved, &document->value)) {
        free(owned);
        return DECAL_DOCUMENT_OUT_OF_MEMORY;
    }
    result = atomic_write(document, path);
    if (result != DECAL_DOCUMENT_OK) {
        value_clear(&saved);
        free(owned);
        return result;
    }
    free(document->path);
    document->path = owned;
    value_clear(&document->saved_value);
    document->saved_value = saved;
    asset_document_state_mark_saved(&document->state);
    return DECAL_DOCUMENT_OK;
}

DecalDocumentResult decal_document_commit_to_registry(
    const DecalDocument *document, AssetRegistry *assets) {
    if (!document || !assets || !document->path ||
        !asset_registry_set_decal_pattern(
            assets, document->value.id, (int)document->value.cols,
            (int)document->value.rows, document->value.cells)) {
        return DECAL_DOCUMENT_INVALID_ARGUMENT;
    }
    (void)asset_registry_bump_generation(assets);
    return DECAL_DOCUMENT_OK;
}