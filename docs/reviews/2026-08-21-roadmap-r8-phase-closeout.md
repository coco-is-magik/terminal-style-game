# Roadmap R8 Phase Closeout — Continuing Full Q4 Review and Roadmap Reassessment — 2026-08-21

## Outcome

**Closeout passed; no blockers. R8 remains Verified (2026-08-21), and R9
(Layered optical rendering, Research track) is confirmed as the next phase with
its prerequisites satisfied.** This is the continuing full-Q4 review and roadmap
reassessment required by the roadmap review schedule after every later major
phase.

## Scope and method

Reviewed the completed R8 vertical-world phase end to end after verification:
the amended v5 schema and format boundary, the shared bounded-cell tracer and its
renderer/selection/highlight consumers, per-pixel occlusion data added for
decals, vertical physics, editor workflows, decal adhesion/occlusion behavior,
performance gates, documentation status, and the deferred-work inventory.
Method: source inspection against the amended contracts, gate re-runs, and a
documentation audit for stale status claims.

## Findings

| # | Area | Finding | Status |
|---|---|---|---|
| F1 | Schema/format | Signed two's-complement heights, explicit floor/ceiling presence grids, reserved diagnostic numbering (`TSG-SCENE-INPUT-0017`), and v1–v4 migration defaults (both surfaces present) are consistent across `scene_types.h`, `scene_format.c`, validation, serializer, and tests. No duplicate authored state. | Closed |
| F2 | Renderer/tracer | Rendering, horizontal picking, and highlights share `heightfield_trace`; flat-default scenes keep the legacy path byte-for-byte; airborne flat scenes switch to bounded tracing; wall faces close lowered-floor/raised-ceiling neighbor gaps. Per-pixel `world_depths`/`world_hit_keys` give decals correct occlusion against generated faces and nearer geometry while preserving decals on their own visible surface. | Closed |
| F3 | Physics | Present-surface traversal requirement holds (missing floor/ceiling is a terminal opening, not a stacked interval); default jump impulse 3.2 gives an uncapped apex ≈ 0.52 units, clearing two default steps. | Closed |
| F4 | Editor | Signed height stepping, presence remove/restore, and multiselect edits remain atomic single undo groups with player-cell safety. Cell-vs-map gravity labels remove the ambiguous duplication while keeping both scopes. | Closed |
| F5 | Decals | Horizontal decals resolve Z per glyph from the authored surface each frame; glyphs on removed surfaces are suppressed; authored decal data is never mutated by rendering. | Closed |
| F6 | Performance/gates | Strict optimized `make check`, full ASan, and full UBSan pass; raised-path benchmark 5.33 ms and stability 5.03 ms stay under the 6 ms gate, deterministic, with exact flat-default framebuffer parity; the decal-occluded frame checksum equals the no-decal raised frame. | Closed |
| F7 | Documentation | Roadmap, requirements plan, decision record, v5 spec, increment records, review G, README, tooltips, error catalog, and `TODO.md` reflect Verified status and the amended contracts; historical records carry superseded banners. | Closed |
| F8 | Housekeeping | Leak-evidence policy formalized and the stale build-profile TODO reconciled (see Resolutions below). | Closed |
| F9 | Backlog | Hover-outline visibility (research) and bulk-selection design remain valid, independent backlog items; mirrors fold into R9 research; R10/R11/R12 stay Proposed and not ready to plan. | Open items retained |

No blocker, split, combine, or reorder actions resulted from this review.

## Resolutions recorded by this closeout

1. **Leak-evidence policy.** Valgrind is unavailable in the verification
   environment (`make leak` reports `SKIP`). The accepted memory-safety evidence
   for R8 is the full ASan suite, which includes LeakSanitizer under the `asan`
   target; it passed without diagnostics. The `make leak` target remains
   available and should be re-run in any environment that provides valgrind.
   Review G's evidence bullet now records this decision instead of leaving the
   skip open.
2. **Build-profile TODO reconciled.** `TODO.md`'s "explicit optimized build
   profiles" item predated the heightfield performance work: the strict default
   is now `-O2 -Wall -Wextra -Wpedantic -Werror`, and every optimization-only
   diagnostic that surfaced under it was resolved without suppression. The item
   is rewritten to keep only the still-open `-O3` profile evaluation and the
   standing rule that optimization must not conceal algorithmic regressions.

## Verification evidence (current)

- Strict optimized aggregate `make check`: passed.
- Full `make asan`: passed without diagnostics (LeakSanitizer included).
- Full `make ubsan`: passed without diagnostics.
- Surface benchmark: raised-height path 5.45 ms average; deterministic; 6 ms
  gate passed.
- Surface stability: raised-height path 5.18 ms over 1000 iterations;
  deterministic; 6 ms gate passed.
- Exact flat-default framebuffer checksum parity retained.
- Occluded-decal frame checksum equals the no-decal raised-height frame.
- `make leak`: `SKIP` (valgrind absent) per the resolution above.

## Roadmap reassessment

- R0 through R8 are Verified.
- **Next phase: R9 — Layered optical rendering (Research track).** Its
  prerequisites (R4 geometry/appearance separation and the final R8 geometry) are
  now both satisfied. The exit gate calls for research prototypes before an
  implementation plan: explicit invisible/collision/ray/light semantics, a
  bounded multiple-hit representation with compositing, translucent materials
  with transmission rules, deterministic ordering with decals/sprites/highlights,
  and mirrors with bounded recursion. The per-pixel frontier introduced for decal
  occlusion is a natural starting point for the ordered-hit-list design.
- **Review H** is the required checkpoint before R9 architectural commitment and
  again after the implemented optical phase.
- R10 (lighting) waits on R9 rules; R11 (sprites/entities) has its R8 geometry
  prerequisite met but stays Proposed; R12 (responsive UI) may move earlier only
  after a Q4 review confirms dependencies.
- Independent backlog (hover-outline visibility, bulk-selection design) may be
  scheduled opportunistically and does not block R9.

## Conclusion

The R8 phase is fully closed: implemented, remediated, verified, documented, and
now reviewed under the continuing schedule with its housekeeping resolved. The
next roadmap action is the R9 research prototype followed by Review H.
