/*
 * string_util.h — String helpers mirroring dist/util/string.js.
 *
 * All functions operate on owned or input strings.  Where possible they
 * return an offset/length pair rather than allocating a new string.
 */
#pragma once
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include "config.h"
#include "util/thread_buffer.h"

/* ── Sentinel marker formatting ───────────────────────────────────────────── */

/** Build a sentinel marker "\0<index><ch>\x7F" and return it in marker_buf (≥32 bytes). */
void work_str_sentinel(size_t index, char ch, char *marker_buf, size_t *marker_len);

/* ── tidy: remove \0 and \x7F from raw input ─────────────────────────────── */
/**
 * Return a newly-allocated copy of s with all '\0' and '\x7F' bytes removed.
 * Caller must free the result.
 */
char *str_tidy(const char *s, size_t len, size_t *out_len);

/**
 * Like str_tidy() but writes into a pre-allocated buffer owned by the caller.
 *
 * The buffer MUST already have capacity of at least len+1 bytes — the caller
 * is responsible for ensuring this via wiki_thread_buf_reserve() before
 * calling this function.  No allocation or reallocation is performed here.
 *
 * The result is null-terminated and its byte length is written to *out_len.
 */
void str_tidy_into(const char *s, size_t len,
                   char *buf, size_t cap,
                   size_t *out_len);

/* ── removeComment: remove half-parsed \0\d+[cn]\x7F tokens ─────────────── */
char *str_remove_comment(const char *s, size_t len, size_t *out_len);

/* ── trimLc: trim whitespace and lowercase ───────────────────────────────── */
char *str_trim_lc(const char *s, size_t len);

/* ── decodeHtmlBasic: decode basic HTML entities ────────────────────────── */
/**
 * Decode &lt; &gt; &amp; &quot; &lbrack; &rbrack; &lbrace; &rbrace; &nbsp;
 * and &#NNN; / &#xHHH; sequences.
 * Returns a newly-allocated string; caller must free.
 */
char *str_decode_html_basic(const char *s, size_t len, size_t *out_len);

/* ── restore: expand \0N\x7F back-references in a string ─────────────────── */
/**
 * Replaces \0<N>\x7F markers in s with stack[N].
 * stack is an array of char* with stack_count entries.
 * Returns a newly-allocated string; caller must free.
 */
/* out_len (optional) receives the actual byte length of the returned string,
 * which may differ from strlen() if the output contains embedded \0 bytes
 * from token-sentinel markers that were not matched by the link-stack pattern.
 *
 * stack_lengths: if non-NULL, stack_lengths[i] is the byte length of stack[i]
 *   (binary-safe, for entries that may contain embedded NUL bytes from sentinels).
 *   If NULL, strlen(stack[i]) is used (safe only when entries have no NUL bytes). */
char *str_restore(const char *s, size_t len,
                  const char **stack, size_t stack_count,
                  const size_t *stack_lengths,
                  size_t *out_len);

/* ── UTF-8 length of next codepoint starting at p ─────────────────────────── */
int utf8_char_len(unsigned char c);

/* ── Case-fold a single UTF-8 codepoint to lowercase (ASCII-only fast path) ─ */
uint32_t utf8_tolower_codepoint(uint32_t cp);

/* ── Uppercase a single UTF-8 codepoint (ASCII + Latin-1 support) ─────────── */
uint32_t utf8_toupper_codepoint(uint32_t cp);

/* ── String search helpers ─────────────────────────────────────────────────── */
/**
 * Case-insensitive strstr for ASCII-range needles.
 * Returns pointer to first occurrence in haystack or NULL.
 */
const char *str_istr(const char *haystack, size_t hlen,
                     const char *needle,   size_t nlen);

/**
 * Check whether the given string starts with a configured interwiki prefix.
 * If a prefix is found, returns a newly-allocated lowercased prefix string
 * and sets *consumed to the number of original bytes consumed by the full
 * match (prefix + optional whitespace + ':'). Caller must free the result.
 * Returns NULL and sets *consumed = 0 if no match.
 */
char *str_extract_interwiki(const char *s, size_t len, const ParserConfig *cfg, size_t *consumed);

/* ── Sentinel scanning helpers ─────────────────────────────────────────── */

static const char SENTINEL_TYPES[] = "exhbru";

typedef void (*SentinelScanCb)(size_t pos,
                                size_t total_len,
                                size_t n,
                                char   type_char,
                                void  *user_data);

void sentinel_scan(const char    *buf,
                   size_t         len,
                   SentinelScanCb cb,
                   void          *user_data);

bool sentinel_scan_next(const char *buf, size_t len, size_t *pos,
                        size_t *out_n, char *out_type, size_t *out_total_len);

/* ── URL scanning helpers (shared with external_links.c) ─────────────────── */

/**
 * Check if a byte is valid for the URL body (extUrlChar).
 * Mirrors JS extUrlChar: non-control, non-bracket, non-quote, non-Zs, non-FFFD.
 */
bool is_url_common_byte(unsigned char c);

/**
 * Match a protocol prefix from cfg->protocol_items against s.
 * Returns the length of the matched prefix, or 0 if no match.
 * Requires cfg->protocol_items_valid == true.
 */
size_t match_proto_prefix(const char *s, size_t len, const ParserConfig *cfg);
