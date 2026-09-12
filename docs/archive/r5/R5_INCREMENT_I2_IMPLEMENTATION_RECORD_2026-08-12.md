# R5 Increment I2 Implementation Record — 2026-08-12

## Status

**Complete and verified.** I2 adds the shared asset-document state seam,
`MaterialDocument`, and a map-scoped material picker with four visible rows,
incremental prefix search, and inline creation when search has no matches.

## Delivered behavior

1. **Shared asset-document state**
   - Added `src/asset_document.h` and `src/asset_document.c`.
   - `AssetDocumentState` owns immutable current/saved/next state identities,
     dirty comparison, advance, restore, and mark-saved operations.
   - Scene command history remains scene-specific; no generic property framework
     or coupling to `SceneDocument` was introduced.

2. **MaterialDocument**
   - Added `src/material_document.h` and `src/material_document.c`.
   - Owns material ID, unique filename-derived name, palette reference, four
     distance glyphs, path, saved snapshot, and independent undo/redo history.
   - Supports create, open, name/palette/glyph edits, undo, redo, preview, Save,
     Save As, Discard, and registry commit.
   - Save writes `id`, `palette`, and `glyphs` through a same-directory temporary
     file, flush, `fsync`, close, and atomic rename. Failure leaves the document
     dirty and the destination unchanged.
   - Names accept alphanumeric characters, `_`, and `-`; duplicates are rejected.

3. **Map-scoped material shortlist**
   - `UnifiedEditorState` owns the shortlist and filtered search results.
   - It is rebuilt once after scene load/import/open/new by scanning authored wall,
     floor, and ceiling references, including latent wall values on empty cells.
   - Materials used by decal patterns referenced by scene decal instances are also
     included. IDs are deduplicated and alphabetized by loaded material name.
   - Successful assignment and inline creation add and re-sort one ID without a
     registry-wide rebuild.
   - The old `editor_count_loaded_materials`,
     `editor_material_at_picker_index`, and
     `editor_find_picker_index_for_material` scans were removed.

4. **Picker interaction**
   - `EDITOR_PICKER_VISIBLE` is four.
   - Up/Down wraps over filtered shortlist results and the existing visible-window
     logic scrolls as selection moves beyond the top or bottom visible row.
   - Typed text performs case-insensitive prefix filtering; Backspace edits the
     query; Escape returns to the inspector; Enter applies the highlighted result.
   - Empty search results show one action, `Create new material...`.
   - Enter creates the lowest-free ID with the typed name, configured default
     palette, and `####` glyph defaults; saves it under the configured material
     root, commits it to the eager registry, adds it to the shortlist, and applies
     it to the selected surface.
   - No file picker or lazy per-map asset loader was added.

## Performance boundary

- One-time shortlist construction is O(authored cells + referenced decal cells).
- Search is O(shortlist) only when the query changes.
- Assignment/create updates are O(shortlist), not O(registry capacity).
- Per-frame picker rendering is O(visible rows), capped at four.
- No 1..255 or 1..65535 material-registry traversal remains in
  `unified_editor.c`.

## Tests

- New `tests/test_material_document.c` covers create, validation, edit, undo,
  redo, preview, Save As, open, discard, registry commit/generation, and failed
  atomic save.
- Unified-editor coverage now protects shortlist ordering/access, assignment
  additions, prefix filtering, four-row rendering, empty-result create/save/apply,
  missing-material presentation, and existing picker navigation.
- Existing scene history and Save behavior remain independent of material history.

## Verification

Passed on 2026-08-12:

- `make all` — clean strict C11 build under `-Wall -Wextra -Wpedantic -Werror`.
- `make test` — full headless repository suite passed.
- Focused `test-material-document` — 4/4 passed.
- Focused `test-unified-editor` — 56/56 passed.
- `make asan` — full AddressSanitizer/LeakSanitizer suite passed.
- `make ubsan` — full UndefinedBehaviorSanitizer suite passed.
- `git diff --check` — passed.
- Source search confirms the three retired picker scan helpers are absent.

## Explicitly deferred

- Full visual material-property editor UI remains deferred; I2 provides the
  reusable tested document API and the agreed inline minimal create flow.
- Registry-wide refresh and scene repair after arbitrary asset commits remain I4.
- `DecalDocument` is I3; scene decal placement remains R6.
- Sprite widening and per-face wall materials remain future decisions.

## Next action

Begin I3 by implementing `DecalDocument` through `AssetDocumentState`, reusing
the existing `decal_painter` and `decal_io` headless boundaries.