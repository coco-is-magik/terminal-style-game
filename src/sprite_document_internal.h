/** sprite_document_internal.h — Focused folder-transaction fault seam. */
#ifndef SPRITE_DOCUMENT_INTERNAL_H
#define SPRITE_DOCUMENT_INTERNAL_H

#include "sprite_document.h"

typedef enum {
    SPRITE_DOCUMENT_SAVE_FAULT_NONE = 0,
    SPRITE_DOCUMENT_SAVE_FAULT_SYNC,
    SPRITE_DOCUMENT_SAVE_FAULT_BACKUP_MOVE,
    SPRITE_DOCUMENT_SAVE_FAULT_PUBLISH_MOVE,
    SPRITE_DOCUMENT_SAVE_FAULT_RESTORE_MOVE,
    SPRITE_DOCUMENT_SAVE_FAULT_CANDIDATE_CLEANUP,
    SPRITE_DOCUMENT_SAVE_FAULT_BACKUP_CLEANUP,
    SPRITE_DOCUMENT_SAVE_FAULT_DURABILITY
} SpriteDocumentSaveFault;

SpriteDocumentResult sprite_document_internal_save(
    SpriteDocument *document, const AssetRegistry *assets,
    const char *sprite_directory, SpriteDocumentSaveFault fault
);

#endif