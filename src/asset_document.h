/** asset_document.h — Shared state identity for editable asset documents. */
#ifndef ASSET_DOCUMENT_H
#define ASSET_DOCUMENT_H

#include "editor_types.h"

#include <stdbool.h>

typedef struct {
    DocumentStateId current_state;
    DocumentStateId saved_state;
    DocumentStateId next_state;
} AssetDocumentState;

void asset_document_state_init(AssetDocumentState *state);
bool asset_document_state_advance(AssetDocumentState *state);
void asset_document_state_restore(AssetDocumentState *state,
                                  DocumentStateId state_id);
void asset_document_state_mark_saved(AssetDocumentState *state);
bool asset_document_state_is_dirty(const AssetDocumentState *state);

#endif