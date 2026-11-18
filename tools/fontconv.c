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
#include "../src/font_fit.h"
#include "../src/sha256.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if FONTCONV_HAVE_FREETYPE
#include <ft2build.h>
#include FT_FREETYPE_H
#endif

#define FONTCONV_PATH_MAX 512U
#define FONTCONV_READ_CHUNK 65536U
/* Bounded refusal report: enough to see the shape of the problem, never a wall of text. */
#define FONTCONV_MAX_REPORTED_REFUSALS 8U
/* "TSGFONT" + version. A reader that does not know this string must refuse the file rather than
   guess at its layout. */
static const char FONTCONV_SET_MAGIC[8] = {'T', 'S', 'G', 'F', 'O', 'N', 'T', '1'};
#define FONTCONV_SET_ENTRY_SIZE 16U   /* u32 codepoint, u8 mode, u8 rows[8], u8 reserved[3] */
#define FONTCONV_SET_HEADER_SIZE 48U  /* magic, cell, count, reserved, provenance digest */

static void usage(FILE *out) {
    fprintf(out,
            "usage: fontconv --manifest PATH [--fonts-dir DIR] --list-fonts\n"
            "       fontconv --manifest PATH [--fonts-dir DIR] --convert FACE --out PATH\n"
            "               [--file NAME] [--size N] [--fit none|squash|scale|normalize]\n"
            "  --list-fonts    report every face and file: present, absent, or hash mismatch\n"
            "  --convert       verify the face's hashes, then convert every codepoint the font\n"
            "                  provides into an 8x8 glyph set. Refuses rather than fitting unless\n"
            "                  --fit names a remedy; the default is none.\n"
            "  --file          which of a multi-file face to convert (default: the first)\n"
            "  --size          decode at this pixel size (default 8). A font is converted from\n"
            "                  its own 8x8 strike when it has one; --size 16 --fit scale is how a\n"
            "                  16x16 source reaches the cell.\n"
            "  --fit           remedy for glyphs that do not fit: none (default), squash, scale,\n"
            "                  normalize\n"
            "  --fonts-dir     where the manifest's file= paths are resolved (default: the\n"
            "                  manifest's own directory)\n"
            "  A run that cannot convert every glyph writes nothing and exits 1, naming the\n"
            "  overshoot and the remedy that would clear it.\n");
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

#if FONTCONV_HAVE_FREETYPE
/* What one conversion run did, in counts. Printed whether or not it aborted, because the first
   run's job is to describe the font. */
typedef struct {
    unsigned converted;
    unsigned blank;
    unsigned refused;
    unsigned unsupported;
    unsigned half_width;
    unsigned full_width;
    unsigned refused_reported;
    unsigned mode_counts[4];
} ConversionTally;

/* Mono bitmaps only. A grayscale or colour strike is a different font design (anti-aliased or
   coloured), and silently thresholding it would invent a glyph the designer did not draw. */
static bool mask_from_bitmap(const FT_Bitmap *bitmap, FontFitMask *mask) {
    unsigned x;
    unsigned y;
    font_fit_mask_init(mask);
    if (bitmap->pixel_mode != FT_PIXEL_MODE_MONO) return false;
    if (bitmap->width == 0U || bitmap->rows == 0U) return true; /* blank glyph, size stays 0 */
    if (bitmap->width > FONT_FIT_SOURCE_MAX || bitmap->rows > FONT_FIT_SOURCE_MAX) return false;
    mask->width = bitmap->width;
    mask->height = bitmap->rows;
    for (y = 0U; y < bitmap->rows; y++) {
        const unsigned char *row = bitmap->buffer + y * (size_t)bitmap->pitch;
        for (x = 0U; x < bitmap->width; x++) {
            /* 1bpp, MSB first, rows padded to a byte: the TrueType convention. */
            unsigned byte = row[x / 8U];
            if ((byte >> (7U - (x % 8U))) & 1U) mask->rows[y] |= (uint32_t)1U << x;
        }
    }
    return true;
}

/* The set's provenance: a digest over the face's declared file digests, in manifest order. It
   changes exactly when the bytes behind the manifest change, which is what a stale set looks
   like. */
static void provenance_digest(const FontManifestFace *face, uint8_t out[SHA256_DIGEST_SIZE]) {
    uint8_t joined[FONT_MANIFEST_MAX_FILES * SHA256_DIGEST_SIZE];
    Sha256 context;
    unsigned index;
    memset(joined, 0, sizeof(joined));
    for (index = 0U; index < face->file_count && index < FONT_MANIFEST_MAX_FILES; index++) {
        memcpy(joined + index * SHA256_DIGEST_SIZE, face->files[index].sha256,
               SHA256_DIGEST_SIZE);
    }
    sha256_init(&context);
    sha256_update(&context, joined, face->file_count * SHA256_DIGEST_SIZE);
    sha256_final(&context, out);
}

static void put_u32(uint8_t *buffer, uint32_t value) {
    buffer[0] = (uint8_t)(value & 0xFFU);
    buffer[1] = (uint8_t)((value >> 8U) & 0xFFU);
    buffer[2] = (uint8_t)((value >> 16U) & 0xFFU);
    buffer[3] = (uint8_t)((value >> 24U) & 0xFFU);
}

/* Serialise the set: header, then entries in ascending codepoint order. Little-endian and fixed
   width on purpose — the file is a build artefact that must compare byte for byte across
   machines, so nothing in it may depend on the host. */
static bool write_glyph_set(const char *path, const uint8_t *entries, unsigned entry_count,
                            const uint8_t provenance[SHA256_DIGEST_SIZE]) {
    uint8_t header[FONTCONV_SET_HEADER_SIZE];
    FILE *file;
    memset(header, 0, sizeof(header));
    memcpy(header, FONTCONV_SET_MAGIC, sizeof(FONTCONV_SET_MAGIC));
    put_u32(header + 8U, FONT_FIT_CELL_SIZE);
    put_u32(header + 12U, (uint32_t)entry_count);
    memcpy(header + 16U, provenance, SHA256_DIGEST_SIZE);
    file = fopen(path, "wb");
    if (!file) return false;
    if (fwrite(header, 1U, sizeof(header), file) != sizeof(header)) {
        fclose(file);
        return false;
    }
    if (entry_count > 0U &&
        fwrite(entries, 1U, (size_t)entry_count * FONTCONV_SET_ENTRY_SIZE, file) !=
            (size_t)entry_count * FONTCONV_SET_ENTRY_SIZE) {
        fclose(file);
        return false;
    }
    return fclose(file) == 0;
}
#endif /* FONTCONV_HAVE_FREETYPE */

#if FONTCONV_HAVE_FREETYPE
/* Verify every declared file of a face, then convert one of them. Returns the process exit code:
   0 converted, 1 the font or a glyph refused, 2 the request itself was wrong. */
static int convert_face(const FontManifestFace *face, const char *fonts_dir, const char *file_name,
                        FontFitMode mode, unsigned pixel_size, const char *out_path) {
    char path[FONTCONV_PATH_MAX];
    const FontManifestFile *chosen = NULL;
    unsigned index;
    FT_Library library = NULL;
    FT_Face ft_face = NULL;
    FT_ULong codepoint;
    FT_UInt glyph_index;
    uint8_t *entries = NULL;
    unsigned capacity = 0U;
    unsigned entry_count = 0U;
    ConversionTally tally;
    uint8_t provenance[SHA256_DIGEST_SIZE];
    int exit_code = 0;

    memset(&tally, 0, sizeof(tally));

    /* All files or none (D12): converting bytes that do not match the manifest would produce a
       glyph set nobody can reproduce. */
    for (index = 0U; index < face->file_count; index++) {
        uint8_t installed[SHA256_DIGEST_SIZE];
        int failure = 0;
        if (!join_path(path, sizeof(path), fonts_dir, face->files[index].file)) {
            fprintf(stderr, "fontconv: path too long for %s\n", face->files[index].file);
            return 2;
        }
        if (!digest_file(path, installed, &failure)) {
            fprintf(stderr, "fontconv: %s is not readable (%s); obtain it from %s\n",
                    face->files[index].file, strerror(failure), face->source);
            return 1;
        }
        if (memcmp(installed, face->files[index].sha256, SHA256_DIGEST_SIZE) != 0) {
            fprintf(stderr,
                    "fontconv: %s does not match its manifest digest; refusing to convert\n",
                    face->files[index].file);
            return 1;
        }
    }

    for (index = 0U; index < face->file_count; index++) {
        if (!file_name || strcmp(face->files[index].file, file_name) == 0) {
            chosen = &face->files[index];
            break;
        }
    }
    if (!chosen) {
        fprintf(stderr, "fontconv: face '%s' declares no file named '%s'\n", face->id,
                file_name ? file_name : "");
        return 2;
    }
    if (face->file_count > 1U && !file_name) {
        fprintf(stderr,
                "fontconv: face '%s' declares %u files; converting '%s'. Use --file to choose "
                "another.\n",
                face->id, face->file_count, chosen->file);
    }
    if (!join_path(path, sizeof(path), fonts_dir, chosen->file)) return 2;

    if (FT_Init_FreeType(&library) != 0) {
        fprintf(stderr, "fontconv: FreeType initialisation failed\n");
        return 2;
    }
    if (FT_New_Face(library, path, 0, &ft_face) != 0) {
        fprintf(stderr, "fontconv: FreeType cannot open '%s'\n", path);
        FT_Done_FreeType(library);
        return 2;
    }
    /* Prefer the font's own 8x8 bitmap strike over rasterising its outline: the bitmap is what the
       designer drew, and rasterising would put a guess in front of a fact. */
    {
        int strike = -1;
        int candidate;
        for (candidate = 0; candidate < ft_face->num_fixed_sizes; candidate++) {
            if (ft_face->available_sizes[candidate].height == (short)pixel_size &&
                ft_face->available_sizes[candidate].y_ppem == (FT_Pos)(pixel_size * 64)) {
                strike = candidate;
                break;
            }
        }
        if (strike >= 0 && FT_Select_Size(ft_face, strike) == 0) {
            printf("path     embedded %ux%u bitmap strike (the font's own glyphs, not "
                   "re-rendered); fitting them into %ux%u\n",
                   pixel_size, pixel_size, FONT_FIT_CELL_SIZE, FONT_FIT_CELL_SIZE);
        } else if (FT_Set_Pixel_Sizes(ft_face, 0U, pixel_size) == 0) {
            printf("path     no %ux%u strike; rendering outlines at %u px, monochrome\n",
                   pixel_size, pixel_size, pixel_size);
        } else {
            fprintf(stderr,
                    "fontconv: '%s' offers neither a %ux%u strike nor a %u px size; it has %d "
                    "fixed strike(s)\n",
                    chosen->file, pixel_size, pixel_size, pixel_size, ft_face->num_fixed_sizes);
            for (candidate = 0; candidate < ft_face->num_fixed_sizes; candidate++) {
                fprintf(stderr, "          strike %d: %dx%d px\n", candidate,
                        ft_face->available_sizes[candidate].width,
                        ft_face->available_sizes[candidate].height);
            }
            if (candidate == 0) {
                fprintf(stderr, "          and no scalable outline either: not a glyph source for "
                                "this cell\n");
            }
            FT_Done_Face(ft_face);
            FT_Done_FreeType(library);
            return 1;
        }
    }

    /* Size the table from the charmap first: one pass to count, one to convert, so there is no
       reallocation and no dependence on growth behaviour. */
    for (codepoint = FT_Get_First_Char(ft_face, &glyph_index); glyph_index != 0U;
         codepoint = FT_Get_Next_Char(ft_face, codepoint, &glyph_index)) {
        capacity++;
    }
    entries = malloc((size_t)capacity * FONTCONV_SET_ENTRY_SIZE);
    if (!entries) {
        fprintf(stderr, "fontconv: cannot hold %u glyphs\n", capacity);
        FT_Done_Face(ft_face);
        FT_Done_FreeType(library);
        return 2;
    }

    for (codepoint = FT_Get_First_Char(ft_face, &glyph_index); glyph_index != 0U;
         codepoint = FT_Get_Next_Char(ft_face, codepoint, &glyph_index)) {
        FontFitMask mask;
        FontFitCell cell;
        FontFitReport report;
        uint8_t *entry = entries + (size_t)entry_count * FONTCONV_SET_ENTRY_SIZE;
        if (FT_Load_Glyph(ft_face, glyph_index,
                          FT_LOAD_MONOCHROME | FT_LOAD_RENDER | FT_LOAD_TARGET_MONO) != 0) {
            tally.unsupported++;
            continue;
        }
        if (!mask_from_bitmap(&ft_face->glyph->bitmap, &mask)) {
            tally.unsupported++;
            continue;
        }
        /* Advance width is reported, not decided: it is the input the width-class question
           (register B2) needs, and collecting it here costs nothing. */
        if (ft_face->glyph->advance.x >= (FT_Pos)(FONT_FIT_CELL_SIZE * 64)) {
            tally.full_width++;
        } else {
            tally.half_width++;
        }
        memset(entry, 0, FONTCONV_SET_ENTRY_SIZE);
        put_u32(entry, (uint32_t)codepoint);
        if (mask.width == 0U || mask.height == 0U) {
            /* Space and friends: a legitimate blank glyph, not a fit failure. */
            entry[4] = (uint8_t)FONT_FIT_MODE_NONE;
            tally.blank++;
            tally.mode_counts[FONT_FIT_MODE_NONE]++;
            entry_count++;
            continue;
        }
        if (font_fit_apply(&mask, mode, &cell, &report) != FONT_FIT_OK) {
            tally.refused++;
            if (tally.refused_reported < FONTCONV_MAX_REPORTED_REFUSALS) {
                char reason[192];
                if (font_fit_describe(&report, reason, sizeof(reason))) {
                    printf("  refused  U+%04lX %s\n", codepoint, reason);
                } else {
                    printf("  refused  U+%04lX\n", codepoint);
                }
                tally.refused_reported++;
            }
            continue;
        }
        entry[4] = (uint8_t)mode;
        memcpy(entry + 5U, cell.rows, FONT_FIT_CELL_SIZE);
        tally.converted++;
        tally.mode_counts[mode]++;
        entry_count++;
    }

    printf("glyphs   %u converted, %u blank, %u refused, %u unsupported\n", tally.converted,
           tally.blank, tally.refused, tally.unsupported);
    printf("advance  %u full-width, %u half-width\n", tally.full_width, tally.half_width);
    printf("fit      none %u, squash %u, scale %u, normalize %u\n",
           tally.mode_counts[FONT_FIT_MODE_NONE], tally.mode_counts[FONT_FIT_MODE_SQUASH],
           tally.mode_counts[FONT_FIT_MODE_SCALE], tally.mode_counts[FONT_FIT_MODE_NORMALIZE]);

    if (tally.refused > 0U || tally.unsupported > 0U) {
        /* D11: abort rather than fit silently, and say what to do about it. */
        if (tally.refused > 0U) {
            printf("stop     %u glyph(s) do not fit the %ux%u cell\n", tally.refused,
                   FONT_FIT_CELL_SIZE, FONT_FIT_CELL_SIZE);
            printf("         rerun with --fit squash (drops detail) or --fit scale (reduces the "
                   "whole glyph), or choose a font drawn for 8x8\n");
        }
        if (tally.unsupported > 0U) {
            printf("stop     %u glyph(s) were not 1bpp monochrome\n", tally.unsupported);
        }
        printf("         no glyph set written\n");
        exit_code = 1;
        goto done;
    }

    provenance_digest(face, provenance);
    if (!write_glyph_set(out_path, entries, entry_count, provenance)) {
        fprintf(stderr, "fontconv: cannot write '%s'\n", out_path);
        exit_code = 2;
        goto done;
    }
    {
        uint8_t digest[SHA256_DIGEST_SIZE];
        char set_digest[SHA256_HEX_SIZE];
        char provenance_hex[SHA256_HEX_SIZE];
        int failure = 0;
        if (!digest_file(out_path, digest, &failure)) {
            fprintf(stderr, "fontconv: wrote '%s' but cannot read it back (%s)\n", out_path,
                    strerror(failure));
            exit_code = 2;
            goto done;
        }
        sha256_to_hex(digest, set_digest);
        sha256_to_hex(provenance, provenance_hex);
        printf("set      %s (%u entries, %u bytes)\n", out_path, entry_count,
               (unsigned)(FONTCONV_SET_HEADER_SIZE + entry_count * FONTCONV_SET_ENTRY_SIZE));
        /* Both digests are printed so two machines can compare without moving the file: same
           source and same set digest is the reproducibility claim, checkable in one line. */
        printf("digest   %s\n", set_digest);
        printf("source   %s\n", provenance_hex);
    }

done:
    free(entries);
    FT_Done_Face(ft_face);
    FT_Done_FreeType(library);
    return exit_code;
}
#endif /* FONTCONV_HAVE_FREETYPE */



int main(int argc, char **argv) {
    const char *manifest_path = NULL;
    const char *fonts_dir = NULL;
    const char *convert_face_id = NULL;
    const char *out_path = NULL;
    const char *file_name = NULL;
    const char *fit_name = NULL;
    FontFitMode fit_mode = FONT_FIT_MODE_NONE;
    unsigned pixel_size = FONT_FIT_CELL_SIZE;
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
        } else if (strcmp(argv[argument], "--convert") == 0 && argument + 1 < argc) {
            convert_face_id = argv[++argument];
        } else if (strcmp(argv[argument], "--out") == 0 && argument + 1 < argc) {
            out_path = argv[++argument];
        } else if (strcmp(argv[argument], "--file") == 0 && argument + 1 < argc) {
            file_name = argv[++argument];
        } else if (strcmp(argv[argument], "--fit") == 0 && argument + 1 < argc) {
            fit_name = argv[++argument];
        } else if (strcmp(argv[argument], "--size") == 0 && argument + 1 < argc) {
            char *end = NULL;
            long parsed = strtol(argv[++argument], &end, 10);
            if (!end || *end != '\0' || parsed < 1L ||
                parsed > (long)FONT_FIT_SOURCE_MAX) {
                fprintf(stderr, "fontconv: --size must be 1..%u pixels\n",
                        FONT_FIT_SOURCE_MAX);
                return 2;
            }
            pixel_size = (unsigned)parsed;
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

    if (!manifest_path || (!list_fonts && !convert_face_id)) {
        if (!list_fonts && !convert_face_id) {
            fprintf(stderr, "fontconv: nothing to do without --list-fonts or --convert\n");
        }
        usage(stderr);
        return 2;
    }
    if (convert_face_id && !out_path) {
        fprintf(stderr, "fontconv: --convert needs --out PATH for the glyph set\n");
        return 2;
    }
    if (fit_name) {
        if (strcmp(fit_name, "none") == 0) {
            fit_mode = FONT_FIT_MODE_NONE;
        } else if (strcmp(fit_name, "squash") == 0) {
            fit_mode = FONT_FIT_MODE_SQUASH;
        } else if (strcmp(fit_name, "scale") == 0) {
            fit_mode = FONT_FIT_MODE_SCALE;
        } else if (strcmp(fit_name, "normalize") == 0) {
            fit_mode = FONT_FIT_MODE_NORMALIZE;
        } else {
            fprintf(stderr, "fontconv: unknown --fit '%s' (none, squash, scale, normalize)\n",
                    fit_name);
            return 2;
        }
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

    if (convert_face_id) {
        const FontManifestFace *face = font_manifest_find_face(&manifest, convert_face_id);
        if (!face) {
            fprintf(stderr, "fontconv: no face '%s' in %s\n", convert_face_id, manifest_path);
            return 2;
        }
#if FONTCONV_HAVE_FREETYPE
        printf("face     %s — %s\n", face->id, face->name);
        return convert_face(face, fonts_dir, file_name, fit_mode, pixel_size, out_path);
#else
        /* An absent font engine is a fact about this build, not about the font: say which, so the
           reader does not go hunting for a missing file. The converter's option values exist and
           are validated above; they are only meaningful on the branch that can act on them. */
        (void)fit_mode;
        (void)pixel_size;
        (void)file_name;
        fprintf(stderr,
                "fontconv: this build has no FreeType, so it can verify fonts but not convert "
                "them\n          install freetype2 (pkg-config freetype2) and rebuild: make "
                "fontconv\n");
        return 2;
#endif
    }

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
