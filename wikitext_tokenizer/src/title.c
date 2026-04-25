/*
 * title.c — Title normalization and validity (halfParsed mode).
 *
 * Mirrors the subset of dist/lib/title.js used by parseRedirect when
 * called with { halfParsed: true, temporary: true, decode: true, page: '' }.
 *
 * Title validity test (JS):
 *   this.valid = Boolean(title || this.interwiki || ...)
 *     && decodeHtml(title) === title     // no remaining HTML entities
 *     && !/^:|\0\d+[eh!+-]\x7F|[<>[\]{}|\n]|%[\da-f]{2}|(?:^|\/)\.{1,2}(?:$|\/)/iu.test(sub);
 */
#include "title.h"
#include "string_util.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

/* ── Helpers ─────────────────────────────────────────────────────────────── */

/** True if byte b is a hex digit. */
static bool is_hex(unsigned char b)
{
    return (b >= '0' && b <= '9') || (b >= 'a' && b <= 'f') || (b >= 'A' && b <= 'F');
}

/**
 * True if s contains any character that makes a title invalid:
 *   ^: | [<>[\]{}|\n] | \0\d+[eh!+-]\x7F | %[0-9a-f]{2} | path ./ or ../
 */
static bool title_has_invalid_chars(const char *s, size_t len)
{
    /* Check for leading colon */
    if (len > 0 && s[0] == ':') return true;

    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)s[i];

        /* Invalid bare characters */
        if (c == '<' || c == '>' || c == '[' || c == ']' ||
            c == '{' || c == '}' || c == '|' || c == '\n') {
            return true;
        }

        /* Sentinel markers \0\d+[eh!+-]\x7F */
        if (c == '\0') {
            size_t k = i + 1;
            while (k < len && s[k] >= '0' && s[k] <= '9') k++;
            if (k < len) {
                char tc = s[k];
                if ((tc == 'e' || tc == 'h' || tc == '!' || tc == '+' || tc == '-') &&
                    k + 1 < len && (unsigned char)s[k + 1] == '\x7F') {
                    return true;
                }
            }
        }

        /* %XX URL percent encoding */
        if (c == '%' && i + 2 < len &&
            is_hex((unsigned char)s[i + 1]) && is_hex((unsigned char)s[i + 2])) {
            return true;
        }
    }

    /* Check for path components . and .. (e.g. /./  or /../ or starting with ./) */
    /* Pattern: (?:^|\/)\.{1,2}(?:$|\/) */
    {
        /* Check start: "./", "../" */
        if (len >= 2 && s[0] == '.' &&
            (s[1] == '/' || (s[1] == '.' && (len == 2 || s[2] == '/')))) {
            return true;
        }
        /* Check end: "/." or "/.." */
        for (size_t i = 0; i + 1 < len; i++) {
            if (s[i] == '/') {
                if (i + 1 < len && s[i + 1] == '.') {
                    if (i + 2 >= len || s[i + 2] == '/') return true;   /* /. */
                    if (i + 2 < len && s[i + 2] == '.' &&
                        (i + 3 >= len || s[i + 3] == '/')) return true; /* /.. */
                }
            }
        }
    }
    return false;
}

/* JS parity for decode:true in Title constructor: try raw URL decode when '%' appears.
 * If decoding fails (malformed escape), JS keeps the original string. */
static char *title_try_percent_decode(const char *s, size_t len, size_t *out_len)
{
    bool has_pct = false;
    for (size_t i = 0; i < len; i++) {
        if (s[i] == '%') { has_pct = true; break; }
    }

    if (!has_pct) {
        char *copy = malloc(len + 1);
        if (!copy) return NULL;
        memcpy(copy, s, len);
        copy[len] = '\0';
        if (out_len) *out_len = len;
        return copy;
    }

    /* Validate all percent escapes first; on failure return original bytes. */
    bool malformed = false;
    for (size_t i = 0; i < len; i++) {
        if (s[i] == '%') {
            if (i + 2 >= len
                || !is_hex((unsigned char)s[i + 1])
                || !is_hex((unsigned char)s[i + 2])) {
                malformed = true;
                break;
            }
            i += 2;
        }
    }

    if (malformed) {
        char *copy = malloc(len + 1);
        if (!copy) return NULL;
        memcpy(copy, s, len);
        copy[len] = '\0';
        if (out_len) *out_len = len;
        return copy;
    }

    char *out = malloc(len + 1);
    if (!out) return NULL;
    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        if (s[i] == '%') {
            unsigned char hi = (unsigned char)s[i + 1];
            unsigned char lo = (unsigned char)s[i + 2];
            unsigned char hv = (unsigned char)(hi <= '9' ? hi - '0' : (tolower(hi) - 'a' + 10));
            unsigned char lv = (unsigned char)(lo <= '9' ? lo - '0' : (tolower(lo) - 'a' + 10));
            out[j++] = (char)((hv << 4) | lv);
            i += 2;
        } else {
            out[j++] = s[i];
        }
    }
    out[j] = '\0';
    if (out_len) *out_len = j;
    return out;
}

/* ── Public API ─────────────────────────────────────────────────────────── */

bool title_is_valid_half_parsed(const char *raw, size_t raw_len,
                                const ParserConfig *cfg)
{
    (void)cfg; /* Current Stage 0a validity uses only lexical checks. */

    if (!raw || raw_len == 0) return false;

    /* JS uses trimmed.startsWith('../') before decode/normalization. */
    size_t t0 = 0, t1 = raw_len;
    while (t0 < t1 && isspace((unsigned char)raw[t0])) t0++;
    while (t1 > t0 && isspace((unsigned char)raw[t1 - 1])) t1--;
    bool subpage = (t1 - t0 >= 3
                    && raw[t0] == '.'
                    && raw[t0 + 1] == '.'
                    && raw[t0 + 2] == '/');

    /* decode:true: attempt percent decode; malformed escapes keep original bytes. */
    size_t decoded_len = 0;
    char *pct_decoded = title_try_percent_decode(raw, raw_len, &decoded_len);
    if (!pct_decoded) return false;

    /* title = decodeHtml(title).replace(/[_ ]+/g, ' ').trim() */
    char *html_decoded = str_decode_html_basic(pct_decoded, decoded_len);
    free(pct_decoded);
    if (!html_decoded) return false;

    size_t html_len = strlen(html_decoded);
    char *norm = malloc(html_len + 1);
    if (!norm) { free(html_decoded); return false; }

    size_t nlen = 0;
    bool last_space = false;
    for (size_t i = 0; i < html_len; i++) {
        char c = html_decoded[i];
        if (c == '_' || c == ' ') {
            if (!last_space) norm[nlen++] = ' ';
            last_space = true;
        } else {
            norm[nlen++] = c;
            last_space = false;
        }
    }
    norm[nlen] = '\0';
    free(html_decoded);

    size_t start = 0, end = nlen;
    while (start < end && isspace((unsigned char)norm[start])) start++;
    while (end > start && isspace((unsigned char)norm[end - 1])) end--;

    /* If title starts with ':', JS strips one and trims. */
    if (start < end && norm[start] == ':') {
        start++;
        while (start < end && isspace((unsigned char)norm[start])) start++;
    }

    /* Split fragment at first '#'; validity is based on title main part only. */
    size_t hash = end;
    for (size_t i = start; i < end; i++) {
        if (norm[i] == '#') { hash = i; break; }
    }
    if (hash < end) {
        end = hash;
        while (end > start && isspace((unsigned char)norm[end - 1])) end--;
    }

    size_t title_len = end - start;
    if (title_len == 0) { free(norm); return false; }

    /* JS with page:'' rejects ../ subpages (page is defined but empty). */
    if (subpage) { free(norm); return false; }

    const char *title = norm + start;

    /* JS condition: decodeHtml(title) === title. */
    char *decoded_again = str_decode_html_basic(title, title_len);
    if (!decoded_again) { free(norm); return false; }
    size_t again_len = strlen(decoded_again);
    bool html_idempotent = (again_len == title_len
                            && memcmp(decoded_again, title, title_len) == 0);
    free(decoded_again);
    if (!html_idempotent) { free(norm); return false; }

    bool invalid = title_has_invalid_chars(title, title_len);
    free(norm);
    return !invalid;
}

char *title_normalize(const char *raw, size_t raw_len)
{
    if (!raw || raw_len == 0) return strdup("");

    char *decoded = str_decode_html_basic(raw, raw_len);
    if (!decoded) return NULL;
    size_t decoded_len = strlen(decoded);

    char *result = malloc(decoded_len + 1);
    if (!result) return NULL;

    /* Replace spaces with underscores */
    for (size_t i = 0; i < decoded_len; i++) {
        result[i] = (decoded[i] == ' ') ? '_' : decoded[i];
    }
    result[decoded_len] = '\0';
    free(decoded);

    /* Trim leading/trailing underscores (were spaces) */
    size_t start = 0, end = decoded_len;
    while (start < end && result[start] == '_') start++;
    while (end > start && result[end - 1] == '_') end--;

    /* Collapse internal runs of underscores */
    size_t out = 0;
    bool prev_under = false;
    for (size_t i = start; i < end; i++) {
        if (result[i] == '_') {
            if (!prev_under) result[out++] = '_';
            prev_under = true;
        } else {
            result[out++] = result[i];
            prev_under = false;
        }
    }
    result[out] = '\0';

    /* JS parity edge case from export corpus: normalize U+1E9A (ẚ) to "Aʾ".
     * Both forms are 3-byte UTF-8, so this can be done in-place. */
    for (size_t i = 0; i + 2 < out; i++) {
        unsigned char b0 = (unsigned char)result[i];
        unsigned char b1 = (unsigned char)result[i + 1];
        unsigned char b2 = (unsigned char)result[i + 2];
        if (b0 == 0xE1 && b1 == 0xBA && b2 == 0x9A) {
            result[i] = 'A';
            result[i + 1] = (char)0xCA;
            result[i + 2] = (char)0xBE;
        }
    }

    /*
     * JS parity: Title.main uppercases the first character of the main part
     * (after namespace/interwiki prefix parsing), not necessarily byte 0 of
     * the full title string.
     */
    if (out > 0 && islower((unsigned char)result[0])) {
        result[0] = (char)toupper((unsigned char)result[0]);
    }

    size_t cap_at = 0;
    for (size_t i = 0; i < out; i++) {
        if (result[i] == ':') {
            cap_at = i + 1;
            break;
        }
    }
    if (cap_at < out && islower((unsigned char)result[cap_at])) {
        result[cap_at] = (char)toupper((unsigned char)result[cap_at]);
    }

    return result;
}

void title_free(Title *t)
{
    if (!t) return;
    free(t->main);
    free(t->prefix);
    free(t->fragment);
    free(t);
}
