#pragma once
#include "stringzilla/small_string.h"

/* ── String list ─────────────────────────────────────────────────────────── */
typedef struct {
    sz_string_t *items;
    size_t  count;
} StrList;

/* ── String map (key->value pairs) ─────────────────────────────────────── */
typedef struct {
    sz_string_t *keys;
    sz_string_t *values;
    size_t       count;
} StrMap;

