#define _POSIX_C_SOURCE 200809L

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>

#include <cmocka.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#include "../src/ui_ele.h"
#include "../src/ui_workbench_store.h"
#include "../src/platform_fs.h"

static unsigned int replace_calls;
static unsigned int fail_replace_call;
static unsigned int fail_replace_call_also;
static unsigned int interrupt_after_replace;
static char retained_original[UI_ELE_PATH_MAX + 32U];

PlatformReplaceResult __real_platform_fs_replace(const char *, const char *, bool);
PlatformReplaceResult __wrap_platform_fs_replace(const char *, const char *, bool);

PlatformReplaceResult __wrap_platform_fs_replace(const char *source, const char *destination,
                                                 bool exists) {
    replace_calls++;
    if ((fail_replace_call && replace_calls == fail_replace_call) ||
        (fail_replace_call_also && replace_calls == fail_replace_call_also)) {
        PlatformReplaceResult failed = {0};
        if (replace_calls == fail_replace_call_also)
            (void)snprintf(retained_original, sizeof(retained_original), "%s", source);
        failed.commit_state = PLATFORM_COMMIT_NOT_COMMITTED;
        return failed;
    }
    PlatformReplaceResult result = __real_platform_fs_replace(source, destination, exists);
    if (interrupt_after_replace && replace_calls == interrupt_after_replace &&
        result.commit_state != PLATFORM_COMMIT_NOT_COMMITTED) _exit(73);
    return result;
}

static void write_text(const char *path, const char *text) {
    FILE *file = fopen(path, "wb");
    assert_non_null(file);
    assert_true(fputs(text, file) >= 0);
    assert_int_equal(fclose(file), 0);
}

static void test_legacy_defaults_and_valid_metadata(void **state) {
    char legacy[] = "build/tsg_ui_workbench_legacy_XXXXXX";
    char metadata[] = "build/tsg_ui_workbench_metadata_XXXXXX";
    int descriptor;
    UiElement *element;
    (void)state;
    descriptor = mkstemp(legacy);
    assert_true(descriptor >= 0);
    assert_int_equal(close(descriptor), 0);
    write_text(legacy, "name=legacy\ntype=button\nx=1\ny=2\nwidth=10\nheight=1\n");
    element = ui_ele_load(legacy, NULL);
    assert_non_null(element);
    assert_string_equal(element->style, "plain");
    assert_string_equal(element->transition, "none");
    assert_string_equal(element->focus_effect, "none");
    ui_ele_destroy(element);
    assert_int_equal(unlink(legacy), 0);

    descriptor = mkstemp(metadata);
    assert_true(descriptor >= 0);
    assert_int_equal(close(descriptor), 0);
    write_text(metadata,
               "name=styled\ntype=button\nstyle=bracket\n"
               "transition=perimeter_burst\nfocus_effect=focus_glitch\n");
    element = ui_ele_load(metadata, NULL);
    assert_non_null(element);
    assert_string_equal(element->style, "bracket");
    assert_string_equal(element->transition, "perimeter_burst");
    assert_string_equal(element->focus_effect, "focus_glitch");
    ui_ele_destroy(element);
    assert_int_equal(unlink(metadata), 0);
}

static void test_invalid_presets_are_rejected(void **state) {
    char path[] = "build/tsg_ui_workbench_invalid_XXXXXX";
    int descriptor;
    (void)state;
    descriptor = mkstemp(path);
    assert_true(descriptor >= 0);
    assert_int_equal(close(descriptor), 0);
    write_text(path, "name=bad\ntype=text\nstyle=bracket\n");
    assert_null(ui_ele_load(path, NULL));
    write_text(path, "name=bad\ntype=button\ntransition=unknown\n");
    assert_null(ui_ele_load(path, NULL));
    write_text(path, "name=bad\ntype=button\nfocus_effect=unknown\n");
    assert_null(ui_ele_load(path, NULL));
    assert_int_equal(unlink(path), 0);
}

static void test_atomic_store_round_trip(void **state) {
    char path[] = "build/tsg_ui_workbench_store_XXXXXX";
    int descriptor;
    UiElement *element;
    UiElement *reloaded;
    UiWorkbenchStoreResult result;
    (void)state;
    descriptor = mkstemp(path);
    assert_true(descriptor >= 0);
    assert_int_equal(close(descriptor), 0);
    write_text(path,
               "name=item\ntype=button\nparent=container\nx=2\ny=3\n"
               "coords=relative\nwidth=20\nheight=1\nalign=center\n"
               "action=start_game\ncontent=START\n");
    element = ui_ele_load(path, NULL);
    assert_non_null(element);
    element->layout.x = 7;
    element->layout.y = 9;
    (void)snprintf(element->style, sizeof(element->style), "inverse");
    (void)snprintf(element->transition, sizeof(element->transition), "center_out");
    (void)snprintf(element->focus_effect, sizeof(element->focus_effect), "focus_pulse");
    result = ui_workbench_store_element(element, path);
    assert_true(result == UI_WORKBENCH_STORE_OK ||
                result == UI_WORKBENCH_STORE_OK_DURABILITY_WARNING);
    reloaded = ui_ele_load(path, NULL);
    assert_non_null(reloaded);
    assert_int_equal(reloaded->layout.x, 7);
    assert_int_equal(reloaded->layout.y, 9);
    assert_string_equal(reloaded->parent_name, "container");
    assert_string_equal(reloaded->style, "inverse");
    assert_string_equal(reloaded->transition, "center_out");
    assert_string_equal(reloaded->focus_effect, "focus_pulse");
    assert_string_equal(reloaded->action, "start_game");
    assert_string_equal(reloaded->content, "START");
    ui_ele_destroy(reloaded);
    ui_ele_destroy(element);
    assert_int_equal(unlink(path), 0);
}

static void test_invalid_store_preserves_destination(void **state) {
    char path[] = "build/tsg_ui_workbench_preserve_XXXXXX";
    int descriptor;
    UiElement element = {0};
    char bytes[64];
    FILE *file;
    (void)state;
    descriptor = mkstemp(path);
    assert_true(descriptor >= 0);
    assert_int_equal(close(descriptor), 0);
    write_text(path, "original\n");
    assert_int_equal(ui_workbench_store_element(&element, path),
                     UI_WORKBENCH_STORE_INVALID_ELEMENT);
    file = fopen(path, "rb");
    assert_non_null(file);
    assert_non_null(fgets(bytes, sizeof(bytes), file));
    assert_int_equal(fclose(file), 0);
    assert_string_equal(bytes, "original\n");
    (void)snprintf(element.name, sizeof(element.name), "same");
    (void)snprintf(element.parent_name, sizeof(element.parent_name), "same");
    element.type = UI_ELE_TEXT;
    element.visible = 1;
    (void)snprintf(element.style, sizeof(element.style), "plain");
    (void)snprintf(element.transition, sizeof(element.transition), "none");
    (void)snprintf(element.focus_effect, sizeof(element.focus_effect), "none");
    assert_int_equal(ui_workbench_store_element(&element, path),
                     UI_WORKBENCH_STORE_INVALID_ELEMENT);
    assert_int_equal(unlink(path), 0);
}

static void test_animation_unit_round_trip_and_validation(void **state) {
    char path[] = "build/tsg_ui_workbench_animation_XXXXXX";
    int descriptor;
    UiElement *element;
    UiElement *reloaded;
    UiWorkbenchStoreResult result;
    (void)state;
    descriptor = mkstemp(path);
    assert_true(descriptor >= 0);
    assert_int_equal(close(descriptor), 0);
    write_text(path,
               "name=pause_glitch_copy\ntype=animation\n"
               "x=3\ny=4\nwidth=20\nheight=9\n"
               "preset=pause_glitch\ntarget=pause_menu_container\n"
               "trigger=while_visible\norientation=vertical\n"
               "loop=1\nrandomize=1\n");
    element = ui_ele_load(path, NULL);
    assert_non_null(element);
    assert_int_equal(element->type, UI_ELE_ANIMATION);
    assert_string_equal(element->preset, "pause_glitch");
    assert_string_equal(element->target, "pause_menu_container");
    assert_string_equal(element->trigger, "while_visible");
    assert_string_equal(element->orientation, "vertical");
    assert_int_equal(element->loop, 1);
    assert_int_equal(element->randomize, 1);
    result = ui_workbench_store_element(element, path);
    assert_true(result == UI_WORKBENCH_STORE_OK ||
                result == UI_WORKBENCH_STORE_OK_DURABILITY_WARNING);
    reloaded = ui_ele_load(path, NULL);
    assert_non_null(reloaded);
    assert_string_equal(reloaded->preset, element->preset);
    assert_string_equal(reloaded->target, element->target);
    assert_string_equal(reloaded->trigger, element->trigger);
    assert_string_equal(reloaded->orientation, element->orientation);
    assert_int_equal(reloaded->loop, element->loop);
    assert_int_equal(reloaded->randomize, element->randomize);
    ui_ele_destroy(reloaded);
    ui_ele_destroy(element);

    write_text(path,
               "name=bad\ntype=animation\npreset=unknown\ntarget=x\n"
               "trigger=context_enter\norientation=horizontal\n");
    assert_null(ui_ele_load(path, NULL));
    write_text(path,
               "name=bad\ntype=animation\npreset=pause_glitch\ntarget=x\n"
               "trigger=unknown\norientation=horizontal\n");
    assert_null(ui_ele_load(path, NULL));
    write_text(path,
               "name=bad\ntype=animation\npreset=pause_glitch\ntarget=x\n"
               "trigger=context_enter\norientation=diagonal\n");
    assert_null(ui_ele_load(path, NULL));
    write_text(path,
               "name=bad\ntype=animation\npreset=pause_glitch\ntarget=x\n"
               "trigger=context_enter\norientation=horizontal\nloop=2\n");
    assert_null(ui_ele_load(path, NULL));
    assert_int_equal(unlink(path), 0);
}

static void test_layout_membership_add_remove_and_failure_restore(void **state) {
    char layout[] = "build/tsg_ui_layout_membership_XXXXXX";
    char master[] = "build/tsg_ui_master_membership_XXXXXX";
    int descriptor;
    char bytes[512];
    FILE *file;
    (void)state;
    descriptor = mkstemp(layout);
    assert_true(descriptor >= 0);
    assert_int_equal(close(descriptor), 0);
    descriptor = mkstemp(master);
    assert_true(descriptor >= 0);
    assert_int_equal(close(descriptor), 0);
    write_text(layout, "name=main_menu\ntype=layout\nelements=one,two\n");
    write_text(master,
               "layout=main_menu\ncache_next=container,one,two\ndrop_after=\n"
               "layout=pause_menu\ncache_next=pause\ndrop_after=\n");
    assert_int_equal(ui_workbench_store_membership(
        layout, master, "main_menu", "three", true), UI_WORKBENCH_STORE_OK);
    file = fopen(layout, "rb");
    assert_non_null(file);
    assert_non_null(fgets(bytes, sizeof(bytes), file));
    assert_non_null(fgets(bytes, sizeof(bytes), file));
    assert_non_null(fgets(bytes, sizeof(bytes), file));
    assert_int_equal(fclose(file), 0);
    assert_string_equal(bytes, "elements=one,two,three\n");
    assert_int_equal(ui_workbench_store_membership(
        layout, master, "main_menu", "two", false), UI_WORKBENCH_STORE_OK);
    file = fopen(layout, "rb");
    assert_non_null(file);
    assert_non_null(fgets(bytes, sizeof(bytes), file));
    assert_non_null(fgets(bytes, sizeof(bytes), file));
    assert_non_null(fgets(bytes, sizeof(bytes), file));
    assert_int_equal(fclose(file), 0);
    assert_string_equal(bytes, "elements=one,three\n");
    assert_int_equal(ui_workbench_store_membership(
        layout, "build/missing/master.txt", "main_menu", "four", true),
        UI_WORKBENCH_STORE_IO_ERROR);
    file = fopen(layout, "rb");
    assert_non_null(file);
    assert_non_null(fgets(bytes, sizeof(bytes), file));
    assert_non_null(fgets(bytes, sizeof(bytes), file));
    assert_non_null(fgets(bytes, sizeof(bytes), file));
    assert_int_equal(fclose(file), 0);
    assert_string_equal(bytes, "elements=one,three\n");
    assert_int_equal(ui_workbench_store_membership(
        layout, master, "main_menu", "bad,injection", true),
        UI_WORKBENCH_STORE_INVALID_ARGUMENT);
    assert_int_equal(ui_workbench_store_membership(
        layout, layout, "main_menu", "four", true),
        UI_WORKBENCH_STORE_INVALID_ARGUMENT);
    write_text(master, "layout=main_menu\ndrop_after=\nlayout=pause_menu\ncache_next=pause\n");
    assert_int_equal(ui_workbench_store_membership(
        layout, master, "main_menu", "four", true), UI_WORKBENCH_STORE_IO_ERROR);
    assert_int_equal(unlink(master), 0);
    assert_int_equal(unlink(layout), 0);
}

static void test_membership_second_write_failure_restores_first(void **state) {
    char layout[] = "build/tsg_membership_layout_XXXXXX";
    char master[] = "build/tsg_membership_master_XXXXXX";
    char link[256];
    char bytes[128] = {0};
    const char *original = "name=main_menu\nelements=one,two\n";
    int descriptor;
    FILE *file;
    (void)state;
    descriptor = mkstemp(layout);
    assert_true(descriptor >= 0);
    assert_int_equal(close(descriptor), 0);
    descriptor = mkstemp(master);
    assert_true(descriptor >= 0);
    assert_int_equal(close(descriptor), 0);
    write_text(layout, original);
    write_text(master, "layout=main_menu\ncache_next=one,two\n");
    assert_true(snprintf(link, sizeof(link), "%s.link", master) < (int)sizeof(link));
    /* Reading succeeds; replacement explicitly rejects a symbolic link. */
    assert_int_equal(symlink(strrchr(master, '/') + 1, link), 0);
    assert_int_equal(ui_workbench_store_membership(layout, link, "main_menu", "three", true),
                     UI_WORKBENCH_STORE_IO_ERROR);
    file = fopen(layout, "rb");
    assert_non_null(file);
    assert_int_equal(fread(bytes, 1, sizeof(bytes) - 1, file), strlen(original));
    assert_int_equal(fclose(file), 0);
    assert_string_equal(bytes, original);
    assert_int_equal(unlink(link), 0);
    assert_int_equal(unlink(master), 0);
    assert_int_equal(unlink(layout), 0);
}

static void test_membership_commit_faults_preserve_originals(void **state) {
    const char *original_layout = "name=main_menu\nelements=one,two\n";
    const char *original_master = "layout=main_menu\ncache_next=one,two\n";
    (void)state;
    for (unsigned int fault = 1; fault <= 2; fault++) {
        char layout[] = "build/tsg_commit_layout_XXXXXX";
        char master[] = "build/tsg_commit_master_XXXXXX";
        const char *paths[] = {layout, master};
        const char *expected[] = {original_layout, original_master};
        for (size_t i = 0; i < 2; i++) {
            int descriptor = mkstemp(i == 0 ? layout : master);
            assert_true(descriptor >= 0);
            assert_int_equal(close(descriptor), 0);
            write_text(paths[i], expected[i]);
        }
        replace_calls = 0;
        fail_replace_call = fault;
        assert_int_equal(ui_workbench_store_membership(layout, master, "main_menu", "three", true),
                         UI_WORKBENCH_STORE_IO_ERROR);
        fail_replace_call = 0;
        assert_int_equal(replace_calls, fault == 1 ? 1 : 3);
        for (size_t i = 0; i < 2; i++) {
            char bytes[128] = {0};
            FILE *file = fopen(paths[i], "rb");
            assert_non_null(file);
            assert_int_equal(fread(bytes, 1, sizeof(bytes) - 1, file), strlen(expected[i]));
            assert_int_equal(fclose(file), 0);
            assert_string_equal(bytes, expected[i]);
            assert_int_equal(unlink(paths[i]), 0);
        }
    }
}

static void test_failed_compensation_retains_synced_original(void **state) {
    char layout[] = "build/tsg_recovery_layout_XXXXXX";
    char master[] = "build/tsg_recovery_master_XXXXXX";
    const char *original = "name=main_menu\nelements=one,two\n";
    char bytes[128] = {0};
    FILE *file;
    int descriptor;
    (void)state;
    descriptor = mkstemp(layout);
    assert_true(descriptor >= 0);
    assert_int_equal(close(descriptor), 0);
    descriptor = mkstemp(master);
    assert_true(descriptor >= 0);
    assert_int_equal(close(descriptor), 0);
    write_text(layout, original);
    write_text(master, "layout=main_menu\ncache_next=one,two\n");
    replace_calls = 0;
    fail_replace_call = 2;
    fail_replace_call_also = 3;
    retained_original[0] = '\0';
    assert_int_equal(ui_workbench_store_membership(layout, master, "main_menu", "three", true),
                     UI_WORKBENCH_STORE_ROLLBACK_FAILED);
    fail_replace_call = 0;
    fail_replace_call_also = 0;
    assert_true(retained_original[0] != '\0');
    file = fopen(retained_original, "rb");
    assert_non_null(file);
    assert_int_equal(fread(bytes, 1, sizeof(bytes) - 1, file), strlen(original));
    assert_int_equal(fclose(file), 0);
    assert_string_equal(bytes, original);
    assert_int_equal(ui_workbench_recover_membership(layout, master), UI_WORKBENCH_STORE_OK);
    assert_int_equal(ui_workbench_recover_membership(layout, master), UI_WORKBENCH_STORE_OK);
    memset(bytes, 0, sizeof(bytes));
    file = fopen(layout, "rb");
    assert_non_null(file);
    assert_int_equal(fread(bytes, 1, sizeof(bytes) - 1, file), strlen(original));
    assert_int_equal(fclose(file), 0);
    assert_string_equal(bytes, original);
    assert_int_equal(unlink(retained_original), 0);
    assert_int_equal(unlink(master), 0);
    assert_int_equal(unlink(layout), 0);
}

static void test_recovery_commit_decision_and_corrupt_record(void **state) {
    char layout[] = "build/tsg_journal_layout_XXXXXX";
    char master[] = "build/tsg_journal_master_XXXXXX";
    char journal[UI_ELE_PATH_MAX + 32U];
    const char *old_layout = "elements=one\n";
    const char *old_master = "layout=main_menu\ncache_next=one\n";
    const char *new_master = "layout=main_menu\ncache_next=one,two\n";
    char record[512];
    char bytes[128] = {0};
    int fd;
    FILE *file;
    (void)state;
    fd = mkstemp(layout);
    assert_true(fd >= 0);
    assert_int_equal(close(fd), 0);
    fd = mkstemp(master);
    assert_true(fd >= 0);
    assert_int_equal(close(fd), 0);
    assert_true(snprintf(journal, sizeof(journal), "%s.membership-recovery", layout) > 0);
    assert_true(snprintf(record, sizeof(record), "%zu\n%zu\n%zu\n%s%s%s",
        strlen(old_layout), strlen(old_master), strlen(new_master),
        old_layout, old_master, new_master) > 0);
    write_text(layout, "elements=one,two\n");
    write_text(master, new_master);
    write_text(journal, record);
    assert_int_equal(ui_workbench_recover_membership(layout, master), UI_WORKBENCH_STORE_OK);
    assert_int_equal(access(journal, F_OK), -1);
    file = fopen(layout, "rb");
    assert_non_null(file);
    assert_true(fread(bytes, 1, sizeof(bytes) - 1, file) > 0);
    assert_int_equal(fclose(file), 0);
    assert_string_equal(bytes, "elements=one,two\n");
    write_text(journal, record);
    write_text(master, "external modification\n");
    assert_int_equal(ui_workbench_recover_membership(layout, master), UI_WORKBENCH_STORE_ROLLBACK_FAILED);
    assert_int_equal(access(journal, F_OK), 0);
    write_text(journal, "4\ninvalid\n");
    assert_int_equal(ui_workbench_recover_membership(layout, master), UI_WORKBENCH_STORE_ROLLBACK_FAILED);
    assert_int_equal(unlink(journal), 0);
    assert_int_equal(unlink(master), 0);
    assert_int_equal(unlink(layout), 0);
}

static void test_recovery_after_process_interruption(void **state) {
    (void)state;
    for (unsigned int stop = 1; stop <= 2; stop++) {
        char layout[] = "build/tsg_interrupted_layout_XXXXXX";
        char master[] = "build/tsg_interrupted_master_XXXXXX";
        char bytes[128] = {0};
        int fd = mkstemp(layout);
        int status;
        pid_t child;
        FILE *file;
        assert_true(fd >= 0);
        assert_int_equal(close(fd), 0);
        fd = mkstemp(master);
        assert_true(fd >= 0);
        assert_int_equal(close(fd), 0);
        write_text(layout, "elements=one\n");
        write_text(master, "layout=main_menu\ncache_next=one\n");
        child = fork();
        assert_true(child >= 0);
        if (child == 0) {
            replace_calls = 0;
            interrupt_after_replace = stop;
            (void)ui_workbench_store_membership(layout, master, "main_menu", "two", true);
            _exit(74);
        }
        assert_int_equal(waitpid(child, &status, 0), child);
        assert_true(WIFEXITED(status));
        assert_int_equal(WEXITSTATUS(status), 73);
        assert_int_equal(ui_workbench_recover_membership(layout, master), UI_WORKBENCH_STORE_OK);
        assert_int_equal(ui_workbench_recover_membership(layout, master), UI_WORKBENCH_STORE_OK);
        file = fopen(layout, "rb");
        assert_non_null(file);
        assert_true(fread(bytes, 1, sizeof(bytes) - 1, file) > 0);
        assert_int_equal(fclose(file), 0);
        assert_string_equal(bytes, stop == 1 ? "elements=one\n" : "elements=one,two\n");
        memset(bytes, 0, sizeof(bytes));
        file = fopen(master, "rb");
        assert_non_null(file);
        assert_true(fread(bytes, 1, sizeof(bytes) - 1, file) > 0);
        assert_int_equal(fclose(file), 0);
        assert_string_equal(bytes, stop == 1 ? "layout=main_menu\ncache_next=one\n" :
                            "layout=main_menu\ncache_next=one,two\n");
        assert_int_equal(unlink(master), 0);
        assert_int_equal(unlink(layout), 0);
    }
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_legacy_defaults_and_valid_metadata),
        cmocka_unit_test(test_recovery_commit_decision_and_corrupt_record),
        cmocka_unit_test(test_recovery_after_process_interruption),
        cmocka_unit_test(test_membership_commit_faults_preserve_originals),
        cmocka_unit_test(test_failed_compensation_retains_synced_original),
        cmocka_unit_test(test_membership_second_write_failure_restores_first),
        cmocka_unit_test(test_invalid_presets_are_rejected),
        cmocka_unit_test(test_atomic_store_round_trip),
        cmocka_unit_test(test_invalid_store_preserves_destination),
        cmocka_unit_test(test_animation_unit_round_trip_and_validation),
        cmocka_unit_test(test_layout_membership_add_remove_and_failure_restore)
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}