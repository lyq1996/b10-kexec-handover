#ifndef KJUMP_LZMA_COMPAT_H
#define KJUMP_LZMA_COMPAT_H

#include <stddef.h>   /* size_t */

#define WATCHDOG_RESET() do { } while (0)

void *memcpy(void *dest, const void *src, size_t n);

#endif /* KJUMP_LZMA_COMPAT_H */
