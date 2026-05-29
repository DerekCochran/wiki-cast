/*
 * env_cache.h — Environment variable cache
 *
 * Caches getenv() results to avoid repeated environment lookups.
 * Thread-safe via pthread_once for initialization.
 */

#ifndef ENV_CACHE_H
#define ENV_CACHE_H

#include <stdbool.h>
#include <stddef.h>

/*
 * env_set — Check if an environment variable is set (exists and non-empty)
 *
 * @name: Environment variable name (e.g., "MY_VAR")
 * Returns: true if set and non-empty, false otherwise
 *
 * Results are cached after the first lookup.
 */
bool env_set(const char *name);
bool env_set_n(const char *name, size_t name_len);

/*
 * env_get — Get the cached value of an environment variable
 *
 * @name: Environment variable name
 * Returns: The value if set and non-empty, NULL otherwise
 *
 * Results are cached after the first lookup.
 */
const char *env_get(const char *name);
const char *env_get_n(const char *name, size_t name_len);

/*
 * env_cache_clear — Clear all cached entries (mainly for testing)
 */
void env_cache_clear(void);

#endif /* ENV_CACHE_H */
