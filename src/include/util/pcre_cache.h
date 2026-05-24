#ifndef PCRE_CACHE_H
#define PCRE_CACHE_H

#include <pcre2.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Simple process-global PCRE2 compilation cache.
 *
 * Callers provide a stable name and the pattern+compile flags. The cache
 * compiles the pattern on first use and returns the compiled
 * `pcre2_code *`. The cache owns the compiled objects and will free them
 * at process exit (or when `pcre_cache_free_all()` is called).
 */

/* Get a compiled pattern from the cache keyed by (pattern text, compile_options).
 * On failure this function logs an error and aborts the process. */
pcre2_code *pcre_cache_get(const char *pattern, uint32_t compile_options);
void pcre_cache_free_all(void);

#ifdef __cplusplus
}
#endif

#endif
