# Editor Wall-Face Highlight Implementation Record — 2026-07-29

## Goal

Implement R0's first visible editor win without changing authored state or
expanding renderer ownership: show the center-ray target and make hovered and
selected wall faces visible in world space.

## Implemented boundary

- `src/editor_highlight.h/.c` owns the editor-only visualization post-pass.
- `src/app.c` composes it after `raycast_render()` and before editor UI.
- `src/unified_editor.c` composes the adaptive center crosshair last so the UI
  reticle wins over world marks.
- The module borrows `Grid`, authoritative `Map`, `Camera`, `SelectionTarget`, and
  `EditorHit`; it allocates nothing and mutates only the current framebuffer.

## Behavior

- Selected wall face: solid `#` projected boundary.
- Hovered wall face: dashed `.` projected boundary.
- If hover and selection identify the same face, selected styling wins.
- Each grid column reuses nearest-wall raycast semantics, so intervening walls
  occlude the target and the wrong cardinal face is not highlighted.
- Only projected boundary cells are replaced; the wall-face interior remains
  visible.
- Foreground/background switch between black and white from the underlying cell
  luminance, independent of world lighting.
- The center crosshair is a single adaptive `+` at the grid center.

## Rejected shortcuts

- Material mutation or temporary material replacement: would violate authored
  state ownership and alter normal rendering.
- Through-wall/X-ray outlines: conflicts with normal world occlusion.
- Heap-backed per-frame geometry: unnecessary and incompatible with the current
  no-per-frame-allocation guard.
- Moving the behavior into `app.c`: orchestration remains there, while projection
  and styling have a narrow owner.

## Regression coverage

`tests/test_editor_highlight.c` covers:

- all four cardinal wall faces;
- selected versus dashed hover style and overlap priority;
- nearer-wall occlusion;
- wrong-face and empty-cell rejection;
- adaptive contrast on dark and bright content;
- preservation of map data and wall interior;
- centered crosshair and null-input safety.

## Verification record

- Initial strict application build failed because `src/app.c` lacked the new
  header include; fixed by including `editor_highlight.h`.
- Initial focused test compile failed because a zero-vararg `fail_msg` call is
  non-pedantic; fixed with an explicit `%s` argument.
- Initial contrast test exposed duplicate corner writes: vertical edge rendering
  recomputed contrast after the horizontal edge had already changed the same
  cell. Fixed by excluding corners from the vertical-edge loop.
- `make -B build/test-editor-highlight`: strict compile passed.
- `./build/test-editor-highlight`: 9/9 tests passed, including a 1100-column
  regression proving there is no fixed-width rendering cutoff.
- `make -B all`: passed under strict C11 warnings-as-errors.
- `make test`: full aggregate suite passed after final implementation changes.
- Focused AddressSanitizer + UndefinedBehaviorSanitizer build/run: 9/9 passed
  with no reported sanitizer error.

## Interactive acceptance

On 2026-07-29 the user ran the editor and accepted the adaptive crosshair,
distinct hover and selected outlines, and their behavior in the rendered world.
The focused roadmap outcome is Verified. Remaining R0 outcomes and the R0 phase
exit gate are unaffected.

## Post-acceptance correction

Interactive use then exposed that Escape closed the wall inspector while leaving
its persistent selection—and therefore its solid outline—active. The controller
now clears persistent selection when Escape dismisses the inspector. Modal Escape,
invalid-selection preservation, and the subsequent Escape-to-exit-prompt flow are
unchanged.

Verification after the correction:

- strict `test-unified-editor`: 27/27 passed;
- strict `test-editor-highlight`: 9/9 passed;
- strict application build passed;
- full aggregate `make test` passed;
- focused AddressSanitizer + UndefinedBehaviorSanitizer editor run: 27/27 passed
  with no reported sanitizer error.

The first focused run exposed two test helpers that attempted to inspect edited
map data through the intentionally cleared selection. Their assertions were
corrected to inspect the authoritative wall coordinate directly; no production
document data had been lost.

The user then repeated the interactive select-and-Escape flow and confirmed that
the solid selected outline now disappears when the inspector closes.