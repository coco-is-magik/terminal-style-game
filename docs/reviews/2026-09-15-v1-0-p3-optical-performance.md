# V1-0 P3 Selective Optical Performance — 2026-09-15

## Outcome

**Complete and verified.** The shipping selective transparent and one-bounce mirror paths
now pass the unchanged 6 ms budget. Output checksums, nearest-frontier ownership, sparse
override precedence, one-bounce/cache limits, and zero timed-loop allocations are preserved.

## Implementation

Three bounded costs were removed:

1. The composition path resolves its nearest layer through the P2 prepared-run sampler,
   already tested field-for-field against the generic nearest sampler.
2. Samples with no render-affecting material or cell override emit their ordinary nearest
   cell immediately. Material-only views use direct validated metadata; views containing
   sparse cell overrides retain the runtime module's sorted lookup and precedence rules.
3. Per-column mirror cache initialization resets only `prepared` and `preparation_count`.
   Reflected camera/column storage is written completely before `prepared` becomes true and
   is unreachable while false.

Selective continuation, compositing arithmetic, reflected tracing, schema, persistence,
editor behavior, scenario coverage, and the 6 ms threshold were not changed.

## Regression evidence

- Runtime-view tests pass 8/8, including material/cell render-mask detection, nonvisual-only
  fields, invalid-input output preservation, and sparse precedence.
- Optical-render tests pass 20/20. A new complete-frame test proves a sparse cell transparency
  override still enters selective composition.
- Mirror tests pass 4/4. Cache initialization now proves guard metadata is reset without
  clearing unused reflected-column storage.
- Existing transparent layers, generated boundaries, openings, glyph/blend math, overlays,
  reflected curvature/viewpoint, bilateral edges, cache reuse, reflected-mirror termination,
  and second-plane darkness tests remain unchanged and pass.

## Performance evidence

Raw accepted outputs are under `build/p3-optical-acceptance/`. The established host-variance
policy requires unchanged deterministic retries after loaded-host overruns; it does not weaken
the per-process 6 ms result. Three benchmark and three 1,000-frame stability processes passed:

| Mode | Transparent range | Mirror range | Result |
|---|---:|---:|---|
| benchmark, 200 frames | 3.603454–3.818739 ms | 3.586869–3.668715 ms | 3/3 pass |
| stability, 1,000 frames | 3.357268–4.061780 ms | 3.424079–4.000891 ms | 3/3 pass |

Every accepted process also kept compatibility and opaque control paths below 6 ms, reported
zero timed-loop allocations, and retained these checksums:

- compatibility/opaque: `17924094251449449076`;
- transparent: `16162899958184009008`;
- mirror: `11094987557799913320`.

Earlier unchanged isolated distributions contained scheduler-sensitive failures that moved
among transparent, mirror, and the unaffected opaque control. They remain preserved under
`build/p3-optical-closeout-final/` and `build/p3-optical-closeout-retry/`; they were not
silently converted into passes.

## Verification

- strict GCC and local Clang focused builds: pass with `-Werror`;
- sequential focused ASan/LeakSanitizer: pass with no diagnostics;
- sequential focused UBSan: pass with no diagnostics;
- strict application build: pass;
- complete functional aggregate: pass;
- `standards-core`: pass;
- `benchmark-headless`: pass, including transparent 4.329952 ms and mirror 3.559374 ms;
- `stability-headless`: pass, including transparent 3.496281 ms and mirror 3.592824 ms.

## Next safe action

Complete M1 mirror-report disposition without changing source unless a distinct failing scene
is supplied. Then reconcile final V1-0 evidence. Owned-upstream SMC newline/pin maintenance
remains deferred to V1-20.