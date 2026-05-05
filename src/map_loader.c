#include "map_loader.h"
#include "config.h"
#include <string.h>

Map* map_load_from_string(const char *map_txt) {
    if (!map_txt) return NULL;

    int width = 0;
    int height = 0;
    int current_width = 0;

    for (int i = 0; map_txt[i] != '\0'; i++) {
        if (map_txt[i] == '\n') {
            if (current_width > width) width = current_width;
            current_width = 0;
            height++;
        } else {
            current_width++;
        }
    }
    if (current_width > 0) {
        if (current_width > width) width = current_width;
        height++;
    }

    Map *map = map_create(width, height);
    if (!map) return NULL;

    int x = 0;
    int y = 0;
    for (int i = 0; map_txt[i] != '\0'; i++) {
        if (map_txt[i] == '\n') {
            x = 0;
            y++;
        } else {
            int mat_id = config_get()->default_material_id;
            if (map_txt[i] >= '0' && map_txt[i] <= '9') {
                mat_id = map_txt[i] - '0';
            }
            // For now, map_set will populate the material_id if we update Map
            // Map module will be refactored next
            map_set(map, x, y, mat_id);
            x++;
        }
    }

    return map;
}
