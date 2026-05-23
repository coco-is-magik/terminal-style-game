/**
 * decal_io.h — Per-file decal load, save, and free
 *
 * Provides the public API for reading and writing individual Decal assets
 * to and from disk.  Used by the Asset Designer to load and save decal
 * files using the same format read by the engine's asset_loader.c.
 *
 * File format:
 *   Key-value lines (key=value) for all spatial and layout fields, followed
 *   by explicit per-row pattern and material lines:
 *
 *     surface=0
 *     x=0.000000
 *     ...
 *     pattern_cols=3
 *     pattern_rows=2
 *     default_material=1
 *     pattern_0=ABC
 *     pattern_1=DEF
 *     material_0=1,2,1
 *     material_1=2,1,2
 *
 *   This is identical to the structured key-value mode supported by
 *   load_decal() in asset_loader.c (Mode A).  Art mode (art= header) is
 *   supported for loading only; saving always uses structured mode.
 *
 * Ownership model:
 *
 *   decal_load_from_file() — allocates the Decal struct and its pattern array
 *     on the heap.  Caller owns both.  Must call decal_free() when done.
 *
 *   decal_save_to_file() — does NOT take ownership.  Caller retains the Decal
 *     and must still free it after saving.
 *
 *   decal_free() — frees decal->pattern (PatternCell array), then frees the
 *     Decal struct itself.  The caller's pointer is NOT nullified; it becomes
 *     a dangling pointer after the call.  Double-free is undefined behaviour
 *     and is NOT safe.  After calling decal_free(d), set d = NULL.
 */

#ifndef DECAL_IO_H
#define DECAL_IO_H

#include "decal.h"  /* Decal, DecalSurface */

/**
 * decal_load_from_file() — Load a single decal from a file
 *
 * Reads a decal asset file in structured key-value format or inline art
 * format (same as asset_loader.c's internal load_decal).
 *
 * @param path  File path to read (e.g. "assets/decals/1.txt")
 * @return      Heap-allocated Decal on success, NULL on failure.
 *              Caller must free with decal_free().
 */
Decal *decal_load_from_file(const char *path);

/**
 * decal_save_to_file() — Save a decal to a file
 *
 * Writes the decal in structured key-value format (Mode A) compatible
 * with the engine's asset_loader.c parser.  The file is overwritten if
 * it already exists.
 *
 * @param path   Destination file path (e.g. "assets/decals/new.txt")
 * @param decal  Decal to write (must not be NULL)
 * @return       0 on success, -1 on failure (file not writable)
 */
int decal_save_to_file(const char *path, const Decal *decal);

/**
 * decal_free() — Free a heap-allocated Decal and its pattern
 *
 * Frees the pattern array and then the Decal struct itself.
 * Safe to call with NULL.
 *
 * @param decal  Decal to free (may be NULL)
 */
void decal_free(Decal *decal);

/**
 * decal_release_contents() — Free internal allocations without freeing struct
 *
 * Frees decal->pattern and sets it to NULL.  Does NOT free the Decal struct
 * itself.  Intended for stack-owned or embedded Decals where the caller owns
 * the struct storage.
 *
 * Safe to call repeatedly: after the first call pattern == NULL, and
 * free(NULL) is safe, so subsequent calls are no-ops.
 * NULL-safe: if decal is NULL, this function is a no-op.
 *
 * @param decal  Decal whose internal pattern to release (may be NULL)
 */
void decal_release_contents(Decal *decal);

#endif /* DECAL_IO_H */
