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
     * finally the chromatic-aberration fringe rework (saturated colour only on
     * the weave's edges, one hue per run) with a carved margin around drawn
     * content; the main context gained its field too (MENU_MAIN and MENU_PAUSE
     * only). */
    {MENU_MAIN, 100, 17206505380091137440ULL},
    {MENU_MAIN, 150, 1538200801492471200ULL},
    {MENU_PAUSE, 100, 17382040781361431038ULL},
    {MENU_SETTINGS, 100, 14648528100631133103ULL},
    {MENU_CONFIRM_QUIT, 100, 18405518230658705158ULL}
};

static const size_t ui_workbench_frame_contract_fixture_count =
    sizeof(ui_workbench_frame_contract_fixtures) /
    sizeof(ui_workbench_frame_contract_fixtures[0]);