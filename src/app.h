/**
 * app.h — Application entry point
 *
 * This header declares the single function that bootstraps the entire
 * engine.  app_main() is called by main() in main.c and handles all
 * initialisation, the main loop, benchmark modes, and cleanup.
 *
 * See app.c for the full implementation.
 */

#ifndef APP_H
#define APP_H

/**
 * app_main() — Top-level application entry point
 *
 * Initialises the renderer, grid, assets, map, camera, and input
 * subsystems, then runs the main frame loop until quit is requested.
 *
 * Supports three run modes (configurable via CLI arguments):
 *   - Normal interactive mode (default)
 *   - Stress benchmark mode  (--benchmark-stress <sec>)
 *   - Stability test mode    (--stability-test <sec>)
 *
 * @param argc  Argument count (from main())
 * @param argv  Argument vector (from main())
 * @return      0 on success, 1 on failure/benchmark-fail
 */
int app_main(int argc, char* argv[]);

#endif /* APP_H */