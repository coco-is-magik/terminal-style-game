# R9 P3 Findings — Terminal Optical Compositor — 2026-08-24

## Outcome

**P3 complete; bounded composition is viable, but must remain selective. Stop
before P4 pending confirmation.** The compile-time-gated prototype deterministically
composes up to four near-to-far sampled terminal cells, preserves opaque legacy
output exactly, distinguishes openings from layer-cap exhaustion, and keeps sight
and light continuation independent.

At 41,600 synthetic cells per frame, composition adds approximately 0.8, 1.4,
2.0, and 2.6 ms over a matched cell-hash baseline for N=1–4. This is inside P2's
tracing envelope, but not free. Combined with P2's rejection of unconditional
multi-hit tracing, P3 supports a nearest-hit fast path plus selective continuation
and composition only for transmissive samples.

This remains Review H research evidence. No shipping renderer, asset, scene,
lighting, or tracer behavior changed.

## Prototype boundary

- `src/r9_optical_compositor.h/.c` is active only with
  `R9_OPTICAL_RESEARCH=1`.
- Input is an allocation-free array of at most four already sampled `Cell` values,
  each paired with P1 `R9OpticalResolved` semantics and generated-boundary identity.
- Layers are supplied near-to-far; composition runs far-to-near.
- Palette selection, distance glyph bands, scalar light-map sampling, and geometry
  collection remain outside the compositor.
- The caller supplies the opening fallback explicitly. Fixtures use the shipping
  renderer's space/black darkness cell.
- Invalid input rejects without modifying output.

## Color rule

Foreground and background RGB channels use the same integer operation for each
surface from far to near:

```text
channel = clamp((surface * opacity + behind * transmission + 127) / 255)
```

Inputs and output are bytes. The `+127` gives deterministic nearest-integer
rounding. Output alpha is 255 because shipping `palette_sample()` also emits fully
opaque terminal colors.

Opacity and transmission remain independent, as established by P1:

- sum below 255 represents absorption;
- sum equal to 255 is energy-neutral in this simple RGB model;
- sum above 255 saturates per channel and remains representable for future
  emissive/stylized behavior;
- P3 does not infer either value from the other.

An opaque legacy sample (`opacity=255`, `transmission=0`) reproduces its sampled
glyph, foreground RGB, and background RGB exactly.

## Glyph rule

The output glyph is the nearest layer whose opacity is at least 128. Layers with
opacity 0–127 tint foreground/background colors but do not replace the glyph. If
no layer reaches the threshold, the output glyph is space.

The threshold is a P3 recommendation for Review H, not approved product
architecture. It is explicit and deterministic; no ASCII dithering is introduced
by this prototype.

## Selective sight and light continuation

Sight and scalar light are independent:

```text
sight continues = !ray_blocks && transmission > 0
light_out = light_blocks ? 0 : round(light_in * transmission / 255)
```

Opacity does not decide either policy. A visually opaque but ray-nonblocking
surface may continue sight if transmission is nonzero. A sight-blocking surface
may still transmit scalar light if `light_blocks` is false. These combinations
are intentional consequences of R1/P1 separation.

The scalar light rule attenuates the existing byte-normalized light quantity.
Colored light remains R10. P3 does not modify `lighting.c` or `palette_sample()`.

## Termination states

Composition reports three mutually exclusive outcomes after consuming supplied
layers:

- `terminated_by_surface`: an optical layer stopped sight;
- `reached_opening`: every consumed layer transmitted and P2 reported a terminal
  geometry opening, so the explicit fallback is valid behind them;
- `layer_cap_exhausted`: every supplied layer transmitted but no opening was
  proven, so unknown farther geometry must not be silently replaced by darkness.

This distinction was added after an API audit of the initial eight-fixture
prototype. The final nine-fixture suite locks it.

## Exact fixture evidence

Focused runner: `tests/test_r9_optical_compositor.c`, 9/9 pass.

| Fixture | Output glyph | Foreground RGB | Background RGB | FNV checksum |
|---|---:|---:|---:|---:|
| Single opaque | `#` | 200,100,50 | 10,20,30 | 4429488470940348798 |
| Glass over wall | `W` | 176,113,88 | 80,70,60 | 11839026229836985365 |
| Two glass layers over wall | `W` | 138,108,104 | 59,56,53 | 8305363331603521064 |
| Glass over opening/darkness | space | 25,38,50 | 5,10,15 | 14523133264066336396 |
| Glass over generated discontinuity | `D` | 206,68,65 | 20,18,196 | 5371222891600692856 |

Additional fixtures prove opacity-127/128 glyph threshold behavior with
absorption, independent sight/light blocking, four transmissive layers reporting
cap exhaustion rather than opening, empty-list fallback, and transactional invalid
input rejection.

## Benchmark method

Command: `make benchmark-r9-optical-compositor`.

- 41,600 synthetic terminal cells per frame, matching the existing 260x160
  surface benchmark dimensions.
- N=1 is opaque; N=2–4 have transmissive front layers and an opaque terminal layer.
- Matched baseline hashes one sampled `Cell` per output cell.
- Every compositor path writes and hashes one output `Cell` per sample.
- 20 untimed warm-up frames; 500 measured frames; three consecutive trials.
- Strict default `-O2 -Wall -Wextra -Wpedantic -Werror` build.
- Geometry tracing, palette sampling, sparse lookup, and SDL rasterization are
  deliberately excluded.

### Measured ms/frame

| Path | Trial 1 | Trial 2 | Trial 3 | Trial range overhead vs baseline |
|---|---:|---:|---:|---:|
| Baseline | 0.495866 | 0.495553 | 0.496622 | — |
| Compose N=1 | 1.295478 | 1.288158 | 1.347397 | 0.792605–0.850775 |
| Compose N=2 | 1.914720 | 1.904595 | 1.928499 | 1.409042–1.431877 |
| Compose N=3 | 2.555238 | 2.560611 | 2.495867 | 1.999245–2.065058 |
| Compose N=4 | 3.095817 | 3.103578 | 3.109295 | 2.599951–2.612673 |

Measured full-path costs are approximately 31–32, 46, 60–62, and 74–75 ns per
sample for N=1–4. All checksums were deterministic across trials. Opaque N=1 and
baseline checksums both equal `17221459664959463043`.

## Hypothesis result

**Supported with constraints:** far-to-near RGB composition plus opacity-threshold
glyph selection is deterministic, exact-fixture testable, allocation-free, and
cheaper than P2's all-geometry collection.

**Not supported as an unconditional shipping pass:** N=4 composition alone adds
about 2.6 ms at full-screen coverage. P2 tracing plus P3 composition cannot fit the
6 ms budget if applied to every sample. The accepted research direction is:

1. retain the existing nearest-hit opaque fast path;
2. resolve P1 optics for that hit;
3. only if sight continues, collect farther prepared intersections selectively;
4. composite only samples that contain transmissive layers;
5. retain four as the bounded cap and report cap exhaustion explicitly.

P4 mirror measurements and final RQ6 aggregation are still required before D3 or
quality presets can be decided.

## Verification evidence

- Focused strict runner: 9/9 pass.
- Gate-off strict translation-unit compile: pass.
- Ordinary strict application build with research symbols absent: pass.
- Strict optimized aggregate `make -j2 check`: pass, including P1–P3 runners.
- Sequential clean ASan/LeakSanitizer and UBSan full suites: pass without
  diagnostics, including P3 9/9.
- Shipping surface benchmark raised path: 4.896944 ms; stability: 5.200202 ms;
  both deterministic and below 6 ms.
- Exact flat-default framebuffer parity and occluded-decal/raised-height checksum
  equality remain intact.

## Limitations

- Synthetic lists do not include material/sparse-override lookup cost.
- No full trace-to-compositor integration is added; selective continuation remains
  a required architecture prototype/input for Review H.
- Scalar light transmission is a policy helper, not wired into `lighting.c`.
- Glyph threshold 128 and the blend equation remain research recommendations.
- Generated discontinuities use identical blend math; their identity is retained
  for future ordering/debug policy rather than special-cased.
- No decals, highlights, sprites, entities, mirrors, or reflected openings are
  composited here. RQ5/P4 remain open.

## Reproduction

```sh
make build/test-r9-optical-compositor
./build/test-r9-optical-compositor
make benchmark-r9-optical-compositor
```

Next increment, only after confirmation: P4 bounded single-bounce mirror research.