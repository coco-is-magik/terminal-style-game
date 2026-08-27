# Roadmap R9 Review H — Implemented Optical Phase Checkpoint — 2026-08-25

## Outcome

**Passed; I1–I3 runtime seams, parity, determinism, allocation, and budget gates are
met.** Review H records disposition to proceed to I5 (v6 schema/migration/persistence
for authored optical data). Editor authoring, mirrors, and presets remain deferred
until their respective increments are reached.

> **Superseded dispatch detail (2026-08-27):** the accepted opt-in renderer seam was
> consolidated after manual review exposed a pipeline-switch roof jump. All current
> production/tests use `raycast_render_height_optical()`; the old renderer is retained
> only as a compiler-deprecated rollback implementation.

This is the second Review H required by the R9 plan: the first conditionally
accepted the architecture and planning direction on 2026-08-24; this review accepts
the implemented optical phase and authorizes the next bounded increment.

## Scope reviewed

- I1 — derived runtime optical lookup (`src/optical_runtime_view.c/h`).
- I2 — selective prepared-column continuation
  (`src/heightfield_trace_selective.c`, `src/heightfield_trace_internal.h`).
- I3 — production terminal-cell compositor (`src/optical_compositor.c/h`) and
  opt-in renderer seam (`src/raycast_optical.c`, `src/raycast_internal.h`).

The review explicitly excludes v6 grammar design, editor UX, mirror enablement, and
preset policy. Those items remain on the I5–I8 roadmap and require their own gates.

## Authority and constraints

Review H reconfirms the locked decisions from 2026-08-24:

- D1 — v6 is the optical persistence vehicle; v5 must not be silently amended.
- D2 — authored placement is material defaults plus sparse per-cell overrides;
  runtime values are derived.
- D3a — hard upper caps of four optical layers and one mirror bounce.
- D3b — quality presets remain deferred until end-to-end measurements justify them.
- RQ5 — two-domain ordering (world-cell depth, then UI-pixel insertion/z).

No new exceptions, preset names, or mirror-enablement decisions are made in this
review.

## Entry-gate checklist

| Gate | Evidence | Result |
|---|---|---|
| I1–I3 implementation records exist | `R9_INCREMENT_I1_IMPLEMENTATION_RECORD_2026-08-24.md`, `R9_INCREMENT_I2_IMPLEMENTATION_RECORD_2026-08-25.md`, `R9_INCREMENT_I3_IMPLEMENTATION_RECORD_2026-08-25.md` | Pass |
| Focused runners pass | I1 7/7, I2 9/9, I3 11/11 | Pass |
| Shipping path unchanged | `raycast_render_height()` remains the application call; `SRC_RAYCAST` module set is the pre-I2 set | Pass |
| Strict build | `make -j2 check` pass with `-Werror -Wall -Wextra -Wpedantic` | Pass |
| Sanitizers | Sequential `make asan && make ubsan` pass with no diagnostics | Pass |
| Default parity | Flat/default and raised/decal-occlusion checksums unchanged | Pass |
| Determinism | Repeated benchmark and test runs produce identical checksums | Pass |
| Allocation-free render loop | I2/I3 benchmarks report zero timed-loop allocations | Pass |
| No unauthorized scope | No v6 schema, editor, mirror, or preset code added | Pass |
| Whitespace | `git --no-pager diff --check` clean | Pass |

## Findings and dispositions

| Finding | Disposition | Required consequence |
|---|---|---|
| F1 — I1 runtime view preserves legacy defaults and sparse overrides transactionally | **Accepted** | v6 migration must serialize the same material-default/sparse-override authored model; the runtime view may be rebuilt on load. |
| F2 — I2 selective continuation reuses prepared intervals without a second DDA and stays allocation-free | **Accepted** | Future tracer changes must keep the prepared-column cache as the single DDA source and must not add per-sample allocation. |
| F3 — I3 compositor reproduces P3 blend/glyph/darkness rules and validates impossible states | **Accepted** | The v6 editor must not be able to author layer sequences that violate the compositor contract; validation must remain server-side/renderer-side, not client-trust. |
| F4 — Nearest-layer depth/hit identity is preserved | **Accepted** | Decal post-processing, editor hover/selection, and UI ordering continue to use the nearest optical layer's frontier. Far-layer decal attachment is not introduced without a separate ordering review. |
| F5 — Default application path is unchanged | **Accepted** | The shipping renderer call site must remain `raycast_render_height()` until v6 supplies a valid optical view. No unconditional optical lookup in the default path. |
| F6 — 6 ms surface budget is preserved under clean isolated runs | **Accepted with host-variance caveat** | Continue running shipping benchmark/stability sequentially for final gates; treat loaded-system overruns as host variance requiring retry, not gate erosion. |
| F7 — Broad transmissive coverage exceeds the budget | **Accepted as a constraint, not a defect** | Preset/coverage policy remains deferred to I8; the engine does not ship broad transparent planes by default. |
| F8 — No schema, editor, mirror, or preset work is present | **Accepted** | I5 is authorized only for v6 persistence/migration; editor (I6), mirrors (I7), and presets (I8) remain separately gated. |

## Stop-gate evidence

### Performance

Shipping surface rendering gates (clean isolated run, optimized binary):

| Path | Benchmark ms | Stability ms | Budget |
|---|---:|---:|---:|
| raised height | 5.322599 | 5.545941 | 6.000 |
| occluded decal | 5.401400 | 5.639415 | 6.000 |

Earlier parallel or loaded runs occasionally exceeded 6 ms while retaining exact
checksums; those runs were retried in isolation and passed. They are recorded as
host-variance outliers, not erased.

### Parity and determinism

| Checksum | Meaning |
|---:|:---|
| `5602340901454607159` | flat/default and authored view; unchanged from pre-R9 baseline |
| `16569300432624360523` | raised height and occluded-decal paths; unchanged |
| `17812527538433777792` | I2 selective 0% coverage exact parity with baseline |
| `17276792261464593835` | I3 optical opaque-frame exact parity with compatibility |
| `18106365475393592681` | I3 localized transparent coverage; deterministic across runs |

### Allocation

- `tests/benchmark_heightfield_selective.c`: `timed_loop_allocations: 0`.
- `tests/benchmark_optical_render.c`: `render_loop_allocations: 0`.

### Scope boundary

Code search and `git diff` confirm:

- No `SceneDocument` optical fields added.
- No scene format/migration changes.
- No editor commands or inspector panels for optics.
- No mirror enablement in the default path.
- No preset names or thresholds.

## Manual acceptance

There is **no applicable in-game manual optical check** at I4 because the
application still calls the compatibility renderer and no v6 view is supplied.
The plan's visible manual checklist items 1–5 were validated through focused
deterministic fixtures in `tests/test_optical_render.c`:

1. default/legacy scenes byte-for-byte unchanged;
2. single translucent over a wall readable/stable;
3. two translucent layers legible and deterministic;
4. translucent opening darkness without stale framebuffer;
5. decal ordering correct on the nearest optical surface.

Full manual acceptance of editor/UI items 6–9 remains deferred until I6 exposes
optics to the user.

## Remaining unresolved items

- Exact v6 field grammar, canonical ordering, diagnostics, and unknown-block policy.
- Migration defaults and byte-equality round-trips for v1–v5.
- Editor atomic undo/redo for material defaults and per-cell overrides.
- Coarse reflected-interval reuse and mirror coverage limits (I7).
- Product preset names and thresholds (I8).
- Sprite/entity ordering and reflection (R11).

These are the planned I5–I8 work items; they do not invalidate the I4 disposition.

## Conclusion and next action

I4 is **complete**. Review H approves advancement to **I5 — v6 schema/migration/persistence**
under the same locked constraints: v5 must load unchanged, zero/default optical values
must reproduce current migration/rendering/collision/checksums exactly, and no editor,
mirror, or preset work may begin in the same increment.
