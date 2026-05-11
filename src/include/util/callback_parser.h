#pragma once
#include <stdbool.h>
#include <stddef.h>

typedef enum {
    LEXER_MATCH_FIRST_CLOSE = 0,    /* sz_find  -- regex-equivalent (default) */
    LEXER_MATCH_OUTERMOST_CLOSE,    /* sz_rfind -- greedy                     */
    LEXER_MATCH_INNERMOST,          /* forward scan with nested-opener probe  */
} LexerMatchMode;

typedef enum {
    LEXER_SEG_TEXT          = 0,    /* plain text outside (or rejected) delimiters */
    LEXER_SEG_INNER         = 1,    /* inner content of a paired open...close      */
    LEXER_SEG_SELF_CLOSING  = 2,    /* self-closing match; segment = full opener   */
} LexerSegmentKind;

typedef struct {
    /* === Delimiters === */
    const char *open_delim;
    size_t      open_len;
    const char *close_delim;
    size_t      close_len;

    /* === Closer-selection strategy === */
    LexerMatchMode match_mode;

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
       the opener is emitted as LEXER_SEG_SELF_CLOSING and the closer search
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
} LexerRules;

/**
 * Callback fired for every segment of the buffer.
 *
 * @param segment   Pointer into the input buffer.
 * @param len       Length of the segment in bytes.
 * @param kind      LEXER_SEG_TEXT          -- plain text; concatenating all TEXT
 *                                             and unaccepted opener/closer spans
 *                                             reproduces the input minus only
 *                                             the open/close delimiters of
 *                                             accepted matches.
 *                  LEXER_SEG_INNER         -- inner content of an accepted
 *                                             paired match; open_delim and
 *                                             close_delim are NOT included.
 *                  LEXER_SEG_SELF_CLOSING  -- the entire matched opener span
 *                                             of a self-closing tag (e.g. the
 *                                             literal bytes "<br />"). Inner
 *                                             content is empty by definition.
 * @param user_data Opaque caller pointer.
 */
typedef void (*LexerCallback)(const char       *segment,
                              size_t            len,
                              LexerSegmentKind  kind,
                              void             *user_data);

/** Single pass over the buffer. */
void lexer_scan(const char       *buf,
                size_t            len,
                const LexerRules *rules,
                LexerCallback     cb,
                void             *user_data);

/**
 * Fixpoint driver: invokes the caller's per-pass routine repeatedly until the
 * caller's mutable buffer stops changing. Required to faithfully reproduce the
 * original `while(1) ... if (prev == out) break;` loop in `parse_simple_args`:
 * the regex forbids `{` inside content, so nested templates can only collapse
 * across multiple passes.
 */
typedef void (*LexerPassFn)(void *user_data);
void lexer_scan_until_stable(void                  *tb,
                             LexerPassFn            run_pass,
                             void                  *user_data,
                             const char *(*get_buf)(void *tb),
                             size_t      (*get_len)(void *tb));

