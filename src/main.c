/**
 * main.c — Program entry point
 *
 * The simplest file in the project: just calls app_main() with the
 * command-line arguments and returns its exit code.  All initialisation,
 * the main loop, and cleanup are handled by app_main() in app.c.
 *
 * This thin wrapper allows the build system to have a clear entry
 * point while keeping the application logic self-contained in app.c.
 */

#include "app.h"        /* app_main() — the real entry point */

/**
 * main() — Standard C entry point
 *
 * Forwards to app_main() which handles:
 *   - Configuration loading (config.ini + CLI args)
 *   - Renderer, grid, asset, map, and camera initialisation
 *   - The main frame loop
 *   - Benchmark/stability-test modes
 *   - Cleanup and JSON result output
 *
 * @param argc  Argument count
 * @param argv  Argument vector
 * @return      Exit code (0 = success, 1 = failure/benchmark-fail)
 */
int main(int argc, char* argv[]) {
    return app_main(argc, argv);
}