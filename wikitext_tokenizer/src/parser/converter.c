#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

#include "parser/converter.h"
#include "string_util.h"
#include "token.h"
#include "log.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <stdio.h>

/*
 * parse_converter — simplified C implementation mirroring JS parseConverter.
 *
 * This implementation locates -{ ... }- fragments in the working string
 * and replaces each with a sentinel marker referencing a ConverterToken
 * pushed onto the accumulator. It supports flags before a '|' and
 * multiple rules separated by top-level semicolons. HTML entity semicolons
 * are temporarily masked during splitting.
 */

/* Helper: trim whitespace from ends; returns newly-allocated string */
static char *trim_copy(const char *s, size_t len)
{
    size_t i = 0, j = len;
    while (i < j && isspace((unsigned char)s[i])) i++;
    while (j > i && isspace((unsigned char)s[j-1])) j--;
    size_t n = j - i;
    char *r = malloc(n + 1);
    assert(r);
    memcpy(r, s + i, n);
    r[n] = '\0';
    return r;
}

/* Build a minimal converter token and push to accum.
 * flags: array of strings (NULL-terminated)
 * rules: array of strings (NULL-terminated)
 */
static Token *build_converter_token(char **flags, char **rules, Accum *accum)
{
    Token *t = token_new(TOKEN_CONVERTER, "converter");
    if (!t) return NULL;

    /* Flags token (always present) */
    Token *flags_tok = token_new(TOKEN_PLAIN, "converter-flags");
    if (!flags_tok) { token_free(t); return NULL; }
    for (size_t i = 0; flags && flags[i]; i++) {
        Token *f = token_new(TOKEN_PLAIN, "converter-flag");
        if (!f) { token_free(flags_tok); token_free(t); return NULL; }
        token_append_text_n(f, flags[i], strlen(flags[i]));
        token_append_child(flags_tok, f);
    }
    token_append_child(t, flags_tok);

    /* Rules tokens: each becomes a converter-rule token with one text child */
    for (size_t i = 0; rules && rules[i]; i++) {
        Token *r = token_new(TOKEN_PLAIN, "converter-rule");
        if (!r) { token_free(t); return NULL; }
        token_append_text_n(r, rules[i], strlen(rules[i]));
        token_append_child(t, r);
    }

    accum_push(accum, t);
    return t;
}

/* Mask HTML-like entities of the form &[#a-z0-9]+; by replacing trailing
 * semicolon with \x01. Returns newly-allocated string. */
static char *mask_entities(const char *s, size_t len)
{
    size_t cap = len + 8;
    char *out = malloc(cap);
    assert(out);
    size_t j = 0;

    for (size_t i = 0; i < len; ) {
        if (s[i] == '&') {
            size_t k = i + 1;
            if (k < len && (s[k] == '#' || isalpha((unsigned char)s[k]))) {
                /* scan until semicolon or break */
                while (k < len && (isalnum((unsigned char)s[k]) || s[k] == '#' || s[k] == 'x' || s[k] == 'X')) k++;
                if (k < len && s[k] == ';') {
                    /* copy until the semicolon, but write placeholder instead */
                    size_t need = (k - i + 1);
                    if (j + need + 1 > cap) { cap = (cap + need) * 2; out = realloc(out, cap); assert(out); }
                    memcpy(out + j, s + i, k - i);
                    j += (k - i);
                    out[j++] = '\x01'; /* placeholder for ';' */
                    i = k + 1;
                    continue;
                }
            }
        }
        if (j + 2 > cap) { cap *= 2; out = realloc(out, cap); assert(out); }
        out[j++] = s[i++];
    }
    out[j] = '\0';
    return out;
}

/* Restore placeholder \x01 back to ';' in-place (returns newly-allocated copy) */
static char *unmask_entities(const char *s)
{
    size_t len = strlen(s);
    char *r = malloc(len + 1);
    assert(r);
    for (size_t i = 0; i < len; i++) r[i] = s[i] == '\x01' ? ';' : s[i];
    r[len] = '\0';
    return r;
}

/* Escape a config variant value so it is safe in a regex alternation. */
static void append_regex_escaped(char **buf, size_t *cap, size_t *len, const char *s)
{
    for (const char *p = s; *p; p++) {
        unsigned char c = (unsigned char)*p;
        bool meta = (c < 0x80)
            && (c == '\\' || c == '.' || c == '^' || c == '$' || c == '|' || c == '?'
                || c == '*' || c == '+' || c == '(' || c == ')' || c == '[' || c == ']'
                || c == '{' || c == '}');
        size_t need = meta ? 2 : 1;
        if (*len + need + 1 > *cap) {
            *cap = (*cap + need + 64) * 2;
            *buf = realloc(*buf, *cap);
            assert(*buf);
        }
        if (meta) (*buf)[(*len)++] = '\\';
        (*buf)[(*len)++] = (char)c;
    }
    (*buf)[*len] = '\0';
}

/* JS parity:
 * new RegExp(String.raw`;(?=(?:[^;]*?=>)?\s*(?:${variants.join('|')})\s*:|(?:\s|\0\d+[cn]\x7F)*$)`, 'iu')
 */
static pcre2_code *compile_converter_split_regex(const ParserConfig *cfg)
{
    if (!cfg || cfg->variants.count == 0) return NULL;

    size_t cap = 256;
    char *pat = malloc(cap);
    assert(pat);
    size_t len = 0;

    const char *prefix = ";(?=(?:[^;]*?=>)?\\s*(?:";
    size_t prefix_len = strlen(prefix);
    if (len + prefix_len + 1 > cap) {
        cap = (len + prefix_len + 64) * 2;
        pat = realloc(pat, cap);
        assert(pat);
    }
    memcpy(pat + len, prefix, prefix_len);
    len += prefix_len;
    pat[len] = '\0';

    for (size_t i = 0; i < cfg->variants.count; i++) {
        if (i > 0) {
            if (len + 2 > cap) {
                cap = (cap + 64) * 2;
                pat = realloc(pat, cap);
                assert(pat);
            }
            pat[len++] = '|';
            pat[len] = '\0';
        }
        append_regex_escaped(&pat, &cap, &len, cfg->variants.items[i]);
    }

    const char *suffix = ")\\s*:|(?:\\s|\\x00\\d+[cn]\\x7F)*$)";
    size_t suffix_len = strlen(suffix);
    if (len + suffix_len + 1 > cap) {
        cap = (len + suffix_len + 64) * 2;
        pat = realloc(pat, cap);
        assert(pat);
    }
    memcpy(pat + len, suffix, suffix_len);
    len += suffix_len;
    pat[len] = '\0';

    PCRE2_SIZE err_offset;
    int err_code;
    pcre2_code *re = pcre2_compile(
        (PCRE2_SPTR)pat, PCRE2_ZERO_TERMINATED,
        PCRE2_CASELESS | PCRE2_UTF,
        &err_code, &err_offset, NULL);
    if (!re) {
        PCRE2_UCHAR8 err_buf[256];
        pcre2_get_error_message(err_code, err_buf, sizeof(err_buf));
        log_error("converter split regex compile error at %zu: %s Pattern: %.200s",
                  (size_t)err_offset, (char *)err_buf, pat);
    }

    free(pat);
    return re;
}

void parse_converter(ThreadBuf *tb, const ParserConfig *cfg, Accum *accum)
{
    if (!tb || !tb->buf) return;
    if (!cfg || cfg->variants.count == 0) return; /* no variants configured */

    if (!cfg->regex_converter) {
        ParserConfig *m = (ParserConfig *)cfg;
        m->regex_converter = (ParserConfigRegex *)compile_converter_split_regex(cfg);
        if (!m->regex_converter) return;
    }
    pcre2_code *re_split = (pcre2_code *)cfg->regex_converter;

    /* Debug: show working string */
    log_debug("parse_converter: entering, tb->len=%zu", tb->len);

    size_t *stack = NULL;
    size_t stack_cap = 0, stack_len = 0;

    size_t out_cap = tb->len * 2 + 64;
    char *out = malloc(out_cap);
    assert(out);
    size_t out_len = 0;

    size_t last_copy = 0;

    for (size_t i = 0; i + 1 < tb->len; ) {
        /* detect opening "-{" */
        if (tb->buf[i] == '-' && tb->buf[i+1] == '{') {
            /* push i */
            if (stack_len >= stack_cap) { stack_cap = stack_cap ? stack_cap * 2 : 16; stack = realloc(stack, stack_cap * sizeof(size_t)); assert(stack); }
            stack[stack_len++] = i;
            i += 2;
            continue;
        }
        /* detect closing "}-" */
        if (tb->buf[i] == '}' && tb->buf[i+1] == '-') {
            if (stack_len == 0) {
                /* unmatched closing — copy and advance */
                if (out_len + 2 > out_cap) { out_cap *= 2; out = realloc(out, out_cap); assert(out); }
                out[out_len++] = tb->buf[i++];
                out[out_len++] = tb->buf[i++];
                continue;
            }

            /* Pop matching opening */
            size_t open_idx = stack[--stack_len];

            /*
             * Nested converter close: leave it untouched for now.
             * We only materialize a converter when the outermost "-{...}-"
             * pair closes so open_idx stays >= last_copy and we do not
             * underflow/corrupt the output window.
             */
            if (stack_len > 0) {
                i += 2;
                continue;
            }

            /* Copy text between last_copy and open_idx */
            size_t before_len = open_idx - last_copy;
            if (out_len + before_len + 8 > out_cap) { out_cap = (out_len + before_len + 8) * 2; out = realloc(out, out_cap); assert(out); }
            memcpy(out + out_len, tb->buf + last_copy, before_len);
            out_len += before_len;

            /* inner content between open_idx+2 and i */
            size_t inner_start = open_idx + 2;
            size_t inner_len   = i - inner_start;
            const char *inner_ptr = tb->buf + inner_start;

            /* split flags/raw on first '|' */
            ssize_t pipe_at = -1;
            for (size_t k = 0; k < inner_len; k++) if (inner_ptr[k] == '|' ) { pipe_at = (ssize_t)k; break; }

            /* Build flags array */
            char **flags = NULL;
            size_t flags_count = 0;
            if (pipe_at != -1) {
                /* flags substring: inner_ptr[0..pipe_at-1] */
                char *fraw = trim_copy(inner_ptr, (size_t)pipe_at);
                /* split on ';' */
                /* simple split */
                flags = malloc(8 * sizeof(char*)); flags_count = 0; size_t fcap = 8;
                char *p = fraw;
                char *tok = NULL;
                while ((tok = strchr(p, ';')) != NULL) {
                    size_t len = (size_t)(tok - p);
                    char *val = malloc(len + 1);
                    assert(val);
                    memcpy(val, p, len);
                    val[len] = '\0';
                    if (flags_count >= fcap) { fcap *= 2; flags = realloc(flags, fcap * sizeof(char*)); }
                    flags[flags_count++] = val;
                    p = tok + 1;
                }
                /* last segment */
                size_t last_len = strlen(p);
                char *last = malloc(last_len + 1);
                assert(last);
                memcpy(last, p, last_len);
                last[last_len] = '\0';
                if (flags_count >= fcap) { fcap *= 2; flags = realloc(flags, fcap * sizeof(char*)); }
                flags[flags_count++] = last;
                free(fraw);
            }

            /* raw part */
            const char *raw_ptr;
            size_t raw_len;
            if (pipe_at != -1) { raw_ptr = inner_ptr + (pipe_at + 1); raw_len = inner_len - (pipe_at + 1); }
            else { raw_ptr = inner_ptr; raw_len = inner_len; }

            /* Mask entities to protect semicolons in entities */
            char *masked = mask_entities(raw_ptr, raw_len);

            /* JS parity: split temp on config.regexConverter separators. */
            char **rules = NULL; size_t rule_cap = 0, rule_count = 0;
            size_t masked_len = strlen(masked);
            size_t cursor = 0;
            pcre2_match_data *split_md = pcre2_match_data_create_from_pattern(re_split, NULL);
            assert(split_md);

            while (cursor <= masked_len) {
                int src = pcre2_match(re_split, (PCRE2_SPTR)masked, masked_len,
                                      cursor, 0, split_md, NULL);
                if (src <= 0) break;
                PCRE2_SIZE *ov = pcre2_get_ovector_pointer(split_md);
                size_t ms = ov[0], me = ov[1];
                if (ms < cursor || me < ms) break;

                {
                    size_t seg_len = ms - cursor;
                    char *seg = malloc(seg_len + 1);
                    memcpy(seg, masked + cursor, seg_len); seg[seg_len] = '\0';
                    char *restored = unmask_entities(seg);
                    free(seg);
                    size_t restored_len = strlen(restored);
                    char *trimmed = malloc(restored_len + 1);
                    assert(trimmed);
                    memcpy(trimmed, restored, restored_len + 1);
                    free(restored);
                    if (rule_count >= rule_cap) { rule_cap = rule_cap ? rule_cap * 2 : 8; rules = realloc(rules, rule_cap * sizeof(char*)); }
                    rules[rule_count++] = trimmed;
                }

                cursor = me;
                if (me == ms) cursor++;
            }

            pcre2_match_data_free(split_md);

            /* last rule */
            if (cursor <= masked_len) {
                size_t seg_len = masked_len - cursor;
                char *seg = malloc(seg_len + 1);
                memcpy(seg, masked + cursor, seg_len); seg[seg_len] = '\0';
                char *restored = unmask_entities(seg);
                free(seg);
                size_t restored_len = strlen(restored);
                char *trimmed = malloc(restored_len + 1);
                assert(trimmed);
                memcpy(trimmed, restored, restored_len + 1);
                free(restored);
                if (rule_count >= rule_cap) { rule_cap = rule_cap ? rule_cap * 2 : 8; rules = realloc(rules, rule_cap * sizeof(char*)); }
                rules[rule_count++] = trimmed;
            }

            /* NULL-terminate arrays */
            if (flags) { flags = realloc(flags, (flags_count + 1) * sizeof(char*)); flags[flags_count] = NULL; }
            else { flags = malloc(sizeof(char*)); flags[0] = NULL; }
            if (rules) { rules = realloc(rules, (rule_count + 1) * sizeof(char*)); rules[rule_count] = NULL; }
            else { rules = malloc(sizeof(char*)); rules[0] = NULL; }

            /* Prepare to build token: record accum length before building */
            size_t tok_idx = accum_len(accum);
            /* Build token and push to accum */
            build_converter_token(flags, rules, accum);

            /* Write sentinel marker for this token */
            char marker[64]; size_t mlen = 0;
            work_str_sentinel(tok_idx, 'v', marker, &mlen);
            if (out_len + mlen + 8 > out_cap) { out_cap = (out_len + mlen + 8) * 2; out = realloc(out, out_cap); assert(out); }
            memcpy(out + out_len, marker, mlen); out_len += mlen;

            /* cleanup flags/rules arrays but do NOT free strings used as children
             * (we passed ownership to tokens via token_append_text which strdup'd)
             */
            log_debug("parse_converter: freeing flags_count=%zu rule_count=%zu", flags_count, rule_count);
            for (size_t fi = 0; fi < flags_count; fi++) free(flags[fi]);
            free(flags);
            for (size_t ri = 0; ri < rule_count; ri++) free(rules[ri]);
            free(rules);
            free(masked);

            /* Advance i past the closing '}-' */
            i = i + 2;
            last_copy = i;
            continue;
        }
        /* Default: advance one byte */
        i++;
    }

    /* Copy any remaining tail */
    if (last_copy < tb->len) {
        size_t rem = tb->len - last_copy;
        if (out_len + rem + 1 > out_cap) { out_cap = out_len + rem + 1; out = realloc(out, out_cap); assert(out); }
        memcpy(out + out_len, tb->buf + last_copy, rem);
        out_len += rem;
    }

    out[out_len] = '\0';

    /* Adopt new buffer into ws */
    wiki_thread_buf_set(tb, out, out_len);
    free(out);
    free(stack);
}
