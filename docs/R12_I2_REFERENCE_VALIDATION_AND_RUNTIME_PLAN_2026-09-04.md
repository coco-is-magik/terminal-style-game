# R12 I2 Reference Validation and Runtime Navigation Plan — 2026-09-04

## Status

**Implemented; automated verification passed on 2026-09-04.** I1 established the
separate persisted `FlowDocument`. I2 adds typed external-reference validation and
pure runtime navigation without integrating the graph into the application or editor.

## Current repository constraint

Native scenes have an authoritative `.tscene` catalog, but authored game menus,
`UiDocument`, and named scene-exit ports do not exist yet. Existing
`assets/ui_layouts` are application-owned UI and are not R12-authored game menus.
Therefore I2 introduces a narrow borrowed catalog seam rather than treating current
application layouts as game content or prematurely changing scene/trigger formats.

## Locked requirements

- A borrowed `FlowReferenceCatalog` describes typed Scene/Menu assets and each
  asset's named outgoing ports.
- Catalog names and ports use the same bounded identifier grammar as I1.
- Duplicate type/name entries and duplicate ports within an entry are invalid.
- Every Scene/Menu graph node resolves to exactly one same-typed catalog entry.
- Every edge leaving a Scene/Menu node uses a port declared by that entry.
- Start remains built in and only uses its I1 `start` port.
- Catalog validation never mutates `FlowDocument` or the catalog.
- A pure `FlowRuntimeSession` owns only the current graph node ID.
- Session initialization starts at the graph's unique Start node after graph and
  reference validation pass.
- A transition resolves exactly one edge from the current node and requested port.
- Missing ports, invalid data, and inconsistent session state are explicit errors
  and leave session/output unchanged.
- Cycles remain valid and deterministic.

## Explicit non-goals

- no disk-scanning inside the validation/runtime modules;
- no `SceneDocument`, trigger enum, or scene-format changes;
- no `UiDocument` or authored-menu format yet;
- no application Start Game integration;
- no editor graph workspace;
- no conditions, scripts, implicit fallback edges, or hidden global state.

## Verification gate

- catalog validation covers valid graphs, missing/type-mismatched assets, missing
  ports, duplicate catalog entries/ports, and malformed inputs;
- runtime tests cover Start navigation, scene/menu transitions, cycles, missing
  ports, invalid session state, deterministic replay, and non-mutation on failure;
- focused strict, ASan/LeakSanitizer, and UBSan runners pass;
- aggregate `make test`, strict application build, and smoke remain healthy.

## Next increment boundary

After I2, define `UiDocument` scope and its minimum authored Menu/button-port schema.
Scene-exit authoring can then be decided against the established borrowed catalog
boundary without changing the graph/runtime core.

## Verification record

- focused strict `flow_reference` runner: **3/3 passed**;
- focused strict `flow_runtime` runner: **3/3 passed**;
- both runners compile under `-std=c11 -Wall -Wextra -Wpedantic -Werror`;
- focused ASan with leak detection: both runners passed **3/3**;
- focused UBSan with halt-on-error: both runners passed **3/3**;
- aggregate `make test`: passed, including both registered I2 runners;
- strict application build and `make smoke`: passed with
  `{"smoke":"ok","map_width":10,"map_height":6}`;
- optional `cppcheck`: skipped because the tool is unavailable.

No existing editor, application menu, scene format, trigger, renderer, or persistence
behavior changed. The catalog is a borrowed typed seam populated by future adapters;
I2 does not claim filesystem discovery for authored menus or scene ports.