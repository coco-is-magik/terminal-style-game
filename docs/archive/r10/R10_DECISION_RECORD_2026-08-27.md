# R10 Decision Record — Colored and Expanded Lighting — 2026-08-27

## Authority

This record captures the product and architecture decisions made in the 2026-08-27
design thread before R10 implementation begins. Decisions are binding for all R10
increments and are grounded in existing code. The authoritative requirements and
increment plan is `R10_REQUIREMENTS_AND_IMPLEMENTATION_PLAN_2026-08-27.md`.

## Scope

R10 is "Colored and expanded lighting" from `docs/FEATURE_ROADMAP.md`. The roadmap
requires deciding RGB-versus-stylized accumulation, implementing colored-light
mixing and transmission, and adding spot lights. Research prototypes cover
directional, area, and emissive types and are separate.

## Code-grounding findings at decision time

- `SceneLight` already carries `id`, `x`, `y`, `red`, `green`, `blue`, `alpha`,
  `intensity`, and `radius` (`src/scene_types.h`). The scene writer already emits
  `color = r,g,b,a`, and the native parser round-trips it.
- The light inspector already exposes X, Y, Red, Green, Blue, Intensity, and
  Radius as transactional field edits (`src/editor_domain.h/c`). Alpha is stored
  and persisted but has **no inspector field today**.
- `Light.color` currently affects **only the billboard marker** (`src/raycast.c:418`).
  Surface illumination is scalar: `Map.light_map` is a single `double` per tile
  written only by `src/lighting.c` and read at four sites in `src/raycast.c` plus
  a resize-copy in `src/scene_document.c:2073`.
- The scalar light array is allocated in **three** places: `src/map.c`,
  `src/scene_format.c` (native parser), and `src/map_loader.c`.
- `src/lighting.c` is both a writer and a reader of `light_map`; it is **not** a
  pure writer.
- `palette_sample()` scales near/mid/far base RGB by one scalar light level and
  clamps each channel to `[0, 255]`. R9 composes already-lit per-channel `Cell`
  samples, so applying colored light at sampling time composes through translucent
  layers for free.

## 1. Mixing model

**Decision:** **RGB per-channel** accumulation. Each light contributes a
per-channel amount `intensity * (component/255)` per tile; tiles accumulate three
channels; `palette_sample` scales each channel independently; overbright clamps
per channel to `[0,255]`; anti-light (negative intensity) subtracts per channel.

**Rejected alternatives (recorded for evidence):**

- **Stylized palette-relative (option B):** a single scalar luminance plus a
  normalized color ratio. Combination of multiple colored lights has no canonical
  answer, anti-light with a scalar has ambiguous semantics, and a per-channel
  transmission filter (R9) fights the model. It also risks satisfying nothing but
  a decorative tint, which the roadmap forbids ("no color stored but ignored").
- **Hybrid scalar + additive tint (option C):** a correct tint must be
  shadow/occlusion-aware and per-channel, so it ends up computing per-channel
  data anyway; it reintroduces two interacting models and two code paths.

## 2. Representation change — single deliberate migration

**Decision:** Change `Map.light_map` from a scalar `double*` to a per-channel
store **atomically in one change**, migrating all three allocators
(`src/map.c`, `src/scene_format.c`, `src/map_loader.c`), the `src/lighting.c`
writer/reader, the four `src/raycast.c` readers, and the resize-copy in
`src/scene_document.c:2073`. No scalar + tint dual path is introduced.

**Rationale:** mirrors the renderer-consolidation principle applied to the
heightfield renderer: change the seam once, deliberately, rather than wedging two
structures together forever. The reader/writer set is small and enumerated.

## 3. Transmission composes for free

**Decision:** Apply colored light at sampling time (before R9 per-channel
composition). Translucent layers then filter light per channel via resolved
`optical.transmission` in the lighting path only; no compositor change.

## 4. Alpha is an emitted channel weight

**Decision:** `SceneLight.alpha` (already persisted) is the emitted channel weight;
the full per-channel value is `intensity * (component/255) * (alpha/255)`, with
`alpha=255` preserving the existing scalar model. The editor exposes a new
`EDITOR_LIGHT_FIELD_ALPHA` (0–255, step 1) in place of the currently absent
control. Alpha scales illumination only; whether it also gates the billboard
marker's visibility is an open follow-up (see below).
## 5. Increment ordering

**Decision:** I1 colored illumination (no serialized change, since `color` already
round-trips), I2 spot lights (new serialized fields), I3 research track. Mirroring
R6/R9 precedent, I1 is the narrowest usable slice and I2 adds the serialization
decision with a formal format version.

## 6. I1 — no serialized change

**Decision:** I1 introduces no scene version bump; `color = r,g,b,a` already
round-trips and the editor already edits R/G/B. Alpha gains an editor field but
the format is unchanged.

## 7. I2 spot — v7 bump + migration

**Decision:** Spot lights require new serialized fields (direction, cone, falloff).
Follow the R9 v5→v6 precedent: add a scene version bump with migration rules, keep
backward compatibility for v6 (and older) files, and do not relax strict
unknown-key parsing.

## 8. Cache

**Implementation refinement (2026-08-27):** `LightSampleResult` remains a scalar
geometric/shadow attenuation result. RGB, alpha, and intensity apply after every
lookup, so appearance edits use cached geometry without stale color or unnecessary
global invalidation. Position/radius and map identity are explicit key inputs.
This supersedes the planning-time per-channel-cache proposal; deterministic cache
hit/miss vectors protect the refined boundary.

## 9. Editor is part of I1

**Decision:** Because color today affects only the billboard, I1 must include the
editor step (add `EDITOR_LIGHT_FIELD_ALPHA`, wire metadata/format/step-request
switches, and refresh runtime light data on color/alpha edit) so colored light has a
user-visible, testable result.

## Open follow-ups (recorded, not blockers)

- Per-scene ambient light and per-channel ambient (separate from R10 as a format
  change; tracked in TODO).
- Per-channel transmission tinting (deferred from I1: R9 transmission is scalar;
  RGB transmission requires a later data/format decision).
- Whether alpha affects the billboard marker's visibility, not just illumination.
- Shadow policy for directional/area/emissive research types.
