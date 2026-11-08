/**
 * tools/mapgen.c — offline map generator
 *
 * Proves the generation seam end to end: build a Map, let mapgen carve it, hand it to
 * scene_document_adopt_map, save the resulting scene. The saved file is an ordinary scene,
 * so anything that loads scenes can open a generated one with no special case.
 *
 * No image, no window, no SDL: the tool is headless by construction so it can run in CI and
 * in the platform containers.
 */

#define _POSIX_C_SOURCE 200809L

#include "../src/mapgen.h"
#include "../src/config.h"
#include "../src/map.h"
#include "../src/scene_document.h"
#include "../src/scene_diagnostic.h"
#include "../src/scene_types.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(FILE *out) {
    fprintf(out,
            "usage: mapgen [--seed N] [--width W] [--height H] [--rooms N]\n"
            "              [--material M] --out PATH.tscene\n"
            "  defaults: seed 1, 24x16, 12 rooms, material 1\n");
}

/* Native scenes carry a .tscene suffix; the document save rejects anything else, so the
   requirement is stated up front instead of surfacing as a save failure. */
static bool out_path_valid(const char *path) {
    static const char suffix[] = ".tscene";
    size_t length = path ? strlen(path) : 0U;
    return length > sizeof(suffix) - 1U &&
           memcmp(path + length - (sizeof(suffix) - 1U), suffix,
                  sizeof(suffix) - 1U) == 0;
}

/* Unsigned decimal parse with full validation: strtoull only, since atoi and sscanf are
   banned by the project's unsafe-call policy. */
static bool parse_unsigned(const char *text, uint64_t *out) {
    char *end = NULL;
    unsigned long long value;
    if (!text || text[0] == '\0' || text[0] == '-') return false;
    errno = 0;
    value = strtoull(text, &end, 10);
    if (errno != 0 || !end || *end != '\0') return false;
    *out = (uint64_t)value;
    return true;
}

int main(int argc, char **argv) {
    MapgenParameters parameters = mapgen_parameters_default();
    MapgenStats stats;
    SceneDocument document;
    const char *out_path = NULL;
    Map *map;
    uint64_t value;
    int i;
    int exit_code = EXIT_FAILURE;

    for (i = 1; i < argc; i++) {
        const char *option = argv[i];
        const char *argument = (i + 1 < argc) ? argv[i + 1] : NULL;
        if (strcmp(option, "--help") == 0 || strcmp(option, "-h") == 0) {
            usage(stdout);
            return EXIT_SUCCESS;
        }
        if (strcmp(option, "--out") == 0) {
            if (!argument) {
                usage(stderr);
                return EXIT_FAILURE;
            }
            out_path = argument;
            i++;
            continue;
        }
        if (!argument) {
            fprintf(stderr, "mapgen: %s needs a value\n", option);
            return EXIT_FAILURE;
        }
        if (strcmp(option, "--seed") == 0) {
            if (!parse_unsigned(argument, &value)) {
                fprintf(stderr, "mapgen: invalid seed '%s'\n", argument);
                return EXIT_FAILURE;
            }
            parameters.seed = value;
        } else if (strcmp(option, "--width") == 0) {
            if (!parse_unsigned(argument, &value) || value > (uint64_t)SCENE_MAX_WIDTH) {
                fprintf(stderr, "mapgen: invalid width '%s'\n", argument);
                return EXIT_FAILURE;
            }
            parameters.width = (int)value;
        } else if (strcmp(option, "--height") == 0) {
            if (!parse_unsigned(argument, &value) || value > (uint64_t)SCENE_MAX_HEIGHT) {
                fprintf(stderr, "mapgen: invalid height '%s'\n", argument);
                return EXIT_FAILURE;
            }
            parameters.height = (int)value;
        } else if (strcmp(option, "--rooms") == 0) {
            if (!parse_unsigned(argument, &value) || value > (uint64_t)MAPGEN_MAX_ROOMS) {
                fprintf(stderr, "mapgen: invalid room count '%s'\n", argument);
                return EXIT_FAILURE;
            }
            parameters.room_attempts = (int)value;
        } else if (strcmp(option, "--material") == 0) {
            if (!parse_unsigned(argument, &value) || value < 1U ||
                value > (uint64_t)ASSET_ID_MAX) {
                fprintf(stderr, "mapgen: invalid material '%s'\n", argument);
                return EXIT_FAILURE;
            }
            parameters.solid_material = (int)value;
        } else {
            fprintf(stderr, "mapgen: unknown option '%s'\n", option);
            usage(stderr);
            return EXIT_FAILURE;
        }
        i++;
    }

    if (!out_path) {
        fprintf(stderr, "mapgen: --out PATH is required\n");
        usage(stderr);
        return EXIT_FAILURE;
    }
    if (!out_path_valid(out_path)) {
        fprintf(stderr, "mapgen: --out must end in .tscene\n");
        return EXIT_FAILURE;
    }
    if (!mapgen_parameters_valid(&parameters)) {
        fprintf(stderr, "mapgen: parameters are not generatable\n");
        return EXIT_FAILURE;
    }

    map = map_create(parameters.width, parameters.height);
    if (!map) {
        fprintf(stderr, "mapgen: out of memory allocating %dx%d\n", parameters.width,
                parameters.height);
        return EXIT_FAILURE;
    }
    if (!mapgen_generate(&parameters, map, &stats)) {
        fprintf(stderr, "mapgen: generation failed\n");
        map_destroy(map);
        return EXIT_FAILURE;
    }
    fprintf(stderr, "mapgen: %dx%d seed=%llu rooms=%d open=%zu solid=%zu\n",
            parameters.width, parameters.height,
            (unsigned long long)parameters.seed, stats.rooms_placed,
            stats.open_cells, stats.solid_cells);

    scene_document_init(&document);
    /* The document reads material and ambient defaults from the engine config, so the
       config must exist before a map can be adopted. */
    config_init_defaults();
    /* The document takes ownership of the map whether the adopt succeeds or not. */
    if (scene_document_adopt_map(&document, map, NULL) != SCENE_LOAD_OK) {
        fprintf(stderr, "mapgen: the document refused the generated map\n");
        scene_document_destroy(&document);
        return EXIT_FAILURE;
    }
    {
        SceneDiagnostic diagnostic;
        SceneSaveResult saved;
        scene_diagnostic_reset(&diagnostic);
        saved = scene_document_save_as_native(&document, out_path, "generated",
                                              &diagnostic);
        if (saved != SCENE_SAVE_OK) {
            fprintf(stderr, "mapgen: save failed (%d)%s%s\n", (int)saved,
                    diagnostic.detail[0] ? ": " : "", diagnostic.detail);
        } else {
            fprintf(stderr, "mapgen: wrote %s\n", out_path);
            exit_code = EXIT_SUCCESS;
        }
    }
    if (exit_code == EXIT_SUCCESS) {
        /* Verify by reloading what was written: the tool is its own acceptance test, so a
           generated scene that cannot be opened again is reported here rather than in the
           editor. */
        SceneDocument reloaded;
        SceneDiagnostic reload_diagnostic;
        SceneLoadResult reloaded_result;
        const Map *reloaded_map;
        scene_document_init(&reloaded);
        scene_diagnostic_reset(&reload_diagnostic);
        reloaded_result = scene_document_load_native(&reloaded, out_path,
                                                     &reload_diagnostic);
        reloaded_map = reloaded_result == SCENE_LOAD_OK
                           ? scene_document_get_map(&reloaded)
                           : NULL;
        if (!reloaded_map || reloaded_map->width != parameters.width ||
            reloaded_map->height != parameters.height) {
            fprintf(stderr, "mapgen: reload verification failed (%d)%s%s\n",
                    (int)reloaded_result,
                    reload_diagnostic.detail[0] ? ": " : "",
                    reload_diagnostic.detail);
            exit_code = EXIT_FAILURE;
        } else {
            fprintf(stderr, "mapgen: verified by reload (%dx%d)\n",
                    reloaded_map->width, reloaded_map->height);
        }
        scene_document_destroy(&reloaded);
    }
    scene_document_destroy(&document);
    return exit_code;
}

