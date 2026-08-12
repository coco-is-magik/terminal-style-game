#include "asset_document.h"

#include <stdint.h>

void asset_document_state_init(AssetDocumentState *state) {
    if (!state) return;
    state->current_state = 1U;
    state->saved_state = 1U;
    state->next_state = 2U;
}

bool asset_document_state_advance(AssetDocumentState *state) {
    if (!state || state->next_state == 0U || state->next_state == UINT64_MAX) {
        return false;
    }
    state->current_state = state->next_state++;
    return true;
}

void asset_document_state_restore(AssetDocumentState *state,
                                  DocumentStateId state_id) {
    if (!state || state_id == 0U) return;
    state->current_state = state_id;
}

void asset_document_state_mark_saved(AssetDocumentState *state) {
    if (!state) return;
    state->saved_state = state->current_state;
}

bool asset_document_state_is_dirty(const AssetDocumentState *state) {
    return state && state->current_state != state->saved_state;
}