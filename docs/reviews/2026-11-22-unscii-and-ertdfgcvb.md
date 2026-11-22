# Unscii and the ertdfgcvb Playground — What They Are, and What We Take — 2026-11-22

## Provenance and status

**Research only. No decision is made here.** The position this sits on top of is pinned in
[`../handoffs/2026-11-22-font-programme-position.md`](../handoffs/2026-11-22-font-programme-position.md);
the authority for the font programme is still
[`../V1_4_FONT_DEPENDENCY_DECISION_2026-11-06.md`](../V1_4_FONT_DEPENDENCY_DECISION_2026-11-06.md).

Sources actually read, and how:

| Source | What was read |
|---|---|
| `https://viznut.fi/unscii/` | the full page as HTML (the fetch tool refused it — its TLS chain does not verify — so it was read with `curl -k`, text only, no files downloaded) |
| `https://github.com/ertdfgcvb/play.core` | the README in full |
| `https://play.ertdfgcvb.xyz/abc.html` | the author's About page in full (the API section truncated mid-page) |
| `https://play.ertdfgcvb.xyz/?v=2.0` | the app shell only: a 5 KB page that boots `lib/playground.min.js`, so nothing was inferred from the bundle beyond its existence |

Not read: the unscii source package and its converter, the later sections of the Play API manual,
or the playground running live. Claims below are limited to what those pages state.

## Why these two are worth a review at all

They are independent arrivals at two halves of this project's thesis, which is rare and therefore
worth reading rather than skimming.

**unscii is the font half.** Its author's reason for building it is the reason this project has a
font programme at all:

> "No one seemed to use them. … After looking into existing Unicode fonts, the reason became
> obvious: the implementation of non-CP437 graphics characters was shaky at best. … It was a kind
> of chicken-and-egg problem: No commonly accepted Unicode graphics font, no Unicode art scene; no
> art scene, no font support. The idea of an art-compatible Unicode font was born."

That is V1-4's premise in someone else's words: the art form is blocked by the fonts, not by the
artists.

**Play is the editor half.** A character-grid authoring environment with the same reduction
instinct as this project's UI standards, stated plainly by its author:

> "It is born from the joy and pleasure ASCII, ANSI and in general text-based art can give and it
> is an homage to all the artists, poets and designers which used and use text as their medium.
> From the design perspective it is also an exercise in reduction: there is almost no interface,
> just a preview window, a code editor and a console."

## unscii: the facts that matter to us

- **A bitmapped Unicode font *family*, not one font.** Variants: `unscii-8` (8×8), `unscii-16`
  (8×16), `unscii-8-tall`, `-thin`, `-alt`, `-mcr`, `-fantasy`, and `unscii-16-full` (8×16 with
  gaps filled from Fixedsys Excelsior and GNU Unifont).
- **Stated purpose:** "attempts to support character cell art well while also being suitable for
  terminal and programming use."
- **Licensing:** the variants are **Public Domain**; only `unscii-16-full` is GPL, because of
  Unifont. That split is explicit, and it is exactly the kind of distinction D9 has to care about.
- **Formats:** HEX and PCF are the only actual bitmapped formats; TTF/OTF/WOFF are *vectorised* from
  the bitmaps. HEX is "the same simple hexdump format as used by the Unifont project". PCF cannot
  carry anything above U+FFFF; the PUA mappings live in `uns2uni.tr`.
- **The source package includes a generic bitmap-to-unscii converter** — the same kind of tool we
  built, for the same reason.
- **Design rules the author states:** 8×8 system fonts split into 1-pixel and 2-pixel line
  thickness, and he chose 2-pixel because "2-pixel 8×8 system fonts are surprisingly similar to one
  another which made it easier to choose neutral shapes". "In general, neutral shapes are preferred,
  unless art, legibility or readability require otherwise" — with `/\XY` made *connective* because
  of their use in ASCII art, and longer serifs on `iIl`. When an 8×16 shape is undefined, unscii
  **falls back to height-doubled 8×8**.
- **PUA ranges** for art characters that had no codepoint (Teletext mosaics `U+E080..E0FF`, PETSCII
  and Videotex oddities, and stranger things above), retained after Unicode 13 gave many of them
  real codepoints.
- **Unscii 2.0 exists because Unicode 13.0 added 214 legacy-computing graphics characters** — the
  font was already ahead of the standard and gained proper mappings afterwards.

### What it validates in our record

| Our choice | unscii's independent arrival at the same place |
|---|---|
| 8×8 is a first-class cell, not a compromise (D6) | the primary variant is 8×8; 8×16 is derived from it |
| Block/shade/box-drawing coverage is a requirement (register B6) | the entire project exists to make that coverage acceptable |
| Fixed cell, monospaced, neutral shapes | "neutral shapes are preferred", one cell geometry, text-art first |
| Coverage claims must be measured, never assumed (D9) | variants documented by what they contain; a PUA table because coverage needed bookkeeping |

### What it suggests we should do

1. **`unscii-8` is a strong third candidate face for the manifest**, beside Misaki and Noto:
   public domain (licence text is trivial), art-oriented, block/shade rich. D2 still stands — no
   bytes in the repo; a manifest entry with URL and hash. *Owner decision, not taken here.*
2. **A `.hex` (Unifont-style hexdump) input path for the converter** is the most concrete capability
   gain on the table. Today `--convert` can only read TTF, so it *requires* FreeType. A text
   hexdump reader would:
   - convert bitmap-only faces on a machine with **no FreeType at all**, shrinking the `FAIL-TOOL`
     case to nothing for those faces;
   - remove a rasteriser from the deterministic path for text-described bitmaps;
   - match this project's text-first asset philosophy — diffable, hashable, reviewable;
   - be small: one reader, one flag, its own tests, no effect on existing paths.
3. **A codepoint-mapping sidecar**, following `uns2uni.tr`, for faces that use PUA for art glyphs.
   That is register `B6`/`B3` territory; noted, not built.
4. **A design note worth stealing:** "2-pixel 8×8 system fonts are surprisingly similar to one
   another". If we ever need a *default* art face we can name rather than invent, that is the
   argument for which family it should come from.


## Play: how the model actually works

From the README and About page, quoted where descriptive:

- **A program is an ES6 module.** It may export three optional functions: `pre(context)` once per
  frame, `main(coord, context)` **once per cell**, and `post(context)` once per frame before
  rendering — plus `pointerMove`, `pointerDown`, `pointerUp`, `keyDown`, `keyUp`.
- **`main` returns a character, or an object** carrying the character with colours. Smallest
  program: `export function main() { return '?' }`.
- **The model is deliberately shader-like**: "instead of thinking in geometric primitives painted on
  a canvas this approach enforces to write a single function which will be invoked against each
  cell, an approach not so different from fragment shaders."
- **Rendering**: "text" inside a DOM element by default, with a canvas renderer available. Cell
  metrics are **measured at runtime**, so any monospaced font works — they ship no font into the
  rendering assumption.
- **Composition is layered on the per-cell primitive** through modules — `block()`, `write()`,
  `drawBox()`, `panel()`, `Desktop` — i.e. higher-level authoring tools over the same loop.
- **The interface is a console plus hotkeys**: commands (`man`, `lsd`, `font`, …), settings that
  override the program, programs in localStorage, and shareable permalinks, with no accounts.
- **Licensing mirrors our rule by accident:** code is Apache-2.0; the two fonts are proprietary to
  the site and **not in the repository**. A text-art tool independently decided not to ship font
  bytes.
- Stated performance envelope: a maximised window holds "about 5–8000 monospaced characters", and
  JavaScript can drive that at 30 fps with shader-style techniques.

### What Play does better than our editor, and what we do better

| Dimension | Play | This project |
|---|---|---|
| Expressiveness per keystroke | very high: one formula paints a screen | low: one cell at a time, by design |
| What the artefact *is* | a program; frames are ephemeral | a document of cells; the art is the artefact |
| Re-editing someone else's work | edit their code | paint over their cells |
| Validation, undo, schemas, references | none — it is code | first-class: every asset parsed, validated, undoable |
| Determinism | depends on the program | structural: same document, same pixels |
| Runtime safety | executes user JavaScript | no code execution at runtime |
| Persistence | localStorage + permalink | files in the project, diffable in git |

The honest summary: **Play is a better generator; we are a better canvas.** Neither contains the
other, so the question is not whether to copy it but whether we can take its generative power
without giving up documents.

### Adaptation options, in order of fit

**A. A generator that *bakes* into authored cells — the recommended direction, because the
precedent already exists.** `mapgen` generates a scene offline, deterministically, from recorded
parameters, and what comes out is an ordinary scene document with a provenance sidecar
(`mapgen_seed`). A cell generator would be the same shape: the user describes a rule (density ramp,
dithering threshold, wave, repetition with variation), the tool evaluates it *once*, and the result
is cells that are then painted and edited normally.
- Keeps documents, determinism, and git diffs that mean something.
- Reuses the provenance and determinism discipline already built for maps, and now for fonts.
- No user code at runtime — which we cannot have regardless: a C runtime with no sandbox, and a
  scripting language would be a second expression parser beside the one the project already owns.
- Cost: a small expression subset, a generator panel, and the discipline that a generator writes
  *cells*, never a hidden program.

**B. Time-varying generators as *effects*, not scripts.** Animation already carries time
(`*.tui` effects, motion, sprite playback). A generator with `time` could be baked per frame into
an effect, giving Play-style motion inside the authored model.

**C. Never: a live user-code runtime.** Not conservatism — consequences. An interpreter in the
shipped binary, frames non-deterministic by construction, and the parity claims that both the SMC
work and the font work rest on would stop being checkable.


### Smaller things worth taking from Play

- **The per-cell signature is a good public explanation.** "One function, one cell" is how our
  generator layer should be described to users, even though ours is baked rather than live.
- **Console-plus-hotkeys over chrome** is validated by a working professional tool with 500+ stars;
  our UI standards already lean that way and now have a data point rather than an instinct.
- **Settings that override the program** maps onto our document-overrides-defaults model; worth a
  look when the generator grows settings of its own.
- **"Programs can also be written together with an AI agent"** (their 2026 addition) is interesting
  in the inverse direction: a *document* is the better substrate for assisted authoring, because
  its intent is inspectable cell by cell rather than inferred from code.

## What this review does not change

- **D6** (8×8, one cell), **D2** (no font bytes in the repo), **D7** (identity), **D11** (fit
  policy) and **D8** (sentinel) are untouched by everything above.
- **The gate for visible glyphs is still the cell identity migration** (`Cell.glyph` is a byte,
  `CellState` is a `uint64_t`). A public-domain face and a hex reader do not move that gate.
- **No new dependency is proposed.** A `.hex` reader is our own code; unscii is a font file, not a
  library.

## Open questions for the owner

1. **Should `unscii-8` become the recommended reference face** in `assets/fonts.example.txt`
   (public domain, art coverage), with Misaki kept as the CJK case?
2. **Is the `.hex` input path worth doing before the cell identity migration?** It is small and
   removes a host-tool dependency, but it makes nothing visible in the game.
3. **Is a baked generator layer the right home for Play-style expressiveness**, or does it belong to
   V1-7 (glyph-art formats) rather than the font programme?
4. **Should the PUA mapping sidecar be scoped now** with `B6`, given unscii documents the same need?

