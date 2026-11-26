# Font Programme Handoff — V1-4 Closed, V1-5 In Progress — 2026-11-22

## Provenance and status

This handoff exists so the position can be recovered without replaying a long working session. It
records what is decided, what is built, what is *proven* (with the evidence), what is deliberately
not built, and exactly where the next pass starts. **Documentation state at the time of writing:**
everything below is committed on `dev` at `828018e`; the working tree is clean.

The decision record this all descends from is
[`../V1_4_FONT_DEPENDENCY_DECISION_2026-11-06.md`](../V1_4_FONT_DEPENDENCY_DECISION_2026-11-06.md)
(D1–D12). Read that first; it is the authority. The cell-identity question that was open in the
first version of this handoff is now **decided** in
[`../V1_5_CELL_IDENTITY_DECISION_2026-11-22.md`](../V1_5_CELL_IDENTITY_DECISION_2026-11-22.md)
(16-bit codepoint, 64-bit state unchanged in shape, and a loud enumerated warning above U+FFFF).
This file is the map of the territory around them.

## Roadmap position

- **V1-4 is closed.** Its exit gate is answered in the decision record's own table, including the
  two items originally left open (fallback fonts, raster-profile measurement).
- **V1-5 is in progress** (identity, raster/catalog, cache, SMC migration). Five slices have landed;
  the sixth — the cell identity migration — has not started.
- **No other phase has moved.** V1-6 and V1-7 are untouched.

## What was decided (and twice corrected)

The first draft of the decision record argued for taller text cells and committed glyph images.
Both were wrong and both were replaced in place, marked `[revised]`, with the superseded reasoning
kept visible:

- **No glyph data in the repository.** The converter runs at build/install time against fonts the
  user supplies; the font manifest (name, per-file SHA-256, source URL, licence, coverage, cell) is
  the contract. `assets/fonts/` is gitignored.
- **One 8×8 cell, no profiles.** 8×8 CJK bitmap faces exist (Misaki), so "CJK needs 16 rows" was
  false; the fit modes exist for fonts that do not fit, not to accommodate a redesigned grid.
- **No fallback, ever.** A glyph the declared face lacks renders as a derived sentinel — `X`, red
  background, magenta foreground — and raises `TSG-ASSET-ENV-0001` once per (face, codepoint).
  Same rule as the missing-decal checker: derived, never serialised.

## What is built

| Module | Job | Tests |
|---|---|---|
| `src/sha256` | FIPS 180-4 digest | 7 |
| `src/font_manifest` | strict `key=value` font contract | 10 |
| `src/font_fit` | the 8×8 fit policy: `none`/`squash`/`scale`/`normalize` | 16 |
| `src/glyph_set` | the only definition of the set format, plus validated reading | 7 |
| `src/glyph_source` | which face answers for a codepoint, or the sentinel | 5 |
| `tools/fontconv` | `--list-fonts`, `--convert`, `--check-set`, `--render` | exercised end to end |

**45 tests**, all inside `make test` (**88 suites**, `TEST=0`). `make standards` clean, cppcheck

**Correction (2026-11-25).** The `88 suites` / `TEST=0` figures above were produced on the source
host and are superseded. A clean rebuild fails `test-r9-optical-semantics` on stale expectations left
by the 2026-11-07 scene-ceiling change; corrected on 2026-11-25. The measured inventory is now
**91 runners / 1005 tests**. See
[`2026-11-25-host-migration-and-suite-recovery.md`](2026-11-25-host-migration-and-suite-recovery.md).

## What is proven, and how

Against the **real** Misaki archive (`misaki_ttf_2021-05-05.zip`, 794,317 bytes):

| Claim | Evidence |
|---|---|
| Verification distinguishes missing, mismatched, uncheckable | exit 0/1/2; expected vs installed digests; source URL printed |
| The converter takes the font's own pixels | 7,171 glyphs from the **embedded 8×8 strike**, 0 refused, all `none` |
| Conversion is deterministic | digest `5e85b3e4…` reproduced across runs **and across sessions** (archive re-downloaded, `0963e11e…` identical) |
| Determinism is checkable by the build | `make font-determinism FACE=…` converts twice, compares, validates; `FAIL-TOOL` when fonts are absent |
| Refusal is exact and non-destructive | `--size 16` refuses 7,094 glyphs naming the overshoot, writes **no** set, exits 1 |
| The threshold is right, not merely plausible | `--size 16 --fit scale` reproduces the shipped 8×8 bitmaps for full-width glyphs byte for byte (U+3042 identical) |
| Producer and consumer agree | `--check-set` reads the real 114,784-byte set through `src/glyph_set`, prints correct pixels |
| Bit order matches the renderer | pinned by test against `renderer.c` (`row_pixels[0] = (row & 1)`) |

## What is deliberately not built

- **Nothing in the game reads a converted set.** The renderer still draws `font8x8_basic`; no visual
  change, no regression surface from any of the above.
- **No manifest is committed.** `assets/fonts.example.txt` ships instead with 64-zero digests, and a
  test keeps them fake; filling it in verifies the real archive.
- **No UTF-8 decode.** `--render` takes hex codepoints. UTF-8/NFC/segmentation is `utf8proc` (D3),
  and vendoring it is its own increment — a hand-rolled decoder would be a second implementation of
  a decision that already has one.
- **No width-class decision.** Misaki reports 7,003 full-width / 168 half-width advances; what a
  half-width glyph does in an 8×8 cell is register `B2`'s call. The converter reports the split.

## Where the next pass starts

**The cell identity migration is the gate for anything visible.** `Cell.glyph` is a `uint8_t` and
`CellState` packs glyph + fg + bg into a `uint64_t`, so a codepoint above 255 cannot travel.

Corrected blast radius (measured 2026-11-22, replacing an earlier estimate of "124 sites across 20
files" that counted decal `glyph_step` and UI `glyph_set` strings — the lesson being that the first
number was searched, not measured):

| Quantity | Count |
|---|---|
| direct `cell->glyph` field reads | 15, across 10 files |
| `grid_set(..., uint8_t glyph, ...)` call sites | 87, across 13 files |
| `ui_canvas_set` call sites | 45 |
| test files touching `.glyph` | 26 |
| **scene file format impact** | **none** — scenes store decals and objects, not cells |
| `glyph_block_cache` key | already `uint32_t glyph_id`, so it upgrades for free |

So the work is dominated by the **API signature** (`grid_set`/`ui_canvas_set` take `uint8_t`), not
by field reads, and no save-format migration is required. The genuinely hard part is `CellState`:
56 of its 64 bits are used (8 glyph + 24 fg + 24 bg), so a 32-bit glyph does not fit and the
representation is a **decision that has not been made** — see the fork below.

Widening it means, in dependency order:

1. a decision on the `CellState` representation (see below);
2. `grid_set` / `ui_canvas_set` signatures and their 132 call sites (mechanical, but each one needs
   reading for intent — some pass `' '`, some pass `0`);
3. dirty comparison and packed-state paths that assume one byte of glyph;
4. SMC stream signatures, checksums, block codec, capture/restore;
5. `make matrix` re-baselined — the eight tracker modes are the parity gate.

**The `CellState` fork, which is why this needs a decision record before code:**

- **128-bit state** (two `uint64_t`, or a 16-byte struct) — semantics are exact, no collisions; the
  SMC batch kernel changes shape and the parity gate must be re-baselined. Straightforward, costly.
- **Keep 64 bits, use an 8-bit glyph *handle*** into a registry — kernel unchanged, but state
  equality across frames becomes subtle: two different glyphs must never share a slot within a
  frame's lifetime, and that invariant is exactly the kind that fails once and silently.
- **Shrink colour precision** to buy bits — rejected on sight: the look is locked and colour is the
  medium.

This is the reason the resolver was built first: when the migration lands, the draw path calls
`glyph_source_resolve` instead of inventing a second rule.

## Reproducing the position from scratch

```sh
cd /tmp && mkdir -p misaki/ttf && cd misaki
curl -LO https://littlelimit.net/arc/misaki/misaki_ttf_2021-05-05.zip   # 794,317 bytes
unzip -oq misaki_ttf_2021-05-05.zip -d ttf
sha256sum ttf/*.ttf        # 0963e11e… gothic, 5f0cbb88… gothic_2nd, e383f92c… mincho
# write a manifest from those digests (shape: assets/fonts.example.txt), then:
make fontconv
./build/fontconv --manifest fonts.txt --fonts-dir ttf --convert misaki-gothic \
    --file misaki_gothic.ttf --out /tmp/g.tsgfont            # expect digest 5e85b3e4…
./build/fontconv --check-set /tmp/g.tsgfont --render 41,3042,2588,1F600
make font-determinism FACE=misaki-gothic MANIFEST=/tmp/misaki/fonts.txt \
    FONTS_DIR=/tmp/misaki/ttf FILE_NAME=misaki_gothic.ttf
```

## Open questions this pass left for the next

1. **Width classes (`B2`)** — the half/full split is measured; the policy is not written.
2. **`utf8proc` vendoring (D3)** — pinned, licensed, image-owned like every other dependency. Not
   started; `vendor/` is fetched by script, so this touches the platform profiles.
3. **FreeType packaging** — a host tool for the converter today (`pkg-config`); recorded as a
   packaging detail in the decision record rather than decided.
4. **In-editor glyph preview** — `--render` exists in the tool; the editor cannot show converted
   glyphs until the cell identity migration lands.

## External references gathered after this point

Two resources were surfaced after this handoff was written and are analysed in
[`../reviews/2026-11-22-unscii-and-ertdfgcvb.md`](../reviews/2026-11-22-unscii-and-ertdfgcvb.md):
unscii (a Unicode 8×8/8×16 bitmap font family whose authoring philosophy is close to ours) and the
ertdfgcvb character-grid playground (a script-driven editor for the same medium). Neither is a
decision; that review records what they do, what we already do, and what would be worth taking.

included. The app builds with every new module linked.
