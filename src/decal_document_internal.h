/** decal_document_internal.h — Focused persistence fault seam. */
#ifndef DECAL_DOCUMENT_INTERNAL_H
#define DECAL_DOCUMENT_INTERNAL_H

#include "decal_document.h"

typedef enum {
    DECAL_DOCUMENT_SAVE_FAULT_NONE = 0,
    DECAL_DOCUMENT_SAVE_FAULT_SYNC,
    DECAL_DOCUMENT_SAVE_FAULT_REPLACE,
    DECAL_DOCUMENT_SAVE_FAULT_DURABILITY
} DecalDocumentSaveFault;

DecalDocumentResult decal_document_internal_save(
    DecalDocument *document, const AssetRegistry *assets,
    DecalDocumentSaveFault fault
);
DecalDocumentResult decal_document_internal_save_as(
    DecalDocument *document, const AssetRegistry *assets,
    const char *decal_directory, DecalDocumentSaveFault fault
);

#endif /* DECAL_DOCUMENT_INTERNAL_H */
