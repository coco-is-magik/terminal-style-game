/** sprite_document.h — Owned editable sprite-pattern file document. */
#ifndef SPRITE_DOCUMENT_H
#define SPRITE_DOCUMENT_H

#include "assets.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    SPRITE_DOCUMENT_OK = 0,
    SPRITE_DOCUMENT_NO_CHANGE,
    SPRITE_DOCUMENT_INVALID_ARGUMENT,
    SPRITE_DOCUMENT_INVALID_DIMENSIONS,
    SPRITE_DOCUMENT_INVALID_MATERIAL,
    SPRITE_DOCUMENT_OUT_OF_BOUNDS,
    SPRITE_DOCUMENT_OUT_OF_MEMORY,
    SPRITE_DOCUMENT_IO_ERROR
} SpriteDocumentResult;

typedef struct {
    uint16_t id;
    size_t cols;
    size_t rows;
    PatternCell *cells;
    char *path;
    bool dirty;
} SpriteDocument;

void sprite_document_init(SpriteDocument *document);
void sprite_document_destroy(SpriteDocument *document);
SpriteDocumentResult sprite_document_create(SpriteDocument *document,
                                            const AssetRegistry *assets,
                                            size_t cols, size_t rows);
SpriteDocumentResult sprite_document_open_loaded(SpriteDocument *document,
                                                 const AssetRegistry *assets,
                                                 uint16_t id,
                                                 const char *sprite_directory);
SpriteDocumentResult sprite_document_paint_cell(SpriteDocument *document,
                                                const AssetRegistry *assets,
                                                size_t x, size_t y,
                                                PatternCell cell);
SpriteDocumentResult sprite_document_erase_cell(SpriteDocument *document,
                                                size_t x, size_t y);
SpriteDocumentResult sprite_document_save(SpriteDocument *document,
                                          const AssetRegistry *assets,
                                          const char *sprite_directory);
SpriteDocumentResult sprite_document_commit_to_registry(
    const SpriteDocument *document, AssetRegistry *assets);

#endif