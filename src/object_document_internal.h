/** object_document_internal.h — Focused atomic-creation fault seam. */
#ifndef OBJECT_DOCUMENT_INTERNAL_H
#define OBJECT_DOCUMENT_INTERNAL_H

#include "object_document.h"

typedef enum {
    OBJECT_DOCUMENT_CREATE_FAULT_NONE = 0,
    OBJECT_DOCUMENT_CREATE_FAULT_SYNC,
    OBJECT_DOCUMENT_CREATE_FAULT_REPLACE,
    OBJECT_DOCUMENT_CREATE_FAULT_DURABILITY
} ObjectDocumentCreateFault;

ObjectDocumentResult object_document_internal_create_atomic(
    const AssetRegistry *assets, const char *object_directory,
    const char *name, uint16_t sprite_id, double front_direction,
    uint16_t *out_id, ObjectDocumentCreateFault fault
);

#endif
