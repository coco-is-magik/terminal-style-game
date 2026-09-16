# V1-1 UI Literal and Consumer Inventory — 2026-09-16

## Scope and result

This is the first evidence artifact required by
`V1_1_UI_RULES_AND_TOKENS_Q1_PLAN_2026-09-16.md`. It inventories current UI ownership,
visual literals, geometry/scale assumptions, and focused regression owners. It does not
select D1-D7 values or authorize implementation.

The codebase already has the required high-level separation, but not a complete semantic
token layer:

- application/editor UI is owned by `ui_ele`, application menu assets, `app.c`, and
  direct unified-editor text rendering;
- authored game Menu UI is owned by persisted `UiDocument` data and rendered through
  `ui_layout_resolver` plus `ui_render_adapter`;
- `UiRenderTheme` is a narrow transient-state bridge for focused, pressed, and disabled
  authored controls; it does not own normal authored presentation;
- `ui_canvas` and `ui_compositor` are value/render services, not theme owners.

## Application/editor UI inventory

### Data-driven application menus

`assets/ui_elements/*.txt` and `assets/ui_layouts/*.txt` own application menu hierarchy,
absolute/relative cell geometry, alignment, visibility, z-order, content, and optional
foreground/background values. `ui_ele.c` supplies white-on-black defaults:

- foreground `(255,255,255,255)`;
- background `(0,0,0,255)`.

Only the checked-in main/pause title assets currently override foreground explicitly;
most controls inherit the defaults. Representative geometry is tuned to the configured
260x160 logical grid: main menu `24x16` at `(120,68)`, pause `24x18` at `(118,66)`,
settings `32x28` at `(116,64)`, and confirm-quit `20x7` at `(120,76)`.

`app.c` then injects state colors procedurally:

- selected menu item: foreground `(50,255,50,255)`, background `(0,40,0,255)`;
- unselected menu item: foreground `(200,200,200,255)`, black background;
- ordinary menu render: white foreground, black background;
- over-budget HUD status: dark red `(150,0,0,255)` background;
- UI-scale feedback uses green/black for committed success and other direct status
  colors in the same adapter.

These procedural values are primary D1/D4 editor-token candidates. Asset-owned geometry
is evidence for D2/D5 but must not be bulk-rewritten before minimum viewports and density
rules are accepted.

### Unified editor overlays

`unified_editor.c` directly renders many editor workspaces with repeated palettes:

- primary text `(220,220,220,255)`;
- black background `(0,0,0,255)`;
- dim text `(150,150,150,255)` or `(160,160,160,255)`;
- highlight/focus `(120,220,160,255)`;
- warning `(220,180,80,255)`.

The authored-Menu preview transient theme is also local to `unified_editor.c`:

- focused foreground/background `(120,220,160,255)` / `(0,40,20,255)`;
- pressed foreground/background `(20,20,20,255)` / `(220,220,120,255)`;
- disabled foreground/background `(120,120,120,255)` / `(20,20,20,255)`.

These repeated values are strong semantic-role candidates. Existing glyph cues (`>`,
`<`, `#`, `!`) are color-independent continuity evidence and must remain until an
accepted replacement proves equivalent accessibility.

Editor geometry also has explicit limits and layout assumptions:

- picker visible rows: 4;
- map chooser visible rows: 10;
- authored Menu preview cap: 80x25;
- many overlays reserve first/last rows and print from column 1;
- sprite workbench columns depend on authored sprite width and fixed local gaps.

These are not automatically global spacing tokens. D2/D5 must classify them as semantic
layout rules, bounded workflow constraints, or content-dependent geometry.

## Authored game Menu inventory

`UiDocument` v3 persists author-owned:

- design width/height;
- per-element x/y/width/height, anchors, and 25%-400% local scale;
- foreground/background RGBA;
- fill/border enablement and glyphs;
- native/sprite visual mode and sprite ID;
- text alignment and default visibility.

Defaults are 80x25, white-on-black, space fill, `#` border glyph, left alignment,
container fill enabled, border disabled, and 100% local scale. Checked-in authored Menus
use those defaults and persist them explicitly.

These values are authored content and format compatibility, **not global token values to
replace**. V1-1 may provide a template/default resolver for newly authored content, but
must not silently restyle existing documents or override explicit author values.

`UiRenderTheme` currently owns only transient focused, pressed, and disabled
foreground/background pairs. Runtime state precedence is:

1. preserve authored colors when explicitly requested;
2. disabled;
3. pressed;
4. focused;
5. normal authored colors.

The adapter additionally supplies non-color markers: disabled `! !`, pressed `# #`, and
focused `> <`. This narrow state overlay is the existing seam for D4 and the future
authored-template adapter.

## Scale, viewport, and rendering services

- Global UI scale presets are exactly 100%, 125%, 150%, and 200% in
  `ui_preferences.c`; 150% is the emergency fallback.
- `ui_compositor_scaled_edge()` uses integer `(edge * scale) / 100` projection and the
  compositor tests adjacency at fractional scales.
- The compositor assumes the current fixed 8x8 raster and supports top-left, centered,
  and bottom-left layer anchors with explicit clipping and stable z/insertion order.
- Authored Menu preview resolutions are 40x15, 60x20, and 80x25. Effective logical
  dimensions are `base * 100 / scale`.
- The shipping configured logical grid is 260x160 with fixed 8x8 cells; V1-4/V1-5 own
  later font/glyph changes.
- `app.c` currently converts SDL window coordinates to render coordinates and then grid
  cells. Complete coordinate/capture semantics remain V1-3 and must not be inferred from
  this adapter alone.

The accepted scale presets, clipping, painter order, and adjacency are preserved
contracts. Minimum product viewports and target dimensions remain unresolved D2/D5
decisions.

## Menu depth and information hierarchy

The application has four menu contexts and a generic `MENU_STACK_MAX` of 8. That storage
capacity is an implementation bound, not an accepted ordinary menu-depth target.
Application routing currently opens Settings and Confirm Quit as nested contexts.

The unified editor exposes many nested workspace modes, but they are stable-parent
subcontexts rather than automatically distinct major contexts. D5 and V1-3 must classify
semantic depth and major contexts; they must not equate enum/mode count with user-visible
menu depth.

## Candidate mapping by Q1 decision

| Decision | Existing evidence | Likely owner after approval |
|---|---|---|
| D1 color/contrast | `ui_ele` defaults; `app.c` state/status colors; editor palettes; preview transient theme | pure token values + separate editor/authored adapters |
| D2 spacing/density/borders | application asset cell geometry; editor row/column gaps; compositor integer scaling; authored border fields | token values for editor defaults; authored geometry stays content-owned |
| D3 typography/labels | fixed 8x8 ASCII renderer; bounded `content`; truncation in render adapter; direct editor help/status strings | V1-1 label policy only; font/shaping remains V1-4/V1-6 |
| D4 focus/status states | procedural menu colors; `UiRenderTheme`; glyph state markers; interaction eligibility | pure state-to-role policy with separate adapters |
| D5 viewport/depth | 260x160 app layout, 80x25 authored default/cap, 40x15 and 60x20 previews, stack capacity 8 | policy constants distinct from storage limits |
| D6 motion | no general UI motion system; explicit time exists elsewhere but no accepted context timings | policy values only in V1-1; runtime implementation in V1-3 |
| D7 ownership/adapters | `ui_ele`/`app.c` versus `UiDocument`/`ui_render_adapter`; immutable borrowed runtime data | one pure value model, two non-owning adapters |

## Regression owners

- `test-ui-ele`: application asset parsing, hierarchy, focus/action data, alignment,
  visibility, and z-order.
- `test-ui-preferences`: scale presets, fallback, precedence, and persistence outcomes.
- `test-ui-compositor`: scale adjacency, clipping, stable ordering, transparency, and
  reference/optimized parity.
- `test-ui-document`: authored visual/layout persistence, validation, migration, and
  transactional failure.
- `test-ui-layout-resolver`: anchors, local scaling, clipping, overflow, and invalid
  documents.
- `test-ui-render-adapter`: authored colors/visuals, transient state precedence,
  color-independent markers, clipping, painter order, and missing dependencies.
- `test-ui-interaction`: topmost hit, deterministic traversal, eligibility, and invalid
  input nonmutation.
- `test-ui-menu-runtime`: focus/press/activation and deterministic runtime replay.
- `test-ui-menu-workspace`: authored visual/layout editing, preview resolutions/scales,
  pointer transactions, history, and persistence.
- `test-unified-editor`: integrated editor workspace rendering/input behavior and
  authored Menu preview/test composition.

The original nine focused UI owners, provisional `test-ui-theme`, bounded
`test-ui-app-theme-adapter`, and diagnostic `test-ui-theme-demo` are in
`make test-ui-standards`; all twelve are in the canonical `make test` inventory.

## Risks and non-token content

1. World/render/debug colors in raycasting, lighting, assets, and benchmark patterns are
   not UI theme candidates merely because they use `SDL_Color`.
2. Authored Menu normal colors, glyphs, geometry, and visibility are creative content.
3. Material/palette colors in sprite-backed UI are asset-owned.
4. `MENU_STACK_MAX`, element capacities, path capacities, and history limits are storage
   bounds, not design-token values.
5. Existing application UI absolute positions are coupled to the current 260x160 grid;
   migrating them before minimum-viewport rules risks making current layouts worse.
6. The existing fixed 8x8 raster constrains current measurements but must not become a
   permanent typography decision ahead of V1-4/V1-5.

## Next Q1 action

The inventory requirement is complete. Prepare concrete D1-D7 candidate values and
formulas, cite exact accessibility guidance for contrast/focus/reduced motion, and
present the behavior-affecting choices for product approval. Do not create token APIs or
migrate components until that decision record is accepted.