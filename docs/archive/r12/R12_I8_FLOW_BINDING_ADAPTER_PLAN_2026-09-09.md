# R12 I8 Flow Binding Adapter Plan — 2026-09-09

## Status

**Implemented; automated verification passed on 2026-09-09.** I8 connects the pure
I6 Button and I7 scene-exit activation results to the I2 flow runtime without adding
loading, rendering, input, application, or editor side effects.

## Locked public contract

`flow_binding` provides two explicit typed entry points:

- `flow_binding_activate_button` accepts an I6 `UiInteractionActivation` and requires
  the current flow node to be a Menu;
- `flow_binding_activate_scene_exit` accepts an I7 `EntityTriggerTickResult` containing
  an active exit request and requires the current flow node to be a Scene.

Both operations consume the activation's borrowed named port and return:

```c
typedef struct {
    FlowNodeId node_id;
    FlowNodeType type;
    const char *asset_name;
} FlowBindingTargetRequest;
```

The target asset name is borrowed from the authoritative `FlowDocument`. Successful
targets are exactly Scene or Menu nodes. Start is a built-in entry node and is not a
valid authored activation target.

## Validation and failure atomicity

- Button activation requires nonzero stable element identity and a valid bounded port.
- Scene-exit activation requires an asserted request, nonzero stable trigger identity,
  and a valid bounded port.
- Button activation from a non-Menu node and scene-exit activation from a non-Scene
  node return a typed wrong-source error.
- Invalid graphs, invalid sessions, missing ports, and invalid targets have distinct
  typed results.
- The adapter transitions a local `FlowRuntimeSession` copy, validates the resulting
  target request, and commits the session and output only after complete success.
- Every failure preserves both the caller's session and output bytes.

## Deterministic boundary

The adapter:

- performs exactly one named I2 transition per successful call;
- adds no time, randomness, allocation, global state, or hidden fallback behavior;
- produces identical session and target results from equivalent inputs;
- does not inspect `UiDocument` or `SceneDocument` internals because I6/I7 already
  produce typed activation results.

## Explicit non-goals

I8 does not:

- route keyboard or pointer input;
- render or host an authored menu;
- load, unload, or resolve scene/menu files;
- mutate `FlowDocument`, `UiDocument`, or `SceneDocument`;
- invoke Start Game application behavior;
- add editor or application state;
- change any persisted format.

## Verification scope

- Menu Button → Scene and Scene exit → Menu cycle;
- stable target ID/type/name results;
- wrong source type and missing port;
- invalid Button and scene-exit payloads, including unterminated ports;
- invalid graph, invalid session, and forbidden Start target;
- null/incomplete arguments;
- byte-preserving session/output failures;
- deterministic equivalent-session replay;
- strict, ASan/LeakSanitizer, UBSan, aggregate, application-build, and smoke gates.

## Next increment boundary

I9 should define and implement the minimum authored Menu runtime host. It should own
the active borrowed/loaded `UiDocument`, transient render/interaction state, and input
mapping; render through I5, interact through I6, and submit successful activations
through I8. Loading target assets and application Start Game integration should remain
outside the host until their ownership and failure behavior are explicitly locked.