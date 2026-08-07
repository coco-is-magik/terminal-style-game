# Review D — R3 Generalized Editor Domain Foundation

**Review date:** 2026-08-07  
**Phase under review:** R3 (Generalized editor domain foundation)  
**Review type:** Q4 phase-gate review  
**Disposition:** Two manual acceptance passes failed; parity correction implemented and awaiting renewed acceptance

## Summary

R3's first manual acceptance found real usability and presentation defects despite the
earlier automated gates: intensity/radius/ambient effects were difficult to distinguish,
camera-distance palette colors looked like camera-following light, numeric controls did
not repeat or accept direct entry, and the one-cell light target was difficult to hover.
RGB was also presented without disclosing that scene illumination is scalar.

The first remediation added held-key repeat, inline bounded numeric replacement, and a
larger RGB-colored marker. A second manual pass confirmed those controls but rejected the
lighting result: Alpha had no visible behavior, the point light appeared ineffective,
camera-following presentation persisted, and legacy floor/ceiling visuals were absent.

The second investigation found that the compared worlds were not equivalent. Legacy play
used ambient `0.2` and globally assembled six numeric decal placements; the native fixture
used ambient about `0.001` and contained no decal instances. Native wall decal anchors also
were not converted to runtime world coordinates. The fixture and adapter now reproduce the
legacy ambient, light, and six placed decals. The rejected stable-palette workaround was
reverted. Alpha remains serialized but is no longer editable. R3 remains Active until a
third interactive acceptance pass verifies the corrected parity fixture.

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
| Interactive acceptance | **Failed**, remediation implemented; renewed run pending |
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

1. Repeat the video-gated interaction checks after remediation. The original run found
   implementation and feedback defects; it was not merely missing evidence.

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

Review D records two failed manual gates and a tested parity correction. Keep R3 Active.
After the renewed manual gate passes, update this review and the implementation record,
then change R3 to Verified.