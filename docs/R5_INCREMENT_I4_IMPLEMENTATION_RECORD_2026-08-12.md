# R5 Increment I4 Implementation Record — 2026-08-12

## Status

**Complete and verified.** I4 adds commit-only transactional eager registry
refresh, rebuilds missing-reference diagnostics including transitive decal
material dependencies, proves independent scene/asset histories, passes Q1–Q3,
and closes Review E without a blocker.

## Delivered behavior

1. **Transactional asset refresh coordinator**
   - Added `src/asset_refresh.h` and `src/asset_refresh.c`.
   - Save and Save As wrappers exist for both `MaterialDocument` and
     `DecalDocument`.
   - Asset persistence completes first. Only a successful asset save triggers
     eager registry refresh.
   - Refresh allocates a separate candidate `AssetRegistry`, loads the complete
     asset root, rebuilds scene diagnostics against the candidate, and swaps
     ownership only when all preparatory steps succeed.
   - Live registry generation advances exactly once per successful refresh.

2. **Failure preservation**
   - Candidate allocation failure and unreadable asset root preserve live
     registry pointers, contents, generation, and scene repair diagnostics.
   - Candidate-owned allocations are released on every failed path.
   - Optional absent directories and invalid individual asset files remain
     tolerant skips; an unreadable root fails.

3. **Repair diagnostic refresh**
   - Added public transactional
     `scene_document_refresh_repair_diagnostics()`.
   - Replacement diagnostics are prepared before prior storage is released and
     scene current/saved identities are unchanged.
   - Existing cell material and decal-pattern diagnostics retain their stable
     `SCENE_DIAGNOSTIC_INPUT_ASSET_MISSING` code and clear text.
   - Scene-used decal patterns are now scanned for missing material dependencies,
     reported with `pattern_material` and scene decal instance identity.

4. **History/save boundary**
   - Asset Save never invokes scene Save or changes scene history identity.
   - Scene Save never invokes asset Save or changes asset document dirty state.
   - Material and decal dirty state is cleared only by their own persistence.
   - Repair-list refresh is derived dependency state, not an authored command.

5. **Production integration**
   - Inline material creation now uses Save As + full eager refresh rather than
     direct slot mutation.
   - `UnifiedEditorState` owns an explicit asset root and rebuilds cached
     shortlists after successful refresh.
   - Startup and smoke paths check reportable registry-loader failure.
   - No renderer, frame-dispatch, or per-frame refresh call was added.

## Tests

- Added `tests/test_asset_refresh.c` covering material/decal Save and Save As,
  generation, eager disk reload, dependency resolution, transitive diagnostics,
  allocation/missing-root rollback, and independent histories.
- Updated scene-document fixture expectations for transitive dependencies.
- Updated inline material creation to exercise a real asset-root layout and full
  eager refresh.

## Q1–Q3

- **Q1:** Passed. Identity, deletion, widening, null, picker, eager-load,
  compatibility, ownership, transaction, and failure policies are documented.
- **Q2:** Passed for I1–I4. Strict builds, focused tests, realistic failures,
  rollback, documentation, and unrelated regression suites pass per increment.
- **Q3:** Passed. Aggregate suite, eight build configurations, format/migration
  suites, ownership sanitizers, stable docs, and applicable controller-level
  interaction checks pass. No hot renderer/simulation path changed.

## Verification

Passed on 2026-08-12:

- `make all` — clean strict C11 build.
- `make test` — 401/401 passed.
- `test-asset-refresh` — 4/4 passed.
- `test-scene-document` — 40/40 passed.
- `test-unified-editor` — 57/57 passed.
- `make matrix` — all 8 configurations passed.
- `make asan` — full AddressSanitizer/LeakSanitizer suite passed.
- `make ubsan` — full UndefinedBehaviorSanitizer suite passed.
- Targeted final coordinator ASan/UBSan — 4/4 passed under each.
- `make check-legacy-unused` and `git diff --check` — passed.
- `make leak` — skipped: Valgrind unavailable.
- `make style` — skipped: cppcheck unavailable.

## Review E

Completed at `docs/reviews/2026-08-12-roadmap-r5-review-e.md`. No blocker remains.
The review confirms the narrow lifecycle seam, owned domain histories,
transactional registry lifetime, and asset/instance separation.

## Remaining explicit constraints

- Individual malformed assets remain tolerant loader skips.
- Direct registry commit helpers remain for isolated/headless tooling; production
  editor commits use full refresh.
- Full visual material-property UI remains deferred.
- Decal placement and surface-projected painting remain R6.
- Sprite widening and per-face wall materials remain future decisions.

## Next action

Begin R6 planning using the verified R5 ownership boundary: reusable decal
patterns remain asset documents; placement transforms and instance commands
remain scene-owned.

### Completed follow-up: name-collision confirmation flow

**Status: Implemented and focused-test verified (2026-08-13).**

**Scope:** `src/input.c`, `src/material_document.c`, `src/unified_editor.c`, and
their focused tests. Decal collision flow remains deferred to R6.

**Trigger:** user creates a new material by name; name collides with an existing
asset file on disk.

**Dialog text:** `"Material '<name>' already exists in assets. Load material?"`

**Controls:**
- Enter — Yes: select the eagerly loaded disk asset and apply it to the scene;
  its scene reference makes it part of the map-derived shortlist.
- Esc — No: abort; no material changes, shortlist untouched.
- O — Overwrite: open second dialog
  `"Overwrite '<name>'? Existing asset will be lost."` (Enter/Esc).
  - Yes: atomically replace the asset using the existing ID, default palette, and
    `####` inline-creation glyphs; reload; apply it to the scene.
  - Esc: back to original dialog.

**Rules:**
- New/overwritten assets enter the map-derived shortlist only after write, reload,
  and scene application. Loading an existing asset applies its already-loaded ID;
  saving that scene reference makes the membership persistent across reloads.
- Overwrite always requires a second confirmation before any file deletion/write.
- Dialog is controller state driven and testable without a real UI.

**Implemented tests:**
- `tests/test_unified_editor.c`: exact collision prompt, Enter load/apply, Esc abort,
  overwrite second confirmation and cancellation, and ID-preserving replacement.
- `tests/test_material_document.c`: replacement documents preserve the loaded ID
  and reject unloaded replacement targets.
- `tests/test_input.c`: unmodified O is a one-frame editor action while Ctrl+O
  remains the scene-open shortcut.