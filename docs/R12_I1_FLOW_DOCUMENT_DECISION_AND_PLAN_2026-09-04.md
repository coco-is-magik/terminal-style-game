# R12 I1 FlowDocument Decision and Implementation Plan — 2026-09-04

## Status

**Implemented; automated verification passed on 2026-09-04.** This record narrows
the broad R12 draft into the first testable implementation boundary.

## Locked decisions

- Progression is a separate, versioned `FlowDocument`, not inline scene/UI data.
- The model is a directed graph. Cycles are valid.
- Version 1 has typed Start, Scene, and Menu nodes with stable graph-local IDs.
- Initialization creates exactly one Start node.
- Scene/Menu nodes carry validated external asset names. Catalog resolution is a
  later adapter responsibility.
- Edges have stable graph-local IDs, source node, named source port, and target node.
- A source node/port pair has at most one outgoing edge.
- Start uses only the `start` port. Scene exits and menu buttons use validated names.
- Every persisted node must be reachable from Start.
- The document owns staged graph data, path, and dirty/saved identity.
- Persistence is canonical and uses same-directory temporary-file replacement.

## Explicit I1 non-goals

- no graph canvas or editor-controller integration;
- no scene-format or `UiDocument` field changes;
- no runtime scene transition adapter;
- no trigger enum extension;
- no conditions, scripts, or graph-layout coordinates;
- no external scene/menu catalog resolution.

## Format v1

The strict text format contains root version/next-ID fields followed by `[node]` and
`[edge]` records. Unknown, duplicate, incomplete, malformed, dangling, unreachable,
or invalidly typed data is rejected rather than guessed.

## Exit gate

- focused construction, cycle, invalid-input, validation, round-trip, and
  transactional-load tests pass;
- strict compiler warnings pass under `-Werror`;
- ASan and UBSan focused runners pass;
- full `make test` and smoke remain healthy;
- no existing app/editor/runtime behavior changes.

## Next increment boundary

After I1 verification, decide and implement the catalog/reference adapter and pure
runtime navigation semantics before exposing the graph in an editor workspace.

## Verification record

- focused strict runner: **4/4 passed** under
  `-std=c11 -Wall -Wextra -Wpedantic -Werror`;
- focused ASan with leak detection: **4/4 passed**;
- focused UBSan with halt-on-error: **4/4 passed**;
- aggregate `make test`: passed, including the registered flow-document runner;
- strict application build and `make smoke`: passed with
  `{"smoke":"ok","map_width":10,"map_height":6}`;
- optional `cppcheck`: skipped because the tool is unavailable.

No editor, renderer, scene-format, trigger-session, or application runtime behavior
changed in I1.