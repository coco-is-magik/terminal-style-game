# R2 Implementation Record — 2026-07-31

## Current status

R2 is **Active**. Q1 planning was approved on 2026-07-31. This record is updated as
each increment is implemented and verified; it must preserve both successful and
failed evidence.

## Accepted planning decisions

- Native extension/root: `.tscene` under `assets/scenes`.
- Scene Save As namespace: `[A-Za-z0-9_-]{1,64}`.
- Optional persisted legacy provenance; never a destination.
- Locale-independent finite decimal parsing and canonical round-trip formatting.
- Same-directory atomic replacement with file and directory durability steps.
- Existing mode preservation; new files remain `0600`.
- Post-rename directory-sync failure means committed Save with durability warning.
- Visible missing-decal fallback, preserved reference, no force-save.
- New creates a bordered 10 by 6 compatibility scene.
- Dirty protection includes window close.

## Increment log

### Increment 1 — Freeze specification

**Work:** Added the approved R2 requirements/implementation plan, native v1 format
specification, this evidence record, diagnostic reservation, and roadmap Active
state.

**Reason:** Q1 requires implementation-visible format, ownership, compatibility,
failure, workflow, and verification contracts before persistence code begins.

**Verification:** Focused bounded checks passed: all three R2 records exist and are
nonempty; the dependency table and detailed R2 section both say Active; the detailed
section links the approved plan; `TSG-SCENE-ENV-0007` occurs once; and the exhausted
ID sentinel rule is present. The first check produced no captured success line even
though the command returned successfully, so a smaller explicit check was run and
reported `checks=True,True,True,True`.

**Known uncertainty:** None that permits reopening accepted R1 decisions. Concrete C
API names may narrow during implementation, but behavior and transaction contracts
are fixed.

### Increment 2 — Authored domain, diagnostics, and stable IDs

**Work:** Added `scene_types.h` with bounded authored light/decal/reference types and
the scene-wide ID limits; added the bounded no-allocation `scene_diagnostic` value
API and permanent ID mapping; expanded `SceneDocument` ownership/lifecycle and const
queries; and added monotonic internal ID allocation with a never-allocated exhausted
sentinel. The temporary digit-grid adapter remains intact for current callers.

**Verification:** Strict focused build passed. `test-scene-document` passed 17/17,
including null-safe lifecycle/queries, bounded diagnostic context, monotonic IDs,
last-ID allocation, and exhaustion without output mutation. Existing
`test-command-system` passed 17/17 and `test-unified-editor` passed 32/32. The full
application compiled with strict C11 warnings as errors. Focused sanitizer evidence
also passed 17/17 with combined AddressSanitizer, UndefinedBehaviorSanitizer, and
leak detection enabled.

**Preserved behavior:** Existing legacy map load/save, wall editing, document state
IDs, command history, camera preservation, and editor workflows are unchanged.

**Known transition:** Public struct fields remain visible because current editor and
tests depend on them. They will be narrowed only after native transactions and the
derived runtime adapter replace those direct dependencies.

### Increment 3 — Headless native v1 format boundary

**Work:** Added `scene_format.h/.c` with an owned candidate lifecycle independent
of `SceneDocument` commit and filesystem I/O. Implemented strict transactional
two-pass parsing and validation for the v1 metadata, cells, lights, and decal
instances, including resource bounds, exact strings/numbers, scene-wide IDs,
high-water validation, bounds, and passable spawn. Implemented deterministic
in-memory serialization with canonical ordering, stable-ID instance sorting,
three-digit cells, LF/final newline, escaping, `%.17g`, and negative-zero `0`.

**Tests:** Added the dedicated `test-scene-format` runner. Its 11 tests cover exact
canonical bytes and fixed-point round trips, CRLF/reordered input, transactional
rejection, required/duplicate/unknown/syntax/numeric/dimension/ID/high-water
diagnostics, wall and floor placement, exhausted sentinel, locale non-mutation,
quoted provenance, embedded NUL, and file limits.

**Review and recovery:** Focused review found missing support for metadata after
sections and missing control-byte validation for programmatic provenance. Both were
added. The first ordering fix destructively split section fields and produced 3/11
passes; a read-only metadata-key probe replaced it, after which all tests passed.

**Verification:** Strict focused tests passed 11/11. Combined AddressSanitizer and
UndefinedBehaviorSanitizer with leak detection passed 11/11. The strict application
build and existing scene-document, command-system, and unified-editor compatibility
runners passed.

### Increment 4 — Import and document transactions

**Work:** Added explicit transactional native load and legacy import entry points on
`SceneDocument`. Native load performs bounded byte-exact reading, strict format
parse/validation, path preparation, and complete authored ownership transfer only
after all fallible work succeeds. Legacy import reuses the current character-grid
parser unchanged, captures active default material and ambient configuration,
retains provenance, has no native destination, and commits dirty/unsaved.

**Tests:** Expanded `test-scene-document` to 22 tests. New coverage verifies complete
native commit, native malformed-input rollback, exact legacy non-digit/ragged-row
mapping, captured defaults, unchanged source bytes, provenance/path/dirty state,
failed import rollback, embedded-NUL rejection, and the 2 MiB pre-allocation bound.

**Review and recovery:** Focused review found that native file load initially passed
`strlen` to the parser and allocated before applying the file bound. The read seam now
returns the exact byte count, rejects oversize input before allocation, and lets the
strict parser observe embedded NUL bytes. Compatibility test source groups now link
the diagnostic and format modules because document load invokes them.

**Verification:** Strict scene-document tests passed 22/22 and the sanitized runner
passed 22/22 with leak detection. Strict scene-format 11/11, command-system 17/17,
unified-editor 32/32, and the application build all passed.

### Increment 5 — Asset repair, fallback, and Save blocking

**Work:** Added a bounded reusable `DecalPatternAsset` registry owned by
`AssetRegistry`, with copy-in lookup APIs and cleanup integrated into registry
lifecycle. The existing decal files are now also loaded as pattern-only reusable
definitions; their legacy placement path remains unchanged until Increment 6. Added
transactional native load with an explicit registry resolution seam. Structurally
valid scenes commit when decal pattern IDs are absent, retain the authored typed
`SceneAssetRef` unchanged, and own at most 256 warning diagnostics with permanent ID
`TSG-SCENE-INPUT-0011`.

**Derived behavior:** Added a process-lifetime conspicuous 2 by 2 checker/`!`
fallback and document lookup that returns either the loaded reusable pattern or that
fallback without mutating or serializing the authored reference. Added an internal
replacement operation that accepts only loaded decal-pattern IDs, recomputes repair
state, and marks an actual authored replacement dirty. Registry availability can
also clear a stale missing-reference condition without manufacturing an authored
edit.

**Save boundary:** Added `SCENE_SAVE_REPAIR_BLOCKED` and structured Save validation.
Any unresolved reference blocks normal Save before file creation and reports
`TSG-SCENE-INPUT-0012`; there is no force-save path. The temporary legacy writer is
otherwise unchanged pending Increment 7.

**Tests:** Expanded `test-scene-document` to 26 tests. New deterministic coverage
checks registry ownership and bounds, all references loaded, one and multiple missing
references, the 256-diagnostic bound and order, unchanged typed IDs, stable visible
fallback identity, loaded-pattern resolution, Save blocking without dirty-state
mutation, rejected unloaded replacement, availability refresh, explicit replacement,
dirty/unblocked state, and failed native-load rollback preserving repair ownership.

**Review and recovery:** The first integration build exposed that the command-system
test source group did not link the newly required asset registry; its narrow source
group was corrected. The first handwritten decal fixture used the wrong section name
and failed 2 new tests; changing it to the specified `[decal_instance <id>]` grammar
restored the suite. Review also added explicit reusable-pattern dimensions and changed
repair refresh from deleting one diagnostic to deterministic recomputation against
the current registry.

**Verification:** Strict scene-document tests passed 26/26. Strict scene-format
11/11, command-system 17/17, unified-editor 32/32, and the full application build
passed. Combined AddressSanitizer and UndefinedBehaviorSanitizer with leak detection
passed the 26/26 scene-document suite.

### Increment 6 — derived runtime adapters and authored ownership

**Implemented:** Added a transactional `SceneDocument` to `WorldState` adapter for
spawn, authored ambient intensity, lights, and decals. Runtime decals deep-copy
resolved reusable patterns; missing references receive the built-in fallback while
their authored `SceneAssetRef` remains unchanged. A separate resolved-decal insertion
path bypasses legacy coordinate migration, so native zero depth and ceiling Z values
are not rewritten. `UnifiedEditorState` owns this disposable runtime view and editor
lighting/raycast consumers now use it rather than the unrelated play-world state.
Lighting uses scene-authored ambient only when the runtime view explicitly carries it,
preserving configuration ambient for legacy play paths. Editor document/runtime loads
are staged together and committed only after both succeed.

**Tests:** Added deterministic runtime derivation coverage for authored ambient/spawn,
light/decal counts, loaded-pattern deep copies, visible missing-pattern fallback,
unchanged authored references, no native placement migration, and transactional
runtime rebuild failure. Added lighting coverage proving authored ambient overrides
configuration only on the derived runtime path. Existing editor compatibility tests
continue to prove camera and selection behavior remains unchanged.

**Review and recovery:** The first editor integration applied scene spawn lazily during
the next update, which changed established camera behavior and failed 6/32 unified
editor tests. The behavior was removed pending the native workflow seam in Increment
8; spawn is nevertheless present and verified in the derived runtime model. Review
also caught that using `world_add_decal()` would apply legacy migration to native
placement, and that loading directly into the live document could leave document and
runtime out of sync after a runtime allocation failure. Both were corrected before
acceptance with the no-migration insertion API and staged document/runtime commit.

**Verification:** Strict scene-document tests passed 28/28, lighting 4/4,
unified-editor 32/32, and the full application build passed. Combined
AddressSanitizer and UndefinedBehaviorSanitizer with leak detection passed both the
28/28 scene-document and 4/4 lighting suites.

## Verification ledger

| Increment | Strict build | Focused tests | Failure tests | Documentation | Result |
|---|---|---|---|---|---|
| 1 — specification | N/A | focused consistency checks pass | N/A | complete | Pass |
| 2 — domain/diagnostics/IDs | pass | 17/17 + 17/17 + 32/32; sanitized 17/17 | ID exhaustion/null paths pass | updated | Pass |
| 3 — native v1 format | pass | 11/11; sanitized 11/11 | malformed/limit/transaction cases pass | updated | Pass |
| 4 — import/document transactions | pass | 22/22 + 11/11 + 17/17 + 32/32; sanitized 22/22 | load/import rollback and byte bounds pass | updated | Pass |
| 5 — asset repair/fallback | pass | 26/26 + 11/11 + 17/17 + 32/32; sanitized 26/26 | missing refs, rollback, cap, Save block pass | updated | Pass |
| 6 — derived runtime | pass | 28/28 + lighting 4/4 + unified editor 32/32; sanitized 28/28 + 4/4 | runtime rebuild rollback and compatibility pass | updated | Pass |
| 8A — typed editor workflows | pass | 32/32 + 33/33 + 11/11 + 17/17; sanitized scene document 32/32 | New/Open/Import/Save As rollback and identity pass | updated | Pass |
| 8B — chooser, Save As, close seam | pass | 33/33 + 5/5 + 7/7 + 17/17; sanitized 33/33 + 32/32 | native/legacy chooser, Save As, overwrite, dirty close, input labels pass | updated | Pass |

## Failures and recovery notes

- The first Increment 1 patch updated the dependency table but matched a different
  roadmap occurrence than the detailed R2 status/Next Action text. Review of the
  rendered file caught and corrected the stale pre-approval wording before code work.
- The first v1 specification said `next_instance_id` must exceed all IDs while also
  allowing `UINT64_MAX` exhaustion. It now reserves `UINT64_MAX` as a never-allocated
  sentinel, making `UINT64_MAX - 1` the largest allocatable ID.
- The first bounded documentation check returned success but its expected Python
  message was not visible in captured output. A narrower check was run instead and
  printed all four boolean results explicitly; no file changes occurred between the
  checks.
- Increment 3 review found metadata ordering and provenance control-byte gaps. The
  first correction exposed a destructive property-probe bug (3/11 passed); replacing
  it with a read-only probe restored the full 11/11 pass.
- Increment 4 review found hidden trailing bytes after NUL and late file-size
  enforcement in the first load seam. Exact byte counts and pre-allocation bounds
  corrected both before the increment was accepted.
- Increment 5 integration initially failed at link time because a focused test source
  group omitted `assets.c`; the dependency was added without broadening production
  interfaces. Two new tests then rejected a handwritten non-spec section name; the
  fixture was corrected rather than weakening the strict parser.
- Increment 6 initially applied authored spawn during the next editor update and used
  the legacy decal insertion function. Compatibility failures and code review led to
  removal of the premature camera mutation, addition of a no-migration runtime insert,
  and transactional staging of document plus runtime ownership.
- Increment 8A's first strict compile referenced an unexported asset-ID limit name;
  the New boundary now applies the locked v1 maximum directly. Broad verification
  was also first invoked with nonexistent convenience target names; the actual
  bounded runner targets and `all` target were then used successfully.
- Increment 8B's first attempt mapped Ctrl+O to the native chooser, which broke the
  legacy test that expected Ctrl+O to open the existing map chooser. The input seam
  was restored to route Ctrl+O to the legacy chooser; explicit Ctrl+I opens the
  legacy import chooser, and Ctrl+Shift+O will open the native chooser via a future
  menu action. The first Save As implementation also left helper functions unused
  under strict warnings; wiring them into modal confirm and text-input paths fixed
  the build.
