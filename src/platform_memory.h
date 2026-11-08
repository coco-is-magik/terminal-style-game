/** platform_memory.h — Installed-memory query for load-time capacity policy. */
#ifndef PLATFORM_MEMORY_H
#define PLATFORM_MEMORY_H

#include <stddef.h>

/* Installed physical memory in bytes, or 0 when the platform cannot report it.
   Callers must treat 0 as "unknown" and fall back to their own policy rather than
   inventing a number. This module owns the platform query only; it holds no policy
   and reads no configuration. */
size_t platform_memory_installed_bytes(void);

#endif /* PLATFORM_MEMORY_H */
