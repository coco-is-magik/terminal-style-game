/** sprite_document.h — Owned staged static/animated sprite-folder document. */
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
    size_t cols;
    size_t rows;
    PatternCell *cells;
} SpriteDocumentFrame;

typedef struct {
    uint16_t id;
    SpriteDocumentFrame *frames;
    size_t frame_count;
    size_t selected_frame;
    double frames_per_second;
    bool loop;
    /* Borrowed aliases for the selected frame, retained for painter simplicity. */
    size_t cols;
    size_t rows;
    PatternCell *cells;
    char *path; /* Owned numeric sprite-folder path after open/save. */
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
SpriteDocumentResult sprite_document_select_frame(SpriteDocument *document,
                                                  size_t frame_index);
SpriteDocumentResult sprite_document_select_previous_frame(SpriteDocument *document);
SpriteDocumentResult sprite_document_select_next_frame(SpriteDocument *document);
SpriteDocumentResult sprite_document_add_frame_after_selected(SpriteDocument *document);
SpriteDocumentResult sprite_document_remove_selected_frame(SpriteDocument *document);
SpriteDocumentResult sprite_document_set_frames_per_second(SpriteDocument *document,
                                                           double fps);
SpriteDocumentResult sprite_document_set_loop(SpriteDocument *document, bool loop);
const SpriteDocumentFrame *sprite_document_previous_frame(const SpriteDocument *document);
const SpriteDocumentFrame *sprite_document_next_frame(const SpriteDocument *document);
SpriteDocumentResult sprite_document_save(SpriteDocument *document,
                                          const AssetRegistry *assets,
                                          const char *sprite_directory);
SpriteDocumentResult sprite_document_delete_saved(const SpriteDocument *document);
SpriteDocumentResult sprite_document_commit_to_registry(
    const SpriteDocument *document, AssetRegistry *assets);

#endif