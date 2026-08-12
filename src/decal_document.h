/** decal_document.h — Owned, undoable reusable decal-pattern document. */
#ifndef DECAL_DOCUMENT_H
#define DECAL_DOCUMENT_H

#include "asset_document.h"
#include "assets.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint16_t id;
    size_t cols;
    size_t rows;
    PatternCell *cells;
} DecalDocumentValue;

typedef struct {
    DecalDocumentValue before;
    DecalDocumentValue after;
    DocumentStateId before_state;
    DocumentStateId after_state;
} DecalDocumentChange;

typedef struct {
    DecalDocumentValue value;
    DecalDocumentValue saved_value;
    DecalDocumentChange *changes;
    size_t change_count;
    size_t change_cursor;
    size_t change_capacity;
    AssetDocumentState state;
    char *path;
} DecalDocument;

typedef enum {
    DECAL_DOCUMENT_OK = 0,
    DECAL_DOCUMENT_NO_CHANGE,
    DECAL_DOCUMENT_NOTHING_TO_UNDO,
    DECAL_DOCUMENT_NOTHING_TO_REDO,
    DECAL_DOCUMENT_INVALID_ARGUMENT,
    DECAL_DOCUMENT_INVALID_DIMENSIONS,
    DECAL_DOCUMENT_INVALID_MATERIAL,
    DECAL_DOCUMENT_OUT_OF_BOUNDS,
    DECAL_DOCUMENT_OUT_OF_MEMORY,
    DECAL_DOCUMENT_STATE_ID_EXHAUSTED,
    DECAL_DOCUMENT_NO_PATH,
    DECAL_DOCUMENT_IO_ERROR
} DecalDocumentResult;

void decal_document_init(DecalDocument *document);
void decal_document_destroy(DecalDocument *document);
DecalDocumentResult decal_document_create(DecalDocument *document,
                                          const AssetRegistry *assets,
                                          size_t cols, size_t rows);
DecalDocumentResult decal_document_open(DecalDocument *document,
                                        const AssetRegistry *assets,
                                        uint16_t id,
                                        const char *path);
DecalDocumentResult decal_document_validate(const DecalDocument *document,
                                            const AssetRegistry *assets);
DecalDocumentResult decal_document_paint_cell(DecalDocument *document,
                                              const AssetRegistry *assets,
                                              size_t x, size_t y,
                                              PatternCell cell);
DecalDocumentResult decal_document_erase_cell(DecalDocument *document,
                                              size_t x, size_t y);
DecalDocumentResult decal_document_fill(DecalDocument *document,
                                        const AssetRegistry *assets,
                                        PatternCell cell);
DecalDocumentResult decal_document_clear(DecalDocument *document);
DecalDocumentResult decal_document_resize(DecalDocument *document,
                                          size_t cols, size_t rows);
DecalDocumentResult decal_document_undo(DecalDocument *document);
DecalDocumentResult decal_document_redo(DecalDocument *document);
void decal_document_discard(DecalDocument *document);
bool decal_document_is_dirty(const DecalDocument *document);
DecalPatternAsset decal_document_preview(const DecalDocument *document);
DecalDocumentResult decal_document_save(DecalDocument *document,
                                        const AssetRegistry *assets);
DecalDocumentResult decal_document_save_as(DecalDocument *document,
                                           const AssetRegistry *assets,
                                           const char *decal_directory);
DecalDocumentResult decal_document_commit_to_registry(
    const DecalDocument *document, AssetRegistry *assets);

#endif