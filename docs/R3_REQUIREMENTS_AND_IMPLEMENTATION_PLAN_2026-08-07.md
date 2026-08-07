# R3 Requirements and Implementation Plan — Generalized Editor Domain Foundation

**Plan date:** 2026-08-07  
**Status:** Active  
**Prerequisite:** R2 Verified  
**Review checkpoint:** Review D

## Objective

Support multiple typed authored domains without turning `UnifiedEditorState` into
an owner of domain rules. Prove each shared seam with the existing wall-face
workflow and one concrete stable-ID scene domain: authored point lights.

## Locked decisions

1. The two proof domains are wall faces and `SceneLight` point lights.
2. Light selection stores `SceneInstanceId`, never a collection index or pointer.
3. R3 supports selecting, inspecting, and editing bounded light fields: position,
   RGBA color, intensity, and radius.
4. Light create/place/move/delete remains deferred to R6.
5. Shared inspector metadata remains typed and is introduced only where both
   concrete domains prove the need. There is no universal property bag.
6. UI invokes document commands. It never mutates authored scene fields directly.
7. Light picking, target validation, commands, and metadata remain deterministic
   and headless. SDL, rendering, and input adaptation remain outside those seams.
8. Point-light propagation remains scalar in R3. RGBA controls the visible source
   marker; colored surface illumination requires a future multi-channel light-map
   contract and is not represented as though it already exists.

## Required outcomes

- A discriminated selection target supports wall faces and stable-ID lights.
- Stable light references resolve through `SceneDocument` and invalidate cleanly.
- Wall and light mutations use one undo/redo history with atomic grouped commands.
- Narrow domain-specific inspector/tool adapters keep domain rules out of the
  unified editor controller.
- Wall and light targets provide domain-specific world highlights.
- The inspector migrates toward the shared data-driven UI using typed validated
  metadata proven by both domains.
- Existing wall selection, material editing, Save, Open/Import, and dirty workflows
  remain green.
- Point-light numeric fields support held Left/Right repeat and inline typed replacement
  through the same typed command boundary.
- Authored ambient/intensity/radius changes remain visually distinguishable from
  camera-distance wall/decal material presentation.

## Forbidden shortcuts

- No raw pointer or collection-index selection identity.
- No untyped property dictionary or generic string-to-value mutation API.
- No direct UI mutation of `SceneDocument`.
- No giant controller-owned switch containing all domain rules.
- No light placement/deletion or unrelated R4–R6 authoring scope.
- No scene-format change unless an implemented R3 field cannot round-trip in v1.

## Increment A — Typed selection and stable references

- Add a light target variant carrying `SceneInstanceId`.
- Add a const document resolver for lights by stable ID.
- Add allocation-free, deterministic center-ray light picking over borrowed
  authored lights. Reject invalid IDs/non-finite positions, respect max range and
  wall occlusion, select nearest forward hit, and break exact ties by stable ID.
- Compose document resolution in the unified editor without coupling the pure
  geometric picker to document lifecycle or I/O.
- Preserve existing wall hit results when no light is eligible.
- Light selection is visible in editor status text but does not open the wall
  inspector; inspector support belongs to Increment C.

**Increment A evidence:** focused selection, scene-document, and unified-editor
runners; strict application build; aggregate suite; ASan/UBSan on changed seams.

## Increment B — Commands and atomic groups

- Add typed light-field commands through `CommandHistory`.
- Add grouped transactions that reserve all required capacity and validate all
  mutations before commit; one group is one undo/redo step.
- Prove grouping with a real multi-field light edit.
- Preserve state IDs, dirty tracking, redo invalidation, and rollback on failure.

## Increment C — Domain inspector/tool adapters

- Introduce the smallest domain adapter contract proven by wall and light tools.
- Move wall-material inspector behavior behind the wall adapter without changing
  controls or results.
- Add typed light fields with bounded values and command-producing edits.
- Keep all authored mutations behind commands.

## Increment D — Highlight providers

- Preserve the existing allocation-free wall-face provider.
- Add a projected light marker keyed by stable target identity.
- Keep highlights derived, occlusion-aware, and independent of authored state.

## Increment E — Inspector UI migration and closeout

- Drive common inspector presentation from typed field metadata proven by both
  domains; avoid speculative fields or domain-independent mutation logic.
- Run Q3: strict aggregate suite, applicable matrix, sanitizers/leak checks,
  interactive acceptance, and benchmark/stability if hot rendering paths change.
- Perform Review D, record findings, and mark R3 Verified only after its exit gate.

## Verification inventory

- Wall selection and face calculation remain unchanged.
- Light picking: nearest, behind-camera, off-ray, behind-wall, max-range,
  non-finite, invalid-ID, and stable-ID tie cases.
- Stable resolver: found, missing, invalid ID, null document.
- Commands: normal/no-change/invalid target, undo/redo, group rollback, OOM, and
  state-ID exhaustion.
- Inspector adapters: correct typed fields/ranges and command-only mutation.
- Highlights: wall regression and light projection/occlusion.
- Lighting: exact authored ambient, intensity/radius falloff, saturation, and stable
  wall color independent of camera-distance palette bands.
- Full Save/Open/reload round-trip after light edits.

## Exit gate

At least wall faces and point lights exercise the shared selection, command,
inspector, and highlight seams; current wall behavior remains green; Q1–Q3 pass;
and Review D records no blocker.
