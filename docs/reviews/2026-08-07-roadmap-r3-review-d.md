# Review D — R3 Generalized Editor Domain Foundation

**Review date:** 2026-08-07  
**Phase under review:** R3 (Generalized editor domain foundation)  
**Review type:** Q4 phase-gate review  
**Disposition:** Verified — interactive acceptance passed after root-cause fix

## Summary

R3's first two manual acceptance passes found real usability and presentation defects.
The first added held-key repeat, inline numeric entry, and a larger RGB marker. The
second confirmed those controls but found that the point light appeared to emit no
light, the floor and ceiling were flat and unlit, camera movement appeared to be the
source of illumination, and ambient changes had no visible effect.

A third investigation traced all of these to a single missing allocation: the native
scene parser's `allocate_candidate_arrays()` allocated `map.cells` but never allocated
`map.light_map`. With `light_map == NULL`, `lighting_update()` returned without writing
any light values, and `raycast_render()` fell back to `light_level = 1.0` on every
surface — producing uniform full brightness, no point-light contribution, no ambient
effect, and camera-distance palette as the only visible variation. The fix adds the
two-line `light_map` allocation. A third manual pass confirmed all issues resolved.

## Evidence

| Gate | Result |
|---|---|
| Strict default application build | Pass (`-Wall -Wextra -Wpedantic -Werror`) |
| Focused lighting/format/document/adapter/selection/editor/highlight/core/decal suites | Pass — 5/5, 11/11, 34/34, 6/6, 17/17, 47/47, 14/14, 49/49, 29/29 |
| Aggregate (`make check`) | Pass — 28/28 runners |
| Feature matrix (`make matrix`) | Pass — 8/8 defined modes |
| Full-suite ASan (`make asan`) | Pass — 28/28, no sanitizer/leak diagnostics |
| Full-suite UBSan (`make ubsan`) | Pass — 28/28, no runtime diagnostics |
| Legacy-symbol guard | Pass |
| Native light edit Save/Open restoration | Pass (automated) |
| Interactive acceptance | **Passed** — three manual passes; final pass confirmed all issues resolved |
| Editor benchmark (`make benchmark-editor-highlight`) | Pass — 20,000 mixed scenarios, 0.170279 ms average, deterministic, 1.000 ms budget |
| Editor stability (`make stability-editor-highlight`) | Pass — 100,000 mixed scenarios, 0.169967 ms average, deterministic |
| Editor stability under ASan/UBSan | Pass — 100,000 iterations each, no diagnostics |

The state-changing Make gates were run sequentially because `matrix`, `asan`, and
`ubsan` clean the shared build directory. Earlier concurrent logs were not accepted
as gate evidence.

## Generalization quality

### API size and abstraction

`editor_domain.h` exposes seven narrow operations: typed inspector dispatch,
presentation and field presentation, wall request construction, light metadata and
formatting, and light step request construction. The abstraction is justified by the
two implemented domains. It does not introduce callbacks, a vtable, a widget system,
or a universal property bag.

### Dependency direction

- `SceneDocument` owns authored map and light values.
- `command_system` is the sole production caller of internal authored mutations.
- `editor_domain` is headless and allocation-free; it returns metadata or owned
  command requests and does not execute commands.
- `unified_editor` adapts input, history execution, runtime rebuilding, and terminal
  presentation. It contains no direct authored light writes.
- `editor_highlight` borrows map/camera/light/target values and writes only the Grid.
- `app.c` composes SDL/render boundaries and supplies borrowed document light views.

No `scene_document_internal_*` call exists in UI, adapter, app, or highlight code.

### Controller growth

`unified_editor.c` remains large and contains explicit branches for the two concrete
inspector control schemes. This is accepted for R3 because navigation and terminal
rendering are controller responsibilities, while domain metadata and mutation rules
are outside it. A third inspector domain is the revisit trigger: add another adapter
only if it can reuse the typed presentation seam without creating another repeated
controller switch tower.

## Findings

### Blocker before R3 verification

None. Interactive acceptance passed on the third manual pass.

### Root-cause defect and fix

The native scene parser's `allocate_candidate_arrays()` in `src/scene_format.c` allocated
`map.cells` but never allocated `map.light_map`.  With `light_map == NULL`:
- `lighting_update()` checked `!map->light_map` and returned without computing any light values;
- `raycast_render()` checked `if (map->light_map)` and fell back to `light_level = 1.0` on every surface.

This caused all six symptoms reported in the second manual pass: no point-light effect, no
ambient effect, flat unlit floor/ceiling, camera-distance palette as the only visible variation
(mistaken for "light following the camera"), uniformly bright/washed-out appearance, and more
saturation than the legacy `.txt` path (which uses `map_create()` and correctly allocates both
arrays).

The fix adds `light_map` allocation immediately after `cells` allocation in
`allocate_candidate_arrays()`.  Two regression tests were added:
`test_native_load_allocates_light_map` and `test_native_load_light_map_is_populated_by_lighting_update`.

### Remediated manual findings

- Intensity, radius, and ambient have deterministic exact-value coverage, including
  the saturation that made the original radius-4 fixture appear washed out.
- Final rendered-grid coverage proves that adding the point light increases ceiling,
  wall, and floor luminance over an ambient-only frame at the same camera.
- Left/Right repeats after a bounded delay. Typing starts inline replacement; Enter
  commits one bounded command, Backspace edits, and Escape cancels entry.
- Light pick tolerance increased from 0.25 to 0.50 world units and the projected marker
  spans three cells while retaining wall occlusion and stable-ID selection.
- The marker displays authored RGB. Inspector text explicitly states that scene
  illumination is scalar; colored surface illumination is not falsely claimed.
- `testscene.tscene` now matches legacy ambient `0.2`, light position `(4.5,2.5)`, and
  numeric decal assets 1–6, including two floor and two ceiling placements.
- Native wall anchor/UV placement is converted to runtime world coordinates consistently
  with legacy loading.
- Alpha was removed from the inspector because no current renderer path uses it; file
  parsing, serialization, commands, and runtime compatibility still preserve it.

### Deferred cleanup

- Deprecated legacy document load/save symbols remain guarded by
  `make check-legacy-unused`, as recorded by Review C.
- Read-only map APIs retain historical non-const signatures.

### Accepted constraints

- R3 edits existing point lights only. Creation, deletion, placement tools, and drag
  movement remain deferred to R6.
- Numeric replacement is inline and bounded; it does not introduce a separate modal or
  bypass the typed command boundary.
- Commands store whole bounded `SceneLight` snapshots; groups reject duplicate targets
  to keep before/after semantics unambiguous.

### Needs product decision

None for R3.

## Manual closeout gate

Run in a video/SDL environment:

```sh
make clean && make
./build/ascii-fps
```

1. Open a native scene containing a point light.
2. Aim at the light: verify hover `o`; press `E`: verify selected `@` and the point
   light inspector.
3. Use Up/Down across X, Y, RGB, intensity, and radius. Use Left/Right to edit;
   verify the rendered light changes where applicable and camera movement stays off.
4. Undo/redo several fields; Save; restart; reopen; verify values persist.
5. Put a wall between camera and light; verify the light highlight is occluded.
6. Select a wall and verify the existing material list, Up/Down/Enter, dashed hover,
   solid selection, Escape hierarchy, Save menus, Open, Import, and New still work.
7. Exercise the editor continuously while alternating wall and light selection/editing
   and confirm there is no visible hitch, corruption, or interaction degradation.
   Headless editor-path performance and sanitizer stability are already recorded.

## Conclusion

Review D records three manual acceptance passes. The first two found real defects that
were remediated. The third pass confirmed that the root-cause fix (missing `light_map`
allocation in the native scene parser) resolved all remaining lighting symptoms. All
non-interactive gates pass. **R3 is Verified.**