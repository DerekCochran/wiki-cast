# Proposal: Generic High-Performance Delimited Scanner (`util/callback_parser.c`)

## Overview

The current wikitext parsing implementation relies heavily on PCRE2 regular expressions for identifying delimited content (`{{...}}`, `{{{...}}}`, `[[...]]`, `-{...}-`, `<ref ...>...</ref>`, `<!--...-->`, etc.). PCRE2 is powerful but introduces significant overhead in the hot paths of the parser.

This proposal introduces `src/util/callback_parser.{h,c}`: a generic, callback-driven, SIMD-accelerated scanner for delimited regions, built on **stringzilla** primitives. A single configurable `ParserRules` struct expresses every delimited construct in MediaWiki wikitext — fixed-string delimiters, variable-length HTML-style openers, self-closing tags, line-anchored constructs, case-insensitive matching, and arbitrary inner-content rejection rules.

## Motivation

1. **Performance**: stringzilla's runtime-dispatched SIMD backends (Skylake AVX-512, Haswell AVX2, Westmere SSE4.2, NEON, SVE, SWAR) replace per-byte automata stepping. No PCRE2 setup, no match-data allocation, no compiled-pattern cache.
2. **Reusability**: one scanner serves `braces.c`, `links.c`, `tables.c`, `parser_tags.c`, `comments.c`, and the entity expander.
3. **Code cleanliness**: specialized parsers no longer carry their own scanning loops, `realloc` choreography, or fixpoint plumbing.

## Faithful Translation of the Original Regex

The reference regex this parser must reproduce is:

```text
(?<!\{)\{\{\{((?:[^\n{}\[]|\[(?!\[)|\n(?![\x00]))*)\}\}\}(?!\})
```

| Regex feature | Parser mechanism |
| --- | --- |
| `(?<!\{)` — opener cannot be preceded by `{` | `no_preceding_byte` check on the byte before each candidate opener |
| `(?!\})` — closer cannot be followed by `}` | `no_following_byte` check on the byte after each candidate closer |
| Inner must not contain `{`, `}`, or `[[`; `\n` allowed unless followed by `\x00` | `prohibited_chars` (byteset scan via `sz_find_byte_from`) + `prohibited_patterns` (substring scan via `sz_find`, length-based so embedded NULs work) |
| Outer fixpoint loop — required for nested templates because the regex itself forbids `{` inside content | `parser_scan_until_stable` re-runs `parser_scan` until the buffer stops changing |

The parser **does not** depth-count `{{{...}}}` like a balanced bracket parser. The regex doesn't either — it forbids `{` inside the inner content, which makes nesting a fixpoint phenomenon: inner templates resolve first, their sentinels let outer templates match on the next pass.

## Match Modes (closer-selection strategy)

| Mode | Closer chosen | When to use | Stringzilla primitive |
| --- | --- | --- | --- |
| `PARSER_MATCH_FIRST_CLOSE` (default) | First closer after the opener. | Regex-equivalent. Correct whenever the inner content forbids the opener's first byte. Pair with the fixpoint driver to collapse outer levels. Right mode for `{{{...}}}`, `{{...}}`, `[[...]]`, `<!--...-->`. | `sz_find` |
| `PARSER_MATCH_OUTERMOST_CLOSE` | Last closer in the buffer. | Niche: greedy heredoc/fence-style delimiters where you want the last closer in the buffer. **`no_following_byte` is ignored** here. | `sz_rfind` |
| `PARSER_MATCH_INNERMOST` | First closer that has no nested opener between it and the candidate opener. | Grammars where inner content may legitimately contain the opener and there is no rejection signal to put in `prohibited_chars`. | `sz_find` (twice per candidate) |

In wikitext, `FIRST_CLOSE` + an opener-byte rejection rule + fixpoint achieves the innermost-first resolution that nested templates need, more cheaply than `INNERMOST`.

## The Interface (`src/util/callback_parser.h`)

```c
#ifndef WIKI_UTIL_CALLBACK_PARSER_H_
#define WIKI_UTIL_CALLBACK_PARSER_H_

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

#endif /* WIKI_UTIL_CALLBACK_PARSER_H_ */
```

## Full Implementation (`src/util/callback_parser.c`)

```c
#include "callback_parser.h"

#include <string.h>
#include <stringzilla/stringzilla.h>

/* ------------------------------------------------------------------------- */
/* Substring search dispatch (case-sensitive vs. case-insensitive).          */
/*                                                                           */
/* sz_utf8_case_insensitive_find takes an inout needle_metadata struct (zero-*/
/* initialized => kernel_id == sz_utf8_case_rune_unknown_k, which triggers   */
/* lazy needle analysis on first call) and an out matched_length. For the    */
/* ASCII needles used by every wikitext rule, matched_length always equals   */
/* n_len, so callers can ignore it. We allocate a fresh metadata struct on   */
/* each call: that costs one stack frame and a one-shot needle scan, which   */
/* is acceptable because needles are short fixed strings (<= ~12 bytes) and  */
/* the fixpoint driver doesn't re-enter this path per byte.                  */
/* ------------------------------------------------------------------------- */

static inline const char *parser_find_(const char *h, size_t h_len,
                                      const char *n, size_t n_len,
                                      bool case_insensitive) {
    if (n_len == 0 || h_len < n_len) return NULL;
    if (case_insensitive) {
        sz_utf8_case_insensitive_needle_metadata_t md = {0};
        sz_size_t matched_length = 0;
        return (const char *)sz_utf8_case_insensitive_find(
            h, h_len, n, n_len, &md, &matched_length);
    }
    return (const char *)sz_find(h, h_len, n, n_len);
}

/* ------------------------------------------------------------------------- */
/* Variable-length opener matching: open_delim + attrs + open_terminator     */
/* On success returns pointer one byte past the terminator and writes the    */
/* full span length into *opener_span_out.                                   */
/* If self-closing, sets *is_self_closing_out = true.                        */
/* ------------------------------------------------------------------------- */

static const char *parser_match_var_opener_(const char *open_start,
                                           const char *end,
                                           const ParserRules *r,
                                           size_t *opener_span_out,
                                           bool   *is_self_closing_out) {
    *is_self_closing_out = false;

    /* Cursor sits just past the fixed prefix. */
    const char *cur = open_start + r->open_len;
    if (cur >= end) return NULL;

    size_t attr_max = (size_t)(end - cur);
    const char *term = NULL;

    /* Locate the opener terminator while rejecting forbidden bytes. We do
       this in two SIMD calls: forbidden-byte scan, then terminator-byte scan,
       and pick whichever is earlier. If forbidden wins, the candidate is
       invalid. */
    const char *forbidden_hit = NULL;
    if (r->open_attr_forbidden && r->open_attr_forbidden_len > 0) {
        forbidden_hit = (const char *)sz_find_byte_from(
            cur, attr_max,
            r->open_attr_forbidden, r->open_attr_forbidden_len);
    }
    term = (const char *)sz_find_byte(cur, attr_max,
                                      &r->open_terminator);
    if (!term) return NULL;
    if (forbidden_hit && forbidden_hit < term) return NULL;

    /* Self-closing detection: do the bytes immediately before `term` equal
       self_closing_marker? */
    if (r->self_closing_marker && r->self_closing_marker_len > 0 &&
        (size_t)(term - cur) >= r->self_closing_marker_len) {
        const char *m = term - r->self_closing_marker_len;
        if (memcmp(m, r->self_closing_marker,
                   r->self_closing_marker_len) == 0) {
            *is_self_closing_out = true;
        }
    }

    *opener_span_out = (size_t)(term + 1 - open_start);
    return term + 1;
}

/* ------------------------------------------------------------------------- */
/* Same for variable-length closer.                                          */
/* ------------------------------------------------------------------------- */

static const char *parser_match_var_closer_(const char *close_start,
                                           const char *end,
                                           const ParserRules *r,
                                           size_t *closer_span_out) {
    const char *cur = close_start + r->close_len;
    if (cur >= end) return NULL;
    size_t attr_max = (size_t)(end - cur);

    const char *forbidden_hit = NULL;
    if (r->close_attr_forbidden && r->close_attr_forbidden_len > 0) {
        forbidden_hit = (const char *)sz_find_byte_from(
            cur, attr_max,
            r->close_attr_forbidden, r->close_attr_forbidden_len);
    }
    const char *term = (const char *)sz_find_byte(cur, attr_max,
                                                  &r->close_terminator);
    if (!term) return NULL;
    if (forbidden_hit && forbidden_hit < term) return NULL;

    *closer_span_out = (size_t)(term + 1 - close_start);
    return term + 1;
}

/* ------------------------------------------------------------------------- */
/* Inner-content validation.                                                 */
/* ------------------------------------------------------------------------- */

static bool inner_is_invalid_(const char *inner, size_t inner_len,
                              const ParserRules *r) {
    if (r->prohibited_chars && r->prohibited_chars_len > 0) {
        if (sz_find_byte_from(inner, inner_len,
                              r->prohibited_chars,
                              r->prohibited_chars_len))
            return true;
    }
    for (size_t i = 0; i < r->prohibited_patterns_count; ++i) {
        const char *pat  = r->prohibited_patterns[i];
        size_t      plen = r->prohibited_pattern_lens[i];
        if (plen == 0) continue;
        if (sz_find(inner, inner_len, pat, plen)) return true;
    }
    return false;
}

/* ------------------------------------------------------------------------- */
/* Line-anchor check: does this position start a line?                       */
/* ------------------------------------------------------------------------- */

static inline bool at_line_start_(const char *buf, const char *p) {
    return p == buf || p[-1] == '\n';
}

/* ------------------------------------------------------------------------- */
/* Locate a closer per match_mode.                                           */
/* Returns pointer to first byte of the closer fixed prefix, or NULL.        */
/*                                                                           */
/* `buf` is the original buffer base, used for the line-anchor check.        */
/* `open_in` / `inner_start_in` are the candidate opener and the byte just   */
/* past it. INNERMOST mode may descend to a nested opener; it writes the     */
/* shifted opener position to *open_out / *inner_start_out so the caller     */
/* can emit the prefix bytes as TEXT and reposition. Other modes leave the   */
/* out-params untouched (the caller initializes them to the originals).      */
/* ------------------------------------------------------------------------- */

static const char *find_closer_(const char *buf,
                                const char *open_in,
                                const char *inner_start_in,
                                const char *end,
                                const ParserRules *r,
                                const char **open_out,
                                const char **inner_start_out) {
    switch (r->match_mode) {
    case PARSER_MATCH_OUTERMOST_CLOSE: {
        /* Greedy: last occurrence in remainder. */
        size_t avail = (size_t)(end - inner_start_in);
        const char *p;
        if (r->case_insensitive) {
            /* sz_rfind has no case-insensitive variant; emulate by
               case-insensitive forward scan and tracking the last hit. */
            p = NULL;
            const char *probe = inner_start_in;
            for (;;) {
                const char *h = parser_find_(probe,
                                            (size_t)(end - probe),
                                            r->close_delim,
                                            r->close_len, true);
                if (!h) break;
                if (!r->line_anchored_close || at_line_start_(buf, h))
                    p = h;
                probe = h + 1;
            }
        } else {
            p = (const char *)sz_rfind(inner_start_in, avail,
                                       r->close_delim, r->close_len);
            /* Walk backwards if the greedy hit isn't line-anchored. */
            while (p && r->line_anchored_close && !at_line_start_(buf, p)) {
                if (p == inner_start_in) { p = NULL; break; }
                size_t left = (size_t)(p - inner_start_in);
                p = (const char *)sz_rfind(inner_start_in, left,
                                           r->close_delim, r->close_len);
            }
        }
        return p;
    }

    case PARSER_MATCH_INNERMOST: {
        /* Walk inward: find the next closer; if a nested opener appears
           before it, descend to that opener and retry. The accepted opener
           is the innermost one with no further nested opener inside.

           Works for both fixed- and variable-length openers: when descending
           into a nested opener we re-resolve its full span via
           parser_match_var_opener_, and self-closing nested openers are
           skipped (they don't open a region).

           LIMITATION: `no_preceding_byte` is checked once on the original
           opener position by the caller, but is NOT re-checked on nested
           openers discovered here. None of the documented wikitext rules
           combine INNERMOST mode with a non-zero `no_preceding_byte`, so
           this is a latent gap rather than an active bug. If a future
           rule needs both, this loop must call the lookbehind check on
           each `nested` candidate before accepting the descent. */
        const char *probe_open  = open_in;
        const char *probe_inner = inner_start_in;
        for (;;) {
            if (probe_inner > end) return NULL;

            /* Find the next line-anchored (if required) closer. */
            const char *close;
            const char *p = probe_inner;
            for (;;) {
                close = parser_find_(p, (size_t)(end - p),
                                    r->close_delim, r->close_len,
                                    r->case_insensitive);
                if (!close) return NULL;
                if (!r->line_anchored_close || at_line_start_(buf, close))
                    break;
                p = close + 1;
            }

            /* Look for a nested opener strictly inside [probe_inner, close). */
            const char *nested = parser_find_(probe_inner,
                                             (size_t)(close - probe_inner),
                                             r->open_delim, r->open_len,
                                             r->case_insensitive);
            if (!nested) {
                /* probe_open .. close is the innermost match. */
                *open_out        = probe_open;
                *inner_start_out = probe_inner;
                return close;
            }

            /* Resolve the nested opener's true span. */
            if (r->open_terminator != 0) {
                size_t nested_span;
                bool   nested_self_closing;
                const char *nested_inner = parser_match_var_opener_(
                    nested, end, r, &nested_span, &nested_self_closing);
                if (!nested_inner) {
                    /* Malformed nested opener: don't treat it as a real
                       opener -- skip one byte and rescan for a closer. */
                    probe_inner = nested + 1;
                    continue;
                }
                if (nested_self_closing) {
                    /* Self-closing tags don't open a region; step past. */
                    probe_inner = nested_inner;
                    continue;
                }
                probe_open  = nested;
                probe_inner = nested_inner;
            } else {
                probe_open  = nested;
                probe_inner = nested + r->open_len;
            }
        }
    }

    case PARSER_MATCH_FIRST_CLOSE:
    default: {
        const char *p = inner_start_in;
        for (;;) {
            const char *h = parser_find_(p, (size_t)(end - p),
                                        r->close_delim, r->close_len,
                                        r->case_insensitive);
            if (!h) return NULL;
            if (!r->line_anchored_close || at_line_start_(buf, h))
                return h;
            p = h + 1;
        }
    }
    }
}

/* ------------------------------------------------------------------------- */
/* Single-pass scanner.                                                      */
/* ------------------------------------------------------------------------- */

void parser_scan(const char *buf, size_t len, const ParserRules *r,
                ParserCallback cb, void *user_data) {
    const char *curr = buf;
    const char *end  = buf + len;

    while (curr < end) {
        /* 1) Find the next opener (with optional case-insensitive search). */
        const char *open = parser_find_(curr, (size_t)(end - curr),
                                       r->open_delim, r->open_len,
                                       r->case_insensitive);
        if (!open) {
            if (end > curr) cb(curr, (size_t)(end - curr),
                               PARSER_SEG_TEXT, user_data);
            return;
        }

        /* 2) Line-anchor check on opener: skip if not at line start. */
        if (r->line_anchored_open && !at_line_start_(buf, open)) {
            size_t emit = (size_t)(open + 1 - curr);
            cb(curr, emit, PARSER_SEG_TEXT, user_data);
            curr = open + 1;
            continue;
        }

        /* 3) Lookbehind: opener must not be preceded by `no_preceding_byte`. */
        if (r->no_preceding_byte != 0 &&
            open > buf && open[-1] == r->no_preceding_byte) {
            size_t emit = (size_t)(open - curr) + 1;
            cb(curr, emit, PARSER_SEG_TEXT, user_data);
            curr = open + 1;
            continue;
        }

        /* 4) Resolve full opener span (variable-length + self-closing). */
        size_t opener_span = r->open_len;
        bool   is_self_closing = false;
        const char *inner_start;
        if (r->open_terminator != 0) {
            inner_start = parser_match_var_opener_(open, end, r,
                                                  &opener_span,
                                                  &is_self_closing);
            if (!inner_start) {
                /* Malformed opener; emit one byte and resume. */
                size_t emit = (size_t)(open - curr) + 1;
                cb(curr, emit, PARSER_SEG_TEXT, user_data);
                curr = open + 1;
                continue;
            }
        } else {
            inner_start = open + r->open_len;
            if (inner_start > end) {
                cb(open, (size_t)(end - open), PARSER_SEG_TEXT, user_data);
                return;
            }
        }

        /* 5) Emit plain text before the opener. */
        if (open > curr)
            cb(curr, (size_t)(open - curr), PARSER_SEG_TEXT, user_data);

        /* 6) Self-closing fast path. */
        if (is_self_closing) {
            cb(open, opener_span, PARSER_SEG_SELF_CLOSING, user_data);
            curr = open + opener_span;
            continue;
        }

        /* 7) Find the closer. INNERMOST mode may shift the accepted opener
              inward; for variable-length openers the shifted span width may
              differ from the original, so opener_span is recomputed below. */
        const char *shifted_open        = open;
        const char *shifted_inner_start = inner_start;
        const char *close = find_closer_(buf, open, inner_start, end, r,
                                         &shifted_open, &shifted_inner_start);
        if (!close) {
            cb(open, (size_t)(end - open), PARSER_SEG_TEXT, user_data);
            return;
        }
        if (shifted_open != open) {
            /* Emit the bytes between the original opener and the innermost
               opener as TEXT so concatenated callback output is lossless. */
            cb(open, (size_t)(shifted_open - open),
               PARSER_SEG_TEXT, user_data);
            open        = shifted_open;
            inner_start = shifted_inner_start;
            /* Recompute opener_span from the shifted positions. Works for
               both fixed-length (inner_start - open == open_len) and
               variable-length openers (parser_match_var_opener_ guarantees
               inner_start == term + 1, so the difference is the full span). */
            opener_span = (size_t)(inner_start - open);
        }

        /* 8) Resolve full closer span (variable-length closers, optional). */
        size_t closer_span = r->close_len;
        if (r->close_terminator != 0) {
            const char *after = parser_match_var_closer_(close, end, r,
                                                        &closer_span);
            if (!after) {
                /* Malformed closer; treat the whole opener span as text. */
                cb(open, opener_span, PARSER_SEG_TEXT, user_data);
                curr = inner_start;
                continue;
            }
        }

        /* 9) Lookahead: closer must not be followed by `no_following_byte`.
              Ignored in OUTERMOST_CLOSE mode (no well-defined "next candidate"
              when we already chose the last closer in the buffer). */
        if (r->match_mode != PARSER_MATCH_OUTERMOST_CLOSE &&
            r->no_following_byte != 0 &&
            close + closer_span < end &&
            close[closer_span] == r->no_following_byte) {
            cb(open, opener_span, PARSER_SEG_TEXT, user_data);
            curr = inner_start;
            continue;
        }

        /* 10) Validate inner content. */
        size_t inner_len = (size_t)(close - inner_start);
        if (inner_is_invalid_(inner_start, inner_len, r)) {
            cb(open, opener_span, PARSER_SEG_TEXT, user_data);
            curr = inner_start;
            continue;
        }

        /* 11) Accept: hand the inner content to the callback. */
        cb(inner_start, inner_len, PARSER_SEG_INNER, user_data);
        curr = close + closer_span;
    }
}

/* ------------------------------------------------------------------------- */
/* Fixpoint driver. Zero-allocation: convergence is detected by comparing    */
/* (length, sz_hash) pairs across passes. Collision probability for a 64-bit */
/* hash on identical-length inputs is ~2^-64; acceptable for fixpoint        */
/* detection on text buffers (a false positive would be impossible to        */
/* exhibit in practice, and would only manifest as one fewer pass anyway --  */
/* the next pass would diverge again and the loop would resume).             */
/* ------------------------------------------------------------------------- */

void parser_scan_until_stable(void *tb, ParserPassFn run_pass, void *user_data,
                             const char *(*get_buf)(void *),
                             size_t      (*get_len)(void *)) {
    sz_u64_t prev_hash = 0;
    size_t   prev_len  = (size_t)-1;   /* sentinel: no previous pass yet */

    for (;;) {
        run_pass(user_data);

        const char *out_buf = get_buf(tb);
        size_t      out_len = get_len(tb);

        sz_u64_t cur_hash = (out_len == 0)
            ? 0
            : sz_hash(out_buf, out_len, /*seed=*/0);

        if (prev_len == out_len && prev_hash == cur_hash) return;

        prev_len  = out_len;
        prev_hash = cur_hash;
    }
}
```

## Stringzilla primitives used

| Operation | Function | Header | Notes |
| --- | --- | --- | --- |
| Forward substring search | `sz_find` | `<stringzilla/find.h>` | Auto-dispatches Skylake / Haswell / Westmere / NEON / SVE / SWAR. |
| Reverse substring search | `sz_rfind` | `<stringzilla/find.h>` | Used by `OUTERMOST_CLOSE`. |
| Case-insensitive substring search | `sz_utf8_case_insensitive_find` | `<stringzilla/utf8_case.h>` | Unicode case-folded; slower than `sz_find`, so opt-in per-rule. |
| Single-byte search | `sz_find_byte` | `<stringzilla/find.h>` | Opener/closer terminator (`>` for HTML tags). |
| Byteset rejection (multiple bytes at once) | `sz_find_byte_from` | `<stringzilla/find.h>` | Inline shortcut around `sz_find_byteset` (Ice Lake AVX-512 / Haswell AVX2 / NEON). |
| 64-bit hashing | `sz_hash` | `<stringzilla/hash.h>` | Fixpoint convergence test (compares `(length, hash)` pairs across passes). |

All headers are included via the umbrella `<stringzilla/stringzilla.h>`. With `SZ_DYNAMIC_DISPATCH=1` the best available SIMD backend is selected at runtime; otherwise the build picks one at compile time and falls back to optimized SWAR.

## Wikitext Coverage

This section enumerates how every wikitext delimited construct maps onto a `ParserRules` instance.

### Constructs handled by direct configuration

| Construct | Configuration |
| --- | --- |
| Template parameter `{{{...}}}` | `open="{{{"`, `close="}}}"`, `prohibited_chars="{}"`, `prohibited_patterns=["[[", "\n\0"]`, `no_preceding_byte='{'`, `no_following_byte='}'`, `match_mode=FIRST_CLOSE`. |
| Template transclusion `{{...}}` | `open="{{"`, `close="}}"`, `prohibited_chars="{}"`, `prohibited_patterns=["[["]`, `match_mode=FIRST_CLOSE`. Nested `{{outer\|{{inner}}}}` resolves via fixpoint. |
| Wikilink / file embed `[[...]]` | `open="[["`, `close="]]"`, `prohibited_chars="[]"`, optional `prohibited_patterns=["{{"]`, `match_mode=FIRST_CLOSE`. Nested wikilinks (e.g. captions in `[[File:...\|...]]`) resolve via fixpoint. |
| Language conversion `-{...}-` | `open="-{"`, `close="}-"`, `prohibited_patterns=["-{"]`, `match_mode=FIRST_CLOSE`. |
| HTML comment `<!--...-->` | `open="<!--"`, `close="-->"`, `match_mode=FIRST_CLOSE`. HTML spec: first `-->` wins. |
| Fixed-name parser tag (no attributes) `<nowiki>...</nowiki>` | `open="<nowiki>"`, `close="</nowiki>"`, `case_insensitive=true`, `match_mode=FIRST_CLOSE`. |
| Parser tag with attributes `<ref name="x">...</ref>` | `open="<ref"`, `open_terminator='>'`, `open_attr_forbidden="<"` (1 byte), `self_closing_marker="/"` (1 byte), `close="</ref"`, `close_terminator='>'`, `close_attr_forbidden="<"`, `case_insensitive=true`, `match_mode=FIRST_CLOSE`. The same shape applies to `<syntaxhighlight lang="cpp">`, `<span style="...">`, `<div class="...">`, `<section ... />`, `<math>`, `<pre>`, `<code>`, `<poem>`, `<score>`, `<hiero>`. |
| Self-closing void tag `<br />` | Same as above; the `self_closing_marker="/"` causes `<br />` to fire `PARSER_SEG_SELF_CLOSING` and skip the closer search. `<nowiki />`, `<ref name="x" />`, `<section end="x" />` all use this path. |
| Wiki table `{|...|}` | `open="{|"`, `close="|}"`, `line_anchored_open=true`, `line_anchored_close=true`, `match_mode=FIRST_CLOSE`. Nested tables resolve via fixpoint. |
| HTML entity reference `&amp;`, `&#123;`, `&#x1F600;` | `open="&"`, `close=";"`, `prohibited_chars=" \t\r\n&<>"` (any byte that cannot appear in an entity name), `match_mode=FIRST_CLOSE`. The callback validates the inner using `sz_find_byte_not_from(inner, len, ALPHA, ALPHA_LEN)` (returns NULL iff every byte is in `[A-Za-z]`); for `#NNN`/`#xHHH` forms, dispatch on `inner[0]=='#'` and re-check against digits or hex digits. Three SIMD scans per candidate over a typically-tiny inner. |
| External link `[url label]` | `open="["`, `close="]"`, `prohibited_chars="[]\n"`, `match_mode=FIRST_CLOSE`. The callback inspects the inner to confirm it begins with a URL scheme; otherwise re-emits the bytes as text. |

### Constructs that are NOT delimited regions

These don't fit any general "delimited scanner" abstraction and remain direct `sz_find` / `sz_find_byte_from` calls in their respective parsers:

- **Behavior switches** (`__TOC__`, `__NOTOC__`, `__FORCETOC__`, `__HIDDENCAT__`, `__NOEDITSECTION__`) — fixed-string scans.
- **Signatures** (`~~~`, `~~~~`, `~~~~~`) — fixed-string scans, line-position-independent.
- **Headings** `==Title==`, `===Title===` — line-anchored fixed-string scans where the closing `=`-run length must equal the opening run; better handled by a dedicated heading parser than by configuring this parser.
- **List items** `*`, `#`, `:`, `;` at line start — single-byte line-anchored markers; trivially handled by a `sz_find_byte` loop that checks the preceding byte.

### Worked traces against real Wikipedia source

**1.** `{{Cite mailing list |url=https://... |title=Wiktionary project launched ...}}` — single-level template. `FIRST_CLOSE` finds first `}}` after `{{`, inner has no `{`, accepted on pass 1.

**2.** `{{#expr:{{NUMBEROF|ARTICLES|{{Wikipedia rank by size|3}}}}/{{NUMBEROF|ARTICLES|total}}*100 round 1}}` — deeply nested. Pass 1 collects the three innermost templates; pass 2 the next layer; pass 3 the outer `{{#expr:...}}`. Fixpoint converges.

**3.** `{{{reason|{{{1}}}}}}` — parameter with a parameter as default. `prohibited_chars="{}"` rejects the outer on pass 1; inner `{{{1}}}` matches first and becomes a sentinel; outer matches pass 2.

**4.** `[[File:Jimbo.jpg|210px|alt=photograph]]` inside `{{External media|...|headerimage = [[File:Jimbo...]]|...}}` — wikilink pass extracts the `[[File:...]]` first; template pass extracts `{{External media|...}}` with the wikilink already replaced by a sentinel. Multiple parsers in sequence per fixpoint cycle.

**5.** `{{IPAc-en|audio=En-uk-Wikipedia.ogg|ˌ|w|ɪ|k|ɪ|ˈ|p|iː|d|i|ə}}` — multi-byte UTF-8 in inner. Parser is byte-oriented; `sz_find` is byte-exact; UTF-8 passes through transparently.

**6.** `<ref group="W">{{cite web |url=...}}</ref>` — variable-length opener and closer. With `open_terminator='>'` and `open_attr_forbidden="<"`, the opener span captures `<ref group="W">`; the inner `{{cite web |url=...}}` is delivered to the callback (which can recursively invoke the template parser); the closer `</ref>` (matched via `close="</ref"` + `close_terminator='>'`) ends the region. `<NoWiki>...</NoWiki>` resolves via `case_insensitive=true`.

**7.** `<br />` mid-paragraph — `open="<br"`, `open_terminator='>'`, `self_closing_marker="/"` recognizes the trailing `/` before `>` and fires `PARSER_SEG_SELF_CLOSING` with the literal bytes `<br />`. No closer search is attempted.

**8.** `\n{| class="wikitable"\n!A!!B\n|-\n|1||2\n|}\n` — `line_anchored_open` and `line_anchored_close` ensure the `{|` after `class="...wikitable"` (had it been on the same line) would not match, and that `|}` is recognized only at the start of a line.

### Pre-pass requirements

Two constructs must be extracted **before** any other lexing pass because their content disables all wiki markup:

1. **`<!-- ... -->` HTML comments** — strip with `open="<!--"`, `close="-->"`, `match_mode=FIRST_CLOSE`. After this pass, comments no longer exist in the buffer.
2. **`<nowiki>...</nowiki>` and `<nowiki />`** — replace with opaque sentinels using two `ParserRules` instances: a paired `<nowiki>...</nowiki>` rule with `case_insensitive=true`, and a self-closing rule for `<nowiki />` (handled by the same `<nowiki` opener with `self_closing_marker="/"`, since the parser routes self-closing matches independently of paired matches).

Recommended pass order matches MediaWiki's preprocessor: comments → `<nowiki>` → other parser tags → templates / parameters / wikilinks / tables (interleaved via fixpoint).

## Example: refactored `parse_simple_args` (`braces.c`)

```c
typedef struct {
    const ParserConfig *cfg;
    Accum              *accum;
    ThreadBuf          *out_tb;
} BracesContext;

static void braces_append(BracesContext *ctx, const char *data, size_t len) {
    wiki_thread_buf_append(ctx->out_tb, (sz_string_view_t){data, len});
}

static void braces_callback(const char *segment, size_t len,
                            ParserSegmentKind kind, void *user_data) {
    BracesContext *ctx = (BracesContext *)user_data;

    if (kind != PARSER_SEG_INNER) {
        /* TEXT and SELF_CLOSING (rare for this rule) pass through verbatim. */
        braces_append(ctx, segment, len);
        return;
    }

    Token *tok = build_from_inner(segment, len, true, NULL, 0, NULL,
                                  ctx->cfg, ctx->accum);
    if (tok) {
        char   sentinel[64];
        size_t slen;
        char   sym = braces_arg_symbol(segment, len, ctx->cfg);
        work_str_sentinel(ctx->accum->count - 1, sym, sentinel, &slen);
        braces_append(ctx, sentinel, slen);
    } else {
        braces_append(ctx, "{{{", 3);
        braces_append(ctx, segment, len);
        braces_append(ctx, "}}}", 3);
    }
}

typedef struct {
    ThreadBuf          *tb;
    const ParserConfig *cfg;
    Accum              *accum;
} BracesPassCtx;

static void braces_run_pass(void *user_data) {
    BracesPassCtx *p = (BracesPassCtx *)user_data;

    /* Newline is allowed unless followed by '\x00' — express as a 2-byte
       prohibited pattern, NOT as a prohibited char.

       WARNING: do NOT "simplify" pat_newline_then_nul to the string literal
       "\n\0". A literal would have sizeof == 3 (the trailing implicit NUL
       makes the array {'\n', '\0', '\0'}), and any code that derives the
       length from `strlen` would see length 1, silently dropping the NUL
       sentinel from the pattern. The brace-initialized array below is
       exactly two bytes; the explicit length 2 in prohibited_pattern_lens
       must match it. */
    static const char  pat_double_lbrack[]    = { '[', '[' };
    static const char  pat_newline_then_nul[] = { '\n', '\0' };
    static const char *prohibited_patterns[]  = {
        pat_double_lbrack, pat_newline_then_nul,
    };
    static const size_t prohibited_pattern_lens[] = { 2, 2 };

    ParserRules rules = {
        .open_delim                = "{{{",
        .open_len                  = 3,
        .close_delim               = "}}}",
        .close_len                 = 3,
        .match_mode                = PARSER_MATCH_FIRST_CLOSE,
        .prohibited_chars          = "{}",
        .prohibited_chars_len      = 2,
        .prohibited_patterns       = prohibited_patterns,
        .prohibited_pattern_lens   = prohibited_pattern_lens,
        .prohibited_patterns_count = 2,
        .no_preceding_byte         = '{',   /* (?<!\{) */
        .no_following_byte         = '}',   /* (?!\})  */
    };

    ThreadBuf *out_tb = wiki_thread_buf_acquire_scratch();
    assert(out_tb);

    BracesContext ctx = { .cfg = p->cfg, .accum = p->accum, .out_tb = out_tb };
    parser_scan(p->tb->buf, p->tb->len, &rules, braces_callback, &ctx);

    if (out_tb->len != p->tb->len ||
        sz_equal(p->tb->buf, out_tb->buf, p->tb->len) != sz_true_k) {
        wiki_thread_buf_set(p->tb, out_tb->buf, out_tb->len);
    }
    wiki_thread_buf_release_scratch(out_tb);
}

static const char *braces_get_buf(void *tb) { return ((ThreadBuf *)tb)->buf; }
static size_t      braces_get_len(void *tb) { return ((ThreadBuf *)tb)->len; }

static void parse_simple_args(ThreadBuf *tb, const ParserConfig *cfg,
                              Accum *accum) {
    BracesPassCtx p = { .tb = tb, .cfg = cfg, .accum = accum };
    parser_scan_until_stable(tb, braces_run_pass, &p,
                            braces_get_buf, braces_get_len);
}
```

## Notes for integrators

https://github.com/ashvardanian/StringZilla/blob/main/include/stringzilla/find.h  
https://github.com/ashvardanian/StringZilla/blob/main/include/stringzilla/utf8_case.h  
https://github.com/ashvardanian/StringZilla/blob/main/include/stringzilla/hash.h  

1. **`sz_string_view_t` literal** — the `braces_append` helper uses a compound literal `(sz_string_view_t){data, len}`. If `wiki_thread_buf_append` expects a different shape (e.g. separate pointer/length args), adjust accordingly.
2. **`sz_equal` return** — `sz_bool_t` is an enum (`sz_true_k`/`sz_false_k`). Compare against `sz_true_k`, don't treat the result as a bare `bool`.
3. **Static-storage rejection arrays** in pass functions are intentional: they avoid per-call rebuild cost and are safe across passes/threads (read-only).
4. **`sz_utf8_case_insensitive_find`** is in `<stringzilla/utf8_case.h>` (pulled in by the umbrella header). Use it only when needed — the byte-exact `sz_find` path is faster.
5. **Self-closing recognition is opt-in** — leave `self_closing_marker=NULL` for rules that should treat `<x />` as a malformed paired tag.
6. **Line anchoring uses the original `buf` pointer** — when a caller invokes `parser_scan` on a slice of a larger document, the slice's first byte is treated as a line start. If that's wrong, prepend `\n` synthetically or keep the slice aligned to a real newline.
7. **The parser is single-threaded per call** but is reentrant; multiple threads may run independent `parser_scan` calls on disjoint buffers concurrently. **Zero allocations**: neither `parser_scan` nor `parser_scan_until_stable` calls `malloc`/`free`. The fixpoint driver detects convergence by comparing `(length, sz_hash)` pairs across passes, so the only allocations in a parsing run are whatever the caller's `run_pass` does.
