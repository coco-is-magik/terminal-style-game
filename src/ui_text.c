/** ui_text.c — see ui_text.h for the four rules and why each is a decision. */

#include "ui_text.h"

#include "text_utf8.h"

#include <stdio.h>
#include <string.h>

const char *ui_text_result_name(UiTextResult result) {
    switch (result) {
    case UI_TEXT_OK: return "OK";
    case UI_TEXT_INVALID_ARGUMENT: return "INVALID_ARGUMENT";
    case UI_TEXT_MALFORMED: return "MALFORMED";
    case UI_TEXT_CONTROL: return "CONTROL";
    case UI_TEXT_BACKSLASH: return "BACKSLASH";
    case UI_TEXT_RIGHT_TO_LEFT: return "RIGHT_TO_LEFT";
    case UI_TEXT_CLUSTER_TOO_LONG: return "CLUSTER_TOO_LONG";
    }
    return "UNKNOWN";
}

static UiTextResult refuse(UiTextDiagnostic *out, UiTextResult result, size_t offset,
                           uint32_t codepoint) {
    if (out) {
        out->result = result;
        out->offset = offset;
        out->codepoint = codepoint;
    }
    return result;
}

UiTextResult ui_text_check(const char *bytes, size_t size, UiTextDiagnostic *out_diagnostic) {
    size_t offset = 0U;

    if (out_diagnostic) {
        out_diagnostic->result = UI_TEXT_OK;
        out_diagnostic->offset = size;
        out_diagnostic->codepoint = 0U;
    }
    if (!bytes && size > 0U) return refuse(out_diagnostic, UI_TEXT_INVALID_ARGUMENT, 0U, 0U);

    /* Characters first: a control or a backslash is reported by position, and the bytes around it need
       never be interpreted. */
    while (offset < size) {
        uint32_t codepoint = 0U;
        size_t start = offset;
        TextUtf8Result decoded = text_utf8_next_codepoint(bytes, size, &offset, &codepoint);

        if (decoded == TEXT_UTF8_END) break;
        if (decoded != TEXT_UTF8_OK) {
            /* `text_utf8_next_codepoint` leaves the offset on the bad sequence's first byte, which is
               the position the person who wrote the text needs. */
            return refuse(out_diagnostic, UI_TEXT_MALFORMED, start, 0U);
        }
        if (codepoint == (uint32_t)'\\') {
            return refuse(out_diagnostic, UI_TEXT_BACKSLASH, start, codepoint);
        }
        switch (text_utf8_classify(codepoint)) {
        case TEXT_UTF8_CLASS_CONTROL:
            return refuse(out_diagnostic, UI_TEXT_CONTROL, start, codepoint);
        case TEXT_UTF8_CLASS_RIGHT_TO_LEFT:
            return refuse(out_diagnostic, UI_TEXT_RIGHT_TO_LEFT, start, codepoint);
        case TEXT_UTF8_CLASS_PLAIN:
            break;
        }
    }

    /* Then clusters: one cluster is one cell (D-B), so a cluster longer than a cell can hold is text this
       application cannot draw and will not store. The character walk above already proved the bytes are
       well-formed, so this cannot fail for any other reason. */
    offset = 0U;
    while (offset < size) {
        uint32_t first_codepoint = 0U;
        size_t length = 0U;
        size_t start = offset;
        TextUtf8Result clustered =
            text_utf8_next_cluster(bytes, size, &offset, &first_codepoint, &length);
        if (clustered == TEXT_UTF8_END) break;
        if (clustered == TEXT_UTF8_CLUSTER_TOO_LONG) {
            return refuse(out_diagnostic, UI_TEXT_CLUSTER_TOO_LONG, start, 0U);
        }
        if (clustered != TEXT_UTF8_OK) {
            /* Unreachable after the character walk; reported as malformed rather than assumed. */
            return refuse(out_diagnostic, UI_TEXT_MALFORMED, start, 0U);
        }
    }
    return UI_TEXT_OK;
}

UiTextResult ui_text_check_cstr(const char *text, UiTextDiagnostic *out_diagnostic) {
    if (!text) {
        if (out_diagnostic) {
            out_diagnostic->result = UI_TEXT_INVALID_ARGUMENT;
            out_diagnostic->offset = 0U;
            out_diagnostic->codepoint = 0U;
        }
        return UI_TEXT_INVALID_ARGUMENT;
    }
    return ui_text_check(text, strlen(text), out_diagnostic);
}

void ui_text_describe(const UiTextDiagnostic *diagnostic, char *buffer, size_t capacity) {
    if (!buffer || capacity == 0U) return;
    buffer[0] = '\0';
    if (!diagnostic) return;
    if (diagnostic->codepoint != 0U) {
        (void)snprintf(buffer, capacity, "%s at byte %zu (U+%04X)",
                       ui_text_result_name(diagnostic->result), diagnostic->offset,
                       (unsigned)diagnostic->codepoint);
    } else {
        (void)snprintf(buffer, capacity, "%s at byte %zu", ui_text_result_name(diagnostic->result),
                       diagnostic->offset);
    }
}
