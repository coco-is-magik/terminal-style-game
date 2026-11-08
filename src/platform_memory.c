#include "platform_memory.h"

#include <SDL3/SDL_cpuinfo.h>

size_t platform_memory_installed_bytes(void) {
    const int megabytes = SDL_GetSystemRAM();
    if (megabytes <= 0) return 0U;
    return (size_t)megabytes * 1024U * 1024U;
}
