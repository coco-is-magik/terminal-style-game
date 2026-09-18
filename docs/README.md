# Documentation Index

This index is the starting point for project documentation. The repository keeps
historical plans, implementation records, reviews, and handoffs as evidence, but
only a small set of documents should be treated as current authority.

## Start here

- [`../README.md`](../README.md) — project overview, build/run basics, controls,
  asset-system overview, and maintainer entry points.
- [`CURRENT_STATUS.md`](CURRENT_STATUS.md) — concise current state, verified
  foundation, next safe action, and known deferred source-level TODOs.
- [`FEATURE_ROADMAP.md`](FEATURE_ROADMAP.md) — dependency-aware roadmap authority
  from the verified R0-R12 foundation through the required v1.0 release proofs.
- [`V1_0_REQUIREMENTS_AND_EVIDENCE_PLAN_2026-09-14.md`](V1_0_REQUIREMENTS_AND_EVIDENCE_PLAN_2026-09-14.md)
  — active V1-0 baseline, required-platform remediation, mirror reconciliation,
  non-ASCII inventory, rollback, and verification plan.
- [`V1_0_W1_WINDOWS_PLATFORM_CAPABILITY_DECISION_2026-09-14.md`](V1_0_W1_WINDOWS_PLATFORM_CAPABILITY_DECISION_2026-09-14.md)
  — accepted filesystem, catalog, locale-number, UTF-8 path, commit-state, fault,
  migration, and native Windows verification boundaries for W2-W4.
- [`UI_LOOK_AND_FEEL_REFERENCE_OF_RECORD.md`](UI_LOOK_AND_FEEL_REFERENCE_OF_RECORD.md) — **binding
  look-and-feel authority.** The guiding star for every colour, animation, transition, and motion
  decision. Cited by every phase that touches interface feel.
- [`APPLICATION_UI_EDITOR_REQUIREMENTS_AND_IMPLEMENTATION_PLAN_2026-09-18.md`](APPLICATION_UI_EDITOR_REQUIREMENTS_AND_IMPLEMENTATION_PLAN_2026-09-18.md)
  — active Step 1 plan: evolve `make ui-workbench` into a complete, readable, palette- and
  motion-directed application-UI editor; includes the anti-drift testing gates. Step 2 (reuse for the
  in-project edit-mode editor) is a note only.
- [`reviews/2026-09-17-ui-workbench-i1.md`](reviews/2026-09-17-ui-workbench-i1.md)
  — the implemented application-UI workbench: bounded contexts, immediate atomic element writes,
  style/transition/effect presets, reusable animation units, shared scale preference. The proven
  host pattern Step 1 builds on.
- [`reviews/2026-09-16-v1-1-full-palette-demo.md`](reviews/2026-09-16-v1-1-full-palette-demo.md) and
  [`reviews/2026-09-16-v1-1-ui-motion-demo.md`](reviews/2026-09-16-v1-1-ui-motion-demo.md)
  — the manually approved palette and D6 motion specimens. The reference the editor must match.
- [`UI_SCENE_SYSTEM_AND_EDITOR_IMPLEMENTATION_PLAN_2026-09-17.md`](UI_SCENE_SYSTEM_AND_EDITOR_IMPLEMENTATION_PLAN_2026-09-17.md)
  — **withdrawn**; retained as reference for the UI Scene migration inventory, v4 schema decisions,
  Phase 0-2 additive seams, ownership model, and deferred migration phases 5-8. Its Phase 3-4
  editor-interface claims do not stand.
- [`reviews/2026-09-17-ui-scene-phase0-baseline-and-decisions.md`](reviews/2026-09-17-ui-scene-phase0-baseline-and-decisions.md)
  — Phase 0 legacy migration inventory, v4 schema decisions, reusable renderer/runtime/workspace
  baseline tests, failures, and Phase 1 entry boundary. Still valid as reference.
- [`reviews/2026-09-18-ui-scene-phase3-editor-core.md`](reviews/2026-09-18-ui-scene-phase3-editor-core.md)
  and [`reviews/2026-09-18-ui-scene-phase4-standalone-host.md`](reviews/2026-09-18-ui-scene-phase4-standalone-host.md)
  — **withdrawn with banners.** Their interface and host-parity claims are not supportable; the staged
  workspace, history, candidate, and input-adapter seams they describe remain valid.
- [`reviews/2026-09-14-v1-0-c1-final-newline-remediation.md`](reviews/2026-09-14-v1-0-c1-final-newline-remediation.md)
  — byte-exact first-party newline correction, pinned-SMC boundary, local verification,
  Docker/classifier failure, and required-profile handoff.
- [`reviews/2026-09-14-v1-0-platform-profile-evidence.md`](reviews/2026-09-14-v1-0-platform-profile-evidence.md)
  — verified harness, bounded Ubuntu image/profile evidence, native Windows portability
  findings, lifecycle cleanup, and the W1/pinned-SMC handoff.
- [`reviews/2026-09-14-v1-0-w2a-platform-foundation.md`](reviews/2026-09-14-v1-0-w2a-platform-foundation.md)
  — implemented path/file capability foundation, focused and sanitizer evidence, current
  63-runner inventory, native Windows compile evidence, and W2-B prototype handoff.
- [`reviews/2026-09-14-v1-0-w2b1-native-replacement-prototype.md`](reviews/2026-09-14-v1-0-w2b1-native-replacement-prototype.md)
  — native Windows PE/import and 9/9 runtime replacement evidence, conservative guarantee
  limits, harness preflight, failures/corrections, and W2-B2 scene-migration handoff.
- [`reviews/2026-09-14-v1-0-w2b2-native-scene-save-migration.md`](reviews/2026-09-14-v1-0-w2b2-native-scene-save-migration.md)
  — native scene platform-adapter migration, preserved save/failure contracts, local
  strict/sanitizer evidence, native compile progress, limitations, and W2-C1 handoff.
- [`reviews/2026-09-14-v1-0-w2c1-ui-preferences-migration.md`](reviews/2026-09-14-v1-0-w2c1-ui-preferences-migration.md)
  — UI preference sync/replacement migration, truthful committed-warning outcomes,
  owner/sanitizer/native compile evidence, limitations, and W2-C2 handoff.
- [`reviews/2026-09-14-v1-0-w2c2-material-document-migration.md`](reviews/2026-09-14-v1-0-w2c2-material-document-migration.md)
  — material sync/replacement migration, committed-result contract, saved-state and
  asset-refresh preservation, verification, limitations, and W2-C3 handoff.
- [`reviews/2026-09-14-v1-0-w2c3-object-document-migration.md`](reviews/2026-09-14-v1-0-w2c3-object-document-migration.md)
  — object atomic-creation migration, output-ID and registry invariants, exact bytes,
  strict/sanitizer/native evidence, limitations, and W2-C4 handoff.
- [`reviews/2026-09-14-v1-0-w2c4-flow-document-migration.md`](reviews/2026-09-14-v1-0-w2c4-flow-document-migration.md)
  — flow-document and workspace committed-warning contracts, exact bytes, transaction
  boundaries, strict/sanitizer/native evidence, limitations, and W2-C5 handoff.
- [`reviews/2026-09-14-v1-0-w2c5-ui-document-migration.md`](reviews/2026-09-14-v1-0-w2c5-ui-document-migration.md)
  — authored UI-document and workspace committed-warning contracts, byte/identity boundaries,
  strict/sanitizer/native evidence, limitations, and W2-D1 handoff.
- [`reviews/2026-09-14-v1-0-w2d1-managed-directory-creation.md`](reviews/2026-09-14-v1-0-w2d1-managed-directory-creation.md)
  — one-level UTF-8 directory capability, owner state preservation, strict/sanitizer/native
  evidence, remaining Windows blockers, and W2-D2 handoff.
- [`reviews/2026-09-14-v1-0-w2d2-sprite-folder-publication.md`](reviews/2026-09-14-v1-0-w2d2-sprite-folder-publication.md)
  — sprite candidate/backup/publication/restoration transaction, committed and incomplete
  outcomes, strict/sanitizer/native evidence, and decal-owner handoff.
- [`reviews/2026-09-14-v1-0-w2c6-decal-document-migration.md`](reviews/2026-09-14-v1-0-w2c6-decal-document-migration.md)
  — decal persistence platform migration, exact bytes and commit-state ownership,
  strict/sanitizer/native evidence, current Clang blockers, recorded failures, and W3
  catalog handoff.
- [`reviews/2026-09-14-v1-0-w3-direct-child-catalog.md`](reviews/2026-09-14-v1-0-w3-direct-child-catalog.md)
  — direct-child platform enumeration and MapCatalog migration, strict/sanitizer/native
  evidence, failure corrections and limitations, and W4 locale handoff.
- [`reviews/2026-09-14-v1-0-w4-locale-independent-number.md`](reviews/2026-09-14-v1-0-w4-locale-independent-number.md)
  — POSIX/UCRT finite-double conversion, exact cross-platform corpus and scene bytes,
  strict/sanitizer/native evidence, failure corrections, and W5 Windows link handoff.
- [`reviews/2026-09-15-v1-0-w5-native-windows-functional.md`](reviews/2026-09-15-v1-0-w5-native-windows-functional.md)
  — scoped native Windows strict application and complete 64-runner evidence, Make link policy,
  persistence and fixture corrections, stack-bound remediation, failures, and L1/W6 handoff.
- [`reviews/2026-09-16-v1-0-closeout.md`](reviews/2026-09-16-v1-0-closeout.md)
  — final V1-0 exit-gate reconciliation, native Linux display/input result, residual
  limitations, and V1-1 handoff.
- [`reviews/2026-09-16-cppcheck-gate-recovery.md`](reviews/2026-09-16-cppcheck-gate-recovery.md)
  — cppcheck 2.18.2 findings, narrow source/gate corrections, timeout classification,
  and final real `make standards`/`make check` evidence.
- [`V1_1_UI_RULES_AND_TOKENS_Q1_PLAN_2026-09-16.md`](V1_1_UI_RULES_AND_TOKENS_Q1_PLAN_2026-09-16.md)
  — active V1-1 decision ledger, invariants, evidence requirements, incremental design,
  and implementation stop boundary.
- [`reviews/2026-09-16-v1-1-ui-literal-consumer-inventory.md`](reviews/2026-09-16-v1-1-ui-literal-consumer-inventory.md)
  — current application/editor versus authored-Menu ownership, literal/token candidates,
  scale/viewport assumptions, non-token content, and focused regression owners.
- [`reviews/2026-09-16-v1-1-d1-d7-candidate-decision.md`](reviews/2026-09-16-v1-1-d1-d7-candidate-decision.md)
  — D1–D7 candidate formulas, geometry, state, viewport/depth, motion, ownership rules,
  accessibility sources, alternatives, and the later accepted D1/D6 amendments.
- [`reviews/2026-09-16-v1-1-provisional-ui-theme-policy.md`](reviews/2026-09-16-v1-1-provisional-ui-theme-policy.md)
  — isolated pure policy implementation, focused regression coverage, development
  failures, full automated evidence, and the subsequently closed manual evaluation gate.
- [`reviews/2026-09-16-v1-1-q2-policy-architecture-review.md`](reviews/2026-09-16-v1-1-q2-policy-architecture-review.md)
  — Q2 seam review, bounded policy corrections, consumer comparison, selected
  application-menu palette migration, rollback, and the historical manual stop gate.
- [`reviews/2026-09-16-v1-1-application-menu-palette-adapter.md`](reviews/2026-09-16-v1-1-application-menu-palette-adapter.md)
  — first bounded production consumer, preserved ownership/behavior, automated and native
  display evidence, rollback, and subsequently approved palette mapping.
- [`reviews/2026-09-16-v1-1-full-palette-demo.md`](reviews/2026-09-16-v1-1-full-palette-demo.md)
  — complete diagnostic specimen, session-only scale controls, semantic/state coverage,
  verification evidence, failures, rollback, and per-role manual review procedure.
- [`reviews/2026-09-16-v1-1-ui-motion-demo.md`](reviews/2026-09-16-v1-1-ui-motion-demo.md)
  — pure explicit-time registration/reassembly model, isolated motion specimen, automated
  evidence, failures, rollback, accepted D6 decision, and V1-3 integration boundary.
- [`V1_3_PAUSE_CONTEXT_MOTION_I1_PLAN_2026-09-16.md`](V1_3_PAUSE_CONTEXT_MOTION_I1_PLAN_2026-09-16.md)
  — first real-context motion plan: stable pause menu, separate decorative layer,
  session-only reduced motion, deterministic tests, rollback, and one-context stop gate.
- [`reviews/2026-09-16-v1-3-pause-context-motion-i1.md`](reviews/2026-09-16-v1-3-pause-context-motion-i1.md)
  — implemented first pause-context consumer, preserved interaction invariants, automated
  evidence, development corrections, rollback boundary, and pending native review.
- [`reviews/2026-09-17-ui-workbench-i1.md`](reviews/2026-09-17-ui-workbench-i1.md)
  — dedicated application-UI asset workbench, validated preset metadata, canonical atomic
  writes/reloads, bounded previews, regression evidence, and explicit deferred boundaries.
- [`UI_WORKBENCH_ASSET_REFERENCE.md`](UI_WORKBENCH_ASSET_REFERENCE.md)
  — copyable application element/animation schemas, fixed preset/trigger semantics, reusable
  templates, workbench controls, and prompt-oriented extension rules.
- [`TODO.md`](TODO.md) — unordered future-feature ideas, deferred work, and design
  questions. This is not a roadmap or priority order.
- [`V1_PRODUCT_AND_AUTHORED_MODEL_FOUNDATION.md`](V1_PRODUCT_AND_AUTHORED_MODEL_FOUNDATION.md)
  — accepted v1 product philosophy, default/template/inference rules, ordered atomic
  entity-part model, and universal progression-connection foundation for the next roadmap.
- [`ARCHITECTURE.md`](ARCHITECTURE.md) — current module boundaries, ownership
  rules, data flows, persistent-data boundaries, and verification ownership.
- [`VERIFICATION_POLICY.md`](VERIFICATION_POLICY.md) — canonical verification gates,
  typed failure outcomes, and the required tool-failure investigation procedure.
- [`UI_DESIGN_AND_TEST_STANDARDS.md`](UI_DESIGN_AND_TEST_STANDARDS.md) — current
  measurable UI rules, focused test ownership, and explicitly undecided design work.
- [`PLATFORM_VERIFICATION_PROFILES.md`](PLATFORM_VERIFICATION_PROFILES.md) — required
  evidence for Gentoo, Ubuntu, Fedora, Steam Deck, Windows, and macOS.
- [`DOCKER_VALGRIND_GATE.md`](DOCKER_VALGRIND_GATE.md) — Docker/Gentoo kernel
  prerequisites, canonical Valgrind image operation, security boundary,
  troubleshooting, and collected implementation lessons.
- [`PLATFORM_TESTING.md`](PLATFORM_TESTING.md) — reproducible Linux compiler/libc
  surveys, profile phases, outcome semantics, commands, artifacts, and current
  compatibility results.

## Current authority

- [`FEATURE_ROADMAP.md`](FEATURE_ROADMAP.md) defines verified roadmap foundations,
  committed v1 outcomes, dependency order, phase-entry gates, rollback boundaries,
  required platforms, and release evidence.
- [`TODO.md`](TODO.md) is the complete unordered inventory for future/deferred
  work. Items must be promoted into focused requirements/planning before
  implementation.
- [`V1_PRODUCT_AND_AUTHORED_MODEL_FOUNDATION.md`](V1_PRODUCT_AND_AUTHORED_MODEL_FOUNDATION.md)
  defines the accepted product and authored-model constraints that future v1 roadmap
  sequencing and focused requirements must preserve. It is not an implementation-status
  claim or a sequenced roadmap.
- [`EDITOR_REQUIREMENTS_AND_REGRESSION_TESTS.md`](EDITOR_REQUIREMENTS_AND_REGRESSION_TESTS.md)
  is the accepted unified-editor behavior and regression contract until a planned
  and tested change deliberately revises it.
- [`VERIFICATION_POLICY.md`](VERIFICATION_POLICY.md) defines complete-suite usage,
  strict tool requirements, failure classification, and reporting requirements.
- [`DOCKER_VALGRIND_GATE.md`](DOCKER_VALGRIND_GATE.md) is the operational authority
  for Docker host requirements and the canonical containerized Valgrind gate.
- [`PLATFORM_TESTING.md`](PLATFORM_TESTING.md) is the operational authority for
  platform survey profiles and their interpretation.
- [`reviews/2026-09-11-roadmap-r12-closeout.md`](reviews/2026-09-11-roadmap-r12-closeout.md)
  records the latest verified roadmap closeout evidence.
- [`reviews/2026-09-11-code-standards-audit.md`](reviews/2026-09-11-code-standards-audit.md)
  records the non-SMC code audit, corrective-action dispositions, and verification
  evidence. SMC is explicitly excluded from that audit and its actions.
- [`reviews/2026-09-12-verification-policy-and-tool-failure.md`](reviews/2026-09-12-verification-policy-and-tool-failure.md)
  records removal of the partial aggregate, strict-gate implementation evidence,
  and the current Valgrind environment investigation.
- [`reviews/2026-09-12-regression-coverage-audit.md`](reviews/2026-09-12-regression-coverage-audit.md)
  maps all current runners and gates, records completed high-risk increments, and
  ranks remaining environment/design gaps.
- [`reviews/2026-09-12-windows-10-platform-survey-plan.md`](reviews/2026-09-12-windows-10-platform-survey-plan.md)
  records the implementation plan for the persistent native Windows 10 x64 VM
  survey; Increments 1–5 and informational profile integration are complete.
- [`reviews/2026-09-13-windows-10-first-result.md`](reviews/2026-09-13-windows-10-first-result.md)
  records the first unchanged UCRT64 build result, validated prerequisites, strict
  compile incompatibility, preserved evidence hashes, and successful VM restoration.

R0–R12 are verified roadmap foundations. They should not be read as a claim that
the editor or game-making experience is feature-complete.

## Stable technical references

- [`C_STYLE_AND_OWNERSHIP.md`](C_STYLE_AND_OWNERSHIP.md) — C style, ownership,
  and resource-lifetime expectations.
- [`ERROR_CATALOG.md`](ERROR_CATALOG.md) — known error categories and handling
  notes.
- [`../assets/README.md`](../assets/README.md) — asset file formats and data
  layout.

## Reviews, handoffs, and historical records

- [`reviews/`](reviews/) contains roadmap reviews and phase closeouts.
- [`archive/`](archive/) contains historical phase plans, implementation records,
  investigations, archived roadmap text, and superseded handoffs.
- [`handoff.md`](handoff.md) is retained as a compatibility pointer. Use
  [`CURRENT_STATUS.md`](CURRENT_STATUS.md) as the current handoff authority.
- Top-level `docs/` is reserved for current authority, stable standards, active
  inventories, and small compatibility pointers.

## Documentation update rules

- Keep current authority small and easy to find.
- Archive or supersede historical documents instead of deleting them.
- Do not leave multiple documents claiming current authority for the same
  behavior.
- Record verification evidence with exact commands, reviews, or manual checks
  before calling behavior Verified.
- Update [`CURRENT_STATUS.md`](CURRENT_STATUS.md), [`FEATURE_ROADMAP.md`](FEATURE_ROADMAP.md),
  and [`TODO.md`](TODO.md) according to their separate roles instead of mixing
  roadmap order, future ideas, and active handoff notes.
