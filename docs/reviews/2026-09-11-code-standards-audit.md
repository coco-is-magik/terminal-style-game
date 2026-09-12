# CODE STANDARDS AUDIT / CORRECTIVE-ACTION PLANNING STUB — 2026-09-11

This document is the dedicated search target and planning authority for the
2026-09-11 non-SMC code standards audit. It records findings for a future
corrective-action pass only; it does not authorize broad code changes by itself.

# Code Standards Audit — 2026-09-11

## Purpose

This audit records source-level standards issues identified on 2026-09-11 in
anticipation of a later corrective-action pass. It is intentionally separate
from roadmap closeout, feature planning, and implementation records so future
cleanup work can cite one focused document.

The findings below are not a request to rewrite large systems in one pass. They
are a backlog of concrete, evidence-backed issues to be addressed incrementally
with regression tests and narrow review scope.

## Scope

Covered by this audit:

- Handwritten project C source and headers under `src/`.
- Source/header module boundaries, public header breadth, and dependency shape.
- Source comments and contract documentation.
- Ordinary project parsing, buffer, and string-handling patterns.
- Code/data authority boundaries for non-SMC application, editor, UI, and asset
  code.
- Non-SMC legacy/deprecated paths that affect ordinary project code.

Not covered by this audit:

- Vendor code under `vendor/`.
- Generated or build output under `build/`.
- Historical archived documentation except where it explains current source
  comments or compatibility paths.
- Any SMC-specific source, build mode, generated artifact, benchmark path, or
  corrective action. See the explicit SMC exclusion below.

## Explicit SMC exclusion

SMC is a special case and is excluded from this audit and from any corrective
action arising from this audit.

Do not use this document as authority to change, refactor, remove, rename, or
otherwise remediate any of the following:

- `src/smc*.c` or `src/smc*.h`.
- SMC-related Makefile feature toggles, mode selection, matrix entries, or build
  integration.
- `vendor/src/smc/**`.
- SMC benchmark reports, SMC planning records, or SMC-specific documentation.
- Generated SMC artifacts such as `build/smc_generated.c`.
- Generated-code inclusion patterns used by the SMC path.

Rationale: SMC has generated-code, performance, benchmark, and mode-selection
tradeoffs that are not representative of normal handwritten project C modules.
Reviewing it together with ordinary code would create noisy or misleading
findings. If SMC needs cleanup, schedule a separate SMC-specific audit covering
generated source handling, benchmark validity, build integration, maintenance
policy, and runtime mode selection.

## Standards referenced

This audit used the current project standards and architecture documents:

- [`../C_STYLE_AND_OWNERSHIP.md`](../C_STYLE_AND_OWNERSHIP.md)
- [`../ARCHITECTURE.md`](../ARCHITECTURE.md)

Key standards applied:

- Compile as C11 with `-Wall -Wextra -Wpedantic -Werror`.
- Keep modules single-purpose and public headers narrow.
- Prefix public symbols with their module name and keep helpers `static`.
- State ownership and transfer in public headers when pointer lifetime can
  outlive a call.
- Avoid circular includes.
- Prefer accessors over exposing mutable storage.
- Parse numbers with checked `strtol`/`strtod`-style logic rather than permissive
  numeric conversion.
- Use checked `size_t` arithmetic for counts and bytes.
- Comments should explain contracts, ownership, failure behavior, and
  non-obvious math; they should not narrate obvious code or retain speculative
  future work.
- Abnormal outcomes must not be silently ignored; recovery behavior and state
  guarantees are part of the error contract.

## Severity labels

- **High** — likely correctness risk, major maintainability risk, or broad
  coupling that blocks safe future work.
- **Medium** — standards gap or maintainability issue that should be fixed when
  touching the area or in a scoped cleanup pass.
- **Low** — localized cleanup, classification, or documentation issue with low
  immediate risk.
- **Managed debt** — intentional compatibility or diagnostic path guarded by
  tests or policy, but still worth tracking.

## Summary of findings

Primary non-SMC hotspots:

1. `unified_editor` is the largest module and exposes a broad mutable public
   state structure.
2. `scene_document` and `scene_format` are large ownership/parser/migration
   modules and should be reviewed for responsibility seams before future growth.
3. `app.c` is correctly an application boundary, but it also carries UI resource
   allocation, concrete UI policy, hardcoded asset roots, and menu layout mapping.
4. Parser behavior is duplicated and inconsistent, especially color parsing.
5. Some source comments are stale or too tutorial-like, while other large state
   machines have too few contract comments.
6. Several app/UI values remain code-driven even though adjacent systems are
   data-backed.
7. Header include cycles were not found in the inspected non-SMC source, but
   include fan-out/fan-in identifies coupling hotspots.
8. Legacy/deprecated ordinary-code paths are present but partly guarded by
   Makefile checks.
9. A test-only runtime-build failure hook is exposed in a production public
   header.

## Findings

### A. Module boundaries and public headers

#### A1. `unified_editor` over-responsibility

Severity: **High**

Evidence observed on 2026-09-11:

- `src/unified_editor.c` is approximately 6677 lines.
- `src/unified_editor.h` is approximately 479 lines.
- `src/unified_editor.h` includes about 20 local project headers.
- `src/unified_editor.c:1-6` describes ownership of `SceneDocument` and
  `CommandHistory`, but the module now also handles editor workflow, UI routing,
  modal state, map selection, save/load prompts, material/decal/sprite/object
  editing, flow workspace integration, authored-menu workspace integration,
  trigger session state, render overlays, input routing, runtime rebuild, and
  test fault injection.

Why this matters:

- One module has too many reasons to change.
- Unrelated editor features can accidentally regress each other through shared
  state and shared helper logic.
- The broad public header forces dependents and tests to compile against many
  internal concepts.
- Future cleanup becomes risky if attempted as one large refactor.

Corrective-action direction:

- Do not rewrite `unified_editor` wholesale.
- Identify stable seams and extract one responsibility at a time.
- Candidate seams include editor session/document ownership, modal handling,
  asset panel state, flow workspace adapter, menu workspace adapter, and runtime
  rebuild/failure handling.
- Preserve behavior with focused regression tests before each extraction.

#### A2. `UnifiedEditorState` exposes too much mutable state

Severity: **High**

Evidence observed on 2026-09-11:

- `src/unified_editor.h:152-280` exposes a concrete `UnifiedEditorState` with
  document ownership, runtime state, command history, catalogs, workspace state,
  UI-menu test runtime state, many picker/menu fields, text buffers, status
  fields, pending actions, and request flags.

Why this matters:

- Public mutable fields weaken ownership and invariant enforcement.
- Tests and other modules can become coupled to layout rather than behavior.
- Encapsulation becomes harder as more fields are added.

Corrective-action direction:

- Prefer incremental encapsulation.
- First split stable sub-state structs where it reduces coupling.
- Then narrow public access through behavior-oriented functions or read-only
  accessors.
- Avoid making the entire struct opaque in a single disruptive pass.

#### A3. `scene_document` / `scene_format` size and responsibility

Severity: **Medium to High**

Evidence observed on 2026-09-11:

- `src/scene_document.c` is approximately 2740 lines.
- `src/scene_format.c` is approximately 2706 lines.
- `src/scene_document.h` is approximately 313 lines.
- `src/scene_document_internal.h` is approximately 238 lines.

Risk:

- These modules sit at the authoritative scene-data boundary and appear to carry
  combinations of document ownership, load/save compatibility, migration,
  validation, runtime view construction, and diagnostics.
- Their responsibilities may be historically justified, but future additions
  should not further mix parser, migration, ownership, and runtime-adapter logic
  without checking seams.

Corrective-action direction:

- Document current responsibilities before modifying.
- Keep `SceneDocument` as the authored-state authority.
- Consider narrow helper modules only where a seam is already clear, such as
  parser helpers, migration helpers, or runtime-build helpers.
- Do not move code merely to reduce line count; move code only when ownership and
  API boundaries become clearer.

#### A4. `app.c` is broad beyond composition

Severity: **Medium**

Evidence observed on 2026-09-11:

- `src/app.c` is approximately 1209 lines.
- `src/app.c` has the highest non-SMC local include fan-out observed: about 31
  local project headers.
- `src/app.c:47-55` defines concrete app UI canvas sizes.
- `src/app.c:81-113` owns app UI canvas resource creation/destruction.
- `src/app.c:279-289` maps `MenuId` values to layout names.
- `src/app.c:291-305` hardcodes menu focus colors.
- `src/app.c:316-341` enters the unified editor and hardcodes asset roots.

Assessment:

- `app.c` is expected to be the application composition boundary, so high
  dependency fan-out is not automatically wrong.
- However, it currently combines composition with concrete UI resource policy,
  visual styling, menu layout mapping, editor startup policy, and root paths.

Corrective-action direction:

- Preserve `app.c` as the top-level composition boundary.
- Consider extracting app-owned UI resource creation/render policy into a narrow
  `app_ui`-style module if touched.
- Review hardcoded style/layout/root values separately before moving them to
  data.

### B. Comments and documentation authority

#### B1. Stale source comment referencing a historical plan section

Severity: **Low**

Evidence observed on 2026-09-11:

- `src/unified_editor.h:83` contains a comment equivalent to:
  `Exit-prompt choices (plan §9). Resume and Cancel both dismiss without exit.`

Why this matters:

- Current source comments should not rely on historical plan section numbers for
  meaning.
- Historical plans may be archived or renumbered and are not current source
  authority.

Corrective-action direction:

- Replace historical-plan references in source comments with current behavior
  contracts.
- Example intent: “Resume and Cancel both dismiss the prompt without exiting.”

#### B2. `asset_loader.c` has dense tutorial-style comments that may duplicate data docs

Severity: **Medium**

Evidence observed on 2026-09-11:

- `src/asset_loader.c` is approximately 1076 lines with about 304 comment-ish
  lines.
- `src/asset_loader.c:1-46` describes many asset file formats inline.
- `src/asset_loader.c:48-60` includes comments narrating included APIs.
- `src/asset_loader.c:65-94` contains utility comments that explain simple
  helper behavior.

Why this matters:

- Source comments can drift from format documentation.
- The project standard says comments should explain contracts, ownership,
  failure behavior, and non-obvious math rather than narrating obvious code.
- Asset format authority likely belongs in `assets/README.md` or focused format
  documentation, with source comments limited to parser contracts and edge cases.

Corrective-action direction:

- Audit comments in `asset_loader.c` for stale format details.
- Keep comments that explain parser failure behavior, compatibility quirks, or
  ownership.
- Remove or shorten comments that duplicate simple code or standard library API
  names.

#### B3. Large modules may lack enough contract comments

Severity: **Medium**

Evidence observed on 2026-09-11:

- Large modules with low comment density include:
  - `src/unified_editor.c`
  - `src/ui_menu_workspace.c`
  - `src/app.c`
  - `src/ui_document.c`
  - `src/ui_ele.c`
  - `src/decal_document.c`
  - `src/material_document.c`

Assessment:

- Low comment density is not automatically a defect.
- But complex state machines, parser rules, save atomicity, ownership transfer,
  failure rollback, and runtime rebuild guarantees need visible contracts.

Corrective-action direction:

- Add comments only where they describe contracts or invariants that code alone
  does not make obvious.
- Prioritize public APIs, state-transition boundaries, parser rejection behavior,
  and cleanup/rollback paths.

### C. Data-vs-code boundaries

#### C1. App UI dimensions and styling are code-driven

Severity: **Medium**

Evidence observed on 2026-09-11:

- `src/app.c:47-55` defines app UI dimensions such as menu, HUD, editor, footer,
  and feedback sizes.
- `src/app.c:291-305` hardcodes selected/unselected button colors.
- `src/app.c:219-226` hardcodes dynamic UI text element names such as HUD field
  names.

Why this matters:

- The architecture distinguishes application UI assets under
  `assets/ui_elements/` and `assets/ui_layouts/` from authored game menus.
- Code-driven UI policy can be appropriate for composition and buffers, but it
  should be intentionally separated from data that designers or tools should own.

Corrective-action direction:

- Classify each value before moving it:
  - resource/buffer constraint that should remain code,
  - app policy constant that may remain code,
  - layout/style data that should live in existing UI asset files.
- Avoid moving values to data without tests and a clear authority rule.

#### C2. Menu layout-name mapping is code-driven

Severity: **Low to Medium**

Evidence observed on 2026-09-11:

- `src/app.c:279-289` maps `MENU_MAIN`, `MENU_PAUSE`, `MENU_SETTINGS`, and
  `MENU_CONFIRM_QUIT` to string layout names.

Assessment:

- This may be acceptable because `MenuId` is application state and not authored
  game data.
- If application menus become configurable or tool-authored, this mapping becomes
  a data-authority issue.

Corrective-action direction:

- Leave as code unless app menu topology becomes authorable/configurable.
- If moved, keep the app/authored-menu separation from `ARCHITECTURE.md` intact.

#### C3. Asset roots are hardcoded at app/editor startup

Severity: **Low**

Evidence observed on 2026-09-11:

- `src/app.c:326-337` uses hardcoded roots including `assets/materials`,
  `assets`, `assets/maps`, and `assets/scenes`.

Assessment:

- These paths likely match current repository conventions.
- They become a problem if project roots become configurable or workspace-based.

Corrective-action direction:

- Do not change by default.
- Revisit only with a broader project-root/configuration requirement.

#### C4. Editor UI preview theme is hardcoded

Severity: **Low**

Evidence observed on 2026-09-11:

- `src/unified_editor.c:39-46` defines `editor_ui_preview_theme` in code.

Assessment:

- A hardcoded editor preview theme may be fine.
- If editor themes become authorable, this should move to an appropriate data or
  preferences boundary.

Corrective-action direction:

- Leave alone unless theme configurability is requested.

### D. Parser, buffer, and C safety

#### D1. Color parsing is duplicated and inconsistent

Severity: **High**

Evidence observed on 2026-09-11:

- `src/asset_loader.c:75-85` parses color with `sscanf("%d,%d,%d,%d", ...)`,
  casts directly to `uint8_t`, and silently leaves the previous/default color on
  malformed input.
- `src/ui_ele.c:97-108` parses color with `sscanf("%d,%d,%d,%d", ...)`, casts
  directly to `uint8_t`, and returns a fallback.
- `src/ui_document.c:178-187` parses color with
  `sscanf("%u,%u,%u,%u%c", ...)`, checks channels are no greater than 255, and
  rejects trailing characters.

Why this matters:

- Negative or greater-than-255 values may wrap through `uint8_t` in some paths.
- Similar data concepts reject or accept invalid input differently across
  modules.
- The project standard prefers checked parsing with full consumption and range
  validation.

Corrective-action direction:

- Decide the intended compatibility behavior before changing parser results.
- Add regression tests covering valid colors, malformed colors, negative values,
  values greater than 255, and trailing garbage.
- Then replace permissive parsing with a shared or consistently duplicated
  bounded parser using checked numeric conversion.
- If legacy fallback behavior must remain, make the fallback explicit and
  diagnostic-aware where appropriate.

#### D2. `sscanf` remains in source parsers

Severity: **Medium to High**

Evidence observed on 2026-09-11:

- `src/asset_loader.c:77`
- `src/ui_document.c:182`
- `src/ui_ele.c:103`

Assessment:

- `ui_document.c` is safer because it checks bounds and trailing characters.
- `asset_loader.c` and `ui_ele.c` are higher risk due direct casts and fallback
  behavior.

Corrective-action direction:

- Replace with checked conversion during a parser-focused cleanup.
- Preserve or intentionally revise invalid-input behavior with tests.

#### D3. Guarded `strcpy` remains in `asset_loader.c`

Severity: **Medium**

Evidence observed on 2026-09-11:

- `src/asset_loader.c:872`
- `src/asset_loader.c:888`

Context:

- Both calls are preceded by a length check against `sizeof(filepath)`.
- The immediate overflow risk appears mitigated, but unchecked string-copy calls
  are still contrary to the project’s buffer-safety preference.

Corrective-action direction:

- Replace with `snprintf` or a bounded path-join helper when touching this area.
- Add tests for long material directory and file names if behavior changes.

#### D4. Silent fallback behavior should be reviewed

Severity: **Medium**

Evidence observed on 2026-09-11:

- `asset_loader.c` color parsing can silently leave default colors.
- `ui_ele.c` color parsing can silently use fallback colors.

Why this matters:

- The current style standard says abnormal outcomes should not be silently
  ignored.
- Some fallback behavior may be compatibility policy, but it should be explicit
  and tested.

Corrective-action direction:

- Classify each fallback as `STATUS`, `INPUT`, or intentional legacy tolerance.
- Add diagnostics only at the boundary that owns the response.

### E. Dependencies and handwritten-code module boundaries

#### E1. No non-SMC header include cycles were found in the audit pass

Severity: **Positive observation**

Evidence observed on 2026-09-11:

- A local include graph over project headers reported no header cycles.

Corrective-action direction:

- Preserve this invariant during future module extraction.
- New modules should use forward declarations and narrow headers where possible.

#### E2. Include fan-out/fan-in hotspots indicate coupling

Severity: **Medium**

Highest local include fan-out observed:

- `src/app.c`: about 31 local includes.
- `src/unified_editor.h`: about 20 local includes.
- `src/unified_editor.c`: about 9 local includes.
- `src/scene_document.h`: about 9 local includes.
- `src/raycast.c`: about 9 local includes.
- `src/raycast.h`: about 8 local includes.

Highest local include fan-in observed:

- `src/assets.h`: about 20 incoming local includes.
- `src/map.h`: about 15.
- `src/config.h`: about 14.
- `src/grid.h`: about 14.
- `src/camera.h`: about 13.
- `src/checked_size.h`: about 13.
- `src/scene_types.h`: about 12.

Assessment:

- High fan-in for foundational types may be acceptable.
- High fan-out from a public feature header such as `unified_editor.h` is more
  concerning.

Corrective-action direction:

- Track include impact when extracting editor modules.
- Prefer narrower public interfaces and private `.c` includes.
- Do not introduce circular includes.

### F. Legacy, deprecated, and redundant ordinary-code paths

#### F1. Deprecated scene/map APIs are guarded but still present

Severity: **Managed debt**

Evidence observed on 2026-09-11:

- The Makefile has a `check-legacy-unused` target that prevents unexpected
  production callers of deprecated or legacy symbols such as `scene_document_load`,
  `write_map_digits`, `scene_document_save`, and `unified_editor_load_scene`.

Assessment:

- This is good regression protection.
- These symbols are still part of compatibility debt and should remain classified
  until removed or replaced by policy.

Corrective-action direction:

- Do not remove guardrails before replacing/removing the deprecated APIs.
- Keep ordinary-code legacy cleanup separate from SMC.

#### F2. Legacy map path remains intentional compatibility

Severity: **Managed debt**

Evidence:

- The current architecture identifies legacy maps under `assets/maps/*.txt` as
  imported rather than overwritten by native scene editing, with a deprecated
  Start Game path.

Corrective-action direction:

- Treat legacy map code as intentional compatibility unless a removal plan is
  approved.
- Do not delete legacy paths as part of general standards cleanup.

#### F3. Multiple non-SMC feature paths should be periodically justified

Severity: **Low to Medium**

Assessment:

- Several renderer/editor/data paths exist because the project has accumulated
  roadmap phases and compatibility layers.
- This audit did not prove dead code in ordinary modules.

Corrective-action direction:

- Future dead-code cleanup should start from call-graph evidence and tests.
- Do not remove alternatives only because they are old.
- SMC alternatives remain out of scope for this document.

### G. Test seams

#### G1. Test-only runtime-build failure hook is exposed in production header

Severity: **Medium**

Evidence observed on 2026-09-11:

- `src/unified_editor.c:37-50` defines a global runtime-build failure test hook.
- `src/unified_editor.h:282` exposes
  `unified_editor_set_runtime_build_failure_for_test(bool fail);`.
- Tests use this hook to verify rollback/failure paths.

Why this matters:

- The hook is valuable for tests.
- It is also production-visible API surface that does not represent normal
  runtime behavior.

Corrective-action direction:

- Preserve failure-path test coverage.
- Consider a test-only compile guard, internal test seam, or explicit dependency
  injection around runtime rebuild failures.
- Do not remove the hook without replacing the tested failure coverage.

## Recommended corrective-action sequencing

1. **Parser and buffer safety cleanup**
   - Address duplicated color parsing, `sscanf`, and guarded `strcpy` patterns.
   - Add regression tests first.
   - Decide invalid-input compatibility before changing behavior.

2. **Comment cleanup and contract documentation**
   - Remove stale historical-plan references from source comments.
   - Reduce tutorial comments that duplicate obvious code.
   - Add narrowly scoped comments for ownership, failure, rollback, parser, and
     state-machine invariants.

3. **App/UI data-boundary classification**
   - Classify hardcoded UI sizes, colors, dynamic text names, menu layout mapping,
     and asset roots.
   - Move only values that clearly belong to app UI data or configuration.

4. **Incremental editor module extraction**
   - Start with the smallest stable `unified_editor` subdomain.
   - Preserve behavior and tests before/after extraction.
   - Track include fan-out and public header shrinkage as acceptance criteria.

5. **Scene document/format seam review**
   - Review parser, migration, validation, document ownership, and runtime-build
     seams before adding more responsibilities.
   - Extract only where it improves ownership and testing clarity.

6. **Legacy ordinary-code review**
   - Keep compatibility guardrails until explicit removal plans exist.
   - Separate legacy map/native scene decisions from general style cleanup.

7. **Test seam cleanup**
   - Replace production-exposed test hooks only after equivalent failure-path test
     support exists.

SMC remains excluded from all sequencing above.

## Regression tests required before fixes

Future corrective action should add or update focused tests for:

- Color parser valid inputs.
- Color parser malformed inputs.
- Negative color channels and channels greater than 255.
- Trailing garbage after numeric fields.
- Long material paths and material filenames if path joining changes.
- Silent fallback/diagnostic behavior for malformed app UI and asset data.
- Editor runtime rebuild rollback if the test hook is moved or replaced.
- Any extracted `unified_editor` submodule behavior, especially modal transitions,
  dirty/save prompts, workspace activation, and command rollback.

## Explicit non-goals

Do not use this document to justify:

- Broad rewrites of `unified_editor.c`.
- Removing legacy map or deprecated scene compatibility paths without a dedicated
  removal plan.
- Hiding `UnifiedEditorState` all at once.
- Introducing macro-heavy generic parsers or framework-like abstractions.
- Moving code constants to data without a clear authority rule.
- Changing parser rejection behavior without tests.
- Editing any SMC source, SMC header, SMC build mode, SMC generated artifact, or
  SMC benchmark path.

## Verification performed for this audit

Read-only inspection performed on 2026-09-11:

- Read current style and architecture documents.
- Inspected Makefile build/test layout.
- Listed source/header inventory.
- Measured largest source/header files.
- Checked source/header pairing exceptions.
- Searched source for TODO/FIXME/HACK/legacy/deprecated-style markers.
- Searched source for `sscanf`, `strcpy`, and related parser/string patterns.
- Built a local include graph and checked for header cycles.
- Measured local include fan-out/fan-in.
- Inspected representative source ranges for `unified_editor`, `app`,
  `asset_loader`, `ui_ele`, and `ui_document`.

No behavior-changing edits were made during the audit. No build or runtime tests
were required for the read-only audit pass.
