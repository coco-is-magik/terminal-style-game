/** material_document_internal.h — Focused persistence fault seam. */
#ifndef MATERIAL_DOCUMENT_INTERNAL_H
#define MATERIAL_DOCUMENT_INTERNAL_H

#include "material_document.h"

typedef enum {
    MATERIAL_DOCUMENT_SAVE_FAULT_NONE = 0,
    MATERIAL_DOCUMENT_SAVE_FAULT_SYNC,
    MATERIAL_DOCUMENT_SAVE_FAULT_REPLACE,
    MATERIAL_DOCUMENT_SAVE_FAULT_DURABILITY
} MaterialDocumentSaveFault;

MaterialDocumentResult material_document_internal_save(
    MaterialDocument *document, MaterialDocumentSaveFault fault
);
MaterialDocumentResult material_document_internal_save_as(
    MaterialDocument *document, const char *material_directory,
    MaterialDocumentSaveFault fault
);

#endif
