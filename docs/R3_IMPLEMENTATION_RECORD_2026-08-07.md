# R3 Implementation Record — Generalized Editor Domain Foundation

**Started:** 2026-08-07  
**Status:** Active — implementation/automated Q3 complete; manual Q3 pending  
**Plan:** `R3_REQUIREMENTS_AND_IMPLEMENTATION_PLAN_2026-08-07.md`

## Increment A — Typed selection and stable references

**Implemented:** Extended the discriminated `SelectionTarget` with a point-light
variant carrying only `SceneInstanceId`. Added a const `SceneDocument` resolver by
stable light ID. Added an allocation-free geometric picker that composes over the
existing wall hit, rejects invalid/non-finite/out-of-range candidates, respects wall
occlusion, chooses nearest forward distance, and uses stable ID as the exact-tie
breaker. `UnifiedEditorState` composes authored light values with the pure picker
and resolves selected IDs through the document.

**Preserved behavior:** Existing wall DDA selection, face identity, material
inspector, command history, highlights, Save/Open/Import, and dirty workflows are
unchanged. Light selection does not open the wall inspector; domain inspector and
highlight support remain assigned to later R3 increments. Escape clears either
typed target before opening the exit menu.

**Boundary evidence:** The pure picker borrows `SceneLight[]` and does not depend on
`SceneDocument`, I/O, SDL, or rendering. The document resolver owns stable-ID lookup.
The editor stores no light pointer or collection index.

**Focused verification:** Strict selection runner passed 16/16, scene-document
runner passed 33/33, and unified-editor runner passed 44/44. Coverage includes wall
regressions, nearest/tie selection, behind-camera/off-ray/out-of-range/non-finite and
invalid-ID rejection, wall occlusion/fallback, invalid arguments, resolver
found/missing/null cases, integrated hover/select by ID, status rendering, and Escape
hierarchy. The aggregate suite passed 27/27 runners; focused ASan and UBSan
passed 16/16, 33/33, and 44/44 without sanitizer diagnostics; and the deprecated
legacy-symbol guard passed.

## Increment B — Commands and atomic groups

**Implemented:** Extended the authored-document command boundary with bounded
`SceneLight` replacement requests addressed by stable `SceneInstanceId`. Added a
fixed-capacity grouped command containing at most eight distinct authored-object
mutations. A group validates every request and reserves history capacity before
changing the document, commits as one state transition, and undoes/redoes as one
history step. Duplicate targets are rejected so snapshots remain unambiguous.

**Atomicity and validity:** Light replacements preserve identity and must satisfy
the native scene's finite, in-map position and positive-radius rules. Invalid,
missing, duplicate-target, exhausted-state, and allocation-failure requests leave
the document, dirty state, state ID, and history unchanged. Execute, undo, and redo
roll back mutations already applied if a later stable target cannot be resolved.
Redo invalidation and monotonic state-ID behavior remain unchanged.

**Boundary evidence:** `command_system` remains the sole production caller of the
internal wall/light mutation functions. It stores owned before/after authored
values; no UI, renderer, SDL, runtime-world pointer, light array index, or file I/O
crosses the command boundary. The real grouped proof edits multiple bounded fields
on two lights and restores both with one undo operation.

**Focused verification:** Strict command-system runner passed 22/22, scene-document
runner passed 33/33, and unified-editor runner passed 44/44. New coverage includes
light normal/no-change/invalid cases, multi-light grouped apply/undo/redo, duplicate
target rejection, pre-commit validation, allocation failure, and partial undo/redo
rollback. The aggregate suite log contains 27/27 passing runners; focused ASan and
UBSan command runners passed 22/22 without diagnostics; and the deprecated
legacy-symbol guard passed.

## Increment C — Domain inspector/tool adapters

**Implemented:** Added a headless `editor_domain` seam with typed wall-material and
point-light adapters. Target dispatch selects the applicable inspector; the wall
adapter constructs the established whole-cell material command request; and the
light adapter exposes eight typed fields (`x`, `y`, RGBA, intensity, radius), labels,
bounded ranges, steps, formatting, and stable-ID replacement requests. There is no
untyped property dictionary or generic string mutation API.

**Controller integration:** Selecting either proof domain now opens its inspector.
The wall material list and Up/Down/Enter behavior remain unchanged. The light
inspector uses Up/Down to select a typed field and Left/Right to submit one bounded
command step. Successful light apply/undo/redo rebuilds the disposable runtime world
through the existing document-to-runtime boundary. Native Save/Open round-trips the
edited authored value. Inspector kind and selected field reset or survive failed
loads with the same transactional rules as the existing selection state.

**Boundary evidence:** `editor_domain` is deterministic and allocation-free. It
borrows `SceneDocument`/`Map` values and returns metadata, formatted values, or an
owned `EditorMutationRequest`; it has no SDL, renderer, grid, filesystem, runtime
world, controller-state, or command-history dependency. `unified_editor` adapts
explicit input edges and renders the existing terminal interface. It never writes a
light or wall material directly.

**Focused verification:** Dedicated adapter runner passed 6/6, input passed 10/10,
and unified editor passed 47/47 under strict warnings. Coverage includes typed
dispatch, wall request construction, every light field's metadata/formatting,
bounded step/clamp and typed-value behavior, invalid target/direction/argument
rejection, held Left/Right repeat, inline numeric replacement, visible light
inspector, command history/dirty state, derived
runtime synchronization, undo/redo, failed-load state preservation, and native
Save/Open persistence. The aggregate suite passed 28/28 runners; focused ASan and
UBSan passed adapter 4/4 and unified editor 45/45 without diagnostics; the strict
application build and deprecated legacy-symbol guard passed.

## Increment D — Highlight providers

**Implemented:** Generalized the allocation-free editor highlight compositor to
dispatch typed wall-face and point-light targets. The existing wall outline provider
and glyph behavior remain unchanged. The light provider resolves borrowed authored
`SceneLight` values by stable `SceneInstanceId`, projects a three-cell eye-height marker using
the world renderer's camera/FOV convention, clips behind-camera/out-of-FOV/off-grid
targets, and rejects markers hidden by the nearest wall ray. Hover renders before
selection, and selection wins when both identify the same typed target.

**Boundary evidence:** `editor_highlight` borrows the map, camera, authored light
array, hover, selection, and grid. It performs no allocation and has no
`SceneDocument`, command, controller, SDL-event, filesystem, runtime-world, or
authored-mutation dependency. `app.c` remains the composition adapter: after world
rendering it supplies the document-owned borrowed light view to the highlight
provider. Marker foreground uses authored RGB against a contrast-selected background and does
not alter authored light color or map data.

**Focused verification:** Strict highlight runner passed 14/14. All ten existing
wall/crosshair regressions remain green. New coverage proves stable-ID lookup with
array order independent of identity, deterministic projection, no borrowed-value
mutation, hover/selection precedence, nearer-wall occlusion, missing ID,
behind-camera and non-finite rejection, pitch clipping, and null safety. The strict
application build passed with the provider integrated at the editor render boundary.
The aggregate suite passed 28/28 runners; focused ASan and UBSan highlight runners
passed 14/14 without diagnostics; and the deprecated legacy-symbol guard passed.
The optional normal-mode `benchmark-raycast` run reported `avg_render_ms=6.11` and
`fail_performance`, but that mode does not execute the editor highlight provider and
is not evidence for or against this editor-only path. Applicable interactive editor
benchmark/stability remains part of Increment E's Q3 closeout.

## Increment E — Inspector presentation migration and closeout

**Implemented:** Added one shared typed inspector presentation descriptor proven by
both concrete domains. It supplies inspector title, controls, optional note, field
count, field label, and typed choice/numeric field shape. Wall remains a one-field
material choice with Up/Down/Enter. Light remains eight bounded numeric fields with
Up/Down/Left/Right. Domain-specific value rendering and mutation stay specialized;
there is no universal property dictionary, string-to-value mutation, generic widget
framework, or direct UI write into authored state.

**Focused verification:** Strict editor-domain runner passed 5/5, unified-editor
runner passed 45/45, and highlight runner passed 14/14. Tests assert the shared
presentation descriptor for both domains and the rendered title/control/note text,
while preserving command-only edits, native Save/Open, wall controls, and typed
highlight behavior.

**Sequential Q3 evidence:** After an initial invalid concurrent attempt was discarded
because several Make targets clean the shared build directory, all state-changing
gates were rerun sequentially. Strict aggregate passed 28/28 runners; the defined
matrix passed 8/8 modes; full-suite ASan passed 28/28 with no sanitizer/leak
diagnostics; full-suite UBSan passed 28/28 with no runtime diagnostics; the legacy
symbol guard passed; and a final normal strict application/focused build passed.
Format, migration, malformed-input, restoration, and native light Save/Open tests
remain part of the passing aggregate suite.

**Editor-path performance/stability:** Added dedicated headless
`benchmark-editor-highlight` and `stability-editor-highlight` Make targets that drive
the real typed highlight provider through four mixed selected/hovered wall/light and
occlusion scenarios. The runner resets and checksums an 80x40 framebuffer each
iteration, so its measured scenario cost is a conservative upper bound on the
provider alone. Final strict runs passed: 20,000 benchmark iterations averaged
`0.170743 ms`; 100,000 stability iterations averaged `0.170572 ms`; both stayed
deterministic under a `1.000 ms` budget. Sustained 100,000-iteration ASan and UBSan
runs also passed without diagnostics.

**Closeout disposition:** All implementation and non-interactive Q3 items pass. R3
remains **Active**, not Verified, only because relevant interactive acceptance has
not yet been recorded. Review D contains the exact checklist. The unrelated
normal-mode benchmark result remains recorded under Increment D.

## Verification ledger

| Increment | Strict build | Focused tests | Boundary/failure tests | Documentation | Result |
|---|---|---|---|---|---|
| A — typed selection and stable references | pass | selection 16/16; scene document 33/33; unified editor 44/44; aggregate 27/27; focused ASan/UBSan pass | invalid geometry/ID/arguments, wall occlusion, stable tie, missing resolver pass | plan, architecture, record updated | Pass |
| B — commands and atomic groups | pass | command system 22/22; scene document 33/33; unified editor 44/44; aggregate 27/27; focused ASan/UBSan pass | invalid/missing/duplicate target, no-change, OOM, state exhaustion, execute/undo/redo rollback pass | record updated | Pass |
| C — domain inspector/tool adapters | pass | editor domain 4/4; input 10/10; unified editor 45/45; aggregate 28/28; focused ASan/UBSan pass | invalid target/direction/arguments, bounds/clamps, failed-load state, command-only edits, runtime sync, Save/Open round-trip pass | architecture and record updated | Pass |
| D — target-specific highlight providers | pass | editor highlight 14/14; aggregate 28/28; focused ASan/UBSan pass | stable ID, projection, precedence, wall occlusion, invalid/invisible targets, borrowed-state preservation pass | architecture and record updated; unrelated normal-mode benchmark result recorded | Pass |
| E — presentation migration and automated closeout | pass (interactive pending) | editor domain 5/5; unified editor 45/45; highlight 14/14; aggregate 28/28; matrix 8/8; full ASan/UBSan 28/28; editor benchmark/stability pass | shared typed presentation, rendered wall/light controls, forbidden-shortcut audit, deterministic 20k/100k editor highlight scenarios pass | README, architecture, record, Review D updated | Non-interactive pass; R3 Active pending interactive acceptance |
