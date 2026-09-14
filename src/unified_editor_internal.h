/** unified_editor_internal.h — Focused editor platform fault seams. */
#ifndef UNIFIED_EDITOR_INTERNAL_H
#define UNIFIED_EDITOR_INTERNAL_H

#include "platform_fs_internal.h"
#include "sprite_document_internal.h"
#include "unified_editor.h"

bool unified_editor_internal_ensure_sprite_directory(
    const UnifiedEditorState *editor, char *out, size_t out_size,
    PlatformFsFault fault
);
bool unified_editor_internal_save_sprite_document(
    UnifiedEditorState *editor, SpriteDocumentSaveFault fault
);

#endif