# Repository Code Review — 2026-07-28

> **Follow-up:** The phased implementation sequence, preserved invariants, exit
> gates, and finding traceability are defined in
> [`REPOSITORY_REMEDIATION_ACTION_PLAN_2026-07-28.md`](REPOSITORY_REMEDIATION_ACTION_PLAN_2026-07-28.md).

## Scope and method

This is a read-only audit of the repository's first-party C source, public headers,
build rules, tests, asset-format documentation, and maintained project documents.
The review evaluates modularization, maintainability, documentation, test
completeness, and style consistency. Vendored dependency internals are outside the
code-quality scope, although their build integration and dependency smoke tests are
included.

Evidence was gathered from project files and a clean invocation of `make test` on
2026-07-28. That command compiled with `-std=c11 -Wall -Wextra -Wpedantic -Werror`
and all 181 registered CMocka tests passed. Benchmark and stability targets were not
run because they are environment- and duration-sensitive and are separate from the
default test target (`Makefile:421-443`).

Severity labels used below:

- **High** — memory safety, resource loss, or a major architectural boundary defect.
- **Medium** — material maintainability, correctness, assurance, or documentation gap.
- **Low** — localized consistency or hygiene issue.

## Executive summary

The repository has a sound module-per-concept foundation and particularly strong
boundaries around the unified editor. `SceneDocument` owns editable data,
`CommandHistory` is the declared mutation path, and selection is separated from
controller behavior (`src/scene_document.h:1-22`, `src/command_system.h:1-6`,
`src/editor_selection.h:1-26`). The build enforces strict compiler diagnostics
(`Makefile:1-3`), and the default suite provides substantial behavioral coverage,
especially for document transactions, undo/redo, editor input hierarchy, decal
projection, and decal painter ownership.

The repository does **not** yet meet the stated standard of uniformly strict,
replaceable module interfaces. The largest gaps are:

1. `app.c` is a 772-line composition root that also owns CLI parsing, UI policy,
   simulation routing, benchmark scenarios, telemetry, acceptance policy, and all
   lifecycle cleanup (`src/app.c:226-406`, `src/app.c:560-768`).
2. Two malformed-input/capacity paths can access memory incorrectly or leak owned
   data: decal dimensions are not bounded, and decal insertion cannot report a full
   world (`src/decal_io.c:130-163`, `src/decal_io.c:199-217`,
   `src/asset_loader.c:443-510`, `src/world.c:121-152`).
3. The renderer's public cell-size parameters disagree with its fixed 8x8
   implementation, creating a heap-corruption risk for configured dimensions below
   eight (`src/renderer.c:176-201`, `src/renderer.c:488-507`,
   `src/config.c:142-149`).
4. Test depth is uneven. The default path is healthy, but application orchestration,
   malformed asset limits, capacity failures, input events, world light/sprite
   capacity, and the feature-flag matrix are not comprehensively protected
   (`Makefile:249-258`, `Makefile:421-432`).
5. Documentation is abundant but lacks one current architecture/API reference and
   contains contradictory benchmark conclusions and stale references
   (`docs/SMC_BENCHMARK_RESULTS.md:753-811`,
   `docs/DYNAMIC_SCENE_VALIDATION_PLAN.md:53-65`,
   `docs/SMC_STREAM_INTEGRATION_PLAN.md:265-278`).

## 1. Modularization

### What is working well

- **Editor mutation has an explicit boundary.** `SceneDocument` owns the editable
  map and state IDs, while `CommandHistory` exposes narrow operations and declares
  itself the sole caller of internal mutations (`src/scene_document.h:1-22`,
  `src/command_system.h:1-6`, `src/command_system.h:52-74`). This is a strong example
  of controlled ownership and replaceable internals.
- **Selection is a pure, reusable subsystem.** Its API accepts a camera/map and
  returns typed selection results rather than reaching into application state
  (`src/editor_selection.h:1-26`).
- **Small foundational modules are cohesive.** `map.c`, `grid.c`, `menu_state.c`,
  `scale.c`, and `math.c` each expose a compact role. Their tests can link only the
  production units they require because the Makefile defines per-runner source
  groups (`Makefile:132-160`, `Makefile:260-350`).
- **No circular public-header include was identified.** Include direction generally
  flows from controllers toward data and service headers. The internal document
  mutation header also makes the privileged boundary visible rather than placing
  mutation functions in the ordinary public API.

### Findings

#### MOD-1 — High — `app.c` has multiple independent responsibilities

`app.c` includes nearly every subsystem (`src/app.c:4-30`), parses CLI modes
(`src/app.c:226-266`), creates and tears down resources (`src/app.c:268-388`,
`src/app.c:657-670`), routes menu/editor actions (`src/app.c:155-224`), drives input,
simulation and rendering (`src/app.c:406-590`), and implements benchmark statistics
and pass/fail policy (`src/app.c:641-768`).

This is more than a composition root: UI presentation, runtime state transitions,
benchmark scenario policy, resource ownership, and engine scheduling cannot be
changed independently. The narrow public `app_main()` interface (`src/app.h:1-6`)
hides the file from callers but does not create test seams inside it.

**Recommended boundary:** retain `app_main()` as composition only; extract argument
parsing, runtime/resource context, menu controller, frame update/render dispatch,
and benchmark recorder/reporting into focused modules with explicit inputs and
results.

#### MOD-2 — High — world insertion APIs do not enforce ownership transfer

`world_add_decal()` is documented as taking ownership when stored, but returns
`void` and silently drops input when full (`src/world.c:51-56`,
`src/world.c:121-152`). The batch loader allocates the pattern and then
unconditionally reports success after calling that API (`src/asset_loader.c:443-510`).
If `MAX_DECALS` has been reached, the pointer is neither stored nor freed.

The same `void`/silent-drop interface is used for lights and sprites
(`src/world.c:69-95`, `src/world.c:97-119`). Those values do not own allocations, but
callers still cannot distinguish successful insertion from lost scene data.

**Recommended boundary:** return a typed result or `bool` from every insertion API;
for decals, explicitly define that ownership transfers only on success.

#### MOD-3 — Medium — UI internals are publicly mutable

`UiElement`, `UiCache`, and `UiLayout` expose all storage, ownership pointers,
counts, colors, and arrays in the public header (`src/ui_ele.h:41-93`). `app.c`
directly mutates button color flags and values (`src/app.c:129-143`). This means UI
storage/layout internals cannot change without changing consumers, contrary to the
requested strictly enforced interface.

**Recommended boundary:** make the implementation types opaque or at least add
accessors such as `ui_element_set_style()` and keep cache/layout storage private.

#### MOD-4 — Medium — `raycast.c` combines casting, world projection, lighting, and decal rendering

The public module exposes ray queries and complete scene rendering together
(`src/raycast.h:1-25`), while its implementation also contains decal basis,
projection, light sampling, and decal rasterization helpers. This makes decal
rendering changes depend on the wall-raycasting module and contributes to broad test
link groups (`Makefile:297-309`).

**Recommended boundary:** keep ray intersection in `raycast`; move projection and
surface-decal rendering behind a scene-render/decal-render interface that consumes
ray results and explicit render context.

#### MOD-5 — Medium — duplicated decal parsers can drift

The runtime batch loader and single-file editor loader independently parse the same
format. `decal_io.c` explicitly states it mirrors `asset_loader.c`
(`src/decal_io.c:1-13`), while `asset_loader.c` contains a separate implementation
(`src/asset_loader.c:400-510`). Compatibility tests are useful but cannot make two
implementations a single source of truth.

**Recommended boundary:** make the batch loader enumerate files and delegate each
decal to one validated parser/ownership API.

## 2. Maintainability and correctness

### What is working well

- **Strict compilation is the default.** Warnings are errors under C11
  (`Makefile:1-3`), and the audited default build/test invocation passed.
- **Newer editor operations are transactional.** Failed loads preserve the prior
  document, save uses a neighboring temporary file before replacement, and command
  allocation failure is tested as non-mutating. Representative transactional
  assertions appear in `tests/test_scene_document.c:410-449` and branch/state-ID
  assertions in `tests/test_command_system.c:330-377`.
- **Ownership is explicit in the decal painter.** The API distinguishes owned
  creation from borrowed attachment, and tests exercise invalid replacement,
  aliasing, dimensions, overflow, and destruction (`src/decal_painter.h:1-72`,
  `tests/test_decal_painter.c:307-320`).
- **Error enums are used at important editor boundaries.** Scene load/save,
  commands, and painter operations return typed outcomes rather than generic
  success alone (`src/scene_document.h:24-42`, `src/command_system.h:21-29`,
  `src/decal_painter.h:12-24`).

### Findings

#### MAINT-1 — High — malformed decal dimensions permit out-of-bounds access

`decal_load_from_file()` parses signed dimensions with `atoi`, multiplies them in
`int`, and allocates only when the resulting product is positive
(`src/decal_io.c:130-163`). Structured mode then indexes fixed `p_buf[64][256]`,
`m_buf[64][256]`, and `mats[256]` using the unbounded declared rows and columns
(`src/decal_io.c:86-94`, `src/decal_io.c:199-217`). More than 64 rows or 256 columns
therefore accesses stack arrays out of bounds; multiplication can also overflow
before allocation.

The duplicated batch parser has the same fixed arrays and unchecked dimensions
(`src/asset_loader.c:421-444`, `src/asset_loader.c:474-494`).

**Recommended fix:** parse with checked conversion, require dimensions within one
documented limit, detect `size_t` multiplication overflow, reject malformed files,
and share the parser.

#### MAINT-2 — High — renderer cell-size contract can corrupt its framebuffer

`renderer_create()` accepts any positive `cell_w` and `cell_h`, stores them, and
allocates `grid * cell` pixels (`src/renderer.c:176-201`). Both renderer paths then
position cells with hard-coded multiples of eight and write an 8x8 block
(`src/renderer.c:488-507`, `src/renderer.c:569-597`). Configuration accepts arbitrary
cell sizes and passes them directly to the renderer (`src/config.c:142-149`,
`src/app.c:268-270`). Values below eight can write beyond the allocated buffer;
values above eight produce incorrect layout.

**Recommended fix:** either reject any dimensions other than 8x8 at configuration
and renderer boundaries or implement actual scaling. Add checked multiplication for
logical dimensions and allocation size.

#### MAINT-3 — High — allocation-size overflow is not uniformly checked

`map_create()` casts neither operand before `width * height` in its `calloc` calls
(`src/map.c:54-75`). `renderer_create()` multiplies signed dimensions before its
allocation (`src/renderer.c:184-201`). `grid_create()` correctly uses `size_t` for
the cell count (`src/grid.c:45-64`) but still does not explicitly reject product
overflow. These are public allocation APIs, so relying only on ordinary configured
sizes weakens their contracts.

**Recommended fix:** centralize checked positive-dimension multiplication and use it
for all grid-like allocations.

#### MAINT-4 — Medium — configuration accepts invalid and unknown values silently

The parser uses `atoi`/`atof`, ignores malformed and unknown keys, and performs no
range validation (`src/config.c:98-170`). Invalid dimensions can reach allocation
and renderer code; nonpositive frame rate can reach timing calculations. The global
singleton also introduces implicit dependency and cross-test state
(`src/config.c:1-16`, `src/config.c:25-29`).

**Recommended fix:** parse into a candidate config with checked conversions, validate
the complete candidate, return structured diagnostics, then commit atomically.
Pass a read-only config to subsystems where practical rather than reading hidden
global state.

#### MAINT-5 — Medium — renderer creates an unused mandatory glyph atlas

The software renderer uses embedded font bytes directly but still creates a glyph
atlas and fails renderer initialization if atlas creation fails
(`src/renderer.c:260-274`). The comments describe the atlas as only a possible
future GPU path. This adds SDL resources, failure modes, and lifecycle coupling to a
path that does not use the object.

**Recommended fix:** remove the atlas from the software backend or place it behind a
separate renderer backend/feature boundary.

#### MAINT-6 — Medium — renderer initialization starts text input for a removed component

Renderer creation globally starts text input and claims it is required by
`asset_designer.c` (`src/renderer.c:315-319`), but no such source file exists in the
current repository and the README describes the old authoring work as deferred
(`README.md:173-177`). This is stale behavior and places input policy in a rendering
constructor.

**Recommended fix:** move text-input activation to the controller that owns a text
entry mode, and remove the stale comment if no current mode needs it.

#### MAINT-7 — Low — implementation hygiene is inconsistent

Examples include unused “future use” includes (`src/map.c:32-34`,
`src/raycast.c:24-26`) and unusually verbose file-level/tutorial comments for very
small functions. In contrast, newer editor modules are concise and contract-focused.
This does not currently break compilation, but it increases review noise and makes
documentation style inconsistent.

## 3. Documentation

### What is working well

- The README accurately documents build flags and the default stream-tracker choice
  (`README.md:31-72`; `Makefile:30-38`).
- Current editor behavior and persistence limits are clearly documented
  (`README.md:136-177`) and backed by a dedicated requirements/regression document
  referenced from the maintainer section (`README.md:127-134`).
- Asset formats have concrete examples and enumerate current map, material, decal,
  light, UI, and sprite status (`assets/README.md:7-205`).
- Public headers for the editor describe ownership and mutation rules at the point
  of use (`src/unified_editor.h:1-7`, `src/scene_document.h:1-7`,
  `src/command_system.h:1-6`).

### Findings

#### DOC-1 — High — benchmark documents give incompatible conclusions

The generated benchmark results mark every summarized mode `RUN_FAIL`, state
`DECISION_FAIL`, and mark every dynamic scenario failed
(`docs/SMC_BENCHMARK_RESULTS.md:753-811`). The dynamic validation plan says all
benchmarks passed (`docs/DYNAMIC_SCENE_VALIDATION_PLAN.md:53-65`), while the stream
integration plan reports different performance values and declares all criteria met
(`docs/SMC_STREAM_INTEGRATION_PLAN.md:265-278`). A maintainer cannot determine which
result is authoritative from these documents alone.

**Recommended fix:** add dates/environment and supersession headers, explain why the
failed generated report is not authoritative if that is intended, and link all
documents to one canonical result record.

#### DOC-2 — Medium — completion documents point to an ambiguous report path

Both plan documents direct readers to `SMC_INTEGRATION_REPORT.md`
(`docs/DYNAMIC_SCENE_VALIDATION_PLAN.md:18-22`,
`docs/SMC_STREAM_INTEGRATION_PLAN.md:276-278`). The repository contains that file at
the project root, not under `docs/`, while a different `SMC_BENCHMARK_RESULTS.md`
lives under `docs/`. Relative links are not used, so the intended evidence trail is
easy to misread.

**Recommended fix:** use explicit Markdown links and consolidate stable technical
reports under `docs/`.

#### DOC-3 — Medium — no current architecture/module-boundary reference exists

The repository contains plans, handoffs, status reports, and feature-specific
requirements, but no concise current architecture document defining dependency
direction, ownership, public interfaces, application composition, render pipeline,
and test ownership for all modules. This is especially important because public
structs expose implementation details and `app.c` coordinates nearly every system.

**Recommended fix:** create `docs/ARCHITECTURE.md` from current verified behavior,
with module responsibilities, allowed dependencies, state ownership, lifecycle,
and extension points. Historical plans should link to it rather than serving as the
only architecture evidence.

#### DOC-4 — Medium — asset documentation overstates parser validation

The decal format says each pattern row must be exactly `pattern_cols` characters and
each material row contains exactly that many IDs (`assets/README.md:100-102`). The
single-file parser pads missing structured glyph/material data with defaults rather
than rejecting it (`src/decal_io.c:199-217`), while art mode treats short rows as a
recoverable marker pattern instead of returning failure (`src/decal_io.c:165-195`).

**Recommended fix:** either enforce the documented schema and return a parse error or
document the actual padding/error-marker recovery behavior.

#### DOC-5 — Low — some source comments are stale

The renderer references a nonexistent `asset_designer.c` consumer
(`src/renderer.c:315-319`), and several comments describe unused future facilities
(`src/renderer.c:260-264`, `src/map.c:32-34`). Comments should document current
contracts or a tracked, linked decision—not speculative implementation.

## 4. Completeness of testing

### Verified strengths

- **Default gate:** 11 runners and 181 registered tests passed on 2026-07-28. The
  target executes dependency, core, decal rendering, menu, decal I/O, painter, UI,
  document, command, selection, and unified-editor suites (`Makefile:421-432`).
- **Strict diagnostics:** all those runners compile with the same warning policy as
  production (`Makefile:1-3`, `Makefile:360-405`).
- **Focused linkage:** runners link only needed production modules rather than the
  entire application (`Makefile:132-160`, `Makefile:260-350`). This improves test
  isolation and exposes dependency growth.
- **Strong advanced coverage:** command tests include allocation failure, state-ID
  exhaustion, branch truncation, and clean/dirty historical state; scene-document
  tests include failed replacement, preservation, temporary-file cleanup, and
  round trips; unified-editor tests include input consumption and save/exit failure
  behavior. Representative command branching is exercised at
  `tests/test_command_system.c:330-377`, document cleanup/derived-state persistence
  at `tests/test_scene_document.c:410-449`, and the editor vertical slice at
  `tests/test_unified_editor.c:925-1044`.
- **Projection regressions are unusually thorough.** The decal suite checks wall,
  floor, ceiling, perspective, orientation, ordering, spacing, backface behavior,
  and whitespace; representative size and perspective assertions appear at
  `tests/test_decals.c:1024-1072`.

### Findings

#### TEST-1 — High — no malformed-dimension tests protect decal loaders

The decal I/O tests cover valid round trips, missing files, invalid save paths, and
engine compatibility (`tests/test_decal_io.c:451-469`) but not zero, negative,
overflowing, more-than-64-row, or more-than-256-column inputs. Those are precisely
the unsafe parser boundaries described in MAINT-1.

#### TEST-2 — High — renderer cell-size contract is not tested

The only invalid renderer construction test passes a negative grid width
(`tests/test_core.c:124-135`). No test verifies non-8 cell sizes, dimension-product
overflow, grid/renderer dimension agreement, or safe rejection before SDL resource
creation.

#### TEST-3 — Medium — optional implementations are not tested as a matrix

Tests inherit the one feature configuration selected by the caller
(`Makefile:249-258`), and `make test` invokes that configuration only
(`Makefile:421-432`). It does not build and run all tracker modes, lighting cache,
glyph cache, or no-tracker baseline. The direct APIs in `glyph_block_cache.c`,
`lighting_cache.c`, `smc_state_tracker.c`, and `smc_indexed_state_tracker.c` have no
dedicated test runners.

**Recommended gate:** add a bounded configuration-matrix target that at minimum
builds and runs renderer correctness tests under each mutually exclusive tracker and
both caches.

#### TEST-4 — Medium — application and event-routing boundaries are untested

`app.c` is not linked into any test runner (`Makefile:138-160`,
`Makefile:260-350`). CLI parsing, initialization rollback, menu dispatch,
application-state transitions, benchmark result policy, and cleanup across partial
initialization therefore lack direct regression tests. `input_process()` is linked
into broad suites but no registered test drives SDL events through it.

The unreferenced `tests/test.c` expects the application to print `hello world`
(`tests/test.c:5-33`) and is not built by `make test`; it is stale and should be
removed or replaced with a real smoke test.

#### TEST-5 — Medium — world capacity and ownership failures are untested

The decal suite tests ordinary insertion, but there are no tests for full light,
sprite, or decal arrays, no assertion of insertion status, and no loader test proving
cleanup when insertion fails. The current `void` API makes the most important decal
ownership assertion impossible (`src/world.c:121-152`).

#### TEST-6 — Medium — no sanitizer, leak, coverage, or style target exists

The Makefile contains strict compiler warnings and runtime benchmark/stability
targets, but no AddressSanitizer/UndefinedBehaviorSanitizer, leak-check, coverage,
formatter verification, or static-analysis target (`Makefile:119-125`,
`Makefile:421-446`). Strict warnings are valuable but do not detect the out-of-bounds
and ownership problems identified above.

**Recommended gates:** add `test-sanitize`, a leak-check target suitable for the
platform, a coverage report with explicit exclusions, and a non-mutating style/static
analysis check.

#### TEST-7 — Medium — lighting behavior is covered only incidentally

Many render tests call `lighting_update()`, but no focused suite verifies multiple
lights, negative lights, shadowing, radius boundaries, invalid maps/worlds, cache
invalidation, or revision behavior. The cache path currently contains hard-coded
map/lighting revisions marked TODO (`src/lighting.c:95-96`), which deserves direct
tests before that option is treated as regression-safe.

## 5. Style consistency

### What is working well

- Naming is generally consistent: module-prefixed functions, PascalCase public
  types, uppercase constants, and `static` private helpers.
- Headers consistently use include guards and C modules avoid unnecessary external
  dependencies beyond SDL and the vendored libraries.
- The project uses fixed-width integer types where representation matters and keeps
  public result enums explicit in newer modules.
- Compiler-enforced warning cleanliness is an effective baseline style gate
  (`Makefile:1-3`).

### Findings

#### STYLE-1 — Medium — public API formatting and error conventions are inconsistent

Older APIs use forms such as `Map*`, one-line guards, `void` silent failure, and raw
integer status; newer editor APIs use spaced pointer declarations, multiline
signatures, and typed result enums. Compare `src/map.h:22-27` and
`src/world.h:49-53` with `src/scene_document.h:24-66` and
`src/command_system.h:21-74`.

**Recommended standard:** document one C style covering pointer placement,
signature wrapping, error/result conventions, const/null policy, ownership language,
and enum use; apply it incrementally when modules are touched.

#### STYLE-2 — Medium — comments vary between contract documentation and tutorial narration

Core files often restate each line of implementation in long comments
(`src/map.c:40-83`, `src/renderer.c:358-385`), while newer modules focus on
invariants and ownership (`src/scene_document.h:1-7`, `src/command_system.h:1-6`).
The latter is easier to keep accurate. Comments should explain constraints,
ownership, non-obvious math, and failure semantics rather than narrating obvious
assignments.

#### STYLE-3 — Low — Makefile readability is uneven

The Makefile has useful source groups and comments but also repeated blank lines,
duplicated include fragments in emitted commands, and a no-op-looking duplicate
assignment (`Makefile:116-121`, `Makefile:212-218`, `Makefile:352-359`). This is not a
functional defect, but extracting common compile recipes and normalizing feature
bundles would make configuration maintenance safer.

## Prioritized remediation plan

The summary below is expanded into actionable increments and verification gates in
[`REPOSITORY_REMEDIATION_ACTION_PLAN_2026-07-28.md`](REPOSITORY_REMEDIATION_ACTION_PLAN_2026-07-28.md).

1. **Secure input/allocation boundaries first.** Validate decal dimensions and
   numeric parsing, detect multiplication overflow, enforce the renderer's 8x8
   contract, and add sanitizer-backed regressions.
2. **Make ownership failures observable.** Change world insertion APIs to return
   typed status and test full-capacity behavior and decal cleanup.
3. **Create one decal parser.** Delegate runtime enumeration to `decal_io` (or a new
   shared asset parser) so format validation and limits cannot drift.
4. **Split the application controller.** Extract CLI, lifecycle context, menu/frame
   controllers, and benchmark recording without changing user-visible behavior.
5. **Add a configuration verification matrix.** Exercise each tracker/cache mode and
   add direct cache/tracker tests.
6. **Establish maintainable project references.** Add `docs/ARCHITECTURE.md` and a
   concise C style/ownership guide; reconcile the SMC evidence trail and remove stale
   comments/tests.
7. **Measure test completeness.** Add coverage reporting after the safety-critical
   gaps are tested; do not use a percentage alone as acceptance.

## Overall assessment by criterion

| Criterion | Assessment | Basis |
|---|---|---|
| Modularization | **Partially meets** | Good module naming and excellent editor boundaries, but `app.c`, public UI structs, raycast/decal coupling, duplicated parsing, and silent world insertion prevent strict isolation. |
| Maintainability | **Partially meets** | Strict builds and typed editor outcomes are strong; unchecked dimensions, implicit global config, duplicated parsers, and stale/unused renderer behavior create surgical-change risk. |
| Documentation | **Partially meets** | Extensive feature and format docs exist, but there is no single current architecture reference and benchmark evidence is contradictory. |
| Testing | **Partially meets** | 181 tests pass and advanced editor/decal regressions are strong; unsafe boundaries, app/event paths, capacities, optional configurations, sanitizers, leaks, coverage, and style checks remain gaps. |
| Style consistency | **Mostly meets** | Naming and warning discipline are consistent; API/error/comment conventions and Makefile presentation differ between older and newer code. |

## Verification record and stop reason

- Inspected all source/header definition inventories and detailed high-risk modules,
  all test inventories, Makefile targets, README/asset documentation, and project
  planning/status documents.
- Ran `make test`; all 181 registered tests passed with strict warnings.
- Did not modify production code or tests.
- Did not claim benchmark performance, sanitizer cleanliness, leak freedom, or
  complete path coverage because those checks were not established by the default
  gate.

Further inspection is unlikely to change the prioritized conclusions: the principal
ownership, parser, renderer-contract, orchestration, documentation, and verification
gaps are directly evidenced, while positive examples and passing tests establish the
areas that should remain unchanged.