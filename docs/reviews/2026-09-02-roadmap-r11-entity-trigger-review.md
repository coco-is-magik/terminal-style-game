# R11 Entity/Trigger Q4 Review — 2026-09-02

## Scope and status

Reviewed the scene-v9 trigger schema/migration, `SceneDocument` ownership,
command-only mutation, pure runtime session, editor selection/inspector/highlight,
input routing, runtime light/teleport effects, tests, and performance evidence.

**Result:** no automated-gate blocker remains. Manual visual/input acceptance is
still required before I3 and the approved I1-I3 scope are marked Verified.

## Findings and dispositions

### Blocker before next phase — closed

1. Initial runtime inside-state used authored array indexes. Reordering could
   transfer entry state between triggers. Corrected to stable-ID keyed state and
   covered by a reorder regression.
2. Scene-wide identity guards initially omitted trigger IDs from existing entity
   insertion checks. Light/decal/sprite insertion now rejects trigger-ID collisions;
   format validation checks all collections and the high-water mark.
3. Trigger count/pointer consistency was initially absent from the shared candidate
   bound. `scene_format_validate()` now enforces both.
4. Dangling light targets initially used the duplicate-ID diagnostic category.
   They now use `SCENE_DIAGNOSTIC_INPUT_INSTANCE_REFERENCE`, matching the error
   catalog’s wrong-kind/dangling-reference contract.
5. Trigger Boolean confirmation initially bypassed runtime-build rollback. It now
   uses the same command/history/runtime transaction boundary as other edits.
6. Generic map-only selection revalidation could discard stable-ID entity
   selections. The controller now applies it only to map-backed selections and
   validates entities against `SceneDocument`.

### Accepted constraint

- `APP_STATE_PLAYING` still uses the deprecated legacy map/`WorldState` loader and
  has no native authored scene. I3 runs in native-scene editor Walk mode. A
  post-editor cleanup task will migrate Start Game to native `SceneDocument`
  ownership and reuse `EntityTriggerSession`; no duplicate trigger source was added.
- I3 ships one action per trigger and session-only flags/light-enabled state. No
  trigger graph, persistent game-state save, object system, or scripting is implied.

### Deferred cleanup

- Trigger inspector/input branches remain local in `unified_editor.c`, consistent
  with current editor architecture. Reusable nested-inspector controls remain R12.
- Broader animation and object/component models require separate Q1 decisions.

## Ownership and dependency findings

- `SceneDocument` solely owns authored trigger arrays; candidate transfer and
  destruction are explicit and sanitizer-covered.
- `command_system` remains the sole production caller of internal trigger mutation.
- `EntityTriggerSession` owns fixed-capacity disposable state, allocates nothing,
  borrows authored values, and never accesses input, rendering, documents, history,
  SDL, or global time.
- Runtime light toggles alter only the disposable runtime-world intensity view;
  authored `SceneLight` values and document state IDs remain unchanged.
- Teleport effects are returned to the editor adapter, which resets existing
  vertical physics rather than letting the session own camera/physics state.

## Verification reviewed

Findings were corrected and every gate was re-run afterward against the final
tree. Binding evidence (also recorded in the I3 implementation record):

- Final strict `make -j2 check`: **43 suites / 568 tests passed** under
  `-Wall -Wextra -Wpedantic -Werror`.
- Focused suites: trigger-session **4/4**, scene-format **22/22**,
  scene-document **48/48**, command-system **44/44**, editor-selection **26/26**,
  editor-highlight **20/20**, editor-domain **13/13**, unified-editor **81/81**,
  input **13/13**.
- Final ASan/LeakSanitizer aggregate: **passed**.
- Final UBSan aggregate: **passed**.
- Eight-configuration build/test matrix: **passed**.
- Smoke: `{"smoke":"ok","map_width":10,"map_height":6}`.
- Colored-lighting benchmark: white **0.130 ms**, colored **0.115 ms**, spot
  **0.081 ms**, deterministic checksums, 6 ms gate **PASS**.
- Sprite benchmark: baseline **2.388348 ms**, sprites **2.797687 ms**, overhead
  **0.409338 ms**, deterministic checksums, 6 ms gate **PASS**.

## Exit condition

Run the final post-review automated gates, then complete the documented live
manual checklist. If manual review passes, update the I3 implementation record,
roadmap, TODO, and handoff to mark the approved I1-I3 scope Verified.