# R9 P1 Findings — Typed Optical Semantics and Placement — 2026-08-21

## Outcome

**P1 complete; proceed to P2 only after confirmation.** The compile-time-gated
spike demonstrates an independent typed semantics model, exact legacy inheritance
from a zero extension, bounded validation, and measured candidate storage costs.
It does not change shipping data, scene v5, collision, tracing, lighting, or
rendering. These findings are evidence for Review H, not architectural approval.

## Prototype boundary

- `src/r9_optical_semantics.h/.c` exists only when
  `R9_OPTICAL_RESEARCH=1` is defined.
- The focused runner is the only target defining that symbol.
- Ordinary application sources see no optical types or behavior.
- `Material`, `SceneAuthoredCell`, `AssetRegistry`, and canonical v5 remain
  byte-for-byte unchanged.
- A gate-off strict compile of the prototype translation unit passes.

## Candidate typed semantics

The eight-byte `R9OpticalExtension` has a six-bit override mask, three bounded
boolean values, three byte-normalized optical values, and one reserved byte.
A clear override bit means “inherit,” so an all-zero extension exactly resolves
to the supplied legacy semantics.

| Channel | Type/range | Legacy occupied cell | Legacy empty cell | Independent from |
|---|---:|---:|---:|---|
| Occupancy | existing `SceneCellOccupancy` | wall | empty | all interaction and appearance channels |
| Player collision | boolean override | blocks | passes | ray/light interaction and alpha |
| Ray interaction | boolean override | blocks | passes | player/light interaction and alpha |
| Light interaction | boolean override | blocks | passes | player/ray interaction and alpha |
| Visual opacity | `uint8_t`, 0–255 | 255 | 0 | collision and ray/light classes |
| Light transmission | `uint8_t`, 0–255 | 0 | 255 | visual opacity; no forced complement |
| Reflectivity | `uint8_t`, 0–255 | 0 | 0 | opacity, transmission, and blocking classes |

Opacity and transmission intentionally are not forced to sum to 255: absorption
must remain representable. P1 does not decide a future blend equation.

The focused test uses a cell that blocks the player and light while allowing a
view ray, with opacity 96, transmission 180, and reflectivity 64. A second case
overrides only ray interaction and proves all other channels inherit unchanged.
Unknown mask bits, non-boolean active values, and nonzero reserved bytes reject
without modifying the output.

## Measured in-memory costs

Command: `make r9-p1-memory-report`. Inputs are the actual project limits:
512x256 = 131,072 cells and 65,536 material IDs.

| Candidate placement | Exact bytes | MiB |
|---|---:|---:|
| 8-byte extension for every material slot | 524,288 | 0.500 |
| 8-byte extension for every maximum-map cell | 1,048,576 | 1.000 |
| 1,024 sparse 12-byte cell records | 12,288 | 0.0117 |
| Material defaults + 1,024 sparse records | 536,576 | 0.5117 |

Sparse records are smaller than dense cell extensions through 87,381 records
(66.7% of a maximum map). Material defaults plus sparse records remain smaller
than a dense cell array through 43,690 overrides (33.3% of a maximum map).
The full 65,536-slot material cost is deliberately conservative; actual loaded
materials are normally sparse, so a packed loaded-material representation could
be lower. P1 does not choose that registry representation.

## Candidate serialized-size model

These are exact payload sizes for the measured **candidate encoding**, excluding
section headers; they are not a committed v6 grammar:

- extension: 16 uppercase hex digits (eight bytes);
- material record: four-hex ID, space, extension, newline = 22 bytes;
- sparse cell record: five-hex maximum-map index, space, extension, newline =
  23 bytes;
- dense cell grid: 16-hex token plus one separator/newline byte per cell.

| Candidate payload | Exact bytes | MiB |
|---|---:|---:|
| All material slots | 1,441,792 | 1.375 |
| Dense maximum-map cells | 2,228,224 | 2.125 |
| 1,024 sparse cell records | 23,552 | 0.0225 |
| All material slots + 1,024 sparse records | 1,465,344 | 1.3975 |

The material figure again represents all possible IDs, not typical loaded
content. The dense payload alone remains within today's 8 MiB scene-file cap,
but size is not the only schema concern.

## D1 — schema vehicle recommendation

**P1 recommendation: introduce optical persistence in v6, not an amended v5.**

Evidence:

1. `scene_block_codec` only parses/formats fixed-width 16-bit row tokens. It is
   not optional-section machinery.
2. Canonical v5 has required named sections and no preservation mechanism for
   unknown optional sections.
3. Adding optical sections to shipped v5 would make one version number describe
   incompatible accepted grammars and would not give older writers lossless
   round trips.

P1 does not design v6. A later implementation plan after Review H must define
version negotiation, optional-block rules, migration defaults, diagnostics,
canonical ordering, and unknown-block policy.

## D2 — property placement recommendation

**P1 recommendation: material-level defaults plus sparse per-cell overrides.**

Rationale:

- Optical appearance normally belongs to reusable materials.
- Independent interaction channels still allow occupancy/collision/appearance
  separation; they are not inferred from material alpha.
- Sparse exceptions avoid a permanent 1 MiB maximum-map dense extension and are
  cheaper until one third of the maximum map when combined with the deliberately
  worst-case full material table.
- The explicit override mask supports inheritance without sentinel values.

Review H may revise this after P2–P4 expose tracer/compositor access patterns.

## Correctness and parity evidence

- Focused strict runner: 6/6 tests pass.
- Zero extension: resolved structure exactly equals legacy occupied semantics.
- Gate-off strict translation-unit compile: pass.
- Shipping structs and paths are untouched, so ordinary renderer checksum parity
  is structural rather than dependent on a dormant runtime branch.
- Strict optimized aggregate `make -j2 check`: pass.
- Full ASan/LeakSanitizer and UBSan suites: pass without diagnostics.
- Surface benchmark raised path: 5.170782 ms; stability raised path: 5.076414 ms;
  both deterministic and below 6 ms.
- Exact flat-default framebuffer parity and occluded-decal/raised-height checksum
  equality remain intact.

## P1 limitations

- No non-default optical value enters shipping tracing or rendering.
- No persistence parser/writer was added; serialized measurements are a candidate
  sizing model only.
- Material allocation strategy, sparse lookup structure, editor UX, compositing,
  multi-hit tracing, and mirror behavior remain undecided.
- P2 must measure whether material defaults plus sparse lookup fit the prepared
  column tracer's access pattern before Review H treats D2 as settled.

## Reproduction

```sh
make build/test-r9-optical-semantics
./build/test-r9-optical-semantics
make r9-p1-memory-report
```

Next increment, only after confirmation: P2 bounded multi-hit tracing research.