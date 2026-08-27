# R10 I3 P3 Findings — Emissive Surfaces — 2026-08-27

## Outcome

**P3 complete; evidence recommends DEFER with a documented upgrade path.**
Emissive cells (floor/ceiling/wall) are conceptually orthogonal to the
lighting loop; current per-tile sampling reads `light_map` before surface
optics, so adding an "emit" term would require an explicit ordering rule
and a new place in the data model. No shipping code, scene data, or runtime
value is changed.

## Prototype boundary

- Read-only analysis; no new translation unit and no new tests.
- `LightLevel { red, green, blue }` (`src/raycast.c`, sampled by
  `palette_sample` in `src/assets.c`) is the only per-tile light data path.
- `SceneAuthoredCell` and the v6 optical extension
  (`src/scene_format.h:19-44`, `src/optical_runtime_view.h`) already host
  per-cell data; an "emit" term would join this surface.

## Required model

An emissive surface is a typed per-cell (or per-face) property:

- `emissive_intensity` — non-negative scalar in the same units as
  `intensity` (`intensity > 0` here; no anti-emit).
- `emissive_color` — three channels, 0..255, separate from
  `Material.palette_id` to avoid coupling.
- Validation: `0 ≤ intensity ≤ INTENSITY_MAX`, with an explicit default of
  `0` (no emission) so the new field does not change the appearance of any
  existing scene.

## Conflicts with the current implementation

1. **`palette_sample` reads the per-tile `light_map` only.** There is no
   per-cell `emit` input. Adding one would either extend `palette_sample`'s
   signature (touching every cell sample site) or sit in a new sampler
   invoked before sampling; both require a per-cell lookup that the
   prepared-column tracer does not currently do.
2. **v6 optical extension already exists** and is the natural home for a
   typed override, but a new "emit" field is a v8 data change because
   `OPTICAL_OVERRIDE_*` mask bits are part of the v6 format. A new bit
   requires migration rules and a new bit number that is reserved today
   (`src/optical_runtime_view.h`).
3. **Spot lights can already emulate an "emissive cone"** by setting
  `intensity = 1`, `cone = 2π`, and a high `falloff`. The roadmap
  forbids shortcuts, but the converse also matters: emissive surfaces
  must not silently reinterpret the semantics of an existing spot light.
4. **Editor UX must learn about a new property** on every cell, decal, and
   face. R5/R6 authoring patterns put these on the material or on a sparse
   cell override; both are admissible and Review H must choose one.
5. **Performance impact is the same as one extra per-tile add** at sample
   time. Cheap when the override is sparse, more expensive when dense;
   no data exists yet to support a claim either way.

## Spot/emissive separation

A spot light with a wide cone is not equivalent to an emissive surface:

- A spot is a per-instance authored object with a position, a direction,
  a color, a radius, and (now) a cone and a falloff; it is selected by
  stable scene ID and removed in a separate inspector row.
- An emissive surface is a per-cell property of the geometry; it is
  selected by selecting the cell, not a separate object.

The inspector already exposes a single `LightLevel` per tile; adding an
`emit` overlay is a new authoring concept and must not be folded into
the existing spot inspector. The decision record's
"shadow policy for directional/area/emissive research types" note applies
verbatim.

## Performance envelope (code-grounded estimate)

- Sparse per-cell override: cost is one byte/pixel of memory in the
  per-cell cache and a single `add` per sample; both are negligible
  relative to the colored-lighting benchmark (white 0.247 ms, spot
  0.152 ms, cache-enabled 0.080 ms).
- Dense per-cell override: would add up to one byte per cell in the
  authored state plus a per-sample add; the budget is the same as the
  current I1 colored-illumination cost, well within the 6 ms gate.
- The performance claim is unverified for the dense case; P3 does not
  require a measurement today, but any future implementation must
  produce a paired `make benchmark-colored-lighting` run with a dense
  emissive overlay.

## D1 — schema vehicle recommendation

**P3 recommendation: do not implement an emissive field in I3.** If a
future phase authorizes it, the cleanest path is:

1. Add `OPTICAL_OVERRIDE_EMISSIVE` (a reserved new bit) to the v6
   extension in a new v8 with strict migration; the legacy
   `OPTICAL_OVERRIDE_*` mask bits remain unchanged.
2. Author the field as material-level default plus sparse per-cell
   override, mirroring the R9 P1 D2 recommendation.
3. Apply the overlay at sample time, after the existing
   `palette_sample` light-map read, with a documented ordering rule
   (emit applied last, before per-channel clamp).
4. Cover the change with a focused emissive test plus the existing
   `make -j2 check`, `make asan`, `make ubsan`, and the
   colored-lighting benchmark.

## Forbidden shortcuts recorded

- Do not attach `emit` to the existing `Light` array; the data model
  is wrong (per-cell surface property vs. per-instance point/spot).
- Do not reuse the spot's `cone = 2π` trick to simulate emission on
  wall surfaces; it would re-introduce the "invisible flag" anti-pattern
  the roadmap forbids.
- Do not implement emissive under a v6 amendment; the v6 mask bit space
  is part of the v6 spec and is reserved for future use.

## Reproduction

- Read-only inspection; no commands.

## Next action

The RQ5/RQ6 synthesis (`R10_I3_RQ5_RQ6_SYNTHESIS_2026-08-27.md`) records
the combined directional/area/emissive outcome and the deferred plan.
Future authorization must be a separate Q1 review with its own decision
record and a paired performance envelope.
