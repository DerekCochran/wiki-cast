#include "util/callback_parser.h"

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
