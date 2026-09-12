# R10 Increment I1 Implementation Record — Colored Illumination — 2026-08-27

## Status

**Implemented and Verified on 2026-08-27.** Automated verification passed and
the combined I1/I2 manual visual/input acceptance passed in a live display
session on 2026-08-27; I1 is Verified. R10 I2 is also Verified; see
`R10_INCREMENT_I2_IMPLEMENTATION_RECORD_2026-08-27.md`.

## Implemented behavior

- `Map.light_map` is now one row-major `LightLevel { red, green, blue }` per tile.
- Ambient initializes all three channels equally. Each point light contributes
  `falloff * intensity * alpha/255 * channel/255`; shadow bounce scales all three
  channels; negative intensity subtracts only the light's weighted channels.
- `palette_sample()` clamps/scales RGB independently. Existing white RGBA=255
  lights remain scalar-equivalent; black RGB emits no surface light.
- All current wall, floor, ceiling, heightfield, decal, optical, mirror, scene
  resize, parser, loader, test, and benchmark owners use the per-channel store.
- The point-light inspector exposes Alpha (0..255, step 1) through typed step and
  numeric requests, command history, runtime rebuild, undo, and existing v6
  persistence. No scene-format version change was needed.

## Cache boundary correction

The Q1 plan proposed caching per-channel/final intensity and invalidating on
color/alpha/intensity edits. Implementation evidence showed that would couple
appearance to the shadow cache unnecessarily. `LightSampleResult` now stores only
shadow-adjusted geometric attenuation; every cache hit applies the current RGB,
alpha, and intensity. Position and radius remain geometric inputs in the key.

The first cache-enabled run exposed real stale-entry defects: simultaneously live
`Map` instances shared keys, and position/radius edits reused old attenuation
because revisions were hard-coded. The key now includes runtime map identity and
exact position/radius bit patterns. The existing durable map-topology/revision TODO
(including allocator-address reuse after map destruction) remains outside I1.

## Transmission extension decision

Per-channel transmitted-light tinting was **deferred**. R9's authored and resolved
`transmission` is one scalar byte, not RGB. Implementing colored filtering would
require new scene data and a format migration, violating I1's locked no-format-
change boundary. Existing scalar transparency composition remains unchanged;
colored already-lit samples continue to compose correctly through R9 layers.

## Tests and evidence

- Default lighting suite: 9/9, including exact RGB/A math, additive colored
  mixing, per-channel anti-light, independent sampler clamp, optical blocking,
  and the source-cell diagonal-wall regression.
- Cache-enabled lighting suite: 10/10; added proof that a cache hit uses edited
  color, alpha, and intensity. Direct cache suite: 3/3.
- Editor domain: 12/12; unified editor: 79/79, including Alpha numeric edit,
  runtime propagation, and undo.
- Core: 61/61; scene document: 46/46; complete default `make test`: pass.
- `benchmark-colored-lighting` (32x24, four lights, 200 updates):
  - default observed runs: white 0.101–0.239 ms, colored 0.096–0.164 ms;
  - cache enabled: white 0.044 ms, colored 0.043 ms;
  - 6.0 ms/update budget passed; deterministic white checksum
    `11839672862453039471`, colored checksum `2975859286826906331` in both modes.

- Strict optimized `make check`: passed (`-Wall -Wextra -Wpedantic -Werror`),
  including the current-renderer caller guard.
- ASan/LeakSanitizer: passed. The full clean UBSan command exhausted the 120 s
  harness while compiling its final target; resuming the already-built UBSan
  `make ... test` step completed with status 0 and all suites passed.
- Optimized app build, smoke (`{"smoke":"ok","map_width":10,"map_height":6}`),
  and `git diff --check`: passed.

## Manual acceptance — passed

The combined I1/I2 manual visual/input acceptance passed in a live display
session on 2026-08-27: red/green/blue mixing, alpha 0/128/255, anti-light
channel subtraction, and save/reopen. I1 is marked Verified; I2 spot lights
were accepted in the same session (see
`R10_INCREMENT_I2_IMPLEMENTATION_RECORD_2026-08-27.md`).