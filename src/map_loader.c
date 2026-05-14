/**
 * map_loader.c — Text-based map parser
 *
 * This file implements the map loader that converts a simple text grid
 * (such as the contents of assets/maps/1.txt) into a Map struct.
 *
 * The text format is straightforward:
 *   - Each character in the text represents one tile/cell on the map.
 *   - A newline ('\n') marks the end of a row.
 *   - Digits '0' through '9' set the material_id directly
 *     (0 = empty/void, 1+ = specific material).
 *   - Any other character (e.g. spaces, letters) defaults to the
 *     default_material_id from config.ini.
 *
 * Example 3×3 map:
 *   111
 *   101
 *   111
 * This creates a 3×3 room with floor material 1, walls material 1,
 * and a single empty cell (material 0) in the centre.
 *
 * The parser works in two passes:
 *   1. First pass — scan the string to determine width and height.
 *   2. Second pass — iterate again, assigning material IDs to each cell
 *      via map_set().
 *
 * Width is determined by the longest row; all rows are padded to that
 * width when stored (rows shorter than the max width are left as whatever
 * map_set() leaves as default — typically material 0 due to calloc).
 */

#include "map_loader.h"   /* map_load_from_string() declaration */
#include "config.h"       /* config_get() — provides default_material_id */
#include <string.h>        /* (included for future use; strlen not used here) */

/**
 * map_load_from_string() — Parse a digit-grid string into a Map
 *
 * Converts a NUL-terminated string of digits separated by newlines into a
 * dynamically-allocated Map struct.  The function works in two passes:
 *
 *   Pass 1 (dimension scan):
 *     Iterates through the string character by character, counting columns
 *     per row and tracking the maximum row width.  Newlines increment the
 *     row count.  This determines the width and height needed for the Map.
 *
 *   Pass 2 (material assignment):
 *     Iterates through the string again.  For each character:
 *       - Newlines: reset column to 0, advance row.
 *       - Digits '0'–'9': convert to integer (ASCII '0' → 0, '1' → 1, etc.)
 *         and use that as the material_id.  '0' means empty/void space.
 *       - Any other character: use config_get()->default_material_id
 *         (typically 1, defined in config.ini).
 *
 * The Map is created via map_create(), which allocates both the cell array
 * and the light_map array (calloc'd to zero).  The caller owns the returned
 * pointer and must call map_destroy() when done.
 *
 * @param map_txt  NUL-terminated string containing the digit-grid map data.
 *                 Must not be NULL.
 * @return         Pointer to a newly-allocated Map, or NULL on failure
 *                 (invalid input or memory allocation failure).
 */
Map* map_load_from_string(const char *map_txt) {
    if (!map_txt) return NULL;

    /* ================================================================
     *  Pass 1: Determine map dimensions
     * ================================================================
     * Walk through the string once to count:
     *   - width  = the number of columns in the longest row
     *   - height = the number of rows
     *
     * A "row" is delimited by '\n'.  If the string does not end with a
     * newline, the final line still counts as a row.
     */

    int width = 0;           /* Max row width seen so far */
    int height = 0;          /* Total row count */
    int current_width = 0;   /* Characters in the current row */

    for (int i = 0; map_txt[i] != '\0'; i++) {
        if (map_txt[i] == '\n') {
            /* End of a row — update max width if this row was longer */
            if (current_width > width) width = current_width;
            current_width = 0;          /* Reset for next row */
            height++;                   /* Count this row */
        } else {
            current_width++;            /* Count this character in the row */
        }
    }

    /* Handle the last row if the string doesn't end with '\n' */
    if (current_width > 0) {
        if (current_width > width) width = current_width;
        height++;                       /* Count the final unterminated row */
    }

    /* If we found no rows at all, there's nothing to load */
    if (width == 0 || height == 0) return NULL;

    /* ================================================================
     *  Pass 2: Create the Map and populate cells
     * ================================================================
     * map_create() allocates the cells array (via calloc — all zeros,
     * i.e. material_id = 0) and the light_map array (also zeros).
     * We then walk the string again and set each cell's material_id.
     */

    Map *map = map_create(width, height);
    if (!map) return NULL;               /* Allocation failure */

    int x = 0;    /* Current column cursor */
    int y = 0;    /* Current row cursor */

    for (int i = 0; map_txt[i] != '\0'; i++) {
        if (map_txt[i] == '\n') {
            /* Newline: advance to the next row, reset column */
            x = 0;
            y++;
        } else {
            /* Determine the material ID for this cell:
             *   - '0'–'9' → digit value (0 = empty, 1–9 = material)
             *   - anything else → config default material */
            int mat_id = config_get()->default_material_id;

            if (map_txt[i] >= '0' && map_txt[i] <= '9') {
                mat_id = map_txt[i] - '0';   /* ASCII digit → integer */
            }
            /* Non-digit characters (spaces, letters, punctuation) use
             * the default material — useful for ASCII art style map
             * definitions where you only mark special tiles. */

            map_set(map, x, y, mat_id);
            x++;   /* Advance to the next column */
        }
    }

    return map;
}