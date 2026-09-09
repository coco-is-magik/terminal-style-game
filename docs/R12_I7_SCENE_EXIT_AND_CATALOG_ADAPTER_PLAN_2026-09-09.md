# R12 I7 Scene Exit and Catalog Adapter Plan — 2026-09-09

## Status

**Implemented; automated verification passed on 2026-09-09.** I7 completes the
scene-owned source-port side of the I1/I2 progression graph without making
`FlowDocument` depend on scene internals.

## Locked authored model

- Native scene v11 adds `SCENE_TRIGGER_ACTION_EXIT_FLOW` / `exit_flow`.
- An exit-flow trigger uses the existing `enter_region` condition and owns exactly one
  `flow_port` payload.
- Flow-port names use the I1 identifier grammar: non-empty ASCII alphanumeric,
  underscore, or hyphen, bounded to 63 bytes plus NUL.
- A scene may own at most 16 flow exits, matching the I2 per-asset port bound.
- Exit-flow port names are unique within a scene. Duplicate names are rejected during
  format validation and command-time mutation.
- `set_flag`, `teleport_to_spawn`, and `toggle_light` require an empty flow-port field.
- Versions 1–10 remain readable. v10→v11 migration adds no synthetic exits and changes
  no existing trigger behavior. Canonical SceneDocument saves write v11.

Canonical example:

```ini
[trigger 42]
min_x = 2
min_y = 3
max_x = 3
max_y = 4
condition = enter_region
action = exit_flow
flow_port = complete
```

## Runtime result

- `EntityTriggerTickResult` can report an exit request as a borrowed flow-port string
  plus the stable trigger ID that emitted it.
- Trigger processing remains stable-ID sorted. If multiple exits fire in one tick, the
  lowest trigger ID supplies the single returned exit request; `fired_count` still
  includes every newly entered trigger.
- The trigger session does not invoke `FlowRuntimeSession`, perform I/O, or load a
  scene/menu.
- Invalid exit payloads preserve both session and caller output.

## Catalog adapter

`scene_flow_adapter` derives one borrowed Scene `FlowReferenceEntry` from a
`SceneDocument`:

- entry name borrows the authoritative scene name;
- entry ports borrow exit-flow trigger strings in scene trigger order;
- the caller-owned view stores only pointer slots and one reference entry;
- malformed pointer/count state, invalid names, duplicate ports, and excessive ports
  fail without modifying output;
- a derived Scene entry and an I3-derived Menu entry can jointly validate a complete
  `FlowDocument` through the unchanged I2 API.

## Existing editor authoring

The current trigger inspector's closed action cycle includes `exit flow`. Selecting it
assigns the first available conventional port name (`exit`, `exit-2`, and so on). The
payload is displayed by the existing inspector. Arbitrary text renaming remains part
of the later property-editor workspace rather than adding an ad hoc text-entry mode.

## Verification scope

- v11 migration, persistence, malformed payload, duplicate port, and version-gate
  regressions;
- trigger runtime deterministic exit reporting and failure atomicity;
- scene catalog derivation and complete Menu→Scene→Menu flow validation;
- command-history duplicate-port rejection and editor action designation;
- strict compiler warnings, focused sanitizers, aggregate tests, application build,
  and smoke.

## Preserved boundaries

I7 does not:

- invoke flow navigation;
- load target scenes or menus;
- alter `FlowDocument` or `UiDocument` formats;
- add application Start Game integration;
- add the flow graph workspace or visual UI canvas;
- turn application-owned UI layouts into authored game menus.

## Next increment boundary

I8 should implement a pure flow-binding adapter that accepts either an I6 Button
activation or an I7 scene-exit request, advances `FlowRuntimeSession` by the named
port, and returns a typed target-node request without performing I/O. A later runtime
host may consume that request to load/show an authored scene or menu.