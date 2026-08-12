/** material_document.h — Owned, undoable material asset document. */
#ifndef MATERIAL_DOCUMENT_H
#define MATERIAL_DOCUMENT_H

#include "asset_document.h"
#include "assets.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    char name[MATERIAL_NAME_CAPACITY];
    uint16_t id;
    uint16_t palette_id;
    uint8_t glyphs[4];
} MaterialDocumentValue;

typedef struct {
    MaterialDocumentValue before;
    MaterialDocumentValue after;
    DocumentStateId before_state;
    DocumentStateId after_state;
} MaterialDocumentChange;

typedef struct {
    MaterialDocumentValue value;
    MaterialDocumentValue saved_value;
    MaterialDocumentChange *changes;
    size_t change_count;
    size_t change_cursor;
    size_t change_capacity;
    AssetDocumentState state;
    char *path;
} MaterialDocument;

typedef enum {
    MATERIAL_DOCUMENT_OK = 0,
    MATERIAL_DOCUMENT_NO_CHANGE,
    MATERIAL_DOCUMENT_NOTHING_TO_UNDO,
    MATERIAL_DOCUMENT_NOTHING_TO_REDO,
    MATERIAL_DOCUMENT_INVALID_ARGUMENT,
    MATERIAL_DOCUMENT_INVALID_NAME,
    MATERIAL_DOCUMENT_DUPLICATE_NAME,
    MATERIAL_DOCUMENT_INVALID_PALETTE,
    MATERIAL_DOCUMENT_OUT_OF_MEMORY,
    MATERIAL_DOCUMENT_STATE_ID_EXHAUSTED,
    MATERIAL_DOCUMENT_NO_PATH,
    MATERIAL_DOCUMENT_IO_ERROR
} MaterialDocumentResult;

void material_document_init(MaterialDocument *document);
void material_document_destroy(MaterialDocument *document);
MaterialDocumentResult material_document_create(MaterialDocument *document,
                                                const AssetRegistry *assets,
                                                const char *name,
                                                uint16_t palette_id,
                                                const char glyphs[4]);
MaterialDocumentResult material_document_open(MaterialDocument *document,
                                              const AssetRegistry *assets,
                                              uint16_t id,
                                              const char *material_directory);
MaterialDocumentResult material_document_set_name(MaterialDocument *document,
                                                  const AssetRegistry *assets,
                                                  const char *name);
MaterialDocumentResult material_document_set_palette(MaterialDocument *document,
                                                     const AssetRegistry *assets,
                                                     uint16_t palette_id);
MaterialDocumentResult material_document_set_glyphs(MaterialDocument *document,
                                                    const char glyphs[4]);
MaterialDocumentResult material_document_undo(MaterialDocument *document);
MaterialDocumentResult material_document_redo(MaterialDocument *document);
void material_document_discard(MaterialDocument *document);
bool material_document_is_dirty(const MaterialDocument *document);
Material material_document_preview(const MaterialDocument *document);
MaterialDocumentResult material_document_save(MaterialDocument *document);
MaterialDocumentResult material_document_save_as(MaterialDocument *document,
                                                 const char *material_directory);
MaterialDocumentResult material_document_commit_to_registry(
    const MaterialDocument *document, AssetRegistry *assets);

#endif