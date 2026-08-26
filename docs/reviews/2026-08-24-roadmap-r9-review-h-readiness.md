# Roadmap R9 Review H Readiness Packet — 2026-08-24

> **Review conducted:** The pre-implementation Review H conditionally passed on
> 2026-08-24. This readiness packet remains the evidence index; the authoritative
> outcome is `2026-08-24-roadmap-r9-review-h.md`.

## Readiness outcome

**Ready to conduct Review H. Not ready to write an implementation plan before the
review outcome.** P1–P4, RQ5, and RQ6 evidence now exists. The evidence validates
typed semantics and bounded correctness, but rejects the straightforward
full-screen multi-hit and per-cell mirror architectures. Review H must choose
between a constrained architecture direction and one additional selective
integration research loop.

## Review H authority

The roadmap requires Review H before R9 architectural commitment and again after
an implemented optical phase. This packet is the first checkpoint. It does not
approve implementation, schema changes, or shipping renderer modifications.

## Entry-gate checklist

- [x] P1–P4 findings notes exist with measured numbers.
- [x] D1–D3 recommendations are recorded with rationale.
- [x] RQ6 budget table states the proposed implementation budget and distinguishes
      direct measurements from projections.
- [x] RQ5 policy table reproduces current decal ordering and current editor/UI
      precedence without adding ordering code.
- [x] No shipping-path optical behavior changed; ordinary builds exclude research
      APIs and deterministic checksum parity remains intact.

## Evidence index

| Evidence | Main result |
|---|---|
| `R9_P1_OPTICAL_SEMANTICS_FINDINGS_2026-08-21.md` | Independent typed semantics; v6 and material-default/sparse-override recommendations |
| `R9_P2_MULTIHIT_TRACE_FINDINGS_2026-08-24.md` | Four-hit correctness; unconditional collection rejected at 12.9–17.8 ms |
| `R9_P3_OPTICAL_COMPOSITOR_FINDINGS_2026-08-24.md` | Exact terminal-cell blend/glyph fixtures; full-screen N=4 adds ≈2.6 ms |
| `R9_P4_MIRROR_FINDINGS_2026-08-24.md` | Correct one-bounce mirrors; per-cell preparation rejected at +1.8 ms for 10% coverage |
| `R9_RQ5_RQ6_SYNTHESIS_2026-08-24.md` | Two-domain ordering policy, aggregate budget, D1–D3, no-go paths, unknowns |

## Findings requiring review disposition

### F1 — typed semantics are viable

Occupancy, player collision, ray interaction, light interaction, opacity,
transmission, and reflectivity can remain independent with exact legacy defaults.
An eight-byte candidate extension is sufficient for research semantics.

**Recommended disposition:** accept the semantic separation, but defer exact v6
field grammar to the post-review implementation plan.

### F2 — schema should advance to v6

Current v5 machinery cannot preserve unknown optional sections. Silent v5
amendment creates incompatible grammars and lossy older-writer behavior.

**Recommended disposition:** accept D1 = v6.

### F3 — material defaults plus sparse exceptions remain the best authored model

P1 memory evidence favors material defaults and sparse overrides. P2–P4 show that
the hot path may need a derived representation, but do not justify collapsing the
authored model.

**Recommended disposition:** accept D2 for authored data; require profiling before
choosing the derived runtime lookup.

### F4 — four layers are a correctness cap, not permission for unconditional work

P2 proves ordered bounded lists and exact N=1 parity, but full-screen collection
is unaffordable. P3 similarly proves composition but not unconditional use.

**Recommended disposition:** accept four as a hard maximum and require nearest-hit
opaque fast path plus selective continuation.

### F5 — one mirror bounce is correct, but current granularity is unaffordable

P4 proves wall reflection and fallback. Per-cell reflected preparation exceeds
headroom even at 10% coverage.

**Recommended disposition:** accept one as the recursion cap; reject per-cell
preparation; require coarser reflected-interval reuse or keep mirrors disabled.

### F6 — three quality presets are not yet evidenced

Only compatibility/default opaque rendering is end-to-end validated. Synthetic
composition and per-cell mirror measurements cannot justify medium/high presets.

**Recommended disposition:** accept D3 caps (four layers, one bounce), but defer
preset names/thresholds until optimized selective end-to-end benchmarks exist.

### F7 — ordering needs two explicit domains

World content is depth-aware in logical cells; editor highlights annotate that
grid; UI layers compose later in pixels by z/insertion order. A single flat z
number would obscure current behavior.

**Recommended disposition:** accept the RQ5 two-domain policy and current decal
tie-break. Keep sprite/entity ordering reserved for R11.

## Explicit no-go paths

Review H should prohibit an implementation plan from proposing any of these
without materially new evidence:

1. unconditional multi-hit collection for every sample;
2. unconditional full-screen optical composition;
3. one reflected prepared column per mirror-covered cell;
4. unbounded layers or recursive mirrors;
5. deriving collision or ray/light interaction from opacity;
6. silently amending v5;
7. one flat ordering number spanning world depth and post-raster UI;
8. claiming reflected entities before R11 defines them.

## Decisions requested from Review H

Review H should record explicit answers:

1. **D1:** Approve v6 as the optical persistence vehicle?
2. **D2:** Approve material defaults plus sparse authored cell overrides, with a
   derived runtime representation selected by profiling?
3. **D3a:** Approve hard caps of four layers and one bounce?
4. **D3b:** Defer product quality presets until end-to-end optimized evidence?
5. Approve the RQ5 two-domain ordering policy and current decal tie-break?
6. Approve P3's blend equation, glyph threshold 128, and explicit
   opening/cap-exhaustion distinction as the next implementation-plan baseline?
7. Approve darkness as reflected miss/opening fallback?
8. Decide whether selective continuation and per-column mirror reuse belong in:
   - the post-review implementation plan with mandatory early performance gates; or
   - one more pre-plan research prototype.

## Recommended Review H outcome

**Conditional architecture acceptance, not immediate broad implementation
approval.** Accept D1/D2, semantic separation, four/one hard caps, the two-domain
ordering model, and the nearest-hit/selective-continuation principle. Require the
implementation plan—if authorized—to begin with reversible runtime lookup and
selective-continuation increments whose stop gates precede schema/editor work.
Mirrors should remain disabled until per-column reuse is proven.

If Review H considers the selective continuation seam too uncertain for planning,
authorize exactly one additional compile-time-gated integration prototype instead.
Do not weaken the 6 ms gate or use projections as acceptance evidence.

## Verification state at readiness

- Strict optimized aggregate tests pass through P4.
- Full sequential ASan/LeakSanitizer and UBSan suites pass through P4.
- Latest synthesis-run shipping benchmark raised path: 5.222781 ms,
  deterministic, pass.
- Latest synthesis-run shipping stability raised path: 4.885022 ms,
  deterministic, pass.
- Shipping stability had one 6.097629 ms host-variance failure followed by three
  deterministic passes at 4.901847, 4.852817, and 4.876414 ms.
- Flat-default framebuffer checksum parity remains exact.
- Occluded-decal checksum equals the raised-height no-visible-decal checksum.
- Research code remains compile-time gated and absent from ordinary behavior.

## Recorded continuation

Review H recorded all eight dispositions in
`2026-08-24-roadmap-r9-review-h.md`. A constrained R9 requirements and
implementation plan is now authorized; shipping implementation remains a later
increment.