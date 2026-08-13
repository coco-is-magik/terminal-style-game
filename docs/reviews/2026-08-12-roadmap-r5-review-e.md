# Roadmap R5 Review E — 2026-08-12

## Scope

Read-only architecture and transaction review of the R5 reusable asset-document
foundation, focused on ownership, cleanup, lifecycle reuse versus
over-abstraction, asset/scene transaction and history boundaries, eager registry
refresh, dependency reporting, and verification evidence.

Reviewed first-party modules include `asset_document`, `material_document`,
`decal_document`, `asset_refresh`, `asset_loader`, `assets`, `scene_document`,
`unified_editor`, their public headers, Make rules, and focused tests.

## Result

**Pass. No blocker before R6.** All findings are closed or classified below.

## Findings and closure

### E1 — Save-only coordinator API was incomplete

**Classification:** Fix during I4 — **closed**.

The initial coordinator exposed Save As + refresh but not Save + refresh. Added
material and decal wrappers for both Save and Save As. Asset-save failure is
distinct from invalid arguments and refresh failure.

### E2 — Transitive decal material dependencies were not reported

**Classification:** Fix during I4 — **closed**.

The pre-I4 repair list detected missing scene materials and missing decal
patterns, but not missing materials inside a loaded scene-used decal pattern.
The shared repair builder now emits `SCENE_DIAGNOSTIC_INPUT_ASSET_MISSING` with
field `pattern_material`, instance identity, and clear text such as
`material asset 50000 is not loaded`.

### E3 — Registry replacement must not partially clear live ownership

**Classification:** Fix during I4 — **closed**.

Refresh allocates and eagerly loads a separate candidate registry, prepares
replacement scene diagnostics against that candidate, then performs a no-fail
ownership swap. Candidate allocation or unreadable-root failure releases only
candidate ownership and preserves live registry pointers, generation, and scene
diagnostics. ASan/LeakSanitizer and UBSan pass.

### E4 — Asset and scene histories must remain independent

**Classification:** Accepted constraint — **verified**.

Asset documents own `AssetDocumentState` and domain-specific histories.
`SceneDocument` owns scene state IDs, diagnostics, and scene Save. The coordinator
does not change scene current/saved IDs, invoke scene Save, or place asset edits
in scene command history. Scene Save does not inspect or mutate asset documents.
Focused tests protect both directions.

### E5 — Shared lifecycle must remain narrow

**Classification:** Accepted constraint — **verified**.

`AssetDocumentState` contains only current/saved/next identity operations.
Material values remain fixed-size while decal values retain explicit deep-copy
ownership. No generic property, command, serializer, or document framework was
introduced. This is the intended reuse boundary proven by two domains.

### E6 — POSIX atomic-writer mechanics are partly duplicated

**Classification:** Deferred cleanup.

Material and decal persistence both use same-directory temporary files, `fsync`,
and rename, but their serializers and saved-snapshot transactions differ. Review
E rejects extracting a broader persistence framework during R5; revisit only if
a third asset document proves a stable common transaction interface.

### E7 — Loader tolerance remains an established compatibility rule

**Classification:** Accepted constraint.

An unreadable asset root is a refresh failure. Missing optional subdirectories
and malformed individual assets remain tolerant skips, matching startup loading.
I4 does not silently make existing asset loading globally strict.

### E8 — Direct per-document registry commit helpers remain public

**Classification:** Accepted constraint.

The I2/I3 headless commit helpers remain useful for isolated tests and non-scene
tooling. Production inline material creation no longer calls them; it uses the
full Save + eager refresh coordinator. No direct commit is reachable per frame.

## Forbidden-shortcut audit

- No direct live-registry editing as a document model: **pass**.
- No non-atomic asset overwrite: **pass**.
- No generic document framework: **pass**.
- No silent reference rewrite or deletion: **pass**.
- No per-frame registry refresh: **pass**.
- No asset Save triggering scene Save: **pass**.
- No scene Save persisting asset documents: **pass**.

## Verification evidence

- Strict C11 `make all`: passed under `-Wall -Wextra -Wpedantic -Werror`.
- Full headless suite: 401/401 passed.
- Focused asset refresh: 4/4 passed.
- Focused scene document: 40/40 passed.
- Focused unified editor: 57/57 passed.
- Eight build-feature matrix configurations: passed.
- Full ASan/LeakSanitizer suite: passed.
- Full UBSan suite: passed.
- Targeted final coordinator ASan and UBSan: 4/4 passed under each.
- `git diff --check` and legacy-call guard: passed.
- Valgrind and cppcheck unavailable; project targets reported explicit skips.
- No hot renderer/simulation path changed; benchmark/stability reruns were not
  applicable.
- No new pointer/video interaction was introduced. Existing inline material
  creation is exercised through deterministic controller input/render tests.

## Post-review amendment: material collision rename path removed

**Date:** 2026-08-13
**Classification:** Accepted constraint — **closed**.

The original I4 material collision prompt included `R=Rename`, which reopened the
picker with the colliding name pre-selected so the user could edit and retry.
Manual acceptance and review found that:

- The retry loop added controller state (`material_rename_active`) and branches
  without a clear user benefit.
- "Pre-select the colliding name" is ambiguous: the user already typed that name
  and received a collision, so retyping is not a meaningful shortcut.
- The three remaining options (Enter=load, Esc=abort, O=overwrite with second
  confirmation) cover the actual use cases cleanly.

The rename path was removed from `src/input.{h,c}`, `src/unified_editor.{h,c}`,
focused tests, and documentation. Verification: 405/405 tests, strict build,
ASan/UBSan, manual collision/overwrite/abort checks. No rename symbols remain.

---

## Review conclusion

R5's lifecycle seam, ownership, transaction boundary, and dependency model are
suitable foundations for R6. R6 must continue to keep reusable decal assets in
`DecalDocument` and placed instances in `SceneDocument`.