#ifndef CONFIG_H
#define CONFIG_H

extern int WINDOW_WIDTH;
extern int WINDOW_HEIGHT;
extern int GRID_WIDTH;
extern int GRID_HEIGHT;
extern int CELL_WIDTH;
extern int CELL_HEIGHT;
extern int TARGET_FPS;

typedef enum {
    RUN_MODE_NORMAL,
    RUN_MODE_BENCHMARK_STRESS,
    RUN_MODE_STABILITY
} RunMode;

typedef enum {
    VISUAL_NORMAL,
    VISUAL_STRESS,
    VISUAL_RAYCAST
} VisualMode;

#endif
