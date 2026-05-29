/*
 * env_cache.c — Environment variable cache implementation
 *
 * Caches getenv() results to avoid repeated environment lookups.
 */

#include "util/env_cache.h"

#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stringzilla/stringzilla.h>

#define ENV_CACHE_INITIAL_CAP 16

typedef struct {
    char *name;
    size_t name_len;
    const char *value;  /* Points to getenv result, do NOT free */
    bool is_set;
} EnvCacheEntry;

static EnvCacheEntry *cache = NULL;
static size_t cache_count = 0;
static size_t cache_cap = 0;
static pthread_once_t cache_once = PTHREAD_ONCE_INIT;
static pthread_mutex_t cache_mutex = PTHREAD_MUTEX_INITIALIZER;

static void env_cache_init(void) {
    cache_cap = ENV_CACHE_INITIAL_CAP;
    cache = (EnvCacheEntry *)calloc(cache_cap, sizeof(EnvCacheEntry));
    if (!cache) {
        cache_cap = 0;
    }
}

static void ensure_initialized(void) {
    pthread_once(&cache_once, env_cache_init);
}

static const char *getenv_from_view(const char *name, size_t name_len) {
    if (!name) return NULL;
    if (name[name_len] == '\0') return getenv(name);

    char *tmp = (char *)malloc(name_len + 1);
    if (!tmp) return NULL;
    sz_copy(tmp, name, name_len);
    tmp[name_len] = '\0';
    const char *v = getenv(tmp);
    free(tmp);
    return v;
}

bool env_set(const char *name) {
    if (!name) return false;
    const char *val = env_get_n(name, strlen(name));
    return (val != NULL);
}

bool env_set_n(const char *name, size_t name_len) {
    const char *val = env_get_n(name, name_len);
    return (val != NULL);
}

const char *env_get(const char *name) {
    if (!name) return NULL;
    return env_get_n(name, strlen(name));
}

const char *env_get_n(const char *name, size_t name_len) {
    if (!name) return NULL;

    ensure_initialized();
    if (!cache) {
        return getenv_from_view(name, name_len);
    }

    /* Fast path: unlocked check for existing entry.
     * This is safe because entries are only added, never modified after insertion.
     * The worst case is a false negative (missed cache hit) which falls through to the locked path. */
    for (size_t i = 0; i < cache_count; i++) {
        if (cache[i].name && cache[i].name_len == name_len &&
            sz_equal(cache[i].name, name, name_len)) {
            return cache[i].is_set ? cache[i].value : NULL;
        }
    }

    /* Not found in unlocked scan, acquire mutex and retry (double-check) */
    pthread_mutex_lock(&cache_mutex);

    /* Re-check after acquiring lock (another thread may have inserted while we were waiting) */
    for (size_t i = 0; i < cache_count; i++) {
        if (cache[i].name && cache[i].name_len == name_len &&
            sz_equal(cache[i].name, name, name_len)) {
            const char *result = cache[i].is_set ? cache[i].value : NULL;
            pthread_mutex_unlock(&cache_mutex);
            return result;
        }
    }

    /* Not found, create new entry */
    if (cache_count >= cache_cap) {
        size_t new_cap = cache_cap * 2;
        EnvCacheEntry *new_cache = (EnvCacheEntry *)realloc(cache, new_cap * sizeof(EnvCacheEntry));
        if (!new_cache) {
            pthread_mutex_unlock(&cache_mutex);
            return getenv_from_view(name, name_len);
        }
        cache = new_cache;
        cache_cap = new_cap;
    }

    const char *val = NULL;
    char *owned_name = NULL;
    if (name[name_len] == '\0') {
        val = getenv(name);
        owned_name = strdup(name);
    } else {
        owned_name = (char *)malloc(name_len + 1);
        if (owned_name) {
            sz_copy(owned_name, name, name_len);
            owned_name[name_len] = '\0';
            val = getenv(owned_name);
        }
    }
    if (!owned_name) {
        pthread_mutex_unlock(&cache_mutex);
        return getenv_from_view(name, name_len);
    }
    cache[cache_count].name = owned_name;
    cache[cache_count].name_len = name_len;
    cache[cache_count].value = val;
    cache[cache_count].is_set = (val != NULL && val[0] != '\0');
    const char *result = cache[cache_count].is_set ? cache[cache_count].value : NULL;
    cache_count++;

    pthread_mutex_unlock(&cache_mutex);
    return result;
}

void env_cache_clear(void) {
    pthread_mutex_lock(&cache_mutex);

    for (size_t i = 0; i < cache_count; i++) {
        free(cache[i].name);
    }
    cache_count = 0;

    pthread_mutex_unlock(&cache_mutex);
}
