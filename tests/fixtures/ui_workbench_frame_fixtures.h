/* A1: application UI workbench composed-frame oracle fixtures.
 *
 * These checksums describe the byte-level composition from the headless oracle
 * at elapsed time 0 with the accepted provisional palette. They reproduce
 * today's validated geometry before the chrome re-base. Refresh them only with
 * an explicit review, never silently.
 *
 * Each value was recorded from the verified oracle run on 2026-09-18 and is
 * asserted by both test-ui-workbench-frame and check-ui-workbench-frame.
 */
#define UI_WORKBENCH_FRAME_FIXTURE_ELAPSED_MS 0.0

static const struct {
    MenuId context;
    int scale_percent;
    uint64_t checksum;
} ui_workbench_frame_contract_fixtures[] = {
    /* Reviewed refresh: docs/reviews/2026-09-19-application-ui-editor-execution.md.
     * Legacy cell contract only; runtime fidelity uses compositor pixel evidence.
     * Reviewed refresh: docs/reviews/2026-10-07-pause-living-field.md — the pause
     * context gained the living_field backdrop, then (2026-10-07) the decorative
     * chromatic material and the intermingled selective-colour model that
     * replaced the spatial hue ramp, then the white-dominant flowing fabric, and
     * finally the RGB-only substrate rework (the fringe draws only the three
     * additive primaries, a directional chromatic aberration, and the field
     * fills behind text and controls instead of carving a margin around them),
     * and the button focus rework (the '>'/'<' arrows are replaced by an
     * animated perimeter frame drawn by the `focus_perimeter` focus effect, so
     * only the focused button is framed), the menu unification (menu transitions
     * dropped; settings and confirm_quit given the same `ambient_field` backdrop
     * and focus_perimeter buttons as main/pause), and the neutral menu-highlight
     * scheme that replaces the old green selection fill. All four contexts have
     * a field; the main context gained its field first.
     * Reviewed refresh: docs/reviews/2026-10-08-menu-surface.md — a menu is now a
     * surface over the whole display rather than an 80x40 centred canvas
     * (APP_UI_MENU_WIDTH/HEIGHT removed), and every backdrop field declares
     * `extent=surface`, so the field fills that surface instead of an authored
     * box. All four contexts changed; nothing else re-tuned. */
    {MENU_MAIN, 100, 12788386773026097164ULL},
    {MENU_MAIN, 150, 8822537883753028476ULL},
    {MENU_PAUSE, 100, 10582059475855600612ULL},
    {MENU_SETTINGS, 100, 12207819633615769261ULL},
    {MENU_CONFIRM_QUIT, 100, 5199418223914077754ULL}
};

static const size_t ui_workbench_frame_contract_fixture_count =
    sizeof(ui_workbench_frame_contract_fixtures) /
    sizeof(ui_workbench_frame_contract_fixtures[0]);