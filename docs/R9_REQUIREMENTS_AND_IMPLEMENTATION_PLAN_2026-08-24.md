# R9 Requirements and Implementation Plan — Layered Optical Rendering — 2026-08-24

## Status
**Active; I1–I7 complete and verified. I8 preset policy remains deferred.**
Pre-implementation Review H conditionally passed on
2026-08-24 (`reviews/2026-08-24-roadmap-r9-review-h.md`). The second Review H on
2026-08-25 (`reviews/2026-08-25-roadmap-r9-review-h-implemented-phase.md`) accepted
the implemented optical phase and authorized advancement to v6 work. Editor work
is complete; mirror work is complete; preset work remains deferred.

Research evidence is complete and unchanged: P1 typed semantics, P2 bounded ordered intersections, P3 terminal-cell compositing, P4 one-bounce mirrors, RQ5 ordering, and RQ6 budget (see the P1–P4 findings and `R9_RQ5_RQ6_SYNTHESIS_2026-08-24.md`). All research prototypes remain compile-time gated behind `R9_OPTICAL_RESEARCH=1`.

## Scope

Implement layered optical rendering for the R8 vertical world, in this strict order:

1. A derived runtime optical lookup seam over authored material defaults and sparse per-cell overrides.
2. Selective sight continuation inside the existing prepared-column tracer.
3. Selective terminal-cell optical composition (P3 rules).
4. Mandatory stop-gate review of the runtime seams before any schema/editor work.
5. Scene **v6** persistence/migration carrying typed optical authored data (material defaults plus sparse per-cell overrides).
6. Editor authoring of optical properties with transactional undo/redo.
7. Optional, separately gated future increment: coarse-reuse single-bounce mirrors.

**Out of scope (locked):** colored/spot lighting (R10), sprites/entities (R11), horizontal mirrors, reflected entities, mirror enablement in the default shipping path, and any final quality-preset promise. These must not re-enter R9 disguised as incremental scope.

## Locked decisions (from Review H)

1. **Typed independent semantics.** Occupancy, player collision, sight-ray interaction, light interaction, visual opacity, transmission, and reflectivity are independent authored semantics. No single ambiguous "invisible" flag.
2. **Lookup/ownership.** Rendered and runtime values are derived; authored semantics are never collapsed for speed. The derived representation is chosen by profiling.
3. **D1 — Schema vehicle: v6.** Do not silently amend v5. v6 defines migration defaults, canonical ordering, diagnostics, and unknown-block policy.
4. **D2 — placement.** Material-level defaults plus sparse authored per-cell overrides. Zero/default values must reproduce current migration, round-trip, collision, rendering, and checksums exactly.
5. **D3a — hard caps.** At most four optical layers per sample and at most one mirror bounce. Both are upper bounds; neither runs unconditionally.
6. **D3b — presets deferred.** Only compatibility/default behavior is currently validated end-to-end. No low/medium/high names or thresholds ship until integrated measurements exist.
7. **Ordering.** Two domains: the depth-aware world-cell domain, then the z/insertion-ordered UI-pixel domain. Existing decal (nearest depth, creation index, pattern row-major) and editor/UI precedence are preserved.
8. **Sight/light rules.** Sight continues only when a resolved hit is non-blocking and transmission is positive. Opacity is never the collision/ray/light signal; scalar light blocking and transmission remain independent.
9. **Composition baseline.** RGB rule per P3; nearest glyph at opacity ≥ 128; opening fallback is explicit darkness; layer-cap exhaustion and proven opening remain distinct.
10. **Mirrors.** Darkness on reflected miss/opening; reflected mirrors are terminal at one bounce; retained for a later coarse-reuse increment, disabled by default.

## Engineering gates (every increment)

- Strict C11 build `-Wall -Wextra -Wpedantic -Werror`; `make check`.
- Deterministic cmocka tests with exact output and preserved-state guarantees.
- `make asan`, `make ubsan` (full ASan/LeakSanitizer is the accepted memory evidence).
- Benchmark/stability inside the existing 6.000 ms surface budget.
- Flat-default framebuffer checksum parity unchanged from the R8 baseline.
- Occluded-decal checksum equals the raised-height no-visible-decal checksum.
- No per-frame allocation in any render/trace/compose path.
- New diagnostics recorded in `ERROR_CATALOG.md` with focused tests.
- Manual optical acceptance deferred to the user before a visible increment is recorded as complete.

## Module boundaries and dependency direction

- `SceneDocument` remains the authoritative owner of authored scene state.
- Research modules (`r9_optical_semantics`, `r9_optical_compositor`, `r9_mirror_trace`) remain research-only and excluded unless `R9_OPTICAL_RESEARCH=1` is defined.
- A new derived runtime optical view/lookup is added adjacent to the runtime, read-only, borrowing from `SceneDocument`, reconstructed on authored-data change.
- Rendering, selection, and highlights keep using existing bounded per-cell tracing.

Documentation changes only in this planning increment; no production or test source is changed.

## Requirements

### R1 — Derived optical view and parity
- Read-only derived view over authored defaults and sparse overrides.
- Exact default parity (occupancy, collision, sight, light, appearance).
- Flat-default framebuffer checksum parity identical to baseline.
- v5 round-trip byte-identical until v6 supersedes.
- No per-frame allocation.
- Focused parity/overhead tests. Proposed default overhead target: at most 0.100 ms on the surface scenario, subject to profiling.

### R2 — Selective continuation
- Keep the nearest-hit opaque fast path.
- From the nearest prepared interval, continue only when the resolved surface is non-blocking and sight-transmissive.
- Bound by four layers/one bounce; no duplicated tracing; no unconditional full-screen collection.
- Preserve terminal-opening and layer-cap-exhaustion semantics.

### R3 — Selective composition
- Deterministic far-to-near composition only on samples with additional optical layers.
- Preserve glyph threshold, opening/cap distinction, and current decal ordering.

### R4 — Opaque parity (cross boundary condition)
- A scene with only default optical values must keep the current framebuffer checksums and the current runtime view paths exactly.

### R5 — v6 persistence and migration
- Add v6 as new canonical version, preserving v5 as migration input.
- Persist material defaults and a sparse per-cell override list.
- Unknown-block policy, canonical ordering, diagnostics, and byte-equality tests.
- Older writers never discard optical data.

### R6 — Editor authoring
- Select/edit material optics and add/remove per-cell overrides.
- All writes through `command_system` as one atomic undoable group.
- Validation, ranges, rollback, preserved state.

## Diagnostics

Add new diagnostics once to `ERROR_CATALOG.md` (Planned → Active at the correct increment) with detection, context, recovery, preserved state, and a focused test. Reserved v6/lookup identifiers are planned; numbers are assigned at implementation within the TSG catalog rules.

## Increments

### I1 — Derived optical runtime lookup seam

**Status:** Complete and verified 2026-08-24. See `R9_INCREMENT_I1_IMPLEMENTATION_RECORD_2026-08-24.md`.

**Goal:** Zero-allocation derived optics with exact default parity; no schema/editor.
Tasks: (1) read-only derived view over authored scene machinery; (2) material + sparse override lookup with the four-layer cap; (3) a query path for the renderer; (4) tests: default parity, override, fallback, byte equality.

**Exit gate:** `make check`; benchmark/stability; default overhead ≤0.100 ms on the surface path; parity unchanged; allocation-free. Stop if parity fails.

### I2 — Selective continuation in the tracer

**Status:** Complete and verified 2026-08-25. See
`R9_INCREMENT_I2_IMPLEMENTATION_RECORD_2026-08-25.md`.

- Chain the derived view into prepared-column samplers.
- Add a bounded selective walk fixture reaching the four-layer cap without duplicating the DDA.
- Assert terminal-opening and layer-cap states per the evidence.

Exit: end-to-end coverage measurements; unconditional collection absent; capped per-sample cost ≤ baseline fast path; default parity retained.

### I3 — Selective composition (P3) in the render path

**Status:** Complete and verified 2026-08-25. See
`R9_INCREMENT_I3_IMPLEMENTATION_RECORD_2026-08-25.md`.

- Wire the compositor into a shipping-consumed seam behind resolved sight.
- Far-to-near; preserve glyph threshold/opening/cap and current ordering.

Exit: the focused optical fixture set passes deterministically; the default opaque path bypasses composition; current decal/editor/UI ordering unchanged; timing in budget.

### I4 — Mandatory stop-gate review (pre-v6)
**Status:** Complete and verified 2026-08-25. See
`reviews/2026-08-25-roadmap-r9-review-h-implemented-phase.md`.

I1–I3 gates passed: 6 ms surface budget, exact parity, zero render-loop allocation,
deterministic checksums, no unauthorized schema/editor/mirror/preset scope, and
clean sanitizers. Review H authorizes advancement to I5.

### I5 — v6 schema/migration/persistence (post I1–I4)
**Status:** Complete and verified 2026-08-25. See
`R9_INCREMENT_I5_IMPLEMENTATION_RECORD_2026-08-25.md` and
`R9_NATIVE_SCENE_V6_SPEC_2026-08-25.md`.

Implement the v6 spec, byte-equality, v1–v5 migration defaults, editor read/write,
canonical ordering, and diagnostics. v5 must load with no loss. No optical editor
authoring, mirror enablement, or preset work belongs in this increment.

### I6 — Editor authoring + undo/redo
**Status:** Complete and verified 2026-08-26. See
`R9_INCREMENT_I6_IMPLEMENTATION_RECORD_2026-08-26.md`.

Add optical inspector and per-cell override editing through the command system with atomic groups, validation, and transactional rollback.

### I7 — Optional coarse-reuse mirrors (gated, off by default)
**Status:** Complete and verified 2026-08-26. See
`R9_INCREMENT_I7_IMPLEMENTATION_RECORD_2026-08-26.md`.

Prototype reflected per-column interval reuse; bound mirror coverage; preserve one bounce and darkness fallback; enable only after an end-to-end coverage gate.

### I8 — Preset / e2e assessment (deferred)
After I1–I6 stabilize, instrument representative coverage and record cost; decide preset names/thresholds with evidence. Do not ship presets now.

## Verification

For each increment: focused deterministic runner and strict build; default/legacy checksum parity (benchmark/stability); ASan/UBSan; applicable manual optical checklist items. Record pass/fail and any host-variance outliers in the handoff and record.

## Exit gate

R9 is not Verified until:
- I1–I4 stop gates pass.
- v6 migration byte parity for all legacy inputs.
- Optical authoring passes; the default opaque frame stays ≤6 ms, deterministic, allocation-free.
- A second Review H records the implemented phase and disposition.
- Manual checks are recorded for visible optical increments.

## Manual acceptance (user-owned)

The user performs manual optical acceptance at each visible increment (I2+):
1. default/legacy scenes byte/visually unchanged;
2. single translucent over a wall readable/stable;
3. two translucent layers legible and deterministic;
4. translucent opening darkness without stale framebuffer;
5. decal ordering correct on optics/discontinuities;
6. editor hover/selection visible, primary wins;
7. UI, feedback, crosshair above world optics;
8. mirrors (when enabled) reflect/fallback correctly, no recursion, no reflected entities before R11;
9. rotation/motion free of flicker, popping, unstable glyphs, objectionable darkness.

Do not mark a visible optical increment complete until relevant checks are recorded.

## Exact continuation point

Stop after I7. Do not begin I8 preset work without separate authorization and a
fresh end-to-end coverage/performance assessment. Preserve one-bounce maximum,
darkness fallback, bounded coverage, typed inheritance, transactional ownership,
exact compatibility checksums, and the four-layer cap. Preset names and
thresholds remain deferred to I8.
