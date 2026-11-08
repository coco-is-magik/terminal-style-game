# V1-4 — Unicode, font, and character-model research

**Status: research recorded 2026-11-06. Items 1, 4 and 5 are answered here; items 2 and 3
cannot proceed until the dependency decision in §7 is made.** No production file was changed.

Reference: `FEATURE_ROADMAP.md` V1-4 (purpose, locked product model, required research 1–5,
rollback point, exit gate); the options register in `TODO.md` §*Fonts, glyphs, and paste*;
`UI_DISPLAY_SURFACE_TARGET_2026-10-07.md` for the frame envelope.

## 1. Starting position (measured, not assumed)

| Fact | Value | Source |
|---|---|---|
| Glyph identity per cell | **one `uint8_t`** (0–255) | `src/grid.h` — `Cell.glyph` |
| Cell metrics | renderer **rejects** anything ≠ 8×8 | `src/renderer.c` — `renderer_create` guard |
| Face | **one**, `font8x8_basic[128][8]`, compiled in | `src/font8x8.h` |
| Atlas | built once from that bitmap at startup | `src/glyph_atlas.c` |
| Raster/shaping dependencies | **none** — `vendor/dist/include` holds only `SDL3`, `SDL3_mixer`, `cmocka`, `enet` | directory listing |
| Frame envelope | **41,600 cells** (260×160), 8×8 cells, **8.33 ms** at 120 fps | `UI_DISPLAY_SURFACE_TARGET_2026-10-07.md` §Grid |
| Text entry | ASCII `text_input` buffer; no IME, no clipboard | `src/input.c`, absence of clipboard calls |
| Asset glyph storage | a byte in every schema: `Material.glyphs[4]`, `PatternCell`, `ui_elements content=`, `.tui content=` | `src/assets.h`, loaders |
| Shipping render path | SMC stays the shipping path through V1-5 | `FEATURE_ROADMAP.md` V1-5 item 5 |

Two consequences follow immediately and shape everything below:

1. **The dependency surface is currently zero.** Adding a TTF rasteriser is not a library
   import; it is a new pinned, hash-verified, image-owned dependency under the vendoring
   discipline the build already applies to SDL and cmocka. That is a governance decision
   before it is a technical one.
2. **The budget is known.** Anything that rasterises or caches per frame must fit a workload
   that already draws 41,600 cells in 8.33 ms. A cache miss at 120 fps is a visible hitch.

## 2. Research item 1 — dependency evaluation

| Option | Licensing | Determinism | Malformed-input safety | Headless | Platforms | Bounded resources |
|---|---|---|---|---|---|---|
| **A. SDL_ttf** (wraps FreeType) | zlib + FTL/GPL | Good **if** hinting/AA pinned and rounding is ours | battle-tested | Yes | Linux/Windows | Yes with explicit cache |
| **B. FreeType directly** | FTL/GPL | Same as A, one layer less | same | Yes | Yes | Yes |
| **C. FreeType + HarfBuzz** | + MIT | Deterministic with pinned versions | most surface area | Yes | Yes | Yes |
| **D. Baked bitmap faces only** | project licence only | Perfect — pixels are assets | trivially safe | Yes | Yes | Yes |
| **E. In-house rasteriser over a vendored outline font** | project licence only | Perfect | must be written and audited | Yes | Yes | Yes, but real risk |

Notes that matter here specifically:

- **A and B are materially the same dependency** — SDL_ttf is a thin wrapper; FreeType gets
  vendored either way.
- **C is only needed for shaping.** The locked model defers full/color emoji and arbitrary ZWJ
  sequences, so shaping can be deferred without violating it — but CJK/kana/hangul need no
  shaping while Latin combining sequences do. Leave the HarfBuzz seam explicit rather than
  shipping it early.
- **D is a serious option.** Curated hand-baked mipmapped bitmap faces cover the locked
  required-script list with perfect determinism, zero dependency governance and trivially
  bounded memory — at the cost of authoring the art and giving up arbitrary TTF. It is the
  lowest-risk way to retire 8×8 while the TTF decision is still open.
- HarfBuzz and ICU are **not** vendored; any shaping path is a new dependency plus pinning work.

## 3. Research item 4 — determinism, parity, and raster rules

These are the rules the service must enforce, written so they can become tests rather than
prose. They are independent of which dependency is chosen.

1. **Hinting off, or one pinned hinting mode.** Hinting that varies by build or platform
   destroys cross-platform parity. Either `FT_LOAD_NO_HINTING` or an exactly pinned mode.
2. **Antialiasing is a fixed, declared mode per raster profile** — never a library default.
3. **Rounding is ours, once.** Outline → mask → coverage → pixels uses one documented rounding
   rule in one function, not at call sites.
4. **Baseline and cell fit.** Integer baseline and ascent/descent from the pinned profile, with
   a declared overflow policy (clip; never per-glyph scaling, or the grid stops being a grid).
5. **Fallback is explicit and deterministic.** A declared per-profile chain; failure yields the
   **missing** glyph — never a platform font, never a varying tofu box (the locked model forbids
   implicit system-font fallback).
6. **Normalization and segmentation are boundary services.** Strict UTF-8 + canonical NFC at
   every text API; a glyph-art cell accepts one cluster only if it resolves to one bounded cell.
7. **Documents stay pure.** No filesystem, SDL or platform fallback inside a document or a
   renderer path: the service resolves, the document references.
8. **Parity proof is a byte comparison, not a screenshot.** A canonical set (a few hundred
   codepoints across the required scripts) must rasterise to identical masks on Linux and
   Windows.

## 4. Research item 5 — glyph identity, catalog, and serialization (proposal)

Today's byte cannot express blank, absent, missing and a codepoint at once — the roadmap names
this directly. Proposed shape, deliberately testable and serialization-friendly:

```text
GlyphRef (32 bits, in-cell)
  kind       2 bits   0 = blank (no ink), 1 = transparent/absent, 2 = missing, 3 = ink
  profile    4 bits   raster profile id (face + metrics + raster mode), 16 max
  index     26 bits   glyph-catalog index within that profile
```

- **Blank vs transparent vs missing are distinct states**, resolving the overloading of byte
  zero and space that V1-5 item 1 calls out. `missing` renders the declared missing glyph and is
  a *validation diagnostic* when it appears in authored content.
- **The catalog maps index → (face, codepoint or cluster id, presentation selector)**, ordered
  deterministically (first-seen with a stable sort key) and written per scene as a
  human-readable table, so a `.tscene` stays diffable while the grid stays dense. That is V1-4
  item 5's "explicit human-readable glyph-cell serialization".
- **Migration is additive.** ASCII keeps working through an adapter mapping 0–127 to catalog
  entries in profile 0, so existing scenes load unchanged and old readers are not removed
  (V1-4/V1-5 rollback points).
- **Wire format** needs a version bump with the catalog as its own block; the strict unknown-key
  policy means it must be a deliberate schema addition.
- **`Cell` grows** from 1 byte of glyph to 4 (colours as today). At 41,600 cells that is
  +125 KB per frame buffer, and it touches dirty comparison, packed state, stream signatures,
  checksums, restore behaviour and every SMC adapter — the exact list in V1-5 item 5.

## 5. Research item 2 — fallback font selection (scoped, blocked)

Criteria, ready to apply once the dependency is chosen: licence compatible with distribution and
export; static or bitmap-only build where possible; coverage of the locked required-script list
(CJK ideographs, hiragana/katakana, hangul, common Latin, punctuation/symbols/box drawing);
per-file size consistent with a bounded atlas; stable, versioned releases.

Process: vendor into the pinned image, record **hash + licence + version** the way existing
vendored artefacts are recorded, add a coverage test asserting every required codepoint resolves
in the declared chain, and record the missing-glyph fallback result.

**Blocked on:** the dependency decision (§7). No font files can be fetched or hashed here.

## 6. Research item 3 — measurement plan (scoped, blocked)

Acceptance is measured against the known envelope — 41,600 cells, 8.33 ms — with these
instruments and thresholds:

| Measure | Method | Target |
|---|---|---|
| Raster cost per new glyph | microbenchmark, warm and cold | a cold miss must not exceed one frame's spare time at 120 fps |
| Atlas/cache memory | fixed budget per profile × glyph count | declared cap, no unbounded growth (V1-5 item 4) |
| Full-frame draw at 260×160 | existing benchmark and stability targets | no regression against the ASCII baseline |
| CJK legibility | visual acceptance at 100/125/150/200% | owner's eye; automated tests cannot assert this |
| UI-scale interaction | render at each preset | no per-glyph scaling, no reflow regressions |
| Cross-platform parity | mask byte comparison | identical |

Candidate raster profiles to measure: 8×8 (current), 8×16, 12×12, 16×16, including CJK at each.

**Blocked on:** the dependency decision (§7); profiles cannot be rasterised without a rasteriser.

## 7. Recommendation and the decision needed

**Recommendation: baked bitmap profiles as the default UI face, plus an optional FreeType-backed
profile for arbitrary TTF.** The default application and editor appearance comes from curated
baked profiles — perfect determinism, no cache risk, no new dependency on the baseline path — and
assets that want a real TTF opt into a FreeType profile where the cost is paid deliberately.
Method: **B or A** for the TTF path (A and B are materially the same dependency); **D** for the
baseline; **C** (HarfBuzz) only when shaping is required by a shipping language.

**Decisions needed to finish V1-4 (items 2 and 3):**

1. **Baseline face source** — baked bitmap profiles (recommended, no new dependency) or TTF from
   day one?
2. **TTF path** — A (SDL_ttf) or B (FreeType directly), and is a new pinned, hash-verified
   vendored dependency acceptable under the image-owned discipline? Does shaping ship with it or
   later?
3. **Scope of "granular fonts"** — is "baked profiles + optional TTF profiles" enough to satisfy
   per-asset font selection, or must arbitrary TTF be available everywhere?

## 8. Risks

- **SMC migration** (V1-5) is the largest cost and is not optional: the identity change in §4
  touches dirty comparison, packed state, stream signatures, checksums and every SMC adapter.
- **Atlas memory with fallback chains** can quietly exceed a profile budget at 260×160; the cap
  must be declared and tested, not discovered.
- **Determinism across platforms** depends on §3 being enforced centrally; one call site that
  inherits a library default breaks parity invisibly.
- **Licensing and export** (V1-19) must carry font licences transitively.
- **Governance** — every dependency is pinned, hash-verified and image-owned, so fewer is better.

## 9. Exit-gate mapping

| V1-4 exit-gate item | State after this record |
|---|---|
| Dependencies evaluated | **done** (§2) |
| Unicode version | **open** — pin with the dependency; all candidates cover current Unicode |
| Fallback fonts | **scoped and blocked** (§5) |
| Raster profile | **scoped, plan written, blocked** (§6) |
| Parity contract | **done as rules** (§3) |
| Resource budgets | **scoped** against the 41,600-cell / 8.33 ms envelope (§6) |
| Glyph identity and format direction | **proposed** (§4) |

One decision record selecting those items closes V1-4 and unblocks V1-5.


