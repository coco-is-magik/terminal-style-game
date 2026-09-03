/** object_document.h — Validated atomic creation of reusable object assets. */
#ifndef OBJECT_DOCUMENT_H
#define OBJECT_DOCUMENT_H

#include "assets.h"
#include "scene_types.h"

typedef enum {
    OBJECT_DOCUMENT_OK = 0,
    OBJECT_DOCUMENT_INVALID_ARGUMENT,
    OBJECT_DOCUMENT_INVALID_NAME,
    OBJECT_DOCUMENT_DUPLICATE_NAME,
    OBJECT_DOCUMENT_INVALID_SPRITE,
    OBJECT_DOCUMENT_FULL,
    OBJECT_DOCUMENT_IO_ERROR
} ObjectDocumentResult;

bool object_asset_name_has_prefix(const char *name, const char *prefix);
int object_asset_find_by_name(const AssetRegistry *assets, const char *name);
uint16_t object_asset_allocate_id(const AssetRegistry *assets);

ObjectDocumentResult object_document_create_atomic(
    const AssetRegistry *assets, const char *object_directory,
    const char *name, uint16_t sprite_id, double front_direction,
    uint16_t *out_id
);

#endif