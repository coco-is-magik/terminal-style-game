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
#include "../src/mapgen_seed.h"
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
            "       mapgen --reproduce PATH.tscene\n"
            "  defaults: seed 1, 24x16, 12 rooms, material 1\n"
            "  a .mapgen sidecar recording the parameters is written beside the scene;\n"
            "  --reproduce regenerates from that sidecar and compares every cell\n");
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

/* The provenance sidecar sits beside the scene: same path, .mapgen suffix. Writing it is part of
   generating a scene, not an extra: without it the seed is lost and the map cannot be
   reproduced. */
static bool sidecar_path_for(const char *scene_path, char *buffer, size_t buffer_size) {
    static const char suffix[] = ".mapgen";
    size_t length = scene_path ? strlen(scene_path) : 0U;
    size_t suffix_length = sizeof(suffix) - 1U;
    if (!scene_path || length == 0U || length + suffix_length >= buffer_size)
        return false;
    memcpy(buffer, scene_path, length);
    memcpy(buffer + length, suffix, suffix_length + 1U);
    return true;
}

static bool write_all(FILE *stream, const char *text, size_t length) {
    return fwrite(text, 1U, length, stream) == length;
}

/* Regenerate the scene at `scene_path` from its sidecar and compare every cell. This is the
   acceptance test for the sidecar itself: if the record is incomplete or the generator is not
   deterministic, the comparison fails here rather than at the next person's playtest. */
static int reproduce_scene(const char *scene_path) {
    char sidecar[4096];
    char text[MAPGEN_SEED_TEXT_MAX];
    MapgenParameters parameters;
    Map *map;
    SceneDocument document;
    SceneDiagnostic diagnostic;
    const Map *loaded;
    FILE *stream;
    size_t read_length;
    int result = EXIT_FAILURE;
    if (!sidecar_path_for(scene_path, sidecar, sizeof(sidecar))) {
        fprintf(stderr, "reproduce: scene path is too long\n");
        return EXIT_FAILURE;
    }
    stream = fopen(sidecar, "rb");
    if (!stream) {
        fprintf(stderr, "reproduce: cannot read %s\n", sidecar);
        return EXIT_FAILURE;
    }
    read_length = fread(text, 1U, sizeof(text) - 1U, stream);
    text[read_length] = '\0';
    if (ferror(stream)) {
        fprintf(stderr, "reproduce: cannot read %s\n", sidecar);
        fclose(stream);
        return EXIT_FAILURE;
    }
    fclose(stream);
    if (mapgen_seed_read_text(text, &parameters) != MAPGEN_SEED_OK) {
        fprintf(stderr, "reproduce: %s is not a valid generation record\n", sidecar);
        return EXIT_FAILURE;
    }
    map = map_create(parameters.width, parameters.height);
    if (!map) {
        fprintf(stderr, "reproduce: out of memory\n");
        return EXIT_FAILURE;
    }
    if (!mapgen_generate(&parameters, map, NULL)) {
        fprintf(stderr, "reproduce: generation failed\n");
        map_destroy(map);
        return EXIT_FAILURE;
    }
    config_init_defaults();
    scene_document_init(&document);
    scene_diagnostic_reset(&diagnostic);
    if (scene_document_load_native(&document, scene_path, &diagnostic) != SCENE_LOAD_OK) {
        fprintf(stderr, "reproduce: cannot load %s%s%s\n", scene_path,
                diagnostic.detail[0] ? ": " : "", diagnostic.detail);
        scene_document_destroy(&document);
        map_destroy(map);
        return EXIT_FAILURE;
    }
    loaded = scene_document_get_map(&document);
    if (!loaded || loaded->width != map->width || loaded->height != map->height ||
        memcmp(loaded->cells, map->cells,
               (size_t)map->width * (size_t)map->height * sizeof(*map->cells)) != 0) {
        fprintf(stderr, "reproduce: %s does not match its generation record\n",
                scene_path);
    } else {
        fprintf(stderr, "reproduce: %s regenerates exactly (seed %llu, %dx%d)\n",
                scene_path, (unsigned long long)parameters.seed, parameters.width,
                parameters.height);
        result = EXIT_SUCCESS;
    }
    scene_document_destroy(&document);
    map_destroy(map);
    return result;
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
        if (strcmp(option, "--reproduce") == 0) {
            if (!argument) {
                usage(stderr);
                return EXIT_FAILURE;
            }
            return reproduce_scene(argument);
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
        char sidecar[4096];
        char text[MAPGEN_SEED_TEXT_MAX];
        size_t length = 0U;
        FILE *stream;
        if (!sidecar_path_for(out_path, sidecar, sizeof(sidecar))) {
            fprintf(stderr, "mapgen: --out path is too long for a sidecar path\n");
            exit_code = EXIT_FAILURE;
        } else if (mapgen_seed_write_text(&parameters, text, sizeof(text), &length) !=
                   MAPGEN_SEED_OK) {
            fprintf(stderr, "mapgen: could not serialise generation parameters\n");
            exit_code = EXIT_FAILURE;
        } else if (!(stream = fopen(sidecar, "wb"))) {
            fprintf(stderr, "mapgen: could not write %s\n", sidecar);
            exit_code = EXIT_FAILURE;
        } else {
            if (!write_all(stream, text, length)) {
                fprintf(stderr, "mapgen: short write to %s\n", sidecar);
                exit_code = EXIT_FAILURE;
            } else {
                fprintf(stderr, "mapgen: recorded %s\n", sidecar);
            }
            if (fclose(stream) != 0 && exit_code == EXIT_SUCCESS) {
                fprintf(stderr, "mapgen: could not flush %s\n", sidecar);
                exit_code = EXIT_FAILURE;
            }
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

