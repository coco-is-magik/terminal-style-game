/**
 * mapgen_seed.h — the provenance record for a generated scene
 *
 * A generated scene is an ordinary scene file: it contains no memory of what produced it. That
 * is deliberate (the scene format stays a scene format, and its strict unknown-key policy means
 * provenance cannot be smuggled into it), but it means the seed would otherwise be lost the
 * moment generation finished. This module is the record: a small key=value document beside the
 * scene, holding exactly the parameters needed to reproduce the map byte for byte.
 *
 * Pure text in, pure text out. No file access, no allocation, no diagnostics plumbing beyond a
 * result code: the caller owns the file so that the same codec serves the tool today and an
 * editor "regenerate" command later, without either owning the parse.
 *
 * The format is not the scene format and is not versioned by it. It carries its own
 * `mapgen_version`, so the sidecar can change shape independently of scenes and an old sidecar
 * is refused by version rather than misread.
 */

#ifndef MAPGEN_SEED_H
#define MAPGEN_SEED_H

#include "mapgen.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Room for every key, value and line terminator with generous slack. A sidecar longer than
   this is refused rather than truncated. */
#define MAPGEN_SEED_TEXT_MAX 512U

#define MAPGEN_SEED_VERSION 1

typedef enum {
    MAPGEN_SEED_OK = 0,
    MAPGEN_SEED_INVALID_ARGUMENT,
    MAPGEN_SEED_BUFFER_TOO_SMALL,
    MAPGEN_SEED_MALFORMED,
    MAPGEN_SEED_UNSUPPORTED_VERSION,
    MAPGEN_SEED_INVALID_VALUE
} MapgenSeedResult;

/* Serialise `parameters` to canonical text (fixed key order, one space around '=', '\n'
   terminated). Deterministic: the same parameters always produce the same bytes, so a sidecar
   can be compared and diffed. */
MapgenSeedResult mapgen_seed_write_text(const MapgenParameters *parameters,
                                        char *buffer, size_t buffer_size,
                                        size_t *out_length);

/* Parse canonical text back into parameters. Rejects unknown keys, duplicate keys, missing
   required keys, out-of-range values and an unsupported `mapgen_version`. On failure
   `out_parameters` is left untouched, so a caller can keep its current state. */
MapgenSeedResult mapgen_seed_read_text(const char *text,
                                       MapgenParameters *out_parameters);

#endif /* MAPGEN_SEED_H */
