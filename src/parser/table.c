#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

#include "parser/table.h"
#include "table_token.h"
#include "string_util.h"
#include "token.h"
#include "parser/tr.h"
#include "parser/td.h"
#include "string_util.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

static pcre2_code *s_re_table_lead = NULL;
static pcre2_code *s_re_table_start = NULL;
static pcre2_code *s_re_table_line = NULL;
static pcre2_code *s_re_table_sep_th = NULL;
static pcre2_code *s_re_table_sep_td = NULL;
static pcre2_code *s_re_td_inner_sep = NULL;

static pcre2_code *compile_table_regex(const char *pattern)
{
    PCRE2_SIZE err_offset;
    int err_code;
    pcre2_code *re = pcre2_compile((PCRE2_SPTR)pattern, PCRE2_ZERO_TERMINATED,
                                   PCRE2_UTF | PCRE2_UCP,
                                   &err_code, &err_offset, NULL);
    assert(re);
    return re;
}

static void ensure_table_regexes(void)
{
    if (!s_re_table_lead) {
        s_re_table_lead = compile_table_regex("^(?:\\s|\\x00\\d+[cno]\\x7F)*");
        s_re_table_start = compile_table_regex("^(:*)((?:\\s|\\x00\\d+[cn]\\x7F)*)(\\{\\||\\{(?:\\x00\\d+[cn]\\x7F)*\\x00\\d+!\\x7F|\\x00\\d+\\{\\x7F)(.*)$");
        s_re_table_line = compile_table_regex("^(?:(\\|\\}|\\x00\\d+!\\x7F\\}|\\x00\\d+\\}\\x7F)|(\\|-+|\\x00\\d+!\\x7F-+|\\x00\\d+-\\x7F-*)(?!-)|(!|(?:\\||\\x00\\d+!\\x7F)\\+?))(.*)$");
        s_re_table_sep_th = compile_table_regex("!!|(?:\\||\\x00\\d+!\\x7F){2}|\\x00\\d+\\+\\x7F");
        s_re_table_sep_td = compile_table_regex("(?:\\||\\x00\\d+!\\x7F){2}|\\x00\\d+\\+\\x7F");
        s_re_td_inner_sep = compile_table_regex("\\||\\x00\\d+!\\x7F");
    }
}

static int match_regex(pcre2_code *re, const char *s, size_t len, size_t start,
                       pcre2_match_data **md_out)
{
    pcre2_match_data *md = pcre2_match_data_create_from_pattern(re, NULL);
    if (!md) return -1;
    int rc = pcre2_match(re, (PCRE2_SPTR)s, len, start, 0, md, NULL);
    if (rc < 0) {
        pcre2_match_data_free(md);
        return rc;
    }
    *md_out = md;
    return rc;
}

static int contains_literal_seq(const char *s, size_t len, const char *needle)
{
    size_t nlen = strlen(needle);
    if (nlen == 0 || len < nlen) return 0;
    for (size_t i = 0; i + nlen <= len; i++) {
        if (memcmp(s + i, needle, nlen) == 0) return 1;
    }
    return 0;
}

static size_t sentinel_len_if(const char *s, size_t len, size_t pos, char marker)
{
    if (!s || pos >= len || s[pos] != '\0') return 0;
    size_t k = pos + 1;
    size_t digits = 0;
    while (k < len && s[k] >= '0' && s[k] <= '9') {
        k++;
        digits++;
    }
    if (digits == 0) return 0;
    if (k + 1 >= len) return 0;
    if (s[k] != marker) return 0;
    if ((unsigned char)s[k + 1] != 0x7F) return 0;
    return (k + 2) - pos;
}

static size_t pipe_unit_len(const char *s, size_t len, size_t pos)
{
    if (!s || pos >= len) return 0;
    if (s[pos] == '|') return 1;
    return sentinel_len_if(s, len, pos, '!');
}

static size_t cell_sep_len(const char *s, size_t len, size_t pos, char cell_char)
{
    if (!s || pos >= len) return 0;

    if (cell_char == '!' && pos + 1 < len && s[pos] == '!' && s[pos + 1] == '!') {
        return 2;
    }

    size_t u1 = pipe_unit_len(s, len, pos);
    if (u1 > 0) {
        size_t u2 = pipe_unit_len(s, len, pos + u1);
        if (u2 > 0) return u1 + u2;
    }

    return sentinel_len_if(s, len, pos, '+');
}

static Token *make_attr_key(const char *key, size_t key_len, Accum *accum)
{
    Token *t = token_new(TOKEN_ATTR_KEY, "attr-key");
    if (!t) return NULL;
    token_append_text_n(t, key, key_len);
    accum_push(accum, t);
    return t;
}

static Token *make_attr_value(const char *val, size_t val_len, Accum *accum)
{
    Token *t = token_new(TOKEN_ATTR_VALUE, "attr-value");
    if (!t) return NULL;
    token_append_text_n(t, val, val_len);
    accum_push(accum, t);
    return t;
}

static Token *make_table_attr_dirty(const char *text, size_t text_len, Accum *accum)
{
    Token *t = token_new(TOKEN_ATOM, "table-attr-dirty");
    if (!t) return NULL;
    token_append_text_n(t, text, text_len);
    accum_push(accum, t);
    return t;
}

static Token *make_table_attr(const char *key, size_t key_len,
                              const char *val, size_t val_len,
                              const char *equal, size_t equal_len,
                              char quote_open, char quote_close,
                              Accum *accum)
{
    Token *t = token_new(TOKEN_EXT_ATTR, "table-attr");
    if (!t) return NULL;

    t->name = str_trim_lc(key, key_len);
    if (equal && equal_len > 0) {
        t->data.ext_attr.equal = malloc(equal_len + 1);
        assert(t->data.ext_attr.equal);
        memcpy(t->data.ext_attr.equal, equal, equal_len);
        t->data.ext_attr.equal[equal_len] = '\0';
    }
    t->data.ext_attr.quote_open = quote_open;
    t->data.ext_attr.quote_close = quote_close;

    Token *attr_key = make_attr_key(key, key_len, accum);
    if (!attr_key) { token_free(t); return NULL; }
    token_append_child(t, attr_key);

    if (val) {
        Token *attr_val = make_attr_value(val, val_len, accum);
        if (!attr_val) { token_free(t); return NULL; }
        token_append_child(t, attr_val);
    }

    accum_push(accum, t);
    return t;
}

static void parse_table_attrs(Token *attrs_tok, const char *attr_str, size_t attr_len, Accum *accum)
{
    if (!attr_str || attr_len == 0) return;

    size_t i = 0;
    char dirty_buf[4096];
    size_t dirty_len = 0;

#define FLUSH_DIRTY() do { \
    if (dirty_len > 0) { \
        Token *dt = make_table_attr_dirty(dirty_buf, dirty_len, accum); \
        if (dt) token_append_child(attrs_tok, dt); \
        dirty_len = 0; \
    } \
} while (0)

    while (i < attr_len) {
        if (attr_str[i] == '/' || attr_str[i] == ' ' || attr_str[i] == '\t' || attr_str[i] == '\n' || attr_str[i] == '\r' || attr_str[i] == '\f' || attr_str[i] == '\v') {
            dirty_buf[dirty_len++] = attr_str[i++];
            continue;
        }

        size_t key_start = i;
        while (i < attr_len && attr_str[i] != '/' && attr_str[i] != '='
               && attr_str[i] != ' ' && attr_str[i] != '\t' && attr_str[i] != '\n'
               && attr_str[i] != '\r' && attr_str[i] != '\f' && attr_str[i] != '\v') {
            i++;
        }
        size_t key_len = i - key_start;
        if (key_len == 0) {
            dirty_buf[dirty_len++] = attr_str[i++];
            continue;
        }

        const char *key = attr_str + key_start;
        unsigned char kc0 = (unsigned char)key[0];
        int valid_key = ((kc0 >= 'A' && kc0 <= 'Z') || (kc0 >= 'a' && kc0 <= 'z') || kc0 == '_' || kc0 == ':');
        for (size_t k = 1; valid_key && k < key_len; k++) {
            unsigned char kc = (unsigned char)key[k];
            valid_key = ((kc >= 'A' && kc <= 'Z') || (kc >= 'a' && kc <= 'z') || (kc >= '0' && kc <= '9') || kc == ':' || kc == '.' || kc == '_' || kc == '-');
        }
        if (!valid_key) {
            for (size_t k = 0; k < key_len; k++) dirty_buf[dirty_len++] = key[k];
            continue;
        }

        size_t ws_start = i;
        while (i < attr_len && (attr_str[i] == ' ' || attr_str[i] == '\t' || attr_str[i] == '\n' || attr_str[i] == '\r' || attr_str[i] == '\f' || attr_str[i] == '\v')) i++;

        if (i >= attr_len || attr_str[i] != '=') {
            FLUSH_DIRTY();
            Token *at = make_table_attr(key, key_len, NULL, 0, NULL, 0, '\0', '\0', accum);
            if (at) token_append_child(attrs_tok, at);
            if (ws_start < i) {
                memcpy(dirty_buf, attr_str + ws_start, i - ws_start);
                dirty_len = i - ws_start;
            }
            continue;
        }

        size_t eq_start = ws_start;
        i++;
        while (i < attr_len && (attr_str[i] == ' ' || attr_str[i] == '\t' || attr_str[i] == '\n' || attr_str[i] == '\r' || attr_str[i] == '\f' || attr_str[i] == '\v')) i++;
        size_t eq_len = i - eq_start;

        char quote_open = '\0', quote_close = '\0';
        const char *val = NULL;
        size_t val_len = 0;

        if (i < attr_len && (attr_str[i] == '"' || attr_str[i] == '\'')) {
            quote_open = attr_str[i++];
            size_t val_start = i;
            while (i < attr_len && attr_str[i] != quote_open) i++;
            val = attr_str + val_start;
            val_len = i - val_start;
            if (i < attr_len && attr_str[i] == quote_open) {
                quote_close = attr_str[i];
                i++;
            }
        } else {
            size_t val_start = i;
            while (i < attr_len && attr_str[i] != ' ' && attr_str[i] != '\t' && attr_str[i] != '\n'
                   && attr_str[i] != '\r' && attr_str[i] != '\f' && attr_str[i] != '\v') i++;
            val = attr_str + val_start;
            val_len = i - val_start;
        }

        FLUSH_DIRTY();
        Token *at = make_table_attr(key, key_len, val, val_len,
                                    attr_str + eq_start, eq_len,
                                    quote_open, quote_close,
                                    accum);
        if (at) token_append_child(attrs_tok, at);
    }

    FLUSH_DIRTY();
#undef FLUSH_DIRTY
}

/*
 * Simplified parseTable implementation.
 *
 * This conservative implementation finds top-level table blocks starting
 * with "{|" and ending with the matching "|}" (supports nesting). For
 * each matched block it creates a TOKEN_TABLE token containing the raw
 * table substring as a single text child, pushes it into the accumulator
 * and replaces the matched substring in the working string with a
 * sentinel marker. The approach avoids allocating extra substrings where
 * possible and operates on ThreadBuf buffers directly.
 */

void parse_table(ThreadBuf *tb, const ParserConfig *cfg, Accum *accum)
{
    if (!tb || !tb->buf) return;
    ensure_table_regexes();

    size_t out_cap = tb->len * 2 + 64;
    char *out_buf = malloc(out_cap);
    assert(out_buf);
    size_t out_len = 0;

    size_t i = 0;

    #define ENSURE_CAP(need) do { \
        while (out_len + (need) >= out_cap) { out_cap *= 2; out_buf = realloc(out_buf, out_cap); assert(out_buf); } \
    } while(0)

    while (i < tb->len) {
        /* Find next opening sequence "{|" (byte-wise, safe with NUL bytes present). */
        size_t p = SIZE_MAX;
        for (size_t k = i; k + 1 < tb->len; k++) {
            if (tb->buf[k] == '{' && tb->buf[k+1] == '|') { p = k; break; }
        }

        if (p == SIZE_MAX) {
            /* No more tables — copy rest */
            size_t rest = tb->len - i;
            ENSURE_CAP(rest + 1);
            memcpy(out_buf + out_len, tb->buf + i, rest);
            out_len += rest;
            break;
        }

        /* JS parity: an indented table start creates a dd token before the table. */
        size_t line_start = p;
        while (line_start > i && tb->buf[line_start - 1] != '\n') line_start--;
        size_t dd_len = p - line_start;
        int only_colons = dd_len > 0;
        for (size_t d = line_start; only_colons && d < p; d++) {
            if (tb->buf[d] != ':') only_colons = 0;
        }

        size_t before = only_colons ? (line_start - i) : (p - i);
        ENSURE_CAP(before + 1);
        memcpy(out_buf + out_len, tb->buf + i, before);
        out_len += before;

        if (only_colons) {
            Token *dd = token_new(TOKEN_DD, "dd");
            if (dd) {
                token_append_text_n(dd, tb->buf + line_start, dd_len);
                accum_push(accum, dd);
                char dd_marker[64];
                size_t dd_mlen = 0;
                work_str_sentinel(accum->count - 1, 'd', dd_marker, &dd_mlen);
                ENSURE_CAP(dd_mlen + 1);
                memcpy(out_buf + out_len, dd_marker, dd_mlen);
                out_len += dd_mlen;
            } else {
                ENSURE_CAP(dd_len + 1);
                memcpy(out_buf + out_len, tb->buf + line_start, dd_len);
                out_len += dd_len;
            }
        }

        /* Find matching closing "|}" with nesting support */
        size_t depth = 1;
        size_t j = p + 2;
        while (j + 1 < tb->len) {
            if (tb->buf[j] == '{' && tb->buf[j+1] == '|' && (j == p || tb->buf[j - 1] == '\n')) {
                depth++;
                j += 2;
                continue;
            }
            if (tb->buf[j] == '|' && tb->buf[j+1] == '}') { depth--; j += 2; if (depth == 0) break; continue; }
            j++;
        }

        if (j + 1 >= tb->len && depth != 0) {
            /* No closing found — treat the remainder as plain text */
            size_t rest = tb->len - p;
            ENSURE_CAP(rest + 1);
            memcpy(out_buf + out_len, tb->buf + p, rest);
            out_len += rest;
            break;
        }

        /* j points just past the closing pair; substring is [p, j) */
        size_t tlen = j - p;
        char *tbl = malloc(tlen + 1);
        assert(tbl);
        memcpy(tbl, tb->buf + p, tlen);
        tbl[tlen] = '\0';

        /* Build a Table token and its child tokens (syntax + attrs + rows/cells).
         * We push the table token first (JS accumulator ordering), then create
         * the syntax and attributes tokens and any row/cell tokens that appear
         * inside the table. */
        size_t tok_idx = accum->count;

        Token *table = token_new(TOKEN_TABLE, "table");
        if (!table) {
            ENSURE_CAP(tlen + 1);
            memcpy(out_buf + out_len, tb->buf + p, tlen);
            out_len += tlen;
            free(tbl);
            i = p + tlen;
            continue;
        }
        accum_push(accum, table);

        /* Extract syntax and attrs from opening line */
        size_t first_nl = 0;
        while (first_nl < tlen && tbl[first_nl] != '\n') first_nl++;
        const char *syntax_ptr = tbl;
        size_t syntax_len = (tlen >= 2 && tbl[0] == '{' && tbl[1] == '|') ? 2 : 1;
        const char *attr_ptr = NULL;
        size_t attr_len = 0;
        if (first_nl > 0) {
            pcre2_match_data *md = NULL;
            int rc = match_regex(s_re_table_start, tbl, first_nl, 0, &md);
            if (rc > 0) {
                PCRE2_SIZE *ov = pcre2_get_ovector_pointer(md);
                if (rc > 3 && ov[6] != PCRE2_UNSET) {
                    syntax_ptr = tbl + ov[6];
                    syntax_len = ov[7] - ov[6];
                }
                if (rc > 4 && ov[8] != PCRE2_UNSET) {
                    attr_ptr = tbl + ov[8];
                    attr_len = ov[9] - ov[8];
                }
                pcre2_match_data_free(md);
            } else {
                attr_ptr = (first_nl > 2) ? tbl + 2 : NULL;
                attr_len = (first_nl > 2) ? (first_nl - 2) : 0;
            }
        }

        Token *syn = token_new(TOKEN_SYNTAX, "table-syntax");
        if (syn) { token_append_text_n(syn, syntax_ptr, syntax_len); accum_push(accum, syn); token_append_child(table, syn); }
        Token *attrs = token_new(TOKEN_ATTRIBUTES, "table-attrs");
        if (attrs) {
            attrs->name = strdup("table");
            parse_table_attrs(attrs, attr_ptr, attr_len, accum);
            accum_push(accum, attrs);
            token_append_child(table, attrs);
        }

        /* Parse the inner lines into rows/cells */
        size_t inner_start = (first_nl < tlen) ? first_nl + 1 : first_nl;
        size_t pos = inner_start;

        Token *current_tr = NULL;
        Token *current_td = NULL;

        while (pos < tlen) {
            /* find end of line */
            size_t next_nl = pos;
            while (next_nl < tlen && tbl[next_nl] != '\n') next_nl++;
            size_t line_len = next_nl - pos;
            const char *line = tbl + pos;
            size_t si = 0;
            pcre2_match_data *lead_md = NULL;
            int lead_rc = match_regex(s_re_table_lead, line, line_len, 0, &lead_md);
            if (lead_rc > 0) {
                PCRE2_SIZE *ov = pcre2_get_ovector_pointer(lead_md);
                si = ov[1];
            }
            if (lead_md) pcre2_match_data_free(lead_md);

            const char *body = line + si;
            size_t body_len = line_len - si;

            if (body_len == 0) {
                /* empty line inside table — append as plain text to current tr or table */
                if (current_tr) {
                    token_append_text_n(current_tr, "\n", 1);
                }
            } else {
                pcre2_match_data *line_md = NULL;
                int line_rc = match_regex(s_re_table_line, body, body_len, 0, &line_md);
                if (line_rc > 0) {
                    PCRE2_SIZE *ov = pcre2_get_ovector_pointer(line_md);
                    size_t closing_s = (line_rc > 1 && ov[2] != PCRE2_UNSET) ? ov[2] : 0;
                    size_t closing_e = (line_rc > 1 && ov[3] != PCRE2_UNSET) ? ov[3] : 0;
                    size_t row_s = (line_rc > 2 && ov[4] != PCRE2_UNSET) ? ov[4] : 0;
                    size_t row_e = (line_rc > 2 && ov[5] != PCRE2_UNSET) ? ov[5] : 0;
                    size_t cell_s = (line_rc > 3 && ov[6] != PCRE2_UNSET) ? ov[6] : 0;
                    size_t cell_e = (line_rc > 3 && ov[7] != PCRE2_UNSET) ? ov[7] : 0;
                    size_t attr_s = (line_rc > 4 && ov[8] != PCRE2_UNSET) ? ov[8] : 0;
                    size_t attr_e = (line_rc > 4 && ov[9] != PCRE2_UNSET) ? ov[9] : 0;

                    if (closing_e > closing_s) {
                        /* closing |} syntax */
                current_td = NULL;
                        size_t clos_len = closing_e - closing_s;
                        char *clos = malloc(si + 1 + clos_len);
                        assert(clos);
                        clos[0] = '\n';
                        if (si > 0) memcpy(clos + 1, line, si);
                        memcpy(clos + 1 + si, body + closing_s, clos_len);
                        Token *clos_tok = token_new(TOKEN_SYNTAX, "table-syntax");
                        if (clos_tok) { token_append_text_n(clos_tok, clos, si + 1 + clos_len); accum_push(accum, clos_tok); token_append_child(table, clos_tok); }
                        free(clos);
                    } else if (row_e > row_s) {
                        /* row separator */
                        size_t row_len = row_e - row_s;
                        char *synbuf = malloc(si + 1 + row_len);
                        assert(synbuf);
                        synbuf[0] = '\n';
                        if (si > 0) memcpy(synbuf + 1, line, si);
                        memcpy(synbuf + 1 + si, body + row_s, row_len);
                        const char *row_attr = (attr_e > attr_s) ? body + attr_s : NULL;
                        size_t row_attr_len = (attr_e > attr_s) ? (attr_e - attr_s) : 0;
                        Token *tr = create_tr_token(synbuf, si + 1 + row_len,
                                                    row_attr, row_attr_len,
                                                    accum);
                        if (tr) token_append_child(table, tr);
                        current_tr = tr;
                        current_td = NULL;
                        free(synbuf);
                    } else if (cell_e > cell_s) {
                        /* cell line: split into cells on exact JS separator regexes */
                        char cellChar = body[cell_s];
                        /* JS parity: '|+' is part of syntax token, not inner content. */
                        size_t cell_prefix_len = 1;
                        if (cellChar == '|' && cell_e > cell_s + 1 && body[cell_s + 1] == '+') {
                            cell_prefix_len = 2;
                        }

                        /* attr part after initial cell syntax */
                        size_t cell_attr_off = cell_e;
                        size_t rem_len = (cell_attr_off < body_len) ? (body_len - cell_attr_off) : 0;
                        char *attrbuf = malloc(rem_len + 1);
                        assert(attrbuf);
                        if (rem_len > 0) memcpy(attrbuf, body + cell_attr_off, rem_len);
                        attrbuf[rem_len] = '\0';

                        /* JS parity: split on exact separator regexes and carry lastSyntax. */
                        size_t scan = 0;
                        size_t last = 0;
                        size_t cur_syn_len = si + cell_prefix_len + 1; /* '\n' + leading bytes */
                        char *cur_syn = malloc(cur_syn_len + 1);
                        assert(cur_syn);
                        cur_syn[0] = '\n';
                        if (si > 0) memcpy(cur_syn + 1, line, si);
                        memcpy(cur_syn + 1 + si, body + cell_s, cell_prefix_len);
                        cur_syn[cur_syn_len] = '\0';

                        while (1) {
                            size_t sep_pos = rem_len;
                            size_t sep_len = 0;
                            pcre2_match_data *sep_md = NULL;
                            pcre2_code *sep_re = (cellChar == '!') ? s_re_table_sep_th : s_re_table_sep_td;
                            int sep_rc = match_regex(sep_re, attrbuf, rem_len, scan, &sep_md);
                            if (sep_rc > 0) {
                                PCRE2_SIZE *sov = pcre2_get_ovector_pointer(sep_md);
                                sep_pos = sov[0];
                                sep_len = sov[1] - sov[0];
                            }
                            if (sep_md) pcre2_match_data_free(sep_md);
                            if (sep_len == 0) {
                                sep_pos = rem_len;
                            }

                            size_t seg_len = sep_pos - last;
                            const char *seg = attrbuf + last;
                            const char *cell_attrs = NULL;
                            const char *inner = NULL;
                            const char *inner_syntax = NULL;
                            size_t cell_attrs_len = 0;
                            size_t inner_len = 0;
                            size_t inner_syntax_len = 0;

                            pcre2_match_data *inner_md = NULL;
                            int inner_rc = match_regex(s_re_td_inner_sep, seg, seg_len, 0, &inner_md);
                            if (inner_rc > 0) {
                                PCRE2_SIZE *iov = pcre2_get_ovector_pointer(inner_md);
                                cell_attrs = seg;
                                cell_attrs_len = iov[0];
                                inner_syntax = seg + iov[0];
                                inner_syntax_len = iov[1] - iov[0];
                                inner = seg + iov[1];
                                inner_len = seg_len - iov[1];
                                if (contains_literal_seq(cell_attrs, cell_attrs_len, "[[")
                                        || contains_literal_seq(cell_attrs, cell_attrs_len, "-{")) {
                                    cell_attrs = "";
                                    cell_attrs_len = 0;
                                    inner_syntax = "";
                                    inner_syntax_len = 0;
                                    inner = seg;
                                    inner_len = seg_len;
                                }
                            } else {
                                cell_attrs = "";
                                cell_attrs_len = 0;
                                inner_syntax = "";
                                inner_syntax_len = 0;
                                inner = seg;
                                inner_len = seg_len;
                            }
                            if (inner_md) pcre2_match_data_free(inner_md);

                            Token *parent = current_tr ? current_tr : table;
                            Token *td = create_td_token(cur_syn, cur_syn_len,
                                                    cell_attrs, cell_attrs_len,
                                                    inner_syntax, inner_syntax_len,
                                                    inner, inner_len,
                                                    accum);
                            if (td) { token_append_child(parent, td); current_td = td; }

                            if (sep_len == 0) break;

                            free(cur_syn);
                            cur_syn_len = sep_len;
                            cur_syn = malloc(cur_syn_len + 1);
                            assert(cur_syn);
                            memcpy(cur_syn, attrbuf + sep_pos, sep_len);
                            cur_syn[cur_syn_len] = '\0';

                            last = sep_pos + sep_len;
                            scan = last;
                        }

                        free(cur_syn);

                        free(attrbuf);
                    } else {
                        /* unmatched line form */
                        if (current_td != NULL
                            && current_td->child_count > 2
                            && !current_td->children[2].is_text
                            && current_td->children[2].token != NULL) {
                            Token *inner_tok = current_td->children[2].token;
                            char *cont = malloc(line_len + 2);
                            cont[0] = '\n';
                            memcpy(cont + 1, line, line_len);
                            cont[line_len + 1] = '\0';
                            token_append_text_n(inner_tok, cont, line_len + 1);
                            free(cont);
                        } else {
                            token_append_text_n(table, line, line_len);
                        }
                    }
                } else {
                    /* default: continuation line — append to current TD's inner content
                     * (with leading newline) so multi-line cell content is preserved. */
                    if (current_td != NULL
                        && current_td->child_count > 2
                        && !current_td->children[2].is_text
                        && current_td->children[2].token != NULL) {
                        Token *inner_tok = current_td->children[2].token;
                        char *cont = malloc(line_len + 2);
                        cont[0] = '\n';
                        memcpy(cont + 1, line, line_len);
                        cont[line_len + 1] = '\0';
                        token_append_text_n(inner_tok, cont, line_len + 1);
                        free(cont);
                    } else {
                        token_append_text_n(table, line, line_len);
                    }
                }
                if (line_md) pcre2_match_data_free(line_md);
            }
            pos = next_nl + 1;
        }

        /* Emit sentinel for table token */
        char marker[64]; size_t mlen = 0;
        work_str_sentinel(tok_idx, 'b', marker, &mlen);
        ENSURE_CAP(mlen);
        memcpy(out_buf + out_len, marker, mlen);
        out_len += mlen;

        free(tbl);

        /* Continue after the table */
        i = p + tlen;
    }

    /* Null-terminate and adopt into ws */
    out_buf[out_len] = '\0';
    wiki_thread_buf_set(tb, out_buf, out_len);
    free(out_buf);
}
