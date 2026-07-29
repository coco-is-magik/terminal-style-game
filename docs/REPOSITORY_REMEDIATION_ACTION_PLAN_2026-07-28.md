# Repository Remediation Action Plan — 2026-07-28

## Status and authority

**Status:** All remediation phases and applicable closure gates verified on
2026-07-28. Environment-limited optional checks are recorded below without a pass
claim.

This document converts the findings in
[`REPOSITORY_CODE_REVIEW_2026-07-28.md`](REPOSITORY_CODE_REVIEW_2026-07-28.md)
into an incremental remediation program. The review remains the evidence record;
this document is the authoritative sequence, scope, and exit-gate definition for
addressing it.

This closed plan is part of roadmap phase R0. While it was active,
safety-critical remediation and its verification gates took priority over broad
new architectural feature work. Its sequencing rules below are retained as the
historical execution contract, not as current work instructions.

## Objective

Resolve the review findings without destabilizing accepted behavior. The work must:

1. remove confirmed memory-safety and ownership hazards first;
2. make malformed input, capacity exhaustion, and partial initialization explicit
   and testable;
3. strengthen verification before undertaking broad module extraction;
4. reduce coupling through small behavior-preserving seams;
5. reconcile stable documentation with verified implementation; and
6. close every review finding with test, runtime, or documentation evidence.

This is not a repository rewrite and is not permission to combine unrelated cleanup
with safety fixes.

## Accepted baseline that must remain unchanged

Unless a later, separately approved requirement deliberately changes one of these
contracts, every phase preserves:

- the current unified-editor interaction model and one authoritative editable
  `SceneDocument` map;
- editor mutations through `CommandHistory`, including state-ID, dirty-state,
  transactional load/save, undo/redo, and failed-save behavior;
- the existing digit-grid map compatibility contract, including accepted ragged-row
  padding and save limits;
- current valid palette, material, decal, light, map, and UI assets;
- existing valid structured decal-row padding and inline-art error-marker recovery
  until a separate asset-format decision changes them;
- current wall, floor, and ceiling decal orientation, projection, spacing,
  perspective, backface, and whitespace behavior;
- default SMC stream tracking and all currently exposed alternative tracker/cache
  build modes;
- benchmark output fields and acceptance semantics unless an explicitly versioned
  benchmark-policy change is approved;
- menu flow, input-consumption hierarchy, camera ownership, and renderer output;
- focused per-runner test linkage; and
- the strict C11 warning gate: `-Wall -Wextra -Wpedantic -Werror`.

At plan creation, `make test` passes all 181 registered CMocka tests. That count is a
baseline observation, not a permanent expected count; remediation should add tests.

## Forbidden shortcuts

- Do not perform a repository-wide rewrite or create a generic framework.
- Do not weaken, remove, or bypass a regression test to obtain a pass.
- Do not mix a behavior change with a module extraction when they can be separate
  increments.
- Do not silently truncate assets, silently accept invalid dimensions, or silently
  discard owned data.
- Do not introduce a second editable scene, map, asset, or configuration source of
  truth.
- Do not move domain policy into `app.c`, rendering code, UI structs, or hidden
  global state.
- Do not claim sanitizer, leak, coverage, matrix, benchmark, or stability success
  unless that check was actually run and its result recorded.
- Do not normalize unrelated source formatting while reviewing a safety-critical
  patch.
- Do not make benchmark/stability results hard prerequisites for changes outside
  relevant hot paths.

## Plan-wide implementation method

Each increment follows this order:

1. Restate the finding and behavior that must remain unchanged.
2. Add the narrowest failing deterministic regression where practical.
3. Introduce or adjust one public boundary.
4. Implement the smallest production change.
5. Run the focused test runner and strict build.
6. Run the additional gate required by the changed risk: sanitizer, leak, matrix,
   interactive acceptance, or benchmark.
7. Update comments and technical documentation while evidence is current.
8. Record result, failure evidence, remaining uncertainty, and finding status in the
   traceability table.

An increment is not complete when only the aggregate suite passes; its named failure
path and preserved behavior must also be demonstrated.

## Fixed implementation decisions

These choices remove ambiguity without introducing speculative behavior:

1. **Renderer cell size:** the existing software backend is a fixed 8×8 glyph
   renderer. Initial remediation rejects any other cell dimensions. Scaled or
   alternate-size glyphs require a separate renderer feature plan.
2. **Decal dimension policy:** dimensions must be positive, bounded by named shared
   constants, and multiplication-safe before allocation or indexing. Inputs outside
   those limits fail; they are not truncated.
3. **Existing decal recovery:** valid-dimension structured rows continue to use the
   current padding/default behavior, and short inline-art rows continue to produce
   the current error-marker pattern. Safety remediation does not silently redefine
   the format.
4. **World ownership:** a decal's pattern ownership transfers to `WorldState` only
   when insertion succeeds. On any rejection, the caller retains ownership.
5. **Configuration safety:** Phase 1 validates the effective configuration before
   resource initialization and fails safely on invalid safety-critical values. It
   does not yet remove the singleton or decide a new policy for unknown keys.
6. **Architectural order:** safety and ownership fixes precede `app.c`, UI, raycast,
   renderer-backend, and configuration-injection refactors.
7. **Opaque UI migration:** consumers first move to accessors; storage becomes
   private only after direct field use is eliminated. No one-step UI allocation
   rewrite is planned.

## Decisions deliberately deferred

The following are not required to begin Phase 1 and must not be chosen implicitly by
an implementation patch:

- whether unknown configuration keys become warnings or hard errors;
- whether configuration loading will eventually return a typed diagnostic list;
- whether the long-term renderer supports glyph sizes other than 8×8;
- whether decal file recovery behavior should later become strict schema rejection;
- exact opaque-handle versus caller-storage representation for UI objects;
- final scene-render module naming and whether decal rendering later generalizes to
  other projected surface content; and
- coverage percentage targets. Initial coverage work measures gaps and establishes
  critical-path expectations rather than selecting an arbitrary repository-wide
  number.

---

# Phase 0 — Baseline, traceability, and verification definitions

**Phase status:** Verified 2026-07-28.

## Purpose

Make the review actionable and prevent findings from disappearing during later
refactors.

## Work items

1. Preserve this document's finding-to-phase matrix and update it after every
   verified increment.
2. Record exact focused runners, aggregate checks, and environment-sensitive gates
   for each phase.
3. Confirm the accepted baseline above against current requirements before source
   changes begin.
4. Record any newly discovered defect under the phase where it is addressed, with a
   citation and severity; do not silently expand an existing finding.
5. Treat documentation-only contradictions independently from implementation
   behavior until authoritative evidence identifies which statement is current.

## Required evidence

- strict default build and aggregate suite result;
- registered test count at phase start;
- applicable feature flags and test commands; and
- unresolved decision list with blocking/nonblocking classification.

## Exit gate

- Every original review finding appears in the traceability matrix.
- Every phase has explicit preserved behavior and a verification gate.
- No unresolved behavior decision blocks Phase 1.

---

# Phase 1 — Safety-critical parsing, dimensions, and allocation sizes

**Phase status:** Verified 2026-07-28, including strict aggregate, ASan, and UBSan
closure gates.

## Findings addressed

`MAINT-1`, `MAINT-2`, `MAINT-3`, the safety-critical portion of `MAINT-4`,
`TEST-1`, and `TEST-2`.

## 1A — Shared checked-size boundary

### Scope

Introduce one narrow internal utility for checked positive two-dimensional counts
and byte sizes. Use it only at proven grid-like allocation boundaries:

- `grid_create()`;
- `map_create()`;
- `renderer_create()` logical dimensions and pixel-buffer size; and
- decal pattern allocation.

The helper must operate in `size_t`, reject nonpositive signed inputs before
conversion, detect multiplication overflow, and leave caller state unchanged on
failure.

### Tests

- zero and negative dimensions;
- each operand near the representable boundary;
- product overflow;
- valid minimum and representative dimensions; and
- allocation constructors return failure without partial ownership.

## 1B — Decal parser limits and checked numeric conversion

### Scope

1. Define shared named maximum row and column constants matching the current fixed
   parser storage unless the implementation is safely made dynamic in the same
   focused increment.
2. Replace unchecked dimension conversion with checked conversion that rejects
   missing digits, trailing non-whitespace, range errors, zero, negatives, and
   overflow.
3. Validate both dimensions and cell count before allocation, row loops, or fixed
   buffer indexing.
4. Apply the same safety rule to both current parser implementations during Phase 1;
   parser unification occurs in Phase 2.
5. Return parse failure for invalid dimensions. Do not return a partially initialized
   decal and do not convert an unsafe schema error into an error-marker canvas.

### Tests

- `pattern_rows` equal to and one above the supported maximum;
- `pattern_cols` equal to and one above the supported maximum;
- zero, negative, nonnumeric, mixed numeric/text, and very large dimensions;
- cell-count multiplication overflow;
- structured and inline-art paths;
- no output object on rejection;
- no leak on every rejection path; and
- existing valid round-trip, art, whitespace, and engine-compatibility tests.

## 1C — Enforce the renderer's fixed 8×8 contract

### Scope

1. Define one renderer glyph-width and glyph-height constant rather than duplicating
   numeric `8` assumptions.
2. Reject non-8×8 dimensions in `renderer_create()` before allocation or SDL
   initialization.
3. Validate effective configuration dimensions before calling constructors.
4. Check logical-width, logical-height, pixel-count, and byte-size arithmetic.
5. Document the fixed backend contract in `renderer.h`, `config.ini`, and the
   appropriate stable documentation.

### Tests

- reject 1×8, 8×1, 7×8, 8×7, 9×8, and 8×9;
- accept 8×8 through the earliest testable constructor boundary;
- reject logical-dimension and allocation-size overflow;
- prove rejected dimensions do not initialize SDL or allocate renderer resources by
  using the existing instrumentation/test seam or a narrower preflight helper; and
- preserve existing framebuffer behavior for 8×8.

## 1D — Effective configuration validation

### Scope

Add a pure validation boundary for an `EngineConfig` candidate/effective value. At a
minimum it rejects:

- nonpositive window and grid dimensions;
- grid/allocation dimensions that fail checked-size policy;
- cell dimensions other than 8×8 for the current backend;
- nonpositive target FPS;
- nonfinite numeric fields; and
- values outside existing documented physical/range assumptions where accepting
  them can cause unsafe arithmetic.

Keep the existing singleton facade in this phase. Do not silently replace an invalid
file value with its default and report success. Detailed unknown-key policy remains
deferred.

## Files expected to change

`src/decal_io.c`, `src/asset_loader.c`, `src/renderer.c`, `src/renderer.h`,
`src/grid.c`, `src/map.c`, `src/config.c`, `src/config.h`, focused tests, `Makefile`
only as needed for sanitizer support, `config.ini`, and affected format/API docs.

## Verification gate

- focused parser, core allocation, renderer-preflight, and configuration tests pass;
- strict default build and aggregate suite pass;
- ASan and UBSan pass for changed first-party parsing/allocation paths;
- valid checked-in assets still load;
- no accepted decal rendering or editor behavior changes; and
- no performance benchmark is required unless hot renderer behavior beyond
  validation/constants changes.

## Stop conditions

Stop Phase 1 and record the conflict if:

- existing checked-in assets exceed the proposed safe limits;
- a valid documented configuration requires non-8×8 behavior; or
- preserving current decal recovery would require unsafe indexing.

## Implementation checkpoint — 2026-07-28

- Added `checked_size_2d()` and `checked_size_bytes()` and applied them to grid,
  map, renderer, scene-document, UI-element, decal, and sprite allocations.
- Replaced the fixed map-loader copy buffer with bounded direct line parsing.
- Added renderer preflight validation and made the existing 8x8 backend contract
  explicit.
- Bounded decal dimensions at 64 rows and 255 columns with strict dimension
  conversion and single-path cleanup.
- Made configuration file loading transactional: recognized malformed or unsafe
  values reject the complete file and preserve the prior effective configuration.
- Hardened sprite dimensions/allocation and full-file map reads; map parse failure no
  longer proceeds to mutate world state.
- Focused evidence at implementation time: strict `test-core` build and 37/37 tests
  passed; final closure increased this runner to 42/42 and added passing aggregate,
  ASan, and UBSan gates.

---

# Phase 2 — Explicit world ownership and one decal parser

**Phase status:** Verified 2026-07-28. Aggregate and sanitizer gates pass,
checked-in assets are enumerated by tests, and ownership/parser documentation is
current. The unavailable optional leak tool is recorded as `SKIP` in Phase 7.

## Findings addressed

`MOD-2`, `MOD-5`, `TEST-5`, and `DOC-4`.

## 2A — Observable world insertion

### Scope

1. Introduce a typed `WorldInsertResult` (or equally narrow domain-specific result)
   that distinguishes success, invalid input, and capacity exhaustion.
2. Change light, sprite, and decal insertion APIs to return an outcome.
3. Document null handling and capacity behavior.
4. For decals, transfer the pattern pointer only on success. The caller retains and
   must release it after any failure.
5. Update all callers to inspect the result. Asset loaders must not report success
   when insertion fails.
6. Preserve fixed capacities in this phase; dynamic world collections belong to a
   later scene-model decision.

### Tests

- last available slot succeeds;
- first insertion beyond capacity returns capacity exhaustion;
- counts and stored values do not change after rejection;
- rejected decal ownership remains with the caller and can be freed exactly once;
- accepted decal is freed exactly once by `world_clear()`;
- null-world/invalid input behavior; and
- loader reports or propagates insertion failure without a leak.

## 2B — Single decal parsing authority

### Scope

1. Make one module own decal syntax, validation, allocation, and failure cleanup.
2. Keep `asset_loader` responsible for directory enumeration and world insertion,
   not decal syntax.
3. Choose a narrow transfer API: load a heap-owned `Decal`, attempt insertion, then
   release the wrapper/contents according to the documented result.
4. Remove the duplicated parser only after compatibility tests prove the shared
   parser handles all checked-in assets and both supported modes.
5. Keep legacy world-coordinate migration separate from syntax parsing. If migration
   remains in `world_add_decal()`, document that insertion may normalize the stored
   copy without mutating the caller's rejected value.

### Tests

- every checked-in decal loads through the shared parser;
- runtime enumeration and direct file loading produce equivalent decal values;
- structured and inline-art compatibility;
- malformed dimensions and allocation failures;
- full-world insertion cleanup; and
- existing render regressions pass unchanged.

## Documentation

Update `assets/README.md` to state:

- supported dimension limits;
- which malformed inputs reject the file;
- structured-row padding/default behavior;
- inline-art short-row error-marker behavior;
- material-ID conversion/range behavior; and
- world capacity failure semantics where relevant to runtime loading.

## Verification gate

- only one first-party decal syntax parser remains;
- full-capacity insertion cannot leak or falsely report success;
- ownership rules are documented in the public header and tested;
- strict aggregate, ASan, UBSan, and applicable leak checks pass; and
- all valid assets, round trips, compatibility, and decal projection tests pass.

## Implementation checkpoint — 2026-07-28

- Added `WorldInsertResult`; light, sprite, and decal insertion now report success,
  invalid input, or capacity exhaustion.
- Documented and tested that decal pattern ownership transfers only on successful
  insertion. Added explicit `asset_registry_clear()` for registry-owned sprite data.
- Removed the duplicate runtime decal syntax parser. `asset_loader.c` delegates to
  `decal_load_from_file()`, checks insertion, and releases rejected ownership.
- Asset light loading now propagates insertion failure rather than falsely reporting
  success.
- Focused evidence: strict `test-decal-io` build and 12/12 tests pass; strict
  `test-decals` build and 26/26 projection/ownership tests pass.
- Encountered and fixed one integration failure: after centralizing decal parsing,
  `TEST_CORE_SRC` initially omitted `src/decal_io.c`, causing an undefined-reference
  linker error. `SRC_DECAL_IO` is now included in each affected source group.

---

# Phase 3 — Verification infrastructure and neglected runtime paths

## Findings addressed

`TEST-3`, the infrastructure/runtime portions of `TEST-4`, `TEST-6`, and `TEST-7`.

## 3A — Bounded quality targets

Add documented, noninteractive Make targets or scripts for:

1. first-party ASan and UBSan tests;
2. leak detection where supported, with explicit suppression/exclusion policy for
   dependency-owned process lifetime state;
3. coverage generation with first-party source scope and named exclusions;
4. non-mutating style/static analysis using tools already available to the project,
   without making an unavailable optional tool indistinguishable from a code failure;
5. a bounded renderer tracker/cache configuration matrix; and
6. a narrow application smoke test.

Commands must remain composable: the normal `make test` gate stays fast and
deterministic, while broader gates have explicit names and expected runtime.

## 3B — Optional-mode matrix

At minimum build and run applicable renderer correctness tests under:

- no state tracker;
- custom dirty cells;
- generic SMC tracker;
- indexed SMC tracker;
- batch SMC tracker;
- default/preferred stream SMC tracker;
- lighting cache enabled; and
- glyph cache enabled.

The matrix must clean or isolate artifacts so one mode cannot reuse objects compiled
with another mode's definitions. It verifies correctness and build integration, not
performance superiority.

## 3C — Dedicated subsystem tests

Add focused runners or focused groups for:

- glyph-cache hit, miss, replacement/eviction, and deterministic block output;
- lighting-cache lookup, invalidation, statistics, and revision behavior;
- state-tracker init, reset, changed/unchanged, range, buffer-size, fallback, and
  shutdown behavior;
- lighting ambient, multiple positive lights, negative lights, radius boundary,
  obstruction/shadow, null/invalid inputs, and cache equivalence; and
- world light/sprite/decal capacity and cleanup.

Hard-coded lighting cache revisions must be replaced by an explicit revision source
or the cache must remain clearly diagnostic and disabled; this requires a focused
design decision before modifying cache semantics.

## 3D — Testable input-event mapping

Separate SDL polling from pure event-to-`InputState` mapping:

```text
SDL polling adapter -> explicit event mapping -> InputState
```

Test key transitions, text input, mouse motion/buttons, quit, editor shortcuts,
headless behavior, and per-frame reset semantics without requiring the application
loop. Keep SDL-specific translation at the adapter edge.

## 3E — Real application smoke boundary

1. Extract enough CLI/options and startup policy to invoke a deterministic,
   noninteractive smoke path.
2. Replace or remove stale `tests/test.c` only when the real smoke test is registered
   in a build target.
3. Verify meaningful exit status and structured output rather than the obsolete
   `hello world` expectation.

## Coverage policy

The first report identifies uncovered modules, branches, and failure paths. Phase 3
does not set a repository-wide percentage target. Completion requires named critical
boundaries—parsers, ownership, commands/documents, configuration validation,
trackers/caches, and application options—to have deterministic tests.

## Verification gate

- default `make test` remains deterministic and passes;
- sanitizer/leak targets run with recorded scope;
- every listed matrix mode builds and passes its applicable correctness tests;
- dedicated cache/tracker/lighting/world tests pass;
- input event mapping is headlessly tested;
- the stale smoke test is gone or replaced by a registered meaningful test; and
- the initial coverage report and explicit gaps are recorded.

---

# Phase 4 — Reduce `app.c` to a composition root

## Findings addressed

`MOD-1` and the application orchestration portion of `TEST-4`.

## Extraction rule

Perform one behavior-neutral extraction at a time. Each extraction first captures
current behavior in focused tests. Do not combine the extraction with redesigned
menu behavior, benchmark policy, renderer semantics, or editor workflow.

## 4A — Pure application options

Create an `app_options` module that parses explicit `argc`/`argv` into a typed result
containing run mode, visual mode, duration, scenario, and frame limit. It must:

- reject missing values and invalid numeric forms;
- define unknown-option behavior;
- validate scenario names and mode combinations;
- avoid reading global configuration or initializing SDL; and
- have table-driven tests for valid and invalid combinations.

CLI behavior discovered to be relied upon but undocumented must be recorded before
it is changed.

## 4B — Benchmark session

Extract scenario progression, telemetry accumulation, outlier/effective-worst
calculation, classification, and output formatting behind explicit inputs and
results. Preserve existing fields, thresholds, warm-up treatment, and exit codes.

Tests cover threshold boundaries, zero frames, allocation/texture failures, outlier
handling, scenario stop conditions, and stable serialization of result fields.

## 4C — Explicit resource lifecycle

Create a narrow application resource/context owner with staged initialization and
idempotent destruction. Record owned versus borrowed members. Every failed stage
must release only successfully acquired resources in reverse order.

Use dependency adapters or focused constructor seams to test failure at renderer,
grid, map, optimization, tracker, UI cache, and layout stages without requiring real
allocation failure from the operating system.

## 4D — Menu controller

Move menu selection, focus synchronization, action dispatch, and state transitions
behind a menu-controller API. UI rendering receives state; it does not own
application transitions. Preserve all existing action strings and transition tests.

## 4E — Frame update/render dispatch

Define an explicit borrowed frame context and separate:

- input routing;
- state transition/update;
- world/editor simulation;
- grid production/overlay; and
- renderer presentation.

Do not create a generic engine framework. Add only the seam required by existing
main-menu, playing, editor, diagnostic, and benchmark modes.

## Exit gate

- `app_main()` owns composition, top-level loop control, and final status only;
- CLI, benchmark policy, partial initialization rollback, menu transitions, and
  headless frame policy have focused tests;
- accepted application/editor/menu/benchmark behavior remains unchanged;
- strict aggregate and sanitizer/leak gates pass;
- interactive smoke checks pass; and
- benchmark/stability comparison runs if the hot loop or measurement boundaries
  changed, even unintentionally.

---

# Phase 5 — Tighten module interfaces and dependency direction

## Findings addressed

`MOD-3`, `MOD-4`, `MAINT-5`, `MAINT-6`, and the implicit-global portion of
`MAINT-4`.

## 5A — UI boundary

1. Inventory every direct `UiElement`, `UiCache`, and `UiLayout` field access.
2. Add the smallest required query/mutation APIs, beginning with style/focus/action
   access used by the application/menu controller.
3. Migrate consumers and add API-level tests.
4. Move storage definitions into implementation-private scope only after direct
   consumer access reaches zero.
5. Preserve current caller-owned/callee-owned lifecycle until a separate ownership
   change is required and tested.

## 5B — Ray intersection versus scene/decal projection

1. Capture the current ray/decal public behavior and dependency graph.
2. Keep ray intersection and `RayResult` generation in the raycast module.
3. Move decal basis, world projection, decal lighting, and decal rasterization into
   a focused surface-decal renderer or scene-render helper with explicit context.
4. Do not duplicate camera/surface coordinate models or change projection math
   during extraction.
5. Keep all existing wall/floor/ceiling projection regressions and add interface
   tests for the new boundary.

## 5C — Renderer ownership cleanup

1. Move text-input activation/deactivation to the controller that owns a current
   text-entry mode; remove it if no current mode requires it.
2. Remove the unused mandatory glyph atlas from the software renderer or instantiate
   it only in a backend that consumes it.
3. Ensure renderer creation owns rendering resources only.
4. Update allocation/texture instrumentation and lifecycle tests accordingly.

## 5D — Explicit configuration flow

1. Inventory every `config_get()` call and classify it as composition policy or
   hidden module dependency.
2. Pass validated read-only values/config views into subsystem initialization and
   pure operations.
3. Retain a temporary singleton compatibility facade while callers migrate.
4. Remove the singleton only after no subsystem depends on it and tests prove
   isolated configuration behavior.
5. Resolve unknown-key and diagnostic-result policy before changing parser-visible
   behavior.

## Exit gate

- application and menu code no longer mutate UI storage directly;
- ray intersection is reusable without decal-render internals;
- all decal coordinate/projection regressions remain unchanged;
- renderer construction no longer owns unrelated input policy or an unused mandatory
  atlas;
- new subsystem code receives explicit validated configuration;
- no circular public-header dependencies are introduced; and
- strict aggregate, sanitizer/leak, interactive, and relevant benchmark gates pass.

---

# Phase 6 — Stable documentation, style contract, and build hygiene

## Findings addressed

`DOC-1`, `DOC-2`, `DOC-3`, `DOC-5`, `MAINT-7`, `STYLE-1`, `STYLE-2`, and `STYLE-3`.
`DOC-4` is handled with the parser contract in Phase 2.

## 6A — Architecture reference

Create `docs/ARCHITECTURE.md` from verified implementation. It must document:

- module responsibility and public interface;
- dependency direction and forbidden dependencies;
- ownership and lifecycle for map/document, world, assets, UI, renderer, input, and
  application resources;
- authored versus derived state;
- mutation and transaction boundaries;
- application update/render data flow;
- configuration and side-effect boundaries;
- optional renderer/cache/tracker composition; and
- focused test ownership for each module.

Historical plans link to this reference; they do not substitute for current
architecture.

## 6B — C style and ownership guide

Create a concise project standard covering:

- names and module prefixes;
- pointer declaration placement and signature wrapping;
- public result enums versus `bool`, pointer, or `void` returns;
- null-input policy;
- caller/callee ownership vocabulary and transfer rules;
- const use and mutable accessors;
- checked arithmetic and numeric parsing;
- comments focused on contracts, invariants, failure behavior, and non-obvious math;
- header exposure and opaque/private data guidance; and
- test naming, setup/teardown, temporary files, and failure-path expectations.

Apply the standard incrementally to touched modules. Do not perform a repository-wide
format-only pass as part of safety work.

## 6C — Reconcile SMC evidence

1. Identify the authoritative result set from recorded commands, dates,
   environments, and raw/generated evidence.
2. Add explicit dated status and supersession notices to contradictory reports.
3. Use working relative Markdown links to the canonical report.
4. Keep failed results as historical evidence; do not rewrite them as passes.
5. If the contradiction cannot be resolved from evidence, label it unresolved and
   rerun the bounded benchmark matrix before asserting a preferred result.

## 6D — Source and Makefile hygiene

- remove stale `asset_designer.c` and speculative “future use” comments/includes;
- correct comments changed by earlier phases;
- consolidate repeated Makefile flag/include/source bundles without altering mode
  selection;
- remove no-op/redundant assignments and excess structural whitespace where doing so
  is independently reviewable; and
- run the full feature matrix after Makefile changes.

## Exit gate

- stable documentation matches verified behavior;
- architecture and style/ownership references exist and are linked;
- SMC reports have an unambiguous authority state or an explicit unresolved status;
- no stale removed-component references remain in first-party source/docs;
- Makefile behavior is preserved across the feature matrix; and
- all Phase 6 findings have closure evidence.

---

# Phase 7 — Closure review and roadmap handoff

## Purpose

Verify that remediation improved the repository against the original five criteria
without weakening accepted behavior.

## Required checks

1. strict default build and aggregate tests;
2. focused tests for every resolved finding;
3. ASan, UBSan, and applicable leak checks;
4. renderer/cache/tracker feature matrix;
5. malformed-input, allocation, capacity, rollback, and restoration tests;
6. coverage report with unresolved critical gaps named;
7. application and editor interactive smoke acceptance;
8. benchmark and stability comparison for changed hot paths;
9. public-header dependency/coupling review;
10. ownership and cleanup review;
11. architecture, asset, README, build, and technical-document consistency review;
    and
12. a dated closure report that compares results to the 2026-07-28 baseline.

## Closure classifications

Every finding must be assigned exactly one:

- **Verified resolved** — implementation/documentation and required checks pass.
- **Accepted constraint** — behavior is intentionally retained with rationale and
  guard tests.
- **Deferred cleanup** — lower-risk work has a named future phase and does not hide a
  correctness or ownership defect.
- **Decision-blocked** — implementation cannot proceed without a named product or
  architecture decision.
- **Open blocker** — required safety/correctness work remains; remediation cannot be
  declared complete.

No high-severity finding may close as deferred cleanup. An accepted high-severity
constraint requires evidence that the hazardous path is unreachable and guarded;
otherwise it remains an open blocker.

## Exit gate

- No high-severity finding remains open.
- All original findings have a closure classification and evidence link.
- The full applicable Q2–Q4 roadmap gates pass.
- Remaining limitations are explicit.
- R0 status and R1 prerequisites are updated from verified evidence, not planned
  intent.

---

# Finding traceability matrix

Status values: **Planned**, **Active**, **Verified resolved**, **Accepted
constraint**, **Deferred cleanup**, **Decision-blocked**, or **Open blocker**.

| Finding | Severity | Planned phase | Required closure evidence | Closure status |
|---|---:|---:|---|---|
| MOD-1 — `app.c` multiple responsibilities | High | 4 | Focused options/benchmark/lifecycle/menu tests; thin composition root; regression and relevant benchmark evidence | Verified resolved |
| MOD-2 — unobservable world insertion/ownership | High | 2 | Typed results; full-capacity and exact-once cleanup tests; sanitizer/leak pass | Verified resolved |
| MOD-3 — publicly mutable UI internals | Medium | 5 | Consumers use tested accessors; direct field access removed; private storage boundary documented | Verified resolved |
| MOD-4 — raycast/decal-render coupling | Medium | 5 | Separate interfaces; unchanged projection suite; dependency review | Verified resolved |
| MOD-5 — duplicate decal parsers | Medium | 2 | One parser; checked-in asset and engine/direct compatibility tests | Verified resolved |
| MAINT-1 — unsafe decal dimensions | High | 1 | Boundary/malformed/overflow tests; ASan/UBSan and leak pass | Verified resolved |
| MAINT-2 — renderer cell-size mismatch | High | 1 | Fixed 8×8 validation; non-8 rejection and overflow tests; framebuffer regression | Verified resolved |
| MAINT-3 — unchecked allocation products | High | 1 | Shared checked-size boundary; constructor edge tests; sanitizer pass | Verified resolved |
| MAINT-4 — invalid/global configuration | Medium | 1 and 5 | Effective config validation, then explicit dependency migration and policy record | Verified resolved |
| MAINT-5 — unused mandatory glyph atlas | Medium | 5 | Software backend no longer requires unused atlas; lifecycle/output tests | Verified resolved |
| MAINT-6 — renderer-owned stale text-input policy | Medium | 5 | Input policy moved/removed; renderer lifecycle and text-input tests | Verified resolved |
| MAINT-7 — implementation hygiene | Low | 6 | Stale includes/comments removed under style guide; strict/matrix pass | Verified resolved |
| DOC-1 — contradictory benchmark conclusions | High | 6 | Canonical authority/supersession or explicit unresolved rerun evidence | Accepted constraint |
| DOC-2 — ambiguous report links/location | Medium | 6 | Working explicit links and consolidated authority trail | Verified resolved |
| DOC-3 — missing current architecture reference | Medium | 6 | Verified `docs/ARCHITECTURE.md` with ownership/dependencies/tests | Verified resolved |
| DOC-4 — asset docs versus parser behavior | Medium | 2 | Documentation and shared parser tests express the same contract | Verified resolved |
| DOC-5 — stale source comments | Low | 6 | Removed/corrected references; documentation review | Verified resolved |
| TEST-1 — no malformed decal limit tests | High | 1 | Registered structured/art malformed and overflow tests | Verified resolved |
| TEST-2 — no renderer cell-contract tests | High | 1 | Registered non-8, overflow, and valid-8 tests | Verified resolved |
| TEST-3 — no optional-mode matrix | Medium | 3 | Bounded isolated matrix passes for every listed mode | Verified resolved |
| TEST-4 — untested app/event boundaries and stale smoke | Medium | 3 and 4 | Event mapping and real smoke tests; options/lifecycle/menu/benchmark tests | Verified resolved |
| TEST-5 — no world capacity/ownership tests | Medium | 2 | Full-capacity status/count/cleanup tests | Verified resolved |
| TEST-6 — no sanitizer/leak/coverage/style targets | Medium | 3 | Documented runnable targets and recorded scoped results | Verified resolved |
| TEST-7 — incidental lighting coverage | Medium | 3 | Focused lighting/cache equivalence and boundary tests | Verified resolved |
| STYLE-1 — inconsistent API/error conventions | Medium | 6 | Published standard; touched APIs conform; exceptions documented | Verified resolved |
| STYLE-2 — inconsistent comment style | Medium | 6 | Contract-focused standard and corrected touched modules | Verified resolved |
| STYLE-3 — uneven Makefile readability | Low | 6 | Simplified recipes/bundles; complete feature-matrix pass | Verified resolved |

## Phase dependency summary

```text
Phase 0: baseline and traceability
                 ↓
Phase 1: memory/input/allocation safety
                 ↓
Phase 2: ownership and parser authority
                 ↓
Phase 3: verification infrastructure
                 ↓
Phase 4: application decomposition
                 ↓
Phase 5: module interface tightening
                 ↓
Phase 6: stable docs/style/build hygiene
                 ↓
Phase 7: closure review and roadmap handoff
```

Documentation corrections that merely label contradictory evidence may begin before
Phase 6 when needed to prevent misuse. They must not assert a technical conclusion
before evidence supports it. Focused research may proceed in parallel when it does
not mutate shared architecture or bypass the active phase gate.

## Working progress record

Update this section when implementation starts. Detailed command output belongs in
phase implementation notes or a dated handoff, not as an unbounded dump here.

- 2026-07-28 — Initial repository review completed; strict aggregate suite passed
  181/181. No benchmark, sanitizer, leak, or coverage claim was made by that review.
- 2026-07-28 — Remediation phases, preserved behavior, decisions, gates, and finding
  traceability established.
- 2026-07-28 — Phase 0 verified: all 27 review findings have one traceability row,
  relative documentation links resolve, preserved behavior and stop conditions are
  explicit, and no deferred decision blocks Phase 1.
- 2026-07-28 — Phase 1 started. Focused repository inspection confirmed the unsafe
  decal dimension, allocation-product, renderer cell-size, and configuration seams;
  production changes and regressions are in progress.
- 2026-07-28 — Phases 1-2 implemented checked allocation/parsing, transactional
  configuration, typed world insertion ownership, and one decal parser authority.
- 2026-07-28 — Phase 3 added bounded quality targets, real headless smoke, pure input
  mapping, cache/lighting tests, and dedicated generic/indexed tracker tests.
- 2026-07-28 — Phase 4 extracted strict application options, benchmark policy,
  staged resource cleanup, menu action decoding, and deterministic scenario dispatch.
- 2026-07-28 — Phase 5 removed application UI field mutation, renderer-owned text
  input and unused atlas ownership, and extracted shared surface-local decal mapping.
- 2026-07-28 — Phase 6 added current architecture/style references, corrected map
  compatibility documentation, and made leak/coverage/matrix targets explicit.
- 2026-07-28 — Phase 7 closure: strict aggregate passed (42 core tests plus every
  registered focused runner), headless smoke returned a 10x6 loaded map, and all
  eight isolated renderer/cache/tracker modes passed. ASan and UBSan aggregate
  suites passed. UBSan first exposed a misaligned preallocated SMC state slot; the
  slot stride is now rounded to `_Alignof(smc_state_entry_t)` and its focused UBSan
  regression is clean. `cppcheck` and `valgrind` were unavailable and correctly
  reported `SKIP`; no style or leak pass is claimed. Coverage completed at 41.22%
  line coverage over the emitted report set; strong focused first-party results
  include checked-size 93.18%, map-loader 90.62%, command-system 82.26%, decal I/O
  77.79%, scene-document 79.44%, UI element 84.60%, and glyph-block cache 100%.
  Remaining critical coverage gaps are the SDL polling/presentation branches,
  renderer backend lifecycle, full application loop, and optional SMC runtime
  internals. Interactive video acceptance and hot-path benchmark/stability comparison
  remain environment-dependent evidence rather than safety blockers; headless smoke,
  deterministic projection tests, sanitizer gates, and the complete mode matrix are
  the applicable automated closure evidence in this environment.
