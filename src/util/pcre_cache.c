#define PCRE2_CODE_UNIT_WIDTH 8
#include "util/pcre_cache.h"
#include "log.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef struct {
    char *pattern;
    pcre2_code *re;
    uint32_t flags;
} PcreEntry;

static PcreEntry *g_entries = NULL;
static size_t g_count = 0;
static size_t g_cap = 0;

static int find_index_by_pattern_and_flags(const char *pattern, uint32_t flags) {
    if(!pattern) return -1;
    for(size_t i = 0; i < g_count; i++) {
        if(g_entries[i].pattern && g_entries[i].flags == flags && strcmp(g_entries[i].pattern, pattern) == 0) return (int)i;
    }
    return -1;
}

pcre2_code *pcre_cache_get(const char *pattern, uint32_t compile_options) {
    if(!pattern) {
        log_error("pcre_cache_get: invalid pattern argument");
        abort();
    }

    int idx = find_index_by_pattern_and_flags(pattern, compile_options);
    if(idx >= 0) return g_entries[idx].re;

    /* compile */
    PCRE2_SIZE err_offset;
    int err_code;
    pcre2_code *re = pcre2_compile((PCRE2_SPTR)pattern, PCRE2_ZERO_TERMINATED,
                                    compile_options, &err_code, &err_offset, NULL);
    if(!re) {
        PCRE2_UCHAR8 err_buf[256];
        pcre2_get_error_message(err_code, err_buf, sizeof(err_buf));
        log_error("pcre_cache: compile error at %zu: %s Pattern: %s",
                  (size_t)err_offset, (const char *)err_buf, pattern);
        abort();
    }

    /* ensure capacity */
    if(g_count + 1 > g_cap) {
        size_t newcap = g_cap ? g_cap * 2 : 8;
        PcreEntry *tmp = realloc(g_entries, newcap * sizeof(PcreEntry));
        if(!tmp) {
            pcre2_code_free(re);
            log_error("pcre_cache: out of memory while expanding cache");
            abort();
        }
        g_entries = tmp;
        g_cap = newcap;
    }

    g_entries[g_count].pattern = strdup(pattern);
    if(!g_entries[g_count].pattern) {
        pcre2_code_free(re);
        log_error("pcre_cache: out of memory while storing pattern");
        abort();
    }
    g_entries[g_count].re = re;
    g_entries[g_count].flags = compile_options;
    g_count++;

    return re;
}

void pcre_cache_free_all(void) {
    if(!g_entries) return;
    for(size_t i = 0; i < g_count; i++) {
        if(g_entries[i].re) pcre2_code_free(g_entries[i].re);
        free(g_entries[i].pattern);
    }
    free(g_entries);
    g_entries = NULL;
    g_count = 0;
    g_cap = 0;
}

/* Ensure cache is freed on process exit. */
__attribute__((destructor))
static void pcre_cache_destroy(void) {
    pcre_cache_free_all();
}
