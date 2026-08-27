# R10 Requirements and Implementation Plan — Colored and Expanded Lighting — 2026-08-27

## Status

**Q1 decision record locked and I1–I3 increment plan recorded on 2026-08-27.**
The Q1 decision record is `R10_DECISION_RECORD_2026-08-27.md`; this document is
the Q2/Q3 increment plan. **I1 and I2 are Implemented and Verified: automated
gates pass and the combined manual visual/input acceptance passed on
2026-08-27.** R9 is Verified; this phase's
prerequisites (R6 point-light authoring and R8/R9 geometry, occlusion, and
occlusion, and scalar transmission semantics) are satisfied.

## Scope

Implement colored and expanded lighting for the current world model:

1. **I1 — Colored illumination.** Make `Light.color` actually illuminate surfaces
   (fixes the roadmap forbidden shortcut "no color stored but ignored"). Migrate
   the scalar `light_map` to per-channel light, propagate through `palette_sample`,
   add alpha authoring, and keep transparency composition free.
2. **I2 — Spot lights.** New serialized light type with direction, cone, and
   falloff, plus editor controls and a scene-version migration.
3. **I3 — Research track.** Directional, area, and emissive lighting: evidence
   write-up, then accept/defer/reject per phase.

Locked decisions (Q1) in `R10_DECISION_RECORD_2026-08-27.md` are binding for all
increments.

## Prerequisites

- R6: point-light authoring (place/delete/select, transactional fields).
- R9: vertical geometry, per-channel translucency/transmission, and the current
  renderer passing 6 ms budget (raised path) and current-renderer flat follow-up.

## I1 — Colored illumination

**Goal:** A colored point light tints surfaces per channel; multi-light and
anti-light behavior and clamping are deterministic; alpha is editable; colored
samples retain their channels through existing scalar translucent composition.

**Task set:**

1. Replace the scalar `Map.light_map` (`double*`) with a per-channel store across
   all three allocators (`src/map.c`, `src/scene_format.c`, `src/map_loader.c`),
   the `src/lighting.c` reader/writer, the four `src/raycast.c` readers, and the
   resize-copy in `src/scene_document.c:2073`.
2. Update `palette_sample` to scale each channel independently and add per-channel
   clamp at `[0,255]`.
3. In `src/lighting.c`, accumulate per-channel contributions
   `contribution_c = intensity * (component_c/255)` for red/green/blue; apply
   shadow/bounce attenuation to each channel; anti-light subtracts per channel.
4. Add `EDITOR_LIGHT_FIELD_ALPHA` to the light inspector enum and wire the
   metadata, format, and step-request switches; refresh runtime data on edits.
5. **Deferred from I1:** per-channel transmitted-light tinting requires RGB
   transmission data; R9 exposes one scalar byte, and I1 forbids a format bump.

### Increment exit-gate (I1)

Deterministic tests cover:
- exact single-light per-channel math;
- multi-colored accumulation (additive, per-channel clamp);
- anti-color (negative intensity) per-channel subtraction;
- `alpha` scaling and editor field wiring;
- cache hits applying current color/alpha/intensity and geometric-key changes
  producing misses;
- save/reload round-trip with unchanged v6 format (color already round-trips);
- a paired scalar→colored benchmark confirming the per-channel cost stays inside
  the existing lighting budget.

Strict `-Wall -Wextra -Wpedantic -Werror`, ASan/UBSan, `make check`, smoke,
`git diff --check` pass.

## I2 — Spot lights

### Goal

Add spot lights with direction, cone, falloff, and editor controls, with a formal
version bump and migration.

### I2 work items

1. Add spot-light fields (direction, cone, falloff) as validated scene data.
2. Add a **scene version bump with migration** (v6→v7, mirroring R9 v5→v6), keep
   backward compatibility for older files, strict unknown-key parsing.
3. Runtime cone test in the lighting DDA: skip tiles outside cone, apply falloff
   curve (honoring the parsed-but-unused `light_falloff_default` config).
4. Editor controls: Direction (0–2π), Cone, Falloff across the light inspector.
5. Full deterministic tests: place/edit/remove spot, round-trip, undo/redo,
   migration path, cache behavior.

### Increment exit-gate (spot)

Deterministic tests for spot math, editor field editing, round-trip, migration,
undo, and cache; strict/passing sanitizers/smoke/`make check`.

**Implementation status (2026-08-27):** automated exit gate passed; the
combined manual visual/input acceptance passed in a live display session on
2026-08-27 and I2 is Verified. See `R10_INCREMENT_I2_IMPLEMENTATION_RECORD_2026-08-27.md`
and `R10_NATIVE_SCENE_V7_SPEC_2026-08-27.md`.

## I3 — Research tracks (roadmap item 4)

Evaluate directional, area, and emissive lighting with evidence write-ups
(reuse the research-doc conventions) and decide accept/defer/reject per type.

### I3 outcome (2026-08-27)

**Research track complete; all three candidates DEFER (one REJECT).** No
shipping code, scene data, cache key, or runtime value is changed. Evidence
is recorded in:

- `R10_I3_P1_DIRECTIONAL_FINDINGS_2026-08-27.md` — directional: DEFER.
- `R10_I3_P2_AREA_FINDINGS_2026-08-27.md` — area (discrete): DEFER; area
  (analytic): REJECT for I3.
- `R10_I3_P3_EMISSIVE_FINDINGS_2026-08-27.md` — emissive surfaces: DEFER with
  a documented v8 upgrade path (material default + sparse per-cell override,
  reserved `OPTICAL_OVERRIDE_*` bit, apply at sample time after light-map).
- `R10_I3_RQ5_RQ6_SYNTHESIS_2026-08-27.md` — combined policy table, RQ6
  envelope (colored-lighting 6 ms gate; white 0.247 ms / colored 0.226 ms /
  spot 0.152 ms; cache-enabled 0.080 ms), D1–D3 recommendations
  (v8, gated, one new type per phase).

The I3 record is `R10_INCREMENT_I3_IMPLEMENTATION_RECORD_2026-08-27.md`. No
implementation is authorized; any future v8 work must start with its own Q1
review and a paired benchmark.

## Test impacts

- `src/map.c`, `src/scene_format.c`, `src/map_loader.c` — representation/allocation.
- `src/lighting.c`, `src/lighting_cache.h/.c` — per-channel math + cache.
- `src/raycast.c` (four `light_map` readers; `palette_sample` from
  `src/assets.h`/`src/assets.c` gains per-channel light input) and
  `src/scene_document.c` (light-array resize copy) — readers.
- `src/editor_domain.h/c` — alpha field (I1).
- Tests: `test_lighting.c`, `test_core.c`, `test_editor_domain.c`,
  `test_unified_editor.c`, `test_scene_document.c`, `test_scene_format.c`,
  new colored-light fixture.

## Affected files checklist (I1)

| File | Change |
|---|---|
| `src/map.h/.c` | `light_map` → per-channel store + allocator |
| `src/scene_format.c` | allocator in native-parser path |
| `src/map_loader.c` | allocator |
| `src/lighting.c` | per-channel accumulate/read, candidate transmission |
| `src/raycast.c` | four `light_map` reader sites updated |
| `src/assets.h/.c` | `palette_sample` per-channel light signature |
| `src/editor_domain.h/.c` | `EDITOR_LIGHT_FIELD_ALPHA` |
| `src/scene_document.c` | resize-copy + invalidation |
| `src/lighting_cache.h/.c` | per-channel result + revision |
| `tests/test_lighting.c`, `test_core.c`, `test_scene_document.c`, `test_editor_domain.c`, `test_unified_editor.c`, `test_scene_format.c` | per-channel assertions, alpha field coverage, round-trip |

## Exit gates (phase)

- R10 Q1 decision: **passed**.
- Deterministic, persistable, editable, and benchmarked on every increment
  (roadmap exit gate).
- Strict/ASan/UBSan/smoke/`git diff --check`.
- Research-track write-up for I3-required evidence.

## Review checkpoint

Targeted lighting/data/performance review; Q4 if renderer or scene ownership
changes broadly.
