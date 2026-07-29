# R0 Current-Map Open/Switch Plan — 2026-07-29

## Goal and scope

Deliver R0 outcome 3: a safe keyboard-driven open/switch workflow for the
existing decimal digit-grid map files. This increment is deliberately bounded to
regular `.txt` files directly under `assets/maps/`; it is not the future scene
format, scene browser, or general file dialog.

## Accepted product contract

- Main-menu **Editor** enters an in-game map chooser instead of loading a
  hard-coded map.
- `Ctrl+O` opens the same chooser while editing.
- The catalog includes only direct-child regular `.txt` files under the configured
  map root. It is deterministic, non-recursive, and rejects symlinks.
- Clean selection attempts a transactional replacement immediately.
- Dirty selection requires **Save / Discard / Cancel**.
- Failed save keeps the dirty document and dirty-open prompt active with a visible
  error. It never proceeds to load.
- Failed target load preserves the current document, path, map, history,
  selection, inspector, mode, and application-owned camera. The chooser remains
  open with a visible error.
- Escape from a dirty-open prompt returns to the chooser. Escape from an
  in-editor chooser returns to the unchanged document. Escape from the initial
  no-document chooser requests return to the main menu.
- No native dialog, arbitrary path entry, recursion, recent files, New, Save As,
  or final scene-system language is included.

## Ownership and module boundaries

### `map_catalog`

A narrow headless module owns discovery results. It:

- borrows a root path only during refresh;
- owns a dynamic array of entries and each entry's display name/path;
- uses directory enumeration plus no-follow metadata checks;
- validates the `.txt` suffix, direct-child regular-file status, path sizing, and
  allocation;
- sorts by display name with `strcmp` for deterministic keyboard order;
- commits a refreshed candidate catalog only after complete success.

It does not load maps, know editor state, render UI, or accept arbitrary paths.

### `UnifiedEditorState`

The editor controller owns the chooser state, selected index, pending dirty-open
target, and dirty-open choice. It invokes `unified_editor_load_scene()` only after
the workflow permits replacement. Existing `SceneDocument` remains the sole map
owner and existing `CommandHistory` remains the sole edit-history owner.

### `app.c`

The application remains composition-only: initialize the editor with the fixed
map-root policy, enter `APP_STATE_EDITOR`, initialize the camera, and honor the
editor's existing request to return to the main menu. It does not enumerate,
select, validate, or load a chosen map itself.

### UI and input

`Ctrl+O` is one explicit edge-triggered editor action. The chooser is rendered as
a bounded terminal overlay using controller state and dynamic filenames. Static
labels/help remain presentation only. This does not create a generic list widget
or imply a scene browser.

## State transitions

1. **Initial entry**
   - initialize editor with no document;
   - refresh catalog and open chooser;
   - successful Enter loads the selected map, resets document-related history and
     selection through the existing successful-load path, and closes chooser;
   - failed discovery/load remains visible and recoverable;
   - Escape returns to main menu.
2. **Open from active document**
   - `Ctrl+O` refreshes and opens chooser without mutating document state;
   - Up/Down changes only chooser selection;
   - Escape closes chooser unchanged.
3. **Clean target**
   - Enter attempts target load;
   - success closes chooser;
   - failure keeps chooser and old state.
4. **Dirty target**
   - Enter records the selected catalog index and opens dirty-open prompt with
     **Cancel** as the safe default;
   - **Save** saves the current path, then attempts the target only on save
     success;
   - **Discard** attempts the target without saving;
   - **Cancel** or Escape returns to chooser unchanged.

## Transaction and failure matrix

| Operation | Failure | Required preserved state | Resulting workflow |
|---|---|---|---|
| Catalog refresh | missing/unreadable root, allocation, metadata/path failure | live document and prior editor state | chooser open, visible catalog error |
| Clean target load | parse/read/validation/allocation | document, path, history, selection, inspector, mode, camera | chooser open, load error |
| Dirty Save | validation/temp/write/flush/close/replace | dirty document, path, history, selection, inspector, mode, camera | dirty-open prompt open, save error |
| Save succeeds; target load fails | target read/parse/validation/allocation | current now-saved document, path, history, selection, inspector, mode, camera | chooser open, load error |
| Dirty Discard target load | target read/parse/validation/allocation | dirty document, path, history, selection, inspector, mode, camera | chooser open, load error |
| Any cancel | none | all authored and session state | previous workflow level |

Successful replacement intentionally resets history, selection, hover, inspector,
and status through `unified_editor_load_scene()`. Camera behavior is unchanged;
the editor controller does not own or reset it.

## Regression matrix

- `test-map-catalog`
  - deterministic sorting;
  - direct regular `.txt` filtering;
  - skip directories, non-`.txt`, and symlinks;
  - empty and missing roots;
  - refresh replacement and cleanup ownership.
- `test-input`
  - `Ctrl+O` sets and frame reset clears only the editor-open edge action;
  - repeat and non-control `O` do not trigger it.
- `test-unified-editor`
  - initial chooser and no-document Escape;
  - chooser navigation/cancel;
  - clean switch success/failure;
  - dirty prompt default/cancel;
  - Save success then switch;
  - Save failure blocks switching;
  - Discard success/failure;
  - failed operations preserve document/path/map/history/selection/mode and camera;
  - successful replacement resets only established document-session state;
  - empty/missing catalog remains safe and visible.
- strict application build and aggregate suite;
- focused AddressSanitizer and UndefinedBehaviorSanitizer runs for catalog/editor;
- interactive main-menu entry, `Ctrl+O`, dirty Save/Discard/Cancel, failed-open,
  and Escape smoke acceptance.

## Rejected shortcuts

- Keep loading `assets/maps/1.txt` before displaying a cosmetic chooser.
- Put directory traversal or dirty-switch policy in `app.c`.
- Reuse editable text input as an arbitrary path field.
- Clear history/selection before target load succeeds.
- Treat save success as permission to discard the current document when target
  load fails.
- Broaden this increment into scene packaging, recent files, New, or Save As.

## Stop and completion conditions

Stop and revisit the design if the existing editor cannot safely remain active
without a loaded map, or if direct-child regular-file validation cannot be kept
inside the narrow catalog module. Completion requires the regression matrix,
strict build, aggregate tests, sanitizers, stable contract updates, and recorded
interactive acceptance. `README.md` is updated last.

## Implementation and verification record — 2026-07-29

Detailed root causes, incomplete corrections, detection gaps, and prevention
lessons are preserved in `R0_MAP_OPEN_SWITCH_RCA_2026-07-29.md`.

Implemented the planned boundary in `src/map_catalog.[ch]`,
`src/unified_editor.[ch]`, `src/input.[ch]`, and `src/app.c`, with focused tests
in `tests/test_map_catalog.c`, `tests/test_input.c`, and
`tests/test_unified_editor.c`. `Makefile` now builds and aggregates the catalog
runner.

The dynamic chooser remains a bounded controller-rendered adapter. This was
chosen because `ui_ele` has no dynamic list data model; adding a generic list or
scene framework for one current-map workflow would violate the plan's smallest
proven-seam constraint.

Observed failures and corrections:

1. The first catalog-test compile lacked `<stdlib.h>` for `mkdtemp`; the test
   include was corrected.
2. A shared catalog fixture was invalid because one test intentionally mutates
   its root; catalog tests were changed to per-test setup/teardown.
3. Focused UBSan reported
   `src/map_catalog.c:174:5: runtime error: null pointer passed as argument 1`
   for `qsort(NULL, 0, ...)` on an empty catalog. Sorting is now skipped unless
   at least two entries exist; the sanitizer rerun passes.
4. Review found that a failed refresh with a newly supplied root replaced the
   remembered root even though catalog entries were transactional. Root commit
   now occurs only after successful refresh; regression coverage asserts prior
   root/catalog/document preservation.
5. Initial interactive acceptance found that the Enter edge used to activate the
   main-menu Editor action reached `unified_editor_update()` in the same frame and
   immediately opened the first map, making the initial chooser appear absent.
   The first correction consumed only generic `confirm`; retest showed Enter also
   sets `editor_confirm_pressed`. Handled menu actions now consume both edges
   before downstream state updates. `test-app-modules` covers both handled and
   unhandled behavior; strict app build and aggregate tests pass after the final
   correction.

Verification completed:

- strict forced application build: `make -B all` — pass;
- focused catalog: 4/4 — pass;
- focused input: 5/5 — pass;
- focused unified editor: 32/32 — pass;
- aggregate `make test` — pass;
- focused combined ASan+UBSan catalog/editor runners — pass after the empty-list
  correction;
- `make smoke` — pass with `{"smoke":"ok","map_width":10,"map_height":6}`.

Interactive acceptance passed on 2026-07-29. With an approved temporary second
map, the user confirmed initial chooser display, initial Escape to main menu,
opening `1.txt`, in-editor `Ctrl+O`, chooser Escape preservation, and dirty Save
and Discard switching. The initial run exposed the same-frame Enter leak above;
the user confirmed the corrected entry flow after both Enter-derived edges were
consumed. The temporary acceptance map was then removed. R0 outcome 3 is Verified.
