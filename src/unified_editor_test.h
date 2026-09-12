/** unified_editor_test.h — Test-only fault injection for unified_editor. */
#ifndef UNIFIED_EDITOR_TEST_H
#define UNIFIED_EDITOR_TEST_H

#include <stdbool.h>

/* Tests must restore this process-global hook to false before completing. */
void unified_editor_set_runtime_build_failure_for_test(bool fail);

#endif