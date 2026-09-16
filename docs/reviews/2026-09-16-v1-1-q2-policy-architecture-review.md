# V1-1 Q2 Provisional Policy Architecture Review — 2026-09-16

## Outcome

**Pass after bounded non-visible corrections.** The isolated `ui_theme` seam is suitable
for one reversible application-owned consumer. This is architecture approval only: D1-D7
remain provisional, and no visible result is finally approved until manual evaluation.

The selected first consumer is the **application menu focus/selection palette** in
`menu_sync_button_colors()` within `src/app.c`. This review does not migrate that consumer;
it defines the next independently reversible increment.

## Review objective and boundary

The review asked whether the provisional policy module:

1. preserves application/editor versus authored-Menu ownership;
2. is deterministic, pure, testable, and free of persistence/runtime-state ownership;
3. exposes enough policy for one meaningful manual evaluation without broad migration;
4. avoids coupling V1-1 to V1-3 input or motion implementation;
5. has a safe rollback point and focused regression owner.

Reviewed consumer candidates were application menu selection, authored-preview transient
states, unified-editor overlays, and application status feedback. Broad editor restyling,
authored normal presentation, pointer conversion, persistence, and motion runtime remain
outside this increment.

## Confirmed architecture facts

1. `ui_theme` has no SDL, document, I/O, persistence, input, allocation, mutable-global,
   or global-clock dependency. It returns immutable values and pure calculations.
2. No production source currently includes or calls `ui_theme`; all behavior remains
   unchanged before the selected consumer increment.
3. Application menu documents and assets are owned by `ui_ele` and are separate from
   authored `UiDocument` Menu content.
4. `menu_sync_button_colors()` is the only production caller of `ui_ele_set_colors()` and
   owns exactly two procedural color pairs: selected and unselected buttons.
5. Focus identity, traversal, activation, labels, geometry, and actions are independent
   of those colors.
6. Application focus already has the required non-color continuity cue:
   `ui_layout_set_focus()` sets `UiElement.focused`, and `ui_ele_render()` writes `>` and
   `<` edge markers. `test-ui-ele` protects focus flags and both rendered markers.
7. The selected path is exercised by main, pause, settings, and confirm-quit layouts and
   appears through the existing scale-aware compositor at 100/125/150/200%.

## Q2 corrections

Review found and corrected three non-visible policy/test weaknesses:

1. The exact-token test checked only four of fifteen palette roles and a subset of
   geometry fields. It now checks every palette and geometry value.
2. Contrast coverage omitted accent, disabled, warning, error, success, and destructive
   candidate pairs. All documented core pairs now have focused threshold assertions.
3. Motion interpolation used `start + (end - start) * progress`, which can overflow the
   subtraction for extreme finite endpoints. It now uses the weighted form
   `start * (1 - progress) + end * progress`, rejects a non-finite result transactionally,
   and has an extreme finite-endpoint regression.

During correction, the first extreme-value expectation incorrectly assumed the weighted
result should fail; it correctly produced a finite result. A second assertion attempted
to pass that large `double` through cmocka's float comparison and narrowed it to infinity.
The final regression checks finiteness and compares a normalized `double` ratio without
test-framework narrowing.

## Consumer comparison

| Candidate | Manual-review value | Ownership/risk | Decision |
|---|---|---|---|
| Application menu focus/selection palette | High: visible immediately in four bounded menu contexts and every UI scale | Application-owned procedural colors; existing non-color markers; two-pair rollback | **Selected** |
| Authored-preview transient theme | High for authored runtime states | Crosses the authored-template adapter boundary and could be confused with author-owned normal colors | Defer until application adapter is proven |
| Unified-editor overlay palette | Highest breadth | Many direct render sites, modes, status meanings, and regression surfaces; not one bounded family | Reject as first consumer |
| HUD/UI-scale status feedback | Low/narrow | Application-owned but transient, semantic-specific, and poor evidence for focus language | Defer |

## Selected next increment

### Scope

1. Add a narrow application adapter that borrows `ui_theme_provisional_tokens()` and
   converts only the required values to existing `SDL_Color` values.
2. Expose selected and unselected application-menu colors through that adapter:
   - selected foreground: provisional `focus`;
   - selected background: provisional `selection_background`;
   - unselected foreground: provisional `text_secondary`;
   - unselected background: provisional `canvas`.
3. Replace only the four color literals in `menu_sync_button_colors()`.
4. Preserve `ui_layout_set_focus()`, `UiElement.focused`, `> <` markers, actions, labels,
   geometry, asset bytes, traversal, activation, clipping, and scale composition.
5. Add a focused adapter test for exact mapping, alpha preservation, stable immutable
   inputs, and null-output rejection. Extend `test-ui-ele` only if needed to assert the
   final selected/unselected rendered cell colors alongside the existing markers.

### Forbidden in that increment

- no authored `UiDocument` access or mutation;
- no asset, preference, scene, or UI format change;
- no changes to menu content, positions, dimensions, target areas, actions, or depth;
- no hover, pointer, pressed, disabled, status, HUD, editor-overlay, or motion work;
- no user-selectable themes, persistence, or fallback-file loading;
- no removal or weakening of the existing `> <` focus cue;
- no claim that the palette is final before manual approval.

### Rollback

Revert the `menu_sync_button_colors()` call site and delete the isolated adapter/runner.
No data migration or restoration is required.

## Required manual evaluation after the selected increment

Inspect main, pause, settings, and confirm-quit menus at 100%, 125%, 150%, and 200% for:

- focused versus unfocused legibility;
- `> <` marker clarity independent of color;
- focus and selection color character against the unchanged menu assets;
- label readability and absence of clipping/regression;
- settings changes and confirmation navigation preserving focus;
- acceptable density despite the existing one-row controls.

The existing one-row controls are compatibility content and this first palette consumer
does not claim to implement the provisional 3x3-cell default for new controls. They are
currently keyboard-operated application menus, not accepted pointer targets. Asset review
found that some adjacent row centers are only two cells/16 source pixels apart, so a
blanket WCAG pointer-spacing exception is explicitly **not** claimed. V1-3 must resolve
target geometry or spacing before making these controls pointer-operable.

Manual review may accept, revise, or reject the palette mapping. A rejection rolls back
the consumer without changing the policy seam or any persisted data.

## Verification evidence

- Strict optimized `test-ui-theme`: **7/7 passed** after Q2 corrections.
- Focused ASan/LeakSanitizer: **7/7 passed**, no diagnostics.
- Focused UBSan: **7/7 passed**, no diagnostics.
- Application button geometry audit: found 16-pixel adjacent centers in the main menu;
  pointer target/spacing conformance is not claimed and remains a V1-3 precondition.
- `make -j2 test-ui-standards`: pass across all ten focused UI owners.
- `make standards`: pass with real cppcheck 2.18.2 and repository policy guards.
- `make -j2 check`: pass; strict application build, complete functional aggregate, real
  cppcheck, static-analysis policy, and structural standards all passed.
- `git diff --check`: pass during review.

## Stop reason

The architecture and affected ownership boundaries are understood, the first consumer is
selected, and further research would not change its scope. Visible migration is deferred
to the next increment so Q2 remains a non-visible, independently reversible review and
policy-correction checkpoint.