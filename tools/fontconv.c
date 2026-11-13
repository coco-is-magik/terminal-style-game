/**
 * tools/fontconv.c — font converter and font-contract verifier
 *
 * The contract (V1_4_FONT_DEPENDENCY_DECISION_2026-11-06.md, D12) says a face is identified by
 * the SHA-256 of every file it ships, declared in a manifest beside the fonts. This tool is the
 * half that answers "is what is installed what the manifest expects?" — and, once the fit policy
 * lands, the half that turns those files into 8x8 glyphs.
 *
 * Headless by construction, like mapgen: no window, no SDL, no image. `--list-fonts` needs no
 * font engine at all, so it works on a machine that has fonts but no FreeType, which is exactly
 * the machine where a hash mismatch is being diagnosed.
 *
 * Exit status, because a build step has to act on it: 0 verified, 1 a face is missing or does not
 * match its manifest hash, 2 the tool could not do its job (usage, unreadable or invalid
 * manifest).
 */

#define _POSIX_C_SOURCE 200809L

#include "../src/font_manifest.h"
#include "../src/sha256.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FONTCONV_PATH_MAX 512U
#define FONTCONV_READ_CHUNK 65536U

static void usage(FILE *out) {
    fprintf(out,
            "usage: fontconv --manifest PATH [--fonts-dir DIR] --list-fonts\n"
            "  --list-fonts    report every face and file: present, absent, or hash mismatch\n"
            "  --fonts-dir     where the manifest's file= paths are resolved (default: the\n"
            "                  manifest's own directory)\n"
            "  a mismatch prints the source= URL the manifest records, because the repair is\n"
            "  fetch-the-right-file, not guess\n");
}

/* Read a file in bounded chunks and digest it. Returns false with `*out_errno` set so the caller
   can distinguish absent from unreadable. */
static bool digest_file(const char *path, uint8_t out_digest[SHA256_DIGEST_SIZE], int *out_errno) {
    uint8_t chunk[FONTCONV_READ_CHUNK];
    Sha256 context;
    FILE *file = fopen(path, "rb");
    size_t read_bytes;
    if (!file) {
        if (out_errno) *out_errno = errno;
        return false;
    }
    sha256_init(&context);
    while ((read_bytes = fread(chunk, 1U, sizeof(chunk), file)) > 0U) {
        sha256_update(&context, chunk, read_bytes);
    }
    if (ferror(file)) {
        if (out_errno) *out_errno = EIO;
        fclose(file);
        return false;
    }
    if (fclose(file) != 0) {
        if (out_errno) *out_errno = errno;
        return false;
    }
    sha256_final(&context, out_digest);
    return true;
}

static bool join_path(char *buffer, size_t buffer_size, const char *directory,
                      const char *file) {
    int written;
    size_t directory_length = strlen(directory);
    bool needs_separator = directory_length > 0U && directory[directory_length - 1U] != '/';
    written = snprintf(buffer, buffer_size, "%s%s%s", directory, needs_separator ? "/" : "", file);
    return written >= 0 && (size_t)written < buffer_size;
}

/* The manifest's text, read whole. A manifest is bounded by FONT_MANIFEST_TEXT_MAX and anything
   larger is a rejection, not a truncation. */
static char *read_manifest_text(const char *path) {
    char *text = malloc((size_t)FONT_MANIFEST_TEXT_MAX + 1U);
    FILE *file;
    size_t length;
    if (!text) return NULL;
    file = fopen(path, "rb");
    if (!file) {
        free(text);
        return NULL;
    }
    length = fread(text, 1U, (size_t)FONT_MANIFEST_TEXT_MAX + 1U, file);
    if (ferror(file)) {
        fclose(file);
        free(text);
        return NULL;
    }
    fclose(file);
    text[length] = '\0';
    return text;
}

/* Directory part of a path, or "." when the manifest sits in the working directory. */
static void directory_of(const char *path, char *buffer, size_t buffer_size) {
    const char *slash = strrchr(path, '/');
    size_t length;
    if (!slash) {
        snprintf(buffer, buffer_size, ".");
        return;
    }
    length = (size_t)(slash - path);
    if (length == 0U || length >= buffer_size) {
        snprintf(buffer, buffer_size, ".");
        return;
    }
    memcpy(buffer, path, length);
    buffer[length] = '\0';
}


/* Report one face. Returns true only when every declared file is installed and matches, so the
   caller can derive the exit status without re-reading anything. */
static bool report_face(const FontManifestFace *face, const char *fonts_dir) {
    unsigned file_index;
    bool face_ok = true;
    printf("face %s — %s (%s, cell=8x8)\n", face->id, face->name, face->licence);
    for (file_index = 0U; file_index < face->file_count; file_index++) {
        const FontManifestFile *entry = &face->files[file_index];
        uint8_t installed[SHA256_DIGEST_SIZE];
        char installed_hex[SHA256_HEX_SIZE];
        char expected_hex[SHA256_HEX_SIZE];
        char path[FONTCONV_PATH_MAX];
        int failure = 0;
        sha256_to_hex(entry->sha256, expected_hex);
        if (!join_path(path, sizeof(path), fonts_dir, entry->file)) {
            /* A path that does not fit is not an absent file: say which one it is rather than
               reporting a fictitious absence. */
            printf("  PATH-TOO-LONG  %s\n", entry->file);
            face_ok = false;
            continue;
        }
        if (!digest_file(path, installed, &failure)) {
            if (failure == ENOENT) {
                printf("  MISSING        %s\n", entry->file);
                printf("                 obtain it from %s\n", face->source);
            } else {
                printf("  UNREADABLE     %s (%s)\n", entry->file, strerror(failure));
                printf("                 declared by %s\n", face->source);
            }
            face_ok = false;
            continue;
        }
        if (memcmp(installed, entry->sha256, SHA256_DIGEST_SIZE) != 0) {
            sha256_to_hex(installed, installed_hex);
            printf("  HASH-MISMATCH  %s\n", entry->file);
            printf("                 expected  %s\n", expected_hex);
            printf("                 installed %s\n", installed_hex);
            /* A different build of the same version is fixed by fetching the declared file, not
               by editing the manifest to agree with whatever happens to be installed. */
            printf("                 declared by %s\n", face->source);
            face_ok = false;
            continue;
        }
        printf("  OK             %s\n", entry->file);
    }
    if (face->version[0] != '\0' || face->coverage[0] != '\0') {
        printf("  declared       version=%s coverage=%s\n",
               face->version[0] != '\0' ? face->version : "-",
               face->coverage[0] != '\0' ? face->coverage : "-");
    }
    if (!face_ok) {
        printf("  repaired by    installing the files above, or correcting the manifest\n");
    }
    return face_ok;
}

int main(int argc, char **argv) {
    const char *manifest_path = NULL;
    const char *fonts_dir = NULL;
    bool list_fonts = false;
    char derived_dir[FONTCONV_PATH_MAX];
    char *text;
    FontManifest manifest;
    FontManifestDiagnostic diagnostic;
    FontManifestResult parsed;
    unsigned face_index;
    bool all_verified = true;
    int argument;

    for (argument = 1; argument < argc; argument++) {
        if (strcmp(argv[argument], "--list-fonts") == 0) {
            list_fonts = true;
        } else if (strcmp(argv[argument], "--manifest") == 0 && argument + 1 < argc) {
            manifest_path = argv[++argument];
        } else if (strcmp(argv[argument], "--fonts-dir") == 0 && argument + 1 < argc) {
            fonts_dir = argv[++argument];
        } else if (strcmp(argv[argument], "--help") == 0) {
            usage(stdout);
            return 0;
        } else {
            fprintf(stderr, "fontconv: unrecognised argument '%s'\n", argv[argument]);
            usage(stderr);
            return 2;
        }
    }

    if (!manifest_path || !list_fonts) {
        if (!list_fonts) fprintf(stderr, "fontconv: nothing to do without --list-fonts\n");
        usage(stderr);
        return 2;
    }

    text = read_manifest_text(manifest_path);
    if (!text) {
        fprintf(stderr, "fontconv: cannot read manifest '%s': %s\n", manifest_path,
                strerror(errno));
        return 2;
    }
    parsed = font_manifest_read_text(text, &manifest, &diagnostic);
    free(text);
    if (parsed != FONT_MANIFEST_OK) {
        /* Malformed input is reported with its line and key: the repair is an edit, and the
           editor needs to know where. */
        if (diagnostic.line > 0U) {
            fprintf(stderr, "fontconv: %s:%u: %s (%s)\n", manifest_path, diagnostic.line,
                    font_manifest_result_name(parsed),
                    diagnostic.key ? diagnostic.key : "unknown key");
        } else {
            fprintf(stderr, "fontconv: %s: %s\n", manifest_path,
                    font_manifest_result_name(parsed));
        }
        return 2;
    }

    if (!fonts_dir) {
        directory_of(manifest_path, derived_dir, sizeof(derived_dir));
        fonts_dir = derived_dir;
    }
    printf("manifest %s\nfonts    %s\n", manifest_path, fonts_dir);

    for (face_index = 0U; face_index < manifest.face_count; face_index++) {
        if (!report_face(&manifest.faces[face_index], fonts_dir)) all_verified = false;
    }

    if (all_verified) {
        printf("PASS: every declared file is installed and matches its manifest hash\n");
        return 0;
    }
    printf("FAIL: at least one declared file is missing or does not match\n");
    return 1;
}
