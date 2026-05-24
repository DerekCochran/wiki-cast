/* brace_pat.h — simple scanner for -{ and }- brace patterns */
#pragma once
#include <stddef.h>

typedef enum {
    BRACE_PAT_OPEN = 0,
    BRACE_PAT_CLOSE = 1,
} BracePatKind;

typedef void (*BracePatCb)(BracePatKind kind, size_t pos, void *user_data);

/* Scan `buf` for all occurrences of "-{" and "}-" in left-to-right order.
 * For each match call `cb(kind, pos, user_data)` where `pos` is the match
 * byte offset in `buf`.
 */
void brace_pat_scan(const char *buf, size_t len, BracePatCb cb, void *user_data);
