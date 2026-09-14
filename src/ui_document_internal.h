/** ui_document_internal.h — Focused persistence fault seam. */
#ifndef UI_DOCUMENT_INTERNAL_H
#define UI_DOCUMENT_INTERNAL_H

#include "ui_document.h"

typedef enum {
    UI_DOCUMENT_SAVE_FAULT_NONE = 0,
    UI_DOCUMENT_SAVE_FAULT_SYNC,
    UI_DOCUMENT_SAVE_FAULT_REPLACE,
    UI_DOCUMENT_SAVE_FAULT_DURABILITY
} UiDocumentSaveFault;

UiDocumentResult ui_document_internal_save_as(
    UiDocument *document, const char *path, UiDocumentSaveFault fault
);
UiDocumentResult ui_document_internal_save(
    UiDocument *document, UiDocumentSaveFault fault
);

#endif