/** flow_document_internal.h — Focused persistence fault seam. */
#ifndef FLOW_DOCUMENT_INTERNAL_H
#define FLOW_DOCUMENT_INTERNAL_H

#include "flow_document.h"

typedef enum {
    FLOW_DOCUMENT_SAVE_FAULT_NONE = 0,
    FLOW_DOCUMENT_SAVE_FAULT_SYNC,
    FLOW_DOCUMENT_SAVE_FAULT_REPLACE,
    FLOW_DOCUMENT_SAVE_FAULT_DURABILITY
} FlowDocumentSaveFault;

FlowDocumentResult flow_document_internal_save_as(
    FlowDocument *document, const char *path, FlowDocumentSaveFault fault
);

#endif