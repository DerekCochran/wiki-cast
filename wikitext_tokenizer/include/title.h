/*
 * title.h — Title normalization and validity checking.
 *
 * Mirrors dist/lib/title.js (halfParsed mode used by parseRedirect).
 */
#pragma once
#include "config.h"
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    char  *main;       /* main title part (after namespace, decoded) */
    char  *prefix;     /* namespace prefix or "" */
    char  *fragment;   /* #fragment part or NULL */
    int    ns;         /* namespace number */
    bool   valid;      /* true if title is acceptable */
} Title;

/**
 * Check title validity in halfParsed mode (matches JS Title constructor
 * with { halfParsed: true, decode: true, page: '' }).
 *
 * @param raw    Raw title string (e.g. "Target Page" from [[Target Page|...]]).
 * @param raw_len Byte length of raw.
 * @param cfg    Parser config for namespace lookup.
 * @return       true if the title is valid for a redirect target.
 */
bool title_is_valid_half_parsed(const char *raw, size_t raw_len,
                                const ParserConfig *cfg);

/**
 * Normalize a title (spaces → underscores, capitalize first letter).
 * Returns a newly-allocated string; caller must free.
 */
char *title_normalize(const char *raw, size_t raw_len);

/**
 * Free a Title struct (and its owned strings).
 */
void title_free(Title *t);
