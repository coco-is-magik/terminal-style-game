# Roadmap R9 Review H — Pre-Implementation Architecture Checkpoint — 2026-08-24

## Outcome

**Conditionally accepted; R9 implementation planning is authorized under mandatory
early stop gates.** Review H accepts the typed semantic model, v6 direction,
material-default/sparse-override authored placement, four-layer and one-bounce hard
caps, and the two-domain ordering policy. It rejects unconditional multi-hit
collection, unconditional full-screen optical composition, and per-cell reflected
column preparation.

This acceptance is deliberately narrow. It authorizes writing the R9 requirements
and implementation plan; it does not authorize broad shipping implementation,
schema/editor work before runtime gates, mirror enablement, or named quality
presets. The roadmap still requires a second Review H after the implemented optical
phase.

## Scope and authority

Reviewed:

- P1 typed optical semantics, placement, memory, and schema evidence;
- P2 bounded ordered intersections and N=1 parity;
- P3 deterministic terminal-cell composition and sight/light separation;
- P4 one-bounce wall reflection, fallback, and coverage cost;
- RQ5 current/future ordering policy;
- RQ6 aggregate budget and rejected paths;
- strict aggregate, sanitizer, checksum, benchmark, and stability evidence.

The review does not approve colored lighting (R10), sprite/entity behavior (R11),
horizontal mirrors, reflected entities, or final quality presets.

## Finding dispositions

| Finding | Disposition | Required consequence |
|---|---|---|
| F1 — Independent typed semantics are viable | **Accepted** | Occupancy, player collision, sight, light, opacity, transmission, and reflectivity remain independent. No ambiguous invisible flag. |
| F2 — Optical persistence should use v6 | **Accepted** | Do not silently amend v5. The future plan must define migration defaults, canonical ordering, diagnostics, and unknown-block policy. |
| F3 — Material defaults plus sparse authored exceptions | **Accepted for authored data** | The runtime representation remains profiling-driven and derived; authored semantics may not be collapsed for speed. |
| F4 — Four layers are a correctness cap | **Accepted with constraint** | Preserve the existing nearest-hit opaque fast path. Farther traversal and composition are selective only. |
| F5 — One bounce is correct; per-cell preparation is too slow | **Accepted cap; rejected implementation granularity** | Mirror support remains disabled until reflected interval reuse at a coarser granularity passes end-to-end gates. |
| F6 — Three presets are not evidenced | **Accepted** | Defer preset names, thresholds, and mirror-enabled modes until optimized end-to-end measurements exist. |
| F7 — Ordering has world-cell and UI-pixel domains | **Accepted** | Preserve current decal depth/source ordering and editor/UI precedence. Sprite/entity tie-break remains reserved for R11. |

## Recorded decisions

### D1 — Schema vehicle

**Approved: v6.** Existing v5 fixed-width token machinery is not extensible
unknown-section preservation. Older writers must never silently discard optics.

### D2 — Property placement

**Approved: material defaults plus sparse authored per-cell overrides.** A derived
runtime lookup/cache may be chosen after profiling. The implementation plan must
prove zero/default values preserve current scene migration, round trips, collision,
rendering, and checksums.

### D3a — Hard caps

**Approved: maximum four optical layers and maximum one mirror bounce.** These are
upper bounds, not permission to execute either operation unconditionally.

### D3b — Quality presets

**Deferred.** Only compatibility/default opaque/no-mirror behavior is presently
end-to-end validated. Do not ship or document low/medium/high optical presets until
selective translucency and any mirror path pass representative full-frame gates.

### RQ5 ordering

**Approved: two-domain policy.** The depth-aware world-cell domain completes before
the z/insertion-ordered UI-pixel domain. Current decal tie-break remains nearest
perpendicular depth, then creation index and pattern row-major order. Editor primary
selection remains above hover/member annotations; crosshair remains the highest
current UI role.

### P3 visual baseline

**Approved as the implementation-plan baseline, subject to later manual visual
acceptance:**

- far-to-near RGB rule
  `clamp((surface * opacity + behind * transmission + 127) / 255)`;
- nearest glyph with opacity at least 128;
- opacity and transmission remain independent;
- sight continuation requires nonblocking ray semantics and nonzero transmission;
- scalar light blocking/transmission remains independent;
- terminal surface, proven opening, and layer-cap exhaustion remain distinct.

The threshold and blend rule may be revisited only with fixture and visual evidence,
not ad hoc implementation preference.

### Mirror fallback

**Approved for the first implementation baseline: darkness on reflected miss or
opening.** Reflected mirror hits are terminal at one bounce. Partial-reflectivity
mixing remains unresolved and must be specified before mirror enablement.

### Authorization fork

**Approved: proceed to a constrained implementation plan rather than another
pre-plan prototype.** The plan must front-load reversible integration and stop
gates. If either early runtime gate fails, return to research/Review H instead of
continuing into schema or editor work.

## Mandatory implementation-plan order and stop gates

The authorized plan must preserve this order:

1. **Derived runtime optical lookup seam.** No schema/editor changes. Zero/default
   lookup must retain exact shipping checksums. Proposed default overhead target:
   at most 0.100 ms; total raised path remains at most 6.000 ms; zero per-frame
   allocations.
2. **Selective continuation integration.** Start from the nearest hit and continue
   only when resolved sight semantics transmit. Benchmark representative optical
   coverage end-to-end; unconditional P2 collection is forbidden.
3. **Selective P3 composition.** Compose only samples with additional optical
   layers. Preserve explicit opening/cap state and current decal ordering.
4. **Stop-gate review.** If the 6 ms gate, default checksum parity, determinism, or
   zero-allocation requirement fails, stop and return to a focused research loop.
5. **Only after gates 1–4 pass:** specify/implement v6 migration and persistence,
   then editor authoring and transactional undo/redo.
6. **Mirrors remain disabled.** A later increment may prototype reflected interval
   reuse per mirror-covered column. It must preserve per-row Z correctness and pass
   an end-to-end coverage gate before product enablement.
7. **Preset decision remains deferred** until measured integrated paths exist.

## Explicitly prohibited paths

The implementation plan must not propose, without materially new Review H
evidence:

1. full-screen multi-hit collection for every sample;
2. full-screen optical composition for opaque/default samples;
3. one reflected prepared column per mirror cell;
4. unbounded layers or recursive mirrors;
5. collision, sight, or light behavior inferred from opacity;
6. an amended-v5 optical grammar;
7. one numeric ordering axis spanning world depth and post-raster UI;
8. reflected sprite/entity claims before R11 defines their behavior;
9. lowering or bypassing the 6 ms gate to accommodate optics.

## Fresh automated evidence

- Strict optimized `make -j2 check`: passed, including P1 6/6, P2 6/6, P3
  9/9, and P4 6/6.
- Shipping surface benchmark: raised path **5.332370 ms**, deterministic, 6 ms
  gate passed.
- Shipping surface stability: raised path **5.266871 ms** over 1000 iterations,
  deterministic, 6 ms gate passed.
- Exact flat-default framebuffer checksum parity retained:
  authored and flat-height checksum `5602340901454607159`.
- Occluded-decal checksum equals raised-height checksum:
  `16569300432624360523`.
- Sequential full ASan/LeakSanitizer and UBSan suites passed during P4 verification;
  no production code changed during RQ5/RQ6 synthesis or this review.
- `git diff --check`: passed before the review record was written.

The earlier isolated 6.097629 ms stability result remains recorded in the readiness
packet; three immediate retries and the fresh Review H run passed. It is treated as
host variance, not erased.

## Manual acceptance deferred to the user

There is **no applicable pre-implementation manual visual check** in this Review H:
all R9 behavior remains compile-time-gated research and ordinary builds display no
translucency or mirrors. A manual pass now could only reconfirm unchanged R8
behavior and would not validate optical product semantics.

The user must perform manual acceptance at the first implementation increment that
exposes visible optics. The implementation plan must include this checklist:

1. default/legacy scenes look byte-for-byte/visually unchanged;
2. one translucent layer over a wall has stable readable glyph/color output;
3. two translucent layers remain legible and deterministic;
4. translucent surface over an opening uses darkness and does not reveal stale
   framebuffer content;
5. decals on/behind translucent and discontinuity surfaces obey the approved
   world ordering;
6. editor hover/selection remain visible and primary selection wins;
7. UI, feedback, and crosshair remain above world optics;
8. if mirrors are later enabled, reflected wall and opening fallback are visually
   correct, mirror-facing-mirror does not recurse, and no reflected entities are
   implied before R11;
9. inspect motion/camera rotation for flicker, layer popping, glyph instability,
   or objectionable darkness fallback.

Until that checklist is completed, visible optical increments may be automated-
verified but must not be marked fully accepted.

## Remaining unresolved items

- Exact derived runtime lookup structure and sparse-override access cost.
- End-to-end selective-continuation coverage thresholds.
- Partial-reflectivity/direct/transmitted color mixing.
- Coarse reflected-interval reuse design and mirror coverage limits.
- Product preset names and thresholds.
- Sprite/entity ordering and reflection, deferred to R11.

These are implementation-plan requirements or later gated decisions; they do not
invalidate the conditional pre-implementation acceptance.

## Conclusion and next action

Review H's pre-implementation checkpoint is **passed with conditions**. Write the
R9 requirements and implementation plan next, preserving the mandatory increment
order and stop gates above. Do not begin shipping implementation in the same
increment as planning. The second Review H remains required after the implemented
optical phase.