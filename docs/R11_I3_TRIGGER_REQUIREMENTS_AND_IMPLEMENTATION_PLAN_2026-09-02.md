# R11 I3 Trigger Requirements and Implementation Plan — 2026-09-02

## Status and authority

**Approved for implementation on 2026-09-02.** This amendment resolves the
remaining wire/runtime/editor choices for the minimal trigger increment in
`R11_DECISION_RECORD_2026-08-28.md`. It authorizes I3 and its Q4 closeout only;
animation, typed objects/components, and game-mode spawn expansion remain deferred.

## Locked behavior

- A trigger is scene-authored, has one stable `SceneInstanceId`, one
  `enter_region` condition, and exactly one action.
- Regions are finite axis-aligned half-open rectangles
  `[min_x,max_x) × [min_y,max_y)`, contained by map world bounds, with positive
  width and height.
- Actions are a closed enum: `set_flag`, `teleport_to_spawn`, `toggle_light`.
- `set_flag` addresses a numeric flag ID `1..64` and assigns a Boolean value.
  Flags are bounded session-only state, initially false, and are not serialized
  independently from the trigger action payload.
- `teleport_to_spawn` has no target/payload. It applies the authored spawn XY and
  angle and requests vertical-physics reset through the application adapter.
- `toggle_light` requires a nonzero stable ID resolving to a scene-owned light.
  Enabled state is session-only, initially true, and resets on scene/session rebuild.
- Runtime firing never mutates `SceneDocument`, command history, authored lights,
  spawn, or trigger records and therefore never dirties the scene.
- Tick evaluates one input-position snapshot in ascending trigger-ID order. Each
  trigger fires once on an outside-to-inside transition. Remaining inside does not
  repeat; leaving then re-entering permits one later firing. Teleport does not
  recursively evaluate another trigger during the same tick. Inside-state is
  recomputed from the final post-action position before tick returns.
- Trigger runtime runs in native-scene editor Walk mode. Editor Edit mode and open
  application menus pause it. Scene load/new/open and any authored trigger/light/
  spawn mutation rebuild or reset session state before another tick.
- `APP_STATE_PLAYING` remains on the deprecated legacy map/`WorldState` loading
  path and has no native `SceneDocument`; it is intentionally not given a second
  trigger source in I3. Migrating Start Game to native-scene ownership is a
  post-editor feature cleanup task after the editor roadmap work stabilizes.
- Removing a light referenced by a trigger is rejected. Structural shrink that
  would place any trigger region outside the resulting map is rejected.

## Bounded data and scene v9

- `SCENE_MAX_TRIGGERS = 128`; `SCENE_TRIGGER_FLAG_CAPACITY = 64`.
- Scene v9 adds repeated canonical `[trigger]` records after sprite records.
- Identity is encoded by the repeated `[trigger <id>]` header, matching other
  scene instances. Required keys in exact canonical order are `min_x`, `min_y`,
  `max_x`, `max_y`, `condition`, `action`, followed by action-specific payload:
  - `set_flag`: `flag_id`, `flag_value`
  - `teleport_to_spawn`: no payload
  - `toggle_light`: `target_id`
- Tokens are exactly `enter_region`, `set_flag`, `teleport_to_spawn`, and
  `toggle_light`. Unknown keys/tokens, duplicate keys/IDs, inactive payload keys,
  invalid regions, and dangling light references are rejected diagnostically.
- v8→v9 migration adds an empty trigger collection; v1-v8 remain readable through
  the existing migration chain. Canonical writes use v9.

## Unified-editor workflow

- `T` places a default 1×1 trigger centered on the aimed map cell, clipped only by
  choosing that exact cell rectangle; placement requires a valid aimed cell.
- Trigger regions are selectable by first-person center ray and displayed by an
  editor-only outline. Stable ID, not coordinates, is selection identity.
- Inspector order: Min X, Min Y, Max X, Max Y, Condition, Action, action payload,
  Remove. Position bounds step by 0.25 world units and reject invalid rectangles.
- Enter cycles Condition/Action or toggles Boolean values; Left/Right step numeric
  values. Remove uses confirmation. All authored edits use command history.
- `T` remains unavailable to sprite painter text handling while paint mode is open;
  painter ownership wins there.

## Required evidence

- Pure session tests: edge rules, once-per-entry, re-entry, stable-ID order, every
  action, no teleport recursion, reset, bounds, invalid inputs, authored immutability.
- Format/document tests: v8→v9, strict grammar, canonical round-trip, malformed and
  dangling reference diagnostics, ownership, capacity, allocation failure, legacy
  compatibility, transactional failed load.
- Command/editor tests: insert/set/remove, undo/redo, stable IDs, failure atomicity,
  referenced-light and shrink guards, placement/selection/edit/remove/save/reopen,
  visible failures, runtime actions not dirtying authored state.
- Strict aggregate, ASan, UBSan, matrix, smoke, colored-lighting and sprite-render
  benchmarks, `git diff --check`, manual visual/input acceptance, then Q4 review.

## Forbidden scope

No action strings, scripting, generic component framework, persistent flag save,
multi-action trigger graph, oriented/solid sprites, object model, animation system,
multi-spawn model, or reusable R12 submenu abstraction is part of I3.