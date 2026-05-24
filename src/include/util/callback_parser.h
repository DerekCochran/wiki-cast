#pragma once
#include <stdbool.h>
#include <stddef.h>

typedef enum {
    PARSER_MATCH_FIRST_CLOSE = 0,    /* sz_find  -- regex-equivalent (default) */
    PARSER_MATCH_OUTERMOST_CLOSE,    /* sz_rfind -- greedy                     */
    PARSER_MATCH_INNERMOST,          /* forward scan with nested-opener probe  */
} ParserMatchMode;

typedef enum {
    PARSER_SEG_TEXT          = 0,    /* plain text outside (or rejected) delimiters */
    PARSER_SEG_INNER         = 1,    /* inner content of a paired open...close      */
    PARSER_SEG_SELF_CLOSING  = 2,    /* self-closing match; segment = full opener   */
} ParserSegmentKind;

typedef struct {
    /* === Delimiters === */
    const char *open_delim;
    size_t      open_len;
    const char *close_delim;
    size_t      close_len;

    /* === Closer-selection strategy === */
    ParserMatchMode match_mode;

    /* === Variable-length opener (e.g. <ref name="foo">) ===
       When `open_terminator != 0`, the full opener is interpreted as:
         open_delim  +  zero-or-more bytes (none in `open_attr_forbidden`)
                     +  open_terminator
       Set open_terminator='>', open_attr_forbidden="<", open_attr_forbidden_len=1
       for HTML-style tags. The matched opener span (prefix + attrs + terminator)
       is what gets skipped/emitted.  When 0 (default) the opener is fixed-string. */
    char        open_terminator;
    const char *open_attr_forbidden;
    size_t      open_attr_forbidden_len;

    /* === Self-closing variant (e.g. <br />, <ref name="x" />) ===
       Only meaningful when open_terminator != 0. If the byte sequence
       immediately preceding the open_terminator equals `self_closing_marker`,
       the opener is emitted as PARSER_SEG_SELF_CLOSING and the closer search
       is skipped. Set self_closing_marker="/" (len=1) for HTML "/>". */
    const char *self_closing_marker;
    size_t      self_closing_marker_len;

    /* === Variable-length closer (e.g. </ref >) ===
       Same shape as variable-length opener. Most HTML tags use a fixed closer
       (e.g. exactly "</ref>") so this is rarely needed; provided for symmetry. */
    char        close_terminator;
    const char *close_attr_forbidden;
    size_t      close_attr_forbidden_len;

    /* === Case-insensitive opener/closer matching ===
       When true, opener and closer searches use sz_utf8_case_insensitive_find
       instead of sz_find. Useful for HTML where `<NoWiki>` == `<nowiki>`. */
    bool case_insensitive;

    /* === Line-anchored opener/closer ===
       When true, the delimiter only matches at start-of-buffer or immediately
       after '\n'. Useful for `\n{|`, `\n|}` (tables), `\n=` (headings),
       `\n*` / `\n#` / `\n:` (lists). The newline is NOT part of the matched
       span (callers see the same delimiter bytes whether anchored or not). */
    bool line_anchored_open;
    bool line_anchored_close;

    /* === Inner-content rejection: byteset === */
    const char *prohibited_chars;
    size_t      prohibited_chars_len;

    /* === Inner-content rejection: substring patterns ===
       Lengths are explicit so embedded NULs are supported (the "\n\0" rule). */
    const char * const *prohibited_patterns;
    const size_t       *prohibited_pattern_lens;
    size_t              prohibited_patterns_count;

    /* === Lookbehind/lookahead === */
    char no_preceding_byte;     /* opener must NOT be preceded by this byte */
    char no_following_byte;     /* closer must NOT be followed by this byte */
} ParserRules;

/**
 * Callback fired for every segment of the buffer.
 *
 * @param segment   Pointer into the input buffer.
 * @param len       Length of the segment in bytes.
 * @param kind      PARSER_SEG_TEXT          -- plain text; concatenating all TEXT
 *                                             and unaccepted opener/closer spans
 *                                             reproduces the input minus only
 *                                             the open/close delimiters of
 *                                             accepted matches.
 *                  PARSER_SEG_INNER         -- inner content of an accepted
 *                                             paired match; open_delim and
 *                                             close_delim are NOT included.
 *                  PARSER_SEG_SELF_CLOSING  -- the entire matched opener span
 *                                             of a self-closing tag (e.g. the
 *                                             literal bytes "<br />"). Inner
 *                                             content is empty by definition.
 * @param user_data Opaque caller pointer.
 */
typedef void (*ParserCallback)(const char       *segment,
                              size_t            len,
                              ParserSegmentKind  kind,
                              void             *user_data);

/** Single pass over the buffer. */
void parser_scan(const char       *buf,
                size_t            len,
                const ParserRules *rules,
                ParserCallback     cb,
                void             *user_data);

/**
 * Fixpoint driver: invokes the caller's per-pass routine repeatedly until the
 * caller's mutable buffer stops changing. Required to faithfully reproduce the
 * original `while(1) ... if (prev == out) break;` loop in `parse_simple_args`:
 * the regex forbids `{` inside content, so nested templates can only collapse
 * across multiple passes.
 */
typedef void (*ParserPassFn)(void *user_data);
void parser_scan_until_stable(void                  *tb,
                             ParserPassFn            run_pass,
                             void                  *user_data,
                             const char *(*get_buf)(void *tb),
                             size_t      (*get_len)(void *tb));

/* ── Result-based (non-callback) API ─────────────────────────────────────── */

/**
 * One segment result — the same data that would be handed to a ParserCallback.
 */
typedef struct {
    const char       *segment; /* pointer into the input buffer; not owned */
    size_t            len;
    ParserSegmentKind kind;
} ParserResult;

/** Growable array of ParserResult. */
typedef struct {
    ParserResult *items;
    size_t        count;
    size_t        cap;
} ParserResultArray;

/** Zero-initialise a result array before first use. */
void parser_result_array_init(ParserResultArray *arr);

/** Free the backing store.  The `segment` pointers inside are NOT freed
 *  (they refer into the caller's input buffer). */
void parser_result_array_free(ParserResultArray *arr);

/** Reset count to 0 without freeing the backing store. */
static inline void parser_result_array_clear(ParserResultArray *arr) {
    if (arr) arr->count = 0;
}

/**
 * Non-callback equivalent of parser_scan.  Appends one ParserResult per
 * segment to `out`; existing contents are preserved (use
 * parser_result_array_clear first if you want a fresh collection).
 *
 * The `segment` pointers in the results point into `buf` and are valid for
 * as long as `buf` is not freed or reallocated.
 */
void parser_scan_collect(const char        *buf,
                         size_t             len,
                         const ParserRules *rules,
                         ParserResultArray *out);

/**
 * Non-callback equivalent of parser_scan_until_stable.
 *
 * `run_pass` receives both the opaque user-data pointer and a pointer to the
 * result array.  A typical implementation:
 *   1. parser_result_array_clear(out)                   -- discard old results
 *   2. parser_scan_collect(old_buf, old_len, rules, out) -- capture new ones
 *   3. transform tb as usual (wiki_thread_buf_set / etc.)
 *
 * After convergence, `out` holds results from the final (stable) pass.  Their
 * `segment` pointers remain valid because the last pass left the buffer
 * unchanged (by definition of convergence).
 */
typedef void (*ParserPassCollectFn)(void *user_data, ParserResultArray *out);

void parser_collect_until_stable(void                   *tb,
                                 ParserPassCollectFn     run_pass,
                                 void                  *user_data,
                                 const char *(*get_buf)(void *tb),
                                 size_t      (*get_len)(void *tb),
                                 ParserResultArray      *out);

