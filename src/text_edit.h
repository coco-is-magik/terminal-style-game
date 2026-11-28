/**
 * text_edit.h — cluster-aware cursor movement, deletion and capacity
 *
 * Every rule here exists because a byte offset is the wrong unit for an editor. A cursor inside a
 * combining sequence, or between the two halves of a ZWJ emoji, is a cursor a user cannot see and a
 * Backspace that deletes half a character is a bug they can. So this module speaks in **cluster
 * boundaries** and refuses to hand back anything else.
 *
 * It is deliberately pure: no I/O, no SDL, no platform calls, no globals. Both text owners — the
 * authored `UiDocument` and the editor's own fields — call it, and neither reimplements a boundary
 * rule (V1-6 plan, D-G). That is also what makes the exit gate's "headless multilingual layout/edit
 * tests" possible at all.
 *
 * Callers pass a buffer and its length explicitly. Nothing here assumes NUL termination, because a
 * field being edited in place has a capacity and a length and the two are not the same thing.
 */

#ifndef TEXT_EDIT_H
#define TEXT_EDIT_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    TEXT_EDIT_OK = 0,
    /* No buffer, no output pointer, or an offset past the end. */
    TEXT_EDIT_INVALID_ARGUMENT,
    /* The buffer is not valid UTF-8. Nothing is written and no offset is produced: a boundary in
       malformed text is not a boundary, it is a guess. */
    TEXT_EDIT_MALFORMED,
    /* `text_edit_fit` copied the longest whole-cluster prefix it could and the rest did not fit. */
    TEXT_EDIT_TRUNCATED,
    /* `text_edit_fit` could not fit even the first cluster, so it copied nothing. */
    TEXT_EDIT_NOTHING_FITS,
    /* `text_edit_replace_range` would have to exceed the caller's capacity. Refused whole: a keystroke
       that does not fit is dropped visibly (nothing happens), never silently shortened. */
    TEXT_EDIT_NO_ROOM
} TextEditResult;

const char *text_edit_result_name(TextEditResult result);

/* True when `offset` sits on a cluster boundary — 0 and `size` always do. False when it does not, or
   when the buffer is malformed (in which case there is no boundary to speak of). */
bool text_edit_is_boundary(const char *bytes, size_t size, size_t offset);

/* Greatest boundary strictly less than `offset`. For `offset == 0` the answer is 0. */
TextEditResult text_edit_prev_boundary(const char *bytes, size_t size, size_t offset, size_t *out);

/* Least boundary strictly greater than `offset`. For `offset == size` the answer is `size`.
   `offset` must itself be a boundary: moving forward from inside a cluster has no meaning, so it is
   TEXT_EDIT_INVALID_ARGUMENT rather than a guess. (`text_edit_prev_boundary` is deliberately more
   forgiving — it snaps back to the boundary before a non-boundary offset, which is what a "move left"
   from a bad cursor should do.) */
TextEditResult text_edit_next_boundary(const char *bytes, size_t size, size_t offset, size_t *out);

/**
 * Delete the cluster ending at `*cursor` and move the cursor to where it began.
 *
 * `*size` is the live length and `*capacity` the buffer's size; both are updated. A cursor that is
 * not a boundary, or is 0, leaves everything untouched: refusing is better than deleting a guess.
 * A cursor at 0 is not an error — there is simply nothing before it — and returns TEXT_EDIT_OK.
 */
TextEditResult text_edit_backspace(char *bytes, size_t capacity, size_t *size, size_t *cursor);

/**
 * Copy `src` into `dst`, truncating only at a cluster boundary.
 *
 * On TEXT_EDIT_OK everything was copied. On TEXT_EDIT_TRUNCATED the destination holds the longest
 * whole-cluster prefix that fit, and `*out_size` says how long it is. On TEXT_EDIT_MALFORMED nothing
 * is written at all — validation happens before any copy, so a caller never inherits half a field.
 *
 * Truncating here and refusing to half-apply a *paste* are not in conflict: the paste path treats
 * anything other than TEXT_EDIT_OK as "refuse", and this same function tells it that (V1-6 plan, D-E).
 */
TextEditResult text_edit_fit(const char *src, size_t src_size, char *dst, size_t capacity,
                             size_t *out_size);

/* Order a selection's two ends. An editor keeps an anchor and a caret because "select leftwards" and
   "select rightwards" must both work; the rest of the module wants the pair in order.
   Pure arithmetic: it does not check that either offset is a boundary — the callers that move a caret
   only ever produce boundaries, and checking here would need the buffer this function deliberately
   does not take. */
void text_edit_selection(size_t anchor, size_t caret, size_t *out_begin, size_t *out_end);

/**
 * Replace the cluster-aligned range [begin, end) with `with`.
 *
 * This is the one primitive typed text needs and Backspace alone cannot express: it covers insertion at
 * a caret (`begin == end`), deleting a selection (`with_size == 0`) and replacing one (`both`).
 *
 * Refusals, all leaving the buffer untouched:
 * - `begin` or `end` is not a cluster boundary, or out of order, or past `*size`: a range that splits a
 *   character is not a range.
 * - `with` is not well-formed UTF-8.
 * - the result would not fit: `capacity` counts the terminator, so the result must satisfy
 *   `new_size < capacity`. **Nothing is truncated** — D-E refuses a whole paste rather than half-applying
 *   it, and the same reasoning applies to a keystroke.
 *
 * On success the buffer is re-terminated, `*size` and `*out_cursor` are updated, and the result is
 * TEXT_EDIT_OK. `out_cursor` may be NULL.
 */
TextEditResult text_edit_replace_range(char *bytes, size_t capacity, size_t *size,
                                       size_t begin, size_t end,
                                       const char *with, size_t with_size,
                                       size_t *out_cursor);

#endif /* TEXT_EDIT_H */
