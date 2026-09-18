# Feature Roadmap to v1.0

## Status and authority

This is the current dependency and sequencing authority for work from the verified
R0-R12 foundation through v1.0. The v1 scope and ordering were accepted on
2026-09-14. A phase listed here is a committed v1 outcome, not a claim that its
detailed requirements or implementation already exist.

Detailed R0-R12 history remains preserved in
[`archive/roadmap/FEATURE_ROADMAP_LONG_R0_R12_2026-09-11.md`](archive/roadmap/FEATURE_ROADMAP_LONG_R0_R12_2026-09-11.md).
Current implementation state is summarized in [`CURRENT_STATUS.md`](CURRENT_STATUS.md).
Unsequenced ideas remain in [`TODO.md`](TODO.md). The product and authored-model
constraints in
[`V1_PRODUCT_AND_AUTHORED_MODEL_FOUNDATION.md`](V1_PRODUCT_AND_AUTHORED_MODEL_FOUNDATION.md)
govern every phase below.

## Status vocabulary

- **Verified foundation** — implemented and supported by named verification evidence.
- **Committed v1 outcome** — required before v1.0, but not implementation-ready until
  its Q1 gate passes.
- **Ready for Q1** — prerequisites exist for focused requirements and design work.
- **Decision-blocked** — a behavior-affecting phase-entry decision is unresolved.
- **Evidence-blocked** — investigation or a reproducible fixture is required.
- **Active** — an approved focused implementation plan is being executed.
- **Verified** — the phase's Q2/Q3 evidence and applicable review passed.
- **Post-v1** — explicitly outside the v1 release gate.

Large scope is not a reason to remove a required capability. Scope is controlled by
the accepted v1 capability benchmark, explicit dependencies, and the distinction
between necessary capability and speculative implementation.

## v1 product outcome

v1 must let an author remain within the editor to create, test, validate, export,
and distribute a small complete game with a hub-and-mission flow, mission success
and failure, character/loadout choices, togglable effects, weapons, configurable
enemies, animation, audio, authored UI, and the selected bounded cooperative
multiplayer model. Exported games do not include the editor.

The benchmark is capability evidence, not a requirement that the engine become one
particular game or encode genre-specific policy. Engine data, defaults, templates,
and tools remain reusable and genre-neutral.

## v1 release platforms

- **Required:** native Linux x64 and native Windows x64.
- **Informational:** Steam Deck / SteamOS. Survey results are recorded honestly but
  do not block v1 unless this policy is deliberately revised.
- **Post-v1:** macOS. v1 architecture must avoid gratuitous barriers to a later port,
  but signing, notarization, packaging, and native macOS verification are not v1 gates.

Windows is no longer merely an informational survey target. Its current strict-build
incompatibility is a product issue that must be resolved early enough to constrain
filesystem, UTF-8, font, IME, clipboard, dependency, and packaging decisions.

## Four independent v1 exit proofs

v1 is not complete until all four proofs pass against a repository-owned benchmark
project.

1. **Authoring proof** — the complete project can be created, edited, validated, and
   saved through supported editor workflows without external source editing.
2. **Runtime proof** — Start Game executes the native project entry flow and the
   complete project can reach its authored success, failure, and return paths.
3. **Export proof** — clean Linux and Windows packages run without the editor and
   contain exactly the required transitive data, fonts, audio, licenses, and runtime
   dependencies without relying on source-machine paths or system fonts.
4. **Quality proof** — deterministic, strict, complete-suite, SMC, sanitizer,
   performance, stability, accessibility, native platform, display/input, failure,
   and manual gates required by the changed systems pass.

## Requirements that govern every v1 phase

### Product philosophy

- Prefer deterministic defaults, then explicit templates, over asking unnecessary
  questions or inferring creative intent.
- Transfer authorship at the smallest meaningful unit; taking control of one value
  does not force unrelated values into manual ownership.
- Keep common controls immediate and advanced depth available through progressive
  disclosure.
- Ask when materially different creative choices remain and no correct default exists.
- Localize failures and block only the smallest unsafe operation.
- Never silently rewrite, reorder, optimize, repair, replace, or delete authored intent.
- Justify limits only with unavoidable representation, resource, platform, safety,
  compatibility, or logical facts. Implementation convenience is not a creative rule.

### Architecture and data

- Persistent authored data has one explicit owner. Runtime views, registries, caches,
  projections, sessions, shaped runs, and glyph rasters are derived.
- Editor UI invokes typed document/workspace commands; renderers do not own input,
  time, persistence, or authored mutation.
- Application/editor UI and authored game UI remain separate data and persistence
  models unconditionally.
- Shared UI facilities are limited to value-level tokens and services such as semantic
  colors, spacing, typography roles, focus rules, motion policy, contrast evaluation,
  text measurement, and font/glyph resolution.
- Reuse common typed event and connection primitives without creating one universal
  graph for project flow, entity behavior, UI, audio, combat, and effects.
- Runtime rules remain deterministic and headless with explicit inputs, outputs, time,
  ordering, seeds, and errors.

### Compatibility and failure

- Existing accepted files continue to load through explicit migration or an
  unambiguous compatibility path.
- New writes use the newest canonical format; migrations are deterministic,
  non-destructive, and tested for semantic preservation.
- Malformed recognized data rejects transactionally. Missing references remain visible
  and diagnosable and are never replaced with supposedly similar content.
- Allocation, validation, I/O, refresh, and export failures preserve the last accepted
  document, registry, history, and destination according to the owning contract.
- Internal C struct ABI is not a persistence promise and may evolve behind tested APIs.

### SMC and performance

- SMC remains mandatory in the shipping renderer. No phase may remove, bypass, demote,
  or turn it into a nominal path whose fallback performs ordinary work.
- Reference rendering remains the correctness oracle; optimized and reference output
  must agree.
- Packed and streamed state representations are explicit and never depend on struct
  padding.
- Every affected SMC mode retains its declared compile/test role. Conservative fallback
  remains correct and exceptional.
- Hot-path work requires paired representative benchmarks, stability evidence, and
  environment records. ASCII-heavy existing scenes and new feature-heavy workloads
  must both be measured.
- A material loss of accepted SMC gains blocks a phase until root cause, remediation
  attempts, and an explicit performance disposition are recorded.

## Reusable quality gates

### Q1 — Design readiness

Required before implementation of a substantial phase or increment:

- required, forbidden, preserved, deferred, and unresolved behavior is explicit;
- behavior-affecting product choices are resolved;
- ownership, APIs, dependency direction, and deterministic boundaries are named;
- data schemas, defaults, migrations, compatibility, and missing-reference rules are known;
- realistic validation, allocation, I/O, tool, platform, and rollback failures are listed;
- normal, boundary, failure, migration, preserved-regression, manual, and performance
  acceptance is defined;
- rejected shortcuts and conditions for revisiting them are recorded;
- the implementation is decomposed into small independently revertible increments.

If these conditions are not met, implementation does not begin.

### Q2 — Increment verification

- strict C11 build passes with `-Wall -Wextra -Wpedantic -Werror`;
- the narrowest focused tests pass;
- realistic no-change and failure paths pass;
- affected persistence and externally visible outputs remain transactional;
- relevant documentation is updated while evidence is current;
- unrelated accepted contracts and tests remain intact;
- no success depends on removing or weakening a regression test.

### Q3 — Phase verification

- `make test`, including SMC runners, passes;
- `make standards` and applicable feature/platform matrices pass or produce a correctly
  classified blocking result under [`VERIFICATION_POLICY.md`](VERIFICATION_POLICY.md);
- ownership changes pass applicable ASan, UBSan, and leak gates;
- format round-trip, migration, malformed-input, and restoration tests pass;
- hot rendering/simulation paths pass paired benchmark and stability gates;
- required Linux and Windows native checks pass where the phase affects their boundary;
- applicable display, pointer, clipboard, IME, audio, networking, and accessibility
  checks are manually or automatically recorded;
- stable documentation describes verified behavior and labels residual gaps accurately.

### Q4 — Full repository review

Perform a Q4 review after the Unicode/cell/SMC foundation, after entity/gameplay
boundaries stabilize, before export hardening, and whenever architecture risk warrants.
Review ownership, coupling, hidden state, data duplication, migrations, deterministic
behavior, UI leakage, SMC/performance, platform evidence, accessibility, documentation,
and whether roadmap order needs correction.

## Dependency overview

```text
R0-R12 verified foundation
          |
          v
V1-0 baseline / platform policy / mirror reconciliation
          |
          +-------------------------+
          |                         |
          v                         v
V1-1 UI rules and tokens      V1-2 mirror correction if distinct
          |
          v
V1-3 pointer and major-context motion
          |
          v
V1-4 Unicode/font research and character model
          |
          v
V1-5 glyph identity, renderer, cache, and mandatory SMC migration
          |
          +-------------------------+
          |                         |
          v                         v
V1-6 Unicode UI/input/IME      V1-7 glyph-art formats and tools
          |                         |
          +------------+------------+
                       v
V1-8 sprite workbench and animation follow-ups
                       |
                       v
V1-9 directional sprite runtime/data -> authoring
                       |
                       v
V1-10 sprite compositions/stacks -> equipment/state authoring
                       |
                       v
V1-11 complete reflected-content policy
                       |
                       v
V1-12 native project execution and Start Game
                       |
                       v
V1-13 entity parts and templates
                       |
                       v
V1-14 navigation, actors, enemies, and combat
                       |
                       v
V1-15 inventory, loadouts, equipment, effects, and particles
                       |
                       v
V1-16 objectives, outcomes, and save state
                       |
                       v
V1-17 audio
                       |
                       v
V1-18 bounded cooperative multiplayer
                       |
                       v
V1-19 export and transitive validation
                       |
                       v
V1-20 benchmark project, release proofs, Linux/Windows v1.0
```

Research and format design for export begin before V1-19: every new asset and runtime
system must expose typed transitive dependencies and keep editor-only data separable.

## Verified foundation summary

R0-R12 remain **Verified foundation**. They establish repository/editor health,
versioned scenes, typed command and selection seams, reusable asset documents,
structural and vertical-world editing, optical rendering, expanded lighting, baseline
sprites/objects/triggers/animation authoring, and authored game-flow/UI foundations.
Evidence is indexed under [`archive/`](archive/) and [`reviews/`](reviews/).

The foundation is not feature completeness and must not be reimplemented through
parallel models.

## V1-0 — Baseline, required-platform remediation, and evidence reconciliation

**Status:** Verified on 2026-09-16. Baseline/platform remediation, native Linux
display/input acceptance, performance/stability, byte inventory, and the distinct mirror
correction all passed their recorded exit gates.

**Purpose:** Establish an unchanged baseline and make Linux/Windows first-class
constraints before cross-platform file, text, font, input, and packaging work.

**Required outcomes:**

1. Record current strict, complete-suite, UI, SMC, sanitizer, standards, benchmark,
   stability, Linux-profile, and Windows-profile results with typed outcomes.
2. Remediate native Windows strict-build failures. Keep Ubuntu Clang as a strict
   informational diagnostic profile without weakening warnings or omitting sources.
3. Establish native Linux display/input smoke evidence and reproducible dependency identities;
   native Windows requires only strict compilation and the complete regression/unit suite.
4. Reconcile any current mirror report with the already verified 2026-09-12 planar
   curvature regression. If the report is distinct, capture its exact scene, camera,
   geometry, viewport, actual output, and expected output as a new failing fixture.
5. Inventory authored non-ASCII bytes before assigning migration meaning.

**Rollback point:** Toolchain/platform remediations and each fixture land independently;
no format or renderer migration occurs here.

**Exit gate:** Required strict builds pass; baseline evidence is reproducible; the
mirror issue is classified as already covered or distinct; no non-ASCII legacy byte is
silently interpreted through a host code page.

**Detailed plan:**
[`V1_0_REQUIREMENTS_AND_EVIDENCE_PLAN_2026-09-14.md`](V1_0_REQUIREMENTS_AND_EVIDENCE_PLAN_2026-09-14.md).
Closeout: [`reviews/2026-09-16-v1-0-closeout.md`](reviews/2026-09-16-v1-0-closeout.md).

**P1 reproduction result (2026-09-15):** The physical-host surface-render failure is
repeatable and deterministic in the current opaque prepared-heightfield path.

**P2 optimization result (2026-09-15):** Complete. Prepared columns now group contiguous
uniform-height runs; the opaque sampler binary-searches horizontal ownership within each run
and evaluates only run-edge boundaries through the same interval evaluator as the generic
reference path. Five isolated benchmark and three isolated 1,000-iteration stability trials
all passed the unchanged 6 ms budget with exact checksums. Optical composition remains a
separate path.

**P3 optical optimization result (2026-09-15):** Complete. Selective optical rendering now
uses the P2-equivalent prepared nearest sampler, bypasses full optical resolution for samples
whose material/cell data cannot affect rendering, and initializes only mirror-cache guard
metadata before a reflected column is needed. Accepted benchmark and 1,000-frame stability
trials pass the unchanged 6 ms budget with exact transparent/mirror checksums and zero
timed-loop allocations. Both complete headless performance aggregates now pass.

**Clang/SMC disposition (2026-09-15):** Ubuntu Clang is informational. Its strict
`-Werror` result remains nonzero for four known SMC final-newline diagnostics, while a
diagnostic-only `make -k` sweep with unlimited Clang error reporting exposes any additional
application or test diagnostics. The four-file SMC source/pin correction is deferred to
V1-20 dependency reconciliation.

## V1-1 — UI architecture, measurable design rules, and semantic tokens

**Status:** D1 palette and D6 motion decisions accepted on 2026-09-16. The current static
palette values and demonstrated 80/160/120/120 ms controlled-registration/reassembly
vocabulary are manually approved. Application-menu focus/selection colors consume the first
adapter, and the pure D6 model plus `make ui-motion-demo` are verified. Remaining V1-1
decisions retain their own gates; no real-context motion integration has begun.

**Purpose:** Convert visual inspiration into reusable, enforceable UI rules without
merging editor UI and authored game UI.

**Required outcomes:**

1. Define semantic color, spacing, density, typography, border, focus, contrast,
   warning/error/success, and motion-policy values.
2. Give editor chrome a crisp grid-aligned, white-dominant, selectively saturated,
   terminal-retrofuturist default with legibility ahead of decoration.
3. Offer the same language as an authored-game template while preserving explicit
   author control.
4. Define minimum viewports, menu-depth discipline, concise labels, progressive
   disclosure, color-independent states, and all supported UI scales.
5. Let the two UI owners consume shared immutable values/services through separate
   adapters; do not share documents or runtime state.

**Rollback point:** Add tokens and adapters before migrating one bounded UI component
family at a time; retain the old view until its replacement passes.

**Exit gate:** Accepted objective rules are added to
[`UI_DESIGN_AND_TEST_STANDARDS.md`](UI_DESIGN_AND_TEST_STANDARDS.md) with named focused
runners. Existing UI ownership, parsing, persistence, traversal, clipping, painter order,
and scaling tests pass.

**Q1 plan:**
[`V1_1_UI_RULES_AND_TOKENS_Q1_PLAN_2026-09-16.md`](V1_1_UI_RULES_AND_TOKENS_Q1_PLAN_2026-09-16.md).

## V1-2 — Mirror correction reconciliation

**Status:** Closed on 2026-09-16 after the distinct general projection defect was
reproduced, corrected, manually confirmed, and locked by analytic regression fixtures.

**Purpose:** Correct only a still-reproducible planar-reflection defect; do not duplicate
or destabilize the existing camera-Z correction.

**Required outcomes if activated:**

1. Add a deterministic display-level grid/framebuffer fixture that fails before source
   changes.
2. Diagnose incoming projection, mirror hit, reflected direction/camera, reflected
   heightfield selection, optical composition, and final cells to find the earliest
   divergence.
3. Apply one narrow correction and retain one bounce, stable camera height, explicit
   darkness fallback, reflected-mirror termination, bounded traces, and valid cache reuse.
4. Confirm the original report manually and benchmark any changed trace/cache frequency.

**Rollback point:** Fixture, diagnostics, and narrow correction are separate increments.

**Exit gate:** New and existing mirror/optical fixtures pass; the visible report is gone;
one-bounce correctness and performance remain within the accepted envelope.

## V1-3 — Pointer model and major-context motion

**Status:** I1 pause-context motion is implemented and passes automated gates: stable menu,
separate decorative layer, explicit-time enter/exit, and session-only reduced motion. Native
visual review is pending; no second context is authorized. Pointer-coordinate requirements
remain in scope. See
[`V1_3_PAUSE_CONTEXT_MOTION_I1_PLAN_2026-09-16.md`](V1_3_PAUSE_CONTEXT_MOTION_I1_PLAN_2026-09-16.md).
The separate application UI asset workbench increment is implemented and verified: dedicated
launch, four bounded contexts, immediate atomic element writes, compatible style presets, and
reusable fixed animation units. Existing units can be cloned into or detached from layouts;
pause glitch, center-out, perimeter burst, and local glitch share one production/workbench
evaluator with bounded triggers/configuration. It does not resolve the deferred pointer model or
add free-form scripting/timeline authoring. See
[`reviews/2026-09-17-ui-workbench-i1.md`](reviews/2026-09-17-ui-workbench-i1.md).

**Purpose:** Supply coherent pointer and transition semantics before graph, canvas,
timeline, and richer editor work.

**Required outcomes:**

1. Define pixel/window/logical-grid/UI-scale/preview coordinate conversion, topmost hit,
   hover, press/release, capture, drag threshold, cancel, and clipping behavior.
2. Preserve keyboard-equivalent access for every pointer operation.
3. Enumerate semantic **major contexts**. Opening and closing those contexts receive
   motion; submenus and nested subcontexts normally change immediately inside the stable
   parent.
4. Permit local motion only when needed to communicate an otherwise unclear relationship.
5. Drive motion from explicit runtime time. Motion cannot mutate documents, control
   focus/activation eligibility, or delay urgent cancel, quit, failure, or accessibility
   response.
6. Reduced-motion mode uses immediate changes and static continuity cues.

**Rollback point:** Pure coordinate/interaction policy precedes display adapters;
transitions migrate one major context at a time.

**Exit gate:** Deterministic pointer replay passes headlessly; native Linux/Windows
activation and coordinate conversion pass display-backed checks; motion and reduced-motion
acceptance pass without changing persistence or input priority.

## V1-4 — Unicode, font, and character-model research

**Status:** Committed v1 outcome; Ready for Q1 after V1-0.

**Purpose:** Select a deterministic, portable character model and dependency set before
changing the one-byte framebuffer and asset formats.

**Locked product model:**

- Files and text APIs use strictly validated UTF-8 and canonical NFC; NFKC does not
  rewrite creative content.
- Ordinary language text and exact-cell glyph art are distinct primitives.
- Text uses grapheme-aware editing, shaping, bidirectional layout, deterministic
  measurement/wrapping, and may consume multiple measured cells.
- One glyph-art cell stores one normalized extended grapheme cluster only when the
  selected glyph-art font resolves it to one bounded cell image.
- Required glyph-art priority is CJK ideographs, hiragana/katakana, hangul, common
  Latin, punctuation/symbols/box drawing, and combining sequences needed by supported
  text. Full/color emoji, arbitrary ZWJ sequences, and cross-cell clusters are deferred.
- Technical identifiers stay restricted and portable; Unicode display names are separate.
- Runtime/export never use implicit system-font fallback.

**Required research:**

1. Evaluate pinned FreeType, HarfBuzz, Unicode normalization/segmentation, and bidi
   dependencies or smaller equivalents for licensing, deterministic output, malformed
   input safety, headless operation, Linux/Windows builds, and bounded resource use.
2. Select bundled fallback fonts, record hashes/licenses, and prove required-script
   coverage.
3. Measure candidate fixed raster profiles (including CJK legibility), cache memory,
   frame cost, UI scales, and existing ASCII workloads before retiring 8x8.
4. Define deterministic hinting, antialiasing, rounding, baseline, fit, fallback,
   missing-glyph, and cross-platform parity rules.
5. Prototype a stable glyph identity/catalog and explicit human-readable glyph-cell
   serialization.

**Rollback point:** Research prototypes do not alter production files or renderer paths.

**Exit gate:** One Q1 decision record selects dependencies, Unicode version, fallback
fonts, raster profile, parity contract, resource budgets, glyph identity, and format
direction with measured evidence.

## V1-5 — Glyph identity, raster service, cache, and mandatory SMC migration

**Status:** Committed v1 outcome; blocked on V1-4.

**Purpose:** Replace raw byte glyph identity and the built-in 128-glyph 8x8 rasterizer
without sacrificing deterministic grid rendering or SMC gains.

**Required outcomes:**

1. Introduce a typed glyph reference and explicit blank, transparent/absent, and missing
   semantics rather than overloading byte zero and space across owners.
2. Provide a headless font service for validation, normalization, segmentation,
   resolution, shaping, measurement, and bounded glyph-mask rasterization.
3. Keep filesystem, SDL, platform fallback, and renderer side effects outside documents.
4. Replace fixed bitmap lookup with bounded generation-aware raster/atlas caches; no
   required per-frame allocation or unbounded growth.
5. Update `Cell`, dirty comparison, packed state, stream signatures, checksums, restore
   behavior, and every SMC adapter explicitly. SMC remains the shipping path.
6. Preserve world projection in logical cells and keep UI accessibility scale independent.

**Rollback point:** Introduce new typed APIs alongside ASCII compatibility adapters;
migrate reference rendering before each optimized path; do not remove the old file readers.

**Exit gate:** Reference/optimized parity passes; all SMC runners/modes pass; fallback is
exceptional; ASCII and multilingual benchmark/stability workloads meet approved budgets;
Q4 reviews the cross-cutting representation change.

## V1-6 — Unicode UI, editing, clipboard, and IME

**Status:** Committed v1 outcome; blocked on V1-3 and V1-5.

**Purpose:** Make authored text and editor text boundaries genuinely Unicode-safe on both
required platforms.

**Required outcomes:**

1. Migrate authored UI content to validated UTF-8 while preserving `UiDocument` IDs,
   hierarchy, workspace history, runtime separation, and old-version migration.
2. Make editor-owned UI content Unicode-safe without combining it with authored UI.
3. Implement grapheme-safe cursoring, selection, Backspace, capacity checks, alignment,
   wrapping, clipping, and bidi visual behavior.
4. Implement IME start/update/commit/cancel with visible pre-edit state and native
   candidate placement where the platform permits.
5. Implement clipboard paste for text with transactional validation.
6. Provide visible linked diagnostics for fallback and missing glyphs.

**Rollback point:** Migrate one text owner/editor at a time behind the shared text API;
legacy ASCII remains valid throughout.

**Exit gate:** Headless multilingual layout/edit tests and native Linux/Windows keyboard,
clipboard, and representative CJK IME acceptance pass.

## V1-7 — Unicode glyph-art formats and authoring tools

**Status:** Committed v1 outcome; blocked on V1-5.

**Purpose:** Make broad glyphs and project/custom fonts first-class in materials, decals,
sprites, borders, fills, particles, and other exact-cell art.

**Required outcomes:**

1. Version material, sprite, decal, and authored-UI glyph fields using an explicit
   unambiguous cell representation; raw UTF-8 byte count never substitutes for cell count.
2. Preserve legacy ASCII reads, space/transparency semantics per owner, deterministic
   migration, canonical writes, and transactional failure.
3. Keep four material distance bands while replacing each byte glyph with a glyph reference.
4. Add a searchable glyph browser with Unicode identity/name, project font coverage,
   fallback/missing state, one-cell suitability, recents, and favorites.
5. Add glyph-art clipboard paste that preserves lines, grows applicable canvases
   transactionally within measured limits, does not silently clip, and makes paste plus
   resize one undoable operation.
6. Add a validated font importer, project font roles (`text`, `display`, `glyph-art`,
   `symbol`), explicit fallback manifests, atomic refresh, hashes, face/style selection,
   and recorded redistribution/embedding status.

**Rollback point:** New version writers land only after old readers and migration tests;
each asset family migrates independently through a shared glyph codec.

**Exit gate:** Round-trip/migration/malformed/OOM/I/O tests pass for every owner; font
refresh is transactional; native projects render identically under the accepted parity
contract; unresolved required font dependencies block export rather than rewriting art.

## V1-8 — Sprite workbench and animation follow-ups

**Status:** Committed v1 outcome; controller characterization can start after V1-0;
final glyph workbench depends on V1-7.

**Purpose:** Turn the accepted R11 painter into a first-class canvas/timeline tool while
preserving `SpriteDocument` ownership and scene-history boundaries.

**Required outcomes:**

1. Characterize existing load, paint, erase, material, frame, FPS, loop, dirty,
   Save/Discard, persistence, refresh, and failure behavior before refactoring.
2. Extract deterministic painter-session transitions from rendering without moving
   filesystem, scene commands, registry refresh, or elapsed time into the session.
3. Deliver keyboard/pointer painting, zoomed exact-cell canvas, glyph/material/color
   selection, frame timeline, neighboring context/onion skin, frame reorder, live preview,
   transactional resize/paste, and document undo/redo.
4. **Add copied frame** deep-copies the selected frame, inserts after it, selects the copy,
   remains distinct from Add Empty, and fails without mutation.
5. While the painter is open, staged **Current** world preview is the default; explicit
   **Saved** switching remains available. Preview borrows staged data and never commits or
   mutates scene history.
6. Expose sprite search/create/edit from object/entity workflows. Saving an asset and
   assigning its numeric reference remain separate operations; assignment is undoable.
7. Measure animation flicker without assuming its cause and correct the demonstrated
   timing, projection, refresh, compositor, or presentation defect.

**Rollback point:** Characterization, session extraction, view replacement, each tool,
and each follow-up are separate increments over unchanged persisted ownership.

**Exit gate:** Existing and new controller/document tests pass; failure injection is
non-mutating; keyboard and pointer manual workflows pass; animation remains explicit-delta,
per-instance, and renderer-time-free.

## V1-9 — Directional and multi-angle sprites

**Status:** Committed v1 outcome; blocked on V1-8 runtime/document seams.

**Purpose:** Communicate entity movement and facing with authored views rather than one
camera-facing image at every angle.

**Required outcomes:**

1. Define typed view sets independent of animation frame order.
2. Select horizontal views deterministically from explicit entity world-facing direction
   and camera-relative direction, with tested bucket boundaries and ties.
3. Support a basic required horizontal set, explicit missing-view fallback, and optional
   authored mirroring. Exact minimum/default bucket count is a Q1 decision based on author
   need and performance, not a permanent arbitrary limit.
4. Let each view own static art or animation; view changes preserve per-instance phase by
   default and define mismatched-clip behavior explicitly.
5. Implement and test runtime/data semantics before editor authoring, then add view
   creation, preview, rotation-driven selection, fallback, and animation tooling.

**Rollback point:** Pure angle selection, versioned asset data, runtime resolution, and
authoring UI land independently; old sprites migrate as an all-directions fallback.

**Exit gate:** Deterministic selection/replay, migration, missing-view, animation, rendering,
benchmark, and full editor acceptance pass.

## V1-10 — Sprite compositions, stacking, appearance, equipment, and state

**Status:** Committed v1 outcome; blocked on V1-9.

**Purpose:** Support customizable appearance, equipment, and visible state changes through
ordered non-destructive visual layers.

**Required outcomes:**

1. Add reusable sprite-composition assets whose ordered layers reference sprite assets or
   other safely bounded composition primitives.
2. Let entity visual assemblies reference reusable sprites/compositions through readable
   typed parts. Do not stamp every combination into copied bytes.
3. Define layer order, anchors/offsets, visibility/state controls, direction/fallback,
   animation synchronization, lighting, depth, selection, missing layers, and bounds.
4. Equipment and runtime state select/control typed layers without rewriting source art.
5. Provide deterministic templates and simple defaults while retaining explicit advanced
   per-layer control.
6. Export dependency discovery traverses every layer, view, frame, material, and font.

**Rollback point:** Composition document/runtime resolution precedes entity integration;
equipment/state bindings and editor views are independent increments.

**Exit gate:** Round trip, migration, cycle/bounds/missing-reference failure, deterministic
ordering, animation, selection, performance, and authoring tests pass. Limits are justified
by measured resources rather than creative preference.

## V1-11 — Complete reflected-content policy

**Status:** Committed v1 outcome; blocked on V1-9/V1-10 and applicable visible effects.

**Purpose:** Extend the planar one-bounce foundation to a complete explicit v1 policy for
visible authored/runtime content.

**Required outcomes:** Define and implement reflected decals, directional/stacked sprites,
objects/entities, transparency, particles/effects, and other applicable visuals with
deterministic depth/ties, animation state, lighting, bounds, and darkness fallback. Do not
introduce recursive mirrors or silently omit a category without a documented disposition.

**Rollback point:** Each reflected content category lands behind the existing one-bounce
trace and independent benchmark fixture.

**Exit gate:** Category fixtures, integrated scenes, one-bounce safety, performance,
stability, and manual optical acceptance pass.

## V1-12 — Native project execution and Start Game replacement

**Status:** Committed v1 outcome; can begin after V1-3, completes after required runtime
content adapters exist.

**Purpose:** Replace the deprecated legacy Start Game path with the authored project flow.

**Required outcomes:** Start at the project `Start` node, load typed Menu/Scene targets,
activate ordinary named outputs, preserve deterministic runtime session state, report
invalid/missing targets transactionally, and never silently substitute legacy maps.

**Rollback point:** A report-only loader adapter precedes application-state transition;
legacy import remains available even after legacy Start Game is removed.

**Exit gate:** Native start, transition, cycle, invalid target, return, and teardown tests
pass headlessly and on Linux/Windows displays.

## V1-13 — Ordered entity parts and templates

**Status:** Committed v1 outcome; blocked on a focused Q1 schema decision.

**Purpose:** Realize the accepted readable, ordered, atomic entity-part model used by later
gameplay systems.

**Required outcomes:** Versioned common records, stable identities, oldest-to-newest
application, composition of compatible parts, newer priority for incompatible parts,
explicit reorder, localized absent-target behavior, ordinary editable template output,
default/template/explicit/derived provenance, validation, and editor authoring.

**Forbidden shortcuts:** Opaque classes, generic dictionaries, broad inference, silently
added prerequisites, hidden reordering, unrestricted scripting, or a universal behavior graph.

**Exit gate:** Deterministic resolution, reorder, migration, missing dependency, template,
undo/redo, persistence, and progressive-depth editor tests pass; Q4 reviews the model.

## V1-14 — Navigation, actors, configurable enemies, and combat

**Status:** Committed v1 outcome; blocked on V1-13.

**Purpose:** Supply genre-neutral typed capabilities sufficient for controllable actors,
movement, targeting, health, factions, configurable enemies, weapons, damage, reactions,
and death without hard-coded enemy classes.

**Required outcomes:** Derived navigation from authored geometry; explicit deterministic
runtime state/actions; configurable movement/perception/pursuit/attack parts; reusable
weapons and damage/effect contracts; editor templates for ordinary actors/enemies; headless
simulation/replay tests; bounded performance.

**Rollback point:** Navigation, actor control, targeting, health/faction, weapon, attack,
reaction, and death increments use separate typed seams.

**Exit gate:** Representative player/enemy encounters replay deterministically, invalid
part combinations remain safely representable, and authored data round-trips.

## V1-15 — Inventory, loadouts, equipment, effects, and particles

**Status:** Committed v1 outcome; blocked on V1-10 and V1-13/V1-14.

**Purpose:** Support character/loadout choice, visible equipment, togglable effects, and
reusable feedback needed by the benchmark project.

**Required outcomes:** Typed inventory/storage/equipment parts, loadout selection, explicit
visual-stack bindings, reusable deterministic effects, and a bounded particle asset/runtime/
editor model with explicit attachment, transform, lifetime, lighting, reflection, quality,
seed, and performance rules.

**Exit gate:** Equip/unequip, loadout, effect activation, visual communication, persistence,
deterministic particles, missing references, bounds, and editor workflows pass.

## V1-16 — Objectives, mission outcomes, and save state

**Status:** Committed v1 outcome; blocked on V1-12 through V1-15.

**Purpose:** Complete hub-and-mission success/failure and persistent player/project progress.

**Required outcomes:** Composable typed objectives; ordinary named scene outputs such as
`success`, `failure`, and `abort`; no parallel mission-transition mechanism; deterministic
local event/rule evaluation; explicit save-state ownership/version/migration; localized
failure; editor validation and testing.

**Exit gate:** Success/failure/abort routes, cycles, save/restore/migration, invalid target,
and benchmark-project mission loops pass without rewriting authored flow.

## V1-17 — Audio runtime, data, and authoring

**Status:** Committed v1 outcome; detailed backend/codec policy Decision-blocked until Q1.

**Purpose:** Add typed reusable audio rather than scattering backend calls through UI and
gameplay code.

**Required outcomes:** Audio assets, typed events, deterministic no-audio/headless adapter,
music and sound playback, volume/accessibility settings, required spatial behavior, editor
preview, entity/UI/effect integration, missing-reference handling, and export dependencies.
The audio backend owns side effects; documents and deterministic simulation emit typed events.

**Exit gate:** Headless event tests, native Linux/Windows playback, lifecycle/device failure,
settings, asset, performance, and benchmark-project acceptance pass.

## V1-18 — Bounded cooperative multiplayer

**Status:** Committed v1 outcome; exact cooperative model Decision-blocked until Q1.

**Purpose:** Implement the selected bounded cooperative model without making core rules
network-owned or nondeterministic.

**Required outcomes:** Explicit topology/session model, deterministic shared simulation or
other approved synchronization contract, versioned protocol, stable actor identity, legal
action/input records, disconnect/rejoin policy, validation, bounded trust/security model,
headless multi-peer replay, and authored project settings.

**Rollback point:** Transport-independent protocol/simulation tests precede ENet adapters;
local single-player remains the same core runtime.

**Exit gate:** Repeated peers converge under normal, delayed, reordered, invalid, and
disconnect scenarios; Linux/Windows interoperability, stability, and benchmark gameplay pass.

## V1-19 — Export, packaging, and transitive validation

**Status:** Committed v1 outcome; architecture constraints apply to all preceding phases.

**Purpose:** Produce distributable games without the editor.

**Required outcomes:** Deterministic dependency traversal; platform-specific runtime packages;
project-relative canonical data; only transitively required assets; font hashes/faces/licenses;
audio/native dependencies; version manifests; missing/dangling/non-exportable diagnostics;
atomic output; reproducible validation; clean-machine Linux/Windows execution.

**Forbidden shortcuts:** Bundling the editor, copying the entire source asset tree, relying
on system fonts, absolute paths, ignoring license status, or packaging invalid flow/content.

**Exit gate:** Valid exports run on clean required-platform hosts; invalid exports fail
before replacement with complete localized diagnostics; Q4 reviews editor/runtime separation.

## V1-20 — Benchmark project, release hardening, and v1.0

**Status:** Committed v1 outcome; blocked on all preceding required outcomes.

**Purpose:** Prove that the integrated product meets the benchmark rather than treating
individually implemented systems as completion.

**Required outcomes:**

1. Author and maintain the repository-owned benchmark project through supported editor UI.
2. Exercise hub/mission flow, success/failure, characters/loadouts, equipment/stacked and
   directional animation, configurable enemies, combat, effects, audio, authored Unicode UI,
   custom/project fonts, save state, and bounded cooperative play.
3. Pass the four v1 exit proofs on Linux x64 and Windows x64.
4. Complete accessibility, migration, missing-reference, failure-injection, performance,
   stability, native input/IME/clipboard/display, network, audio, and packaging reviews.
5. Resolve or explicitly reject every remaining v1 blocker; do not rename unverified work
   as post-v1 merely to declare release.
6. Reconcile owned upstream dependency pins, including the four deferred SMC final-newline
   corrections, and rerun the strict informational Clang diagnostic profile.

**Exit gate:** A final Q4 review confirms requirements, architecture, implementation,
documentation, and evidence agree. Only then may the version be declared v1.0.

## Explicit v1 non-goals

- Merging editor UI and authored UI ownership models.
- A universal graph for progression, entities, combat, audio, effects, and UI.
- Unrestricted scripting or unvalidated action strings.
- Broad creative-intent inference or silent prerequisite insertion.
- Multi-bounce/recursive mirrors unless separately promoted by an unavoidable v1 need.
- Full-color emoji, arbitrary emoji ZWJ coverage, or cross-cell glyph-art clusters.
- Implicit operating-system font fallback.
- Per-cell font controls imposed on ordinary workflows; the runtime may retain a clean seam.
- Sprite stacking implemented by flattening or animation-frame overloading.
- Directional views encoded as animation order.
- macOS release support in v1.
- Treating informational Steam Deck evidence as a support claim.
- Implementing every unsequenced `TODO.md` idea.

## Roadmap maintenance

- Promote ideas from `TODO.md` only through an accepted requirements decision; mark the
  inventory entry as promoted and link here rather than maintaining two competing plans.
- Before a phase becomes Active, create its focused requirements/decision/implementation
  record under `docs/`; archive it when the phase closes.
- Update [`CURRENT_STATUS.md`](CURRENT_STATUS.md) after every verified increment or blocker.
- Update stable user documentation only after implementation and verification.
- Preserve failed attempts, benchmarks, and superseded plans under `docs/archive/`.
- Reorder phases when evidence demands it, but record the dependency or assumption that changed.
- The UI Scene migration is governed by [`UI_SCENE_SYSTEM_AND_EDITOR_IMPLEMENTATION_PLAN_2026-09-17.md`](UI_SCENE_SYSTEM_AND_EDITOR_IMPLEMENTATION_PLAN_2026-09-17.md); progress there is referenced from `CURRENT_STATUS.md` and from this section once the phase enters Active status.

## Next safe action

Begin **V1-0** with a focused requirements and evidence plan. In parallel, documentation-only
Q1 preparation may inventory V1-1 UI rules and V1-4 Unicode/font research questions.
Production mirror changes remain conditional on a distinct failing reproduction. Final sprite
workbench formats remain blocked on the glyph model, although behavior characterization and
headless painter-session extraction may proceed earlier under their own Q1 plan.

The active UI Scene migration has verified Phases 0-3. Phase 4's standalone host and automated
standalone/embedded parity are implemented and pass; its required native 1920x1080 readability and
smooth-interaction acceptance remains pending. Phase 5 is blocked until that manual evidence is
recorded under
[`UI_SCENE_SYSTEM_AND_EDITOR_IMPLEMENTATION_PLAN_2026-09-17.md`](UI_SCENE_SYSTEM_AND_EDITOR_IMPLEMENTATION_PLAN_2026-09-17.md).