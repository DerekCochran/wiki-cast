#include <unicode/uchar.h>
#include <unicode/utf8.h>
#include "util/log.h"
#include "parser/magic_links.h"
#include "util/string_util.h"
#include "stringzilla/stringzilla.h"
#include "wiki_cast/token.h"
#include "util/wiki_parser_rules.h"
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Build a MagicLinkToken and push to accum. */
static Token *build_magic_link(const char *s, size_t len,
                               const char *type_name, Accum *accum) {
    Token *t = token_new(TOKEN_MAGIC_LINK, type_name);
    if (!t) return NULL;
    token_append_text_owned(t, s, len);
    accum_push(accum, t);
    return t;
}

/* ── Helpers for forward scanning ───────────────────────────────────────── */

static bool utf8_prev_cp(const char *s, size_t len, size_t pos, UChar32 *out_cp) {
    if (!s || !out_cp || pos == 0 || pos > len) return false;
    size_t i = pos - 1;
    while (i > 0 && (((unsigned char)s[i]) & 0xC0) == 0x80) i--;
    int32_t p = (int32_t)i;
    UChar32 cp = 0;
    U8_NEXT(s, p, (int32_t)len, cp);
    if (cp < 0) return false;
    *out_cp = cp;
    return true;
}

static bool utf8_cp_at_is_word(const char *s, size_t len, size_t pos) {
    if (pos >= len) return false;
    int32_t p = (int32_t)pos;
    UChar32 cp = 0;
    U8_NEXT(s, p, (int32_t)len, cp);
    if (cp < 0) return false;
    return cp == '_' || u_isalnum(cp);
}

static bool magic_left_boundary_ok(const char *s, size_t len, size_t i) {
    if (i == 0) return true;
    UChar32 prev = 0;
    if (!utf8_prev_cp(s, len, i, &prev)) return true;
    return !(prev == '_' || u_isalnum(prev));
}

/* match_proto_prefix() and is_url_common_byte() are now defined in string_util.c */

/* is_url_common_byte() is now defined in string_util.c */

/* URL-body continuation across parser sentinels. */
static size_t parse_cnht_sentinel(const char *s, size_t len, size_t i) {
    if (i + 3 >= len || (unsigned char)s[i] != 0) return 0;
    size_t j = i + 1;
    if (s[j] < '0' || s[j] > '9') return 0;
	const char *not_digit = sz_find_byte_not_from(s + j, len - j, "0123456789", 10);
	if (not_digit) j = (size_t)(not_digit - s);
	else j = len;
    if (j + 1 >= len) return 0;
    char t = s[j];
    bool is_url_sentinel = (t == 'c' || t == 'n' || t == '!' || t == '~');
    if (!is_url_sentinel) return 0;
    if ((unsigned char)s[j + 1] != 0x7F) return 0;
    return j + 2 - i;
}

/* ── Magic link matcher structs and functions ────────────────────────────── */

typedef struct {
    enum {
        MAGIC_MATCH_URL = 1,
        MAGIC_MATCH_KEYWORD = 2,
    } kind;
    size_t mstart, mend;
    size_t lead_s, lead_e;
    bool has_p1;
    size_t p1_s, p1_e; /* URL body after protocol, only when has_p1=true */
} MagicScanMatch;

static size_t consume_ascii_digit_span(const char *s, size_t len, size_t i, size_t max_digits) {
    if (!s || i >= len || max_digits == 0) return 0;
    const char *digits = "0123456789";
    const char *p = sz_find_byte_not_from(s + i, len - i, digits, 10);
    size_t span = p ? (size_t)(p - (s + i)) : (len - i);
    if (span > max_digits) span = max_digits;
    return span;
}

static size_t consume_magic_space(const char *s, size_t len, size_t i) {
    if (i >= len) return 0;
    if (s[i] == '\t') return 1;
    size_t zs = str_js_zs_len_at(s, len, i);
    if (zs > 0) return zs;
    if (i + 6 <= len && str_ci_eq_n(s + i, "&nbsp;", 6)) return 6;
    if (i + 4 < len && s[i] == '&' && s[i + 1] == '#') {
        size_t j = i + 2;
        if (j < len && (s[j] == 'x' || s[j] == 'X')) {
            j++;
            const char *hex_nz = sz_find_byte_not_from(s + j, len - j, "0", 1);
            j = hex_nz ? (size_t)(hex_nz - s) : len;
            if (j + 2 < len && s[j + 2] == ';') {
                char a = (char)fast_tolower((unsigned char)s[j]);
                char b = (char)fast_tolower((unsigned char)s[j + 1]);
                if (a == 'a' && b == '0') return (j + 3) - i;
            }
            return 0;
        }
        const char *dec_nz = sz_find_byte_not_from(s + j, len - j, "0", 1);
        j = dec_nz ? (size_t)(dec_nz - s) : len;
        if (j + 2 < len && s[j] == '1' && s[j + 1] == '6' && s[j + 2] == '0' && j + 3 < len && s[j + 3] == ';')
            return (j + 4) - i;
    }
    return 0;
}

static bool parse_rfc_or_pmid(const char *s, size_t len, size_t i, size_t *out_end) {
    size_t p = i;
        if (len - p >= 3 && sz_equal(s + p, "RFC", 3) == sz_true_k) p += 3;
        else if (len - p >= 4 && sz_equal(s + p, "PMID", 4) == sz_true_k) p += 4;
    else return false;

    size_t sp = consume_magic_space(s, len, p);
    if (sp == 0) return false;
    p += sp;
    while (true) {
        size_t nsp = consume_magic_space(s, len, p);
        if (nsp == 0) break;
        p += nsp;
    }

    const char *not_digit = sz_find_byte_not_from(s + p, len - p, "0123456789", 10);
    size_t end = not_digit ? (size_t)(not_digit - s) : len;
    if (end == p) return false;
    if (end < len && utf8_cp_at_is_word(s, len, end)) return false;
    *out_end = end;
    return true;
}

static bool parse_isbn_core_10(const char *s, size_t len, size_t p, size_t *core_end) {
	int digits = 0;
	while (p < len && digits < 9) {
        size_t span = consume_ascii_digit_span(s, len, p, (size_t)(9 - digits));
        if (span == 0) break;
        digits += (int)span;
        p += span;

        /* JS parity for (?:\d[\s-]?){9}: consume at most one separator token
         * after each consumed digit run. */
        size_t sep = consume_magic_space(s, len, p);
        if (sep > 0) {
            p += sep;
        } else if (p < len && s[p] == '-') {
            p++;
		}
	}
	if (digits != 9 || p >= len) return false;
    if ((unsigned char)(s[p] - '0') <= 9 || s[p] == 'x' || s[p] == 'X') p++;
	else return false;
	if (p < len && utf8_cp_at_is_word(s, len, p)) return false;
	*core_end = p;
	return true;
}

static bool parse_isbn(const char *s, size_t len, size_t i, size_t *out_end) {
    size_t p = i;
        if (len - p < 4 || sz_equal(s + p, "ISBN", 4) != sz_true_k) return false;
    p += 4;

    size_t sp = consume_magic_space(s, len, p);
    if (sp == 0) return false;
    p += sp;
    while (true) {
        size_t nsp = consume_magic_space(s, len, p);
        if (nsp == 0) break;
        p += nsp;
    }

    size_t best_end = 0;
    size_t end0 = 0;
    if (parse_isbn_core_10(s, len, p, &end0)) {
        best_end = end0;
    }

    if (p + 2 < len && s[p] == '9' && s[p + 1] == '7' && (s[p + 2] == '8' || s[p + 2] == '9')) {
        size_t p13 = p + 3;
        size_t sep = consume_magic_space(s, len, p13);
        if (sep > 0) {
            p13 += sep;
        } else if (p13 < len && s[p13] == '-') {
            p13++;
        }
        size_t end13 = 0;
        if (parse_isbn_core_10(s, len, p13, &end13) && end13 > best_end) {
            best_end = end13;
        }
    }

    if (best_end == 0) return false;
    *out_end = best_end;
    return true;
}

static bool parse_protocol_url(const char *s, size_t len, size_t i,
                               const ParserConfig *cfg,
                               size_t *body_s, size_t *body_e, size_t *out_end) {
    if (i >= len) {
        return false;
    }
    
    size_t pfx = match_proto_prefix(s + i, len - i, cfg);
    if (pfx == 0) return false;
    size_t p = i + pfx;
    *body_s = p;

    if (p >= len) return false;
    if (s[p] == '[') {
        size_t q = p + 1;
        while (q < len && (isxdigit((unsigned char)s[q]) || s[q] == ':' || s[q] == '.')) q++;
        if (q <= p + 1 || q >= len || s[q] != ']') return false;
        p = q + 1;
    } else {
        if (str_js_zs_len_at(s, len, p) > 0) return false;
        if (p + 2 < len && (unsigned char)s[p] == 0xEF &&
            (unsigned char)s[p + 1] == 0xBF && (unsigned char)s[p + 2] == 0xBD) {
            return false;
        }
        if (!is_url_common_byte((unsigned char)s[p])) return false;
        p++;
    }

    static const char url_stop_bytes[] =
        "\x00\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0A\x0B\x0C\x0D\x0E\x0F"
        "\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1A\x1B\x1C\x1D\x1E\x1F\x20"
        "[]<>\"\x7F\xC2\xE1\xE2\xE3\xEF";
    while (p < len) {
        size_t sc = parse_cnht_sentinel(s, len, p);
        if (sc > 0) {
            p += sc;
            continue;
        }

        const char *stop = sz_find_byte_from(s + p, len - p,
                                             url_stop_bytes, sizeof(url_stop_bytes) - 1);
        if (!stop) {
            p = len;
            break;
        }
        size_t stop_p = (size_t)(stop - s);
        if (stop_p > p) {
            p = stop_p;
            continue;
        }

        if (str_js_zs_len_at(s, len, p) > 0) break;
        if (p + 2 < len && (unsigned char)s[p] == 0xEF &&
            (unsigned char)s[p + 1] == 0xBF && (unsigned char)s[p + 2] == 0xBD) break;
        if (!is_url_common_byte((unsigned char)s[p])) break;
        p++;
    }

    /* BUG FIX: the original code had two wrong checks here:
     *  1. "if(p >= len) return false" — rejected valid URLs at end of buffer.
     *  2. An explicit terminator whitelist that omitted '\n' (0x0A), causing URLs
     *     immediately before a newline to return false even though the body scan
     *     already stopped at '\n' via is_url_common_byte (which rejects c<=0x20).
     * Correct behaviour: any non-URL-body byte (or end-of-buffer) is a valid
     * terminator.  Just require that at least one body byte was consumed. */
    if (p <= *body_s) return false; /* must have consumed at least one body byte */
    *body_e = p;
    *out_end = p;
    return true;
}

static bool magic_find_next(const char *s, size_t len, size_t at,
                            const ParserConfig *cfg, MagicScanMatch *m) {
    if (!s || !m) return false;
    for (size_t i = at; i < len; i++) {
        unsigned char c = (unsigned char)s[i];
        unsigned char cl = (unsigned char)fast_tolower(c);
        bool keyword_start = (cl == 'r' || cl == 'p' || cl == 'i');
        bool proto_start = (cfg && cfg->protocol_initials[cl]);
        if (!keyword_start && !proto_start) {
            continue;
        }

        if (!magic_left_boundary_ok(s, len, i)) continue;

        size_t body_s = 0, body_e = 0, mend = 0;
        if (parse_protocol_url(s, len, i, cfg, &body_s, &body_e, &mend)) {
            m->kind = MAGIC_MATCH_URL;
            m->mstart = i; /* plain text runs from search_at to mstart */
            m->lead_s = i;
            m->lead_e = i; /* keyword/URL starts here; no separate lead span */
            m->has_p1 = true;
            m->p1_s = body_s;
            m->p1_e = body_e;
            m->mend = mend;
            return true;
        }

        if (parse_rfc_or_pmid(s, len, i, &mend) || parse_isbn(s, len, i, &mend)) {
            m->kind = MAGIC_MATCH_KEYWORD;
            m->mstart = i;
            m->lead_s = i;
            m->lead_e = i;
            m->has_p1 = false;
            m->p1_s = m->p1_e = 0;
            m->mend = mend;
            return true;
        }
    }
    return false;
}

/* ── Main parse_magic_links using callback scanner ───────────────────────── */

void parse_magic_links(ThreadBuf *tb, const ParserConfig *cfg, Accum *accum) {
    if (!tb || !tb->buf || !cfg) return;

    /* Allocate output buffer (same strategy as old code) */
    size_t out_cap = tb->len * 2 + 64;
    ThreadBuf *tmp_out = wiki_thread_buf_acquire_scratch();
    char *out_buf = NULL;
    if (tmp_out) {
        wiki_thread_buf_reserve(tmp_out, out_cap);
        out_buf = tmp_out->buf;
    } else {
        out_buf = malloc(out_cap);
        assert(out_buf);
    }
    size_t out_len = 0;
    size_t search_at = 0;

#define ENSURE_CAP(need)                                         \
    do {                                                           \
        while (out_len + (size_t)(need) >= out_cap) {               \
            out_cap *= 2;                                           \
            if (tmp_out) {                                           \
                wiki_thread_buf_reserve(tmp_out, out_cap);         \
                out_buf = tmp_out->buf;                            \
            } else {                                               \
                out_buf = realloc(out_buf, out_cap);                \
                assert(out_buf);                                   \
            }                                                      \
        }                                                          \
    } while (0)

    MagicScanMatch mm;
    while (magic_find_next(tb->buf, tb->len, search_at, cfg, &mm)) {
        /* Emit plain text before match */
        if (mm.mstart > search_at) {
            size_t before = mm.mstart - search_at;
            ENSURE_CAP(before + 1);
            sz_copy(out_buf + out_len, tb->buf + search_at, before);
            out_len += before;
        }

        size_t mstart = mm.mstart, mend = mm.mend;
        size_t lead_s = mm.lead_s, lead_e = mm.lead_e;
        bool has_p1 = mm.has_p1;
        size_t url_body_s = mm.p1_s, url_body_e = mm.p1_e;

        /* Emit the lead character (if any) */
        if (lead_e > lead_s) {
            size_t llen = lead_e - lead_s;
            ENSURE_CAP(llen + 2);
            sz_copy(out_buf + out_len, tb->buf + lead_s, llen);
            out_len += llen;
        }

        if (has_p1) {
            /* URL case - apply JS post-match trimming */
            const char *url_ptr = tb->buf + lead_e;
            size_t url_len = mend - lead_e;
            size_t p1_len = url_body_e - url_body_s;

            /* --- Entity truncation --- */
            size_t entity_at = url_len;
            for (size_t k = 0; k < url_len && k + 1 < url_len; k++) {
                if (url_ptr[k] != '&') continue;
                size_t rem = url_len - k;

                /* &lt; or &gt; */
                if (rem >= 4 && url_ptr[k + 3] == ';' &&
                    (str_ci_eq_n(url_ptr + k + 1, "lt", 2) ||
                     str_ci_eq_n(url_ptr + k + 1, "gt", 2))) {
                    entity_at = k;
                    break;
                }
                /* &nbsp; */
                if (rem >= 6 && str_ci_eq_n(url_ptr + k, "&nbsp;", 6)) {
                    entity_at = k;
                    break;
                }
                /* &#x...; */
                if (rem >= 5 && url_ptr[k + 1] == '#' && (url_ptr[k + 2] == 'x' || url_ptr[k + 2] == 'X')) {
                    size_t j = k + 3;
                    while (j < url_len && url_ptr[j] == '0') j++;
                    bool ok = false;
                    if (j + 2 < url_len && url_ptr[j + 2] == ';') {
                        char h0 = (char)fast_tolower((unsigned char)url_ptr[j]);
                        char h1 = (char)fast_tolower((unsigned char)url_ptr[j + 1]);
                        ok = (h0 == '3' && (h1 == 'c' || h1 == 'e')) ||
                             (h0 == 'a' && h1 == '0');
                    }
                    if (ok) { entity_at = k; break; }
                    continue;
                }
                /* &#...; */
                if (rem >= 4 && url_ptr[k + 1] == '#' && url_ptr[k + 2] >= '0' && url_ptr[k + 2] <= '9') {
                    size_t j = k + 2;
                    while (j < url_len && url_ptr[j] == '0') j++;
                    bool ok = false;
                    if (j + 2 < url_len && url_ptr[j + 2] == ';') {
                        ok = (url_ptr[j] == '6' && (url_ptr[j + 1] == '0' || url_ptr[j + 1] == '2'));
                    } else if (j + 3 < url_len && url_ptr[j + 3] == ';') {
                        ok = (url_ptr[j] == '1' && url_ptr[j + 1] == '6' && url_ptr[j + 2] == '0');
                    }
                    if (ok) { entity_at = k; break; }
                    continue;
                }
            }

            /* Build trail from entity-truncated part */
            size_t trail_cap = url_len + 4;
            char *trail = malloc(trail_cap);
            assert(trail);
            size_t trail_len = 0;

            if (entity_at < url_len) {
                trail_len = url_len - entity_at;
                sz_copy(trail, url_ptr + entity_at, trail_len);
                url_len = entity_at;
            }

            /* --- Trailing punctuation stripping (JS parity) --- */
            bool has_open_paren = false;
            for (size_t k = 0; k < url_len; k++) {
                if (url_ptr[k] == '(') {
                    has_open_paren = true;
                    break;
                }
            }

            size_t punct_start = url_len;
            while (punct_start > 0) {
                char ch = url_ptr[punct_start - 1];
                bool is_punct = (ch == ',' || ch == ';' || ch == '\\' || ch == '.' ||
                                 ch == ':' || ch == '!' || ch == '?' ||
                                 (!has_open_paren && ch == ')'));
                if (!is_punct) {
                    break;
                }
                punct_start--;
            }

            if (punct_start > 0 && punct_start < url_len) {
                char prev_ch = url_ptr[punct_start - 1];
                bool prev_is_punct = (prev_ch == ',' || prev_ch == ';' || prev_ch == '\\' ||
                                      prev_ch == '.' || prev_ch == ':' || prev_ch == '!' ||
                                      prev_ch == '?' || (!has_open_paren && prev_ch == ')'));
                if (!prev_is_punct) {
                    size_t correction = 1;

                    /* If first stripped char is ';', preserve a trailing entity ';'. */
                    if (url_ptr[punct_start] == ';' && punct_start >= 1) {
                        size_t prefix_end = punct_start - 1; /* JS: url.slice(0, sepChars.index) */
                        size_t amp = prefix_end;
                        while (amp > 0 && url_ptr[amp - 1] != '&') {
                            amp--;
                        }
                        if (amp > 0 && url_ptr[amp - 1] == '&' && amp <= prefix_end) {
                            const char *q = url_ptr + amp;
                            size_t qlen = prefix_end - amp;
                            bool entity_like = false;
                            if (qlen > 0) {
                                /* [a-z]+ */
                                entity_like = true;
                                for (size_t i = 0; i < qlen; i++) {
                                    unsigned char lc = (unsigned char)fast_tolower((unsigned char)q[i]);
                                    if (!(lc >= 'a' && lc <= 'z')) {
                                        entity_like = false;
                                        break;
                                    }
                                }

                                /* #x[0-9a-f]+ */
                                if (!entity_like && qlen > 2 && q[0] == '#' &&
                                    (q[1] == 'x' || q[1] == 'X')) {
                                    entity_like = true;
                                    for (size_t i = 2; i < qlen; i++) {
                                        if (!isxdigit((unsigned char)q[i])) {
                                            entity_like = false;
                                            break;
                                        }
                                    }
                                }

                                /* #[0-9]+ */
                                if (!entity_like && qlen > 1 && q[0] == '#') {
                                    entity_like = true;
                                    for (size_t i = 1; i < qlen; i++) {
                                        if (q[i] < '0' || q[i] > '9') {
                                            entity_like = false;
                                            break;
                                        }
                                    }
                                }
                            }

                            if (entity_like) {
                                correction = 2;
                            }
                        }
                    }

                    size_t keep_until = punct_start + (correction - 1);
                    if (keep_until < url_len) {
                        size_t stripped_len = url_len - keep_until;
                        if (trail_len + stripped_len >= trail_cap) {
                            while (trail_len + stripped_len >= trail_cap) {
                                trail_cap *= 2;
                            }
                            trail = realloc(trail, trail_cap);
                            assert(trail);
                        }
                        memmove(trail + stripped_len, trail, trail_len);
                        sz_copy(trail, url_ptr + keep_until, stripped_len);
                        trail_len += stripped_len;
                        url_len = keep_until;
                    }
                }
            }

            if (trail_len >= p1_len) {
                /* Bail out - emit original match */
                size_t rest_len = mend - lead_e;
                ENSURE_CAP(rest_len + 1);
                sz_copy(out_buf + out_len, tb->buf + lead_e, rest_len);
                out_len += rest_len;
                free(trail);
                search_at = mend + (mend == mstart ? 1 : 0);
                continue;
            }

            /* Build MagicLink token for trimmed URL */
            Token *ml = build_magic_link(url_ptr, url_len, "free-ext-link", accum);
            if (ml) {
                size_t idx = accum->count - 1;
                char sent[64];
                size_t slen;
                work_str_sentinel(idx, 'w', sent, &slen);
                ENSURE_CAP(slen + trail_len + 1);
                sz_copy(out_buf + out_len, sent, slen);
                out_len += slen;
                if (trail_len > 0) {
                    sz_copy(out_buf + out_len, trail, trail_len);
                    out_len += trail_len;
                }
            } else {
                size_t rest_len = mend - lead_e;
                ENSURE_CAP(rest_len + 1);
                sz_copy(out_buf + out_len, tb->buf + lead_e, rest_len);
                out_len += rest_len;
            }
            free(trail);
        } else {
            /* RFC/PMID/ISBN case */
            const char *inner_ptr = tb->buf + lead_e;
            size_t inner_len = (mend > lead_e) ? (mend - lead_e) : 0;

            if (mm.kind != MAGIC_MATCH_KEYWORD) {
                size_t rest_len = mend - lead_e;
                ENSURE_CAP(rest_len + 1);
                sz_copy(out_buf + out_len, tb->buf + lead_e, rest_len);
                out_len += rest_len;
            } else {
                Token *ml = build_magic_link(inner_ptr, inner_len, "magic-link", accum);
                if (ml) {
                    size_t idx = accum->count - 1;
                    char sent[64];
                    size_t slen;
                    work_str_sentinel(idx, 'i', sent, &slen);
                    ENSURE_CAP(slen + 1);
                    sz_copy(out_buf + out_len, sent, slen);
                    out_len += slen;
                } else {
                    size_t rest_len = mend - lead_e;
                    ENSURE_CAP(rest_len + 1);
                    sz_copy(out_buf + out_len, tb->buf + lead_e, rest_len);
                    out_len += rest_len;
                }
            }
        }

        search_at = mend + (mend == mstart ? 1 : 0);
    }

    /* Append trailing text */
    if (search_at < tb->len) {
        size_t rest = tb->len - search_at;
        ENSURE_CAP(rest + 1);
        sz_copy(out_buf + out_len, tb->buf + search_at, rest);
        out_len += rest;
    }

    out_buf[out_len] = '\0';
    wiki_thread_buf_set(tb, out_buf, out_len);
    if (tmp_out) {
        wiki_thread_buf_release_scratch(tmp_out);
    } else {
        free(out_buf);
    }
}
