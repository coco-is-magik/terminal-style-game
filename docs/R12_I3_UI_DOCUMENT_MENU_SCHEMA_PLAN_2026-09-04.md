# R12 I3 UiDocument Menu Schema Plan — 2026-09-04

## Status

**Implemented; automated verification passed on 2026-09-04.** I1/I2 established
progression persistence, typed reference validation, and pure navigation. I3
establishes the minimum authored game-menu document and exports its button ports
through the I2 seam.

## Locked requirements

- One `UiDocument` represents one reusable authored game UI screen.
- I3 supports document kind `menu` only; HUD activation and game-state binding remain
  later work.
- The document uses fixed-capacity flat element storage with stable nonzero IDs and
  parent IDs representing a validated tree.
- Initialization creates exactly one root Container with ID 1 and name `root`.
- Element types are Container, Text, and Button.
- Every element has a unique bounded identifier name and bounded text content.
- Every non-root element references an existing Container parent.
- Parent relationships must be acyclic and every element must descend from root.
- Each Button owns one explicit flow-port name unique within the menu.
- Containers and Text elements cannot own flow ports.
- A caller-owned derived `UiFlowReferenceView` exposes the menu name and Button ports
  as an I2 `FlowReferenceEntry`; it never becomes a second authored source.
- Mutations are failure-atomic and advance document dirty identity.
- Load is transactional; save uses same-directory temporary-file replacement.

## Explicit non-goals

- no coordinates, anchors, constraints, flow layout, dimensions, or scale fields;
- no sprite references, style tokens, borders, focus order, or visual states;
- no HUD/game-state binding;
- no undo/redo command layer;
- no renderer, runtime activation, editor canvas, hierarchy UI, or app integration;
- no migration or reuse of application-owned `assets/ui_elements`/`ui_layouts`.

These omissions prevent I3 from committing an absolute layout format immediately
before R12 decides responsive layout semantics.

## Verification gate

- focused tests cover creation, stable IDs, tree validation, Button-port export,
  malformed/duplicate/dangling/cyclic data, round-trip, dirty state, and transactional
  load failure;
- strict, ASan/LeakSanitizer, and UBSan focused runners pass;
- aggregate `make test`, strict application build, and smoke remain healthy;
- existing application/editor UI remains unchanged.

## Next increment boundary

After I3, decide responsive layout and per-item scale semantics before adding visual
placement fields or a canvas editor.

## Verification record

- focused strict `ui_document` runner: **6/6 passed** under
  `-std=c11 -Wall -Wextra -Wpedantic -Werror`;
- focused ASan with leak detection: **6/6 passed**;
- focused UBSan with halt-on-error: **6/6 passed**;
- aggregate `make test`: passed, including the registered I3 runner;
- strict application build and `make smoke`: passed with
  `{"smoke":"ok","map_width":10,"map_height":6}`;
- optional `cppcheck`: skipped because the tool is unavailable.

Only `ui_document.c` consumes the new public API. No editor/application caller was
added, and the persisted schema contains no coordinate, size, scale, anchor,
constraint, sprite, or style field.