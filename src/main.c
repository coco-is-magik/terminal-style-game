#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <enet/enet.h>
#include <stdio.h>

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    printf("Initializing SDL3...\n");
    if (!SDL_Init(SDL_INIT_EVENTS)) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    printf("Initializing SDL3_mixer...\n");
    if (!MIX_Init()) {
        fprintf(stderr, "MIX_Init failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    printf("Initializing ENet...\n");
    if (enet_initialize() != 0) {
        fprintf(stderr, "enet_initialize failed\n");
        MIX_Quit();
        SDL_Quit();
        return 1;
    }

    printf("All libraries initialized successfully!\n");

    enet_deinitialize();
    MIX_Quit();
    SDL_Quit();

    return 0;
}
