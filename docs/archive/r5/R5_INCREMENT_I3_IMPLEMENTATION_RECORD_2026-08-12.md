# R5 Increment I3 Implementation Record — 2026-08-12

## Status

**Complete and verified.** I3 adds an owned, undoable `DecalDocument`, exposes
the reusable rectangular pattern-grid editor through headless commands, and
adds a cached scene-scoped decal shortlist while preserving the boundary between
reusable decal assets and scene-owned placement instances.

## Delivered behavior

1. **DecalDocument lifecycle**
   - Added `src/decal_document.h` and `src/decal_document.c`.
   - A document owns its working grid, saved snapshot, independent change
     history, path, numeric asset ID, and `AssetDocumentState`.
   - Supports New, Open, validation, undo, redo, preview, Save, Save As,
     Discard, and registry commit.
   - New IDs use the lowest free reusable decal-pattern slot at or above 1.
   - Open loads the caller-supplied asset path through `decal_io`, preserving
     compatibility with both numeric and named decal files. Registry commit uses
     a deep copy; the live registry is never the mutable document model.

2. **Pattern-grid editor**
   - Cell paint and erase, whole-grid fill and clear, and resize-with-crop route
     through the existing `decal_painter` boundary.
   - Dimensions are bounded to 255 columns by 64 rows, matching `decal_io` and
     the reusable registry asset contract.
   - Resize preserves the top-left overlap, crops removed rows/columns, and
     zero-initializes newly exposed cells.
   - Every mutation is one independent asset-history command with deep-owned
     before/after values. Allocation failures preserve the active grid and
     existing history.
   - Preview returns a borrowed `DecalPatternAsset` view over the document-owned
     working grid.

3. **Pattern validation**
   - ID 0 remains reserved and cannot identify a document.
   - A visible glyph requires a nonzero loaded material.
   - Any nonzero material reference, including one on a blank/latent cell, must
     resolve through `material_id_is_loaded()`.
   - Out-of-bounds paint/erase and invalid dimensions fail without mutation.

4. **Atomic persistence through decal_io**
   - `DecalDocument` creates a same-directory temporary destination and delegates
     structured serialization to `decal_save_to_file()`.
   - The temporary file is flushed by close, reopened and `fsync`ed, then renamed
     atomically over the destination.
   - Save failure preserves dirty state, current path, and destination.
   - Erased in-memory glyph 0 serializes as a text-space rather than embedding a
     NUL byte in the asset file.
   - `decal_save_to_file()` now propagates buffered stream errors as well as
     close failures.

5. **Registry identity and commit**
   - Added `asset_registry_allocate_decal_pattern_id()` using the accepted
     lowest-free policy and `pattern == NULL` loaded marker.
   - Commit deep-copies through `asset_registry_set_decal_pattern()` and bumps
     registry generation only after successful replacement.
   - Full registry reload and document-repair refresh remain I4.

6. **Scene-scoped decal shortlist**
   - `UnifiedEditorState` owns a cached shortlist of reusable decal IDs referenced
     by the active scene's decal instances.
   - The shortlist is rebuilt only after successful scene load, native open,
     legacy import, reload, or New.
   - IDs are deduplicated and sorted numerically because the registry has no
     decal-name table.
   - Missing IDs remain in the shortlist so repair targets are visible rather
     than silently filtered out.
   - No registry-capacity traversal or per-frame shortlist rebuild was added.
   - The current controller exposes the shortlist model through bounded query
     APIs. Interactive placement/instance mutation is intentionally deferred to
     R6 because no decal-instance selection workflow exists in R5.

7. **16-bit missing-reference repair**
   - Corrected native scene decal `asset_id` parsing from the stale 255 cap to
     `ASSET_ID_MAX` (65,535), matching the widened registry and scene type.
   - Existing `SCENE_DIAGNOSTIC_INPUT_ASSET_MISSING` repair mode now accepts and
     reports missing high-ID decal patterns rather than rejecting the scene as
     malformed before repair analysis.

## Tests

- Added `tests/test_decal_document.c` with deterministic coverage for:
  - lowest-free New and owned default grid;
  - paint, preview, resize/crop, undo, redo, and Discard;
  - dimension, bounds, visible-null-material, and missing-material validation;
  - Save As, Open, registry commit/generation, and clean Discard;
  - failed Save As preserving dirty state and path.
- Unified-editor coverage protects scene-derived ordering, deduplication,
  missing high-ID repair visibility, bounds behavior, and New-scene reset.
- Existing decal I/O, painter, scene format, material picker, scene history,
  reload, repair, and rendering tests remain enabled and unchanged in intent.

## Verification

Passed on 2026-08-12:

- `make all` — clean strict C11 build under `-Wall -Wextra -Wpedantic -Werror`.
- `make test` — 397/397 headless tests passed.
- Focused `test-decal-document` — 4/4 passed.
- Focused `test-decal-io` — 15/15 passed.
- Focused `test-unified-editor` — 57/57 passed.
- Focused `test-scene-format` — 17/17 passed.
- `make asan` — full AddressSanitizer/LeakSanitizer suite passed.
- `make ubsan` — full UndefinedBehaviorSanitizer suite passed.
- `make check-legacy-unused` — passed.
- `git diff --check` — passed.
- `make leak` — skipped: Valgrind is not installed.
- `make style` — skipped: cppcheck is not installed.

## Explicitly deferred

- Full registry reload, dependency reporting refresh, and scene repair rerun after
  asset commit remain I4.
- Interactive surface-projected painting, decal placement/spray, instance
  selection, transforms, duplication, and deletion remain R6.
- A decal-name namespace is not introduced; existing decal identity remains
  numeric and file-compatible.
- Sprite widening and per-face wall materials remain future decisions.

## Next action

Begin I4 with successful-commit registry refresh and repair-diagnostic rerun,
then verify explicit scene-history versus asset-history/save boundaries and
schedule Review E.