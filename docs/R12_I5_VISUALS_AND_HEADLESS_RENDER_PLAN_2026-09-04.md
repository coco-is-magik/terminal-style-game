# R12 I5 Visual Vocabulary and Headless Render Plan — 2026-09-04

## Status

**Implemented; automated verification passed on 2026-09-04.** I1–I4 established
progression, Menu documents, and responsive geometry. I5 adds validated authored
visual intent and a headless canvas adapter before editor or application integration.

## Locked visual vocabulary

- `UiDocument` v3 gives each element either Native or Sprite visual mode.
- Native visuals persist foreground/background RGBA, explicit fill enable/glyph,
  border enable/glyph, text alignment, and visible-by-default.
- Sprite visuals persist an existing numeric sprite ID. The renderer consumes frame 0;
  animation phase/time remains owned by later runtime adapters.
- Container defaults to native blank fill; Text/Button default to transparent native
  backgrounds with white text.
- v1/v2 migrate to those explicit native defaults; canonical writes use v3.
- Missing Sprite or material dependencies are render errors, not silent substitution.

## State and theme boundary

- Focused, pressed, disabled, and runtime visibility are transient caller inputs keyed
  by stable element ID; they are not persisted.
- State precedence is Disabled > Pressed > Focused > Normal.
- Normal colors are authored. State colors come from one explicit borrowed
  `UiRenderTheme`, avoiding duplicated persisted state palettes.
- Button state remains non-color-only: Disabled uses `!`, Pressed uses `#`, and Focused
  uses `>`/`<` edge markers.

## Render boundary

- `ui_render_adapter` resolves I4 geometry and writes to a caller-owned `UiCanvas`.
- It owns no allocation, I/O, input, focus traversal, animation time, app state, or
  editor state.
- It validates document, state IDs, theme, geometry, and all sprite/material
  dependencies before clearing or changing the destination.
- Document order is painter order. Parent/viewport clips are honored.
- Native content is one-line, clipped, and left/center/right aligned.
- Sprite patterns use deterministic nearest-neighbor scaling into the resolved
  rectangle. Whitespace/NUL pattern cells are transparent.
- Sprite cell color is the referenced material palette's near color at full light.

## Explicit non-goals

- no editor canvas, hit testing, focus navigation, or input routing;
- no application runtime activation or Start Game integration;
- no theme asset/persistence system;
- no authored state-specific style duplication;
- no animated UI phase ownership;
- no migration of application-owned UI assets.

## Verification gate

- v3 round-trip and v1/v2 migration tests pass;
- renderer tests cover native fill/border/text, state precedence and markers, sprite
  scaling/material color, clipping/order, missing dependencies, and destination
  non-mutation on failure;
- focused strict, ASan/LeakSanitizer, UBSan, aggregate tests, and smoke pass.

## Next increment boundary

After I5, add pure hit testing and focus/navigation semantics against resolved geometry
before integrating authored UI into the editor.

## Verification record

- focused strict `ui_document` runner: **10/10 passed**;
- focused strict `ui_layout_resolver` runner: **3/3 passed**;
- focused strict `ui_render_adapter` runner: **6/6 passed**;
- affected runners compile under `-std=c11 -Wall -Wextra -Wpedantic -Werror`;
- focused ASan with leak detection: document and final renderer runners passed;
- focused UBSan with halt-on-error: document and final renderer runners passed;
- aggregate `make test`: passed, including all three I5-affected runners;
- strict application build and `make smoke`: passed with
  `{"smoke":"ok","map_width":10,"map_height":6}`;
- optional `cppcheck`: skipped because the tool is unavailable.

The final render review bounded fill, sprite, and border work to the visible clip and
uses 64-bit intermediates for large off-screen authored rectangles. No existing
application/editor module calls the new visual mutation or render APIs.