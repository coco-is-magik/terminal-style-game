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
     * Legacy cell contract only; runtime fidelity uses compositor pixel evidence. */
    {MENU_MAIN, 100, 13910126345440706794ULL},
    {MENU_MAIN, 150, 1637535885879394538ULL},
    {MENU_PAUSE, 100, 1475701774946690991ULL},
    {MENU_SETTINGS, 100, 9870106462813291221ULL},
    {MENU_CONFIRM_QUIT, 100, 12275623073148853648ULL}
};

static const size_t ui_workbench_frame_contract_fixture_count =
    sizeof(ui_workbench_frame_contract_fixtures) /
    sizeof(ui_workbench_frame_contract_fixtures[0]);