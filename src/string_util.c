/*
 * string_util.c — String helpers mirroring dist/util/string.js.
 */
#include "string_util.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include "config.h"

/* ── Sentinel marker formatting ──────────────────────────────────────────── */

void work_str_sentinel(size_t index, char ch, char *marker_buf, size_t *marker_len){
    /* Format: \0<decimal index><ch>\x7F */
    marker_buf[0] = '\0';
    int n = snprintf(marker_buf + 1, 28, "%zu%c", index, ch);
    marker_buf[1 + n] = '\x7F';
    if (marker_len) *marker_len = (size_t)(2 + n);
    /* The null byte at position 0 is intentional (it IS part of the sentinel).
     * The 'len' returned should be passed to memcpy, not strlen. */
}

/* ── tidy ─────────────────────────────────────────────────────────────────── */

char *str_tidy(const char *s, size_t len, size_t *out_len)
{
    char *result = malloc(len + 1);
    assert(result);
    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)s[i];
        /* JS parity for /[\0\x7F]|\r$/gmu:
         * remove NUL/DEL and any CR at end-of-line (before \n) or end-of-input. */
        if (c == '\0' || c == '\x7F') continue;
        if (c == '\r' && (i + 1 == len || s[i + 1] == '\n')) continue;
        result[j++] = (char)c;
    }
    result[j] = '\0';
    if (out_len) *out_len = j;
    return result;
}

void str_tidy_into(const char *s, size_t len,
                   char *buf, size_t cap,
                   size_t *out_len)
{
    /* The caller must have reserved at least len+1 bytes via
     * wiki_thread_buf_reserve().  Buffer ownership and resizing belong
     * exclusively to the thread_buffer layer. */
    assert(buf && cap >= len + 1);
    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == '\0' || c == '\x7F') continue;
        if (c == '\r' && (i + 1 == len || s[i + 1] == '\n')) continue;
        buf[j++] = (char)c;
    }
    buf[j] = '\0';
    if (out_len) *out_len = j;
}

/* ── removeComment ──────────────────────────────────────────────────────── */
/*
 * JS pattern: /\0\d+[cn]\x7F/gu
 * Removes half-parsed comment-like tokens (sentinel markers with type 'c' or 'n').
 */
char *str_remove_comment(const char *s, size_t len, size_t *out_len)
{
    char *result = malloc(len + 1);
    assert(result);
    size_t j = 0, i = 0;
    while (i < len) {
        if ((unsigned char)s[i] == '\0') {
            /* look for: \0 <digits> [cn] \x7F */
            size_t k = i + 1;
            while (k < len && s[k] >= '0' && s[k] <= '9') k++;
            if (k < len && (s[k] == 'c' || s[k] == 'n') &&
                k + 1 < len && (unsigned char)s[k + 1] == '\x7F') {
                /* skip the entire marker */
                i = k + 2;
                continue;
            }
        }
        result[j++] = s[i++];
    }
    result[j] = '\0';
    if (out_len) *out_len = j;
    return result;
}

/* ── trimLc ─────────────────────────────────────────────────────────────── */

char *str_trim_lc(const char *s, size_t len)
{
    /* Trim leading whitespace */
    size_t start = 0;
    while (start < len && isspace((unsigned char)s[start])) start++;
    /* Trim trailing whitespace */
    size_t end = len;
    while (end > start && isspace((unsigned char)s[end - 1])) end--;

    size_t out_len = end - start;
    char *result = malloc(out_len + 1);
    assert(result);
    for (size_t i = 0; i < out_len; i++) {
        result[i] = (char)tolower((unsigned char)s[start + i]);
    }
    result[out_len] = '\0';
    return result;
}

/* ── decodeHtmlBasic ────────────────────────────────────────────────────── */

/* Named HTML entities we handle (mirrors JS names object) */
static const struct { const char *name; char ch; } HTML_NAMES[] = {
    {"lt",     '<'},
    {"gt",     '>'},
    {"lbrack", '['},
    {"rbrack", ']'},
    {"lbrace", '{'},
    {"rbrace", '}'},
    {"nbsp",   ' '},  /* narrow no-break space → space */
    {"amp",    '&'},
    {"quot",   '"'},
};
#define HTML_NAMES_COUNT ((int)(sizeof(HTML_NAMES) / sizeof(HTML_NAMES[0])))

static char *encode_codepoint(uint32_t cp, char *out, int *bytes_written)
{
    /* Encode a Unicode codepoint as UTF-8 */
    if (cp < 0x80) {
        out[0] = (char)cp;
        *bytes_written = 1;
    } else if (cp < 0x800) {
        out[0] = (char)(0xC0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3F));
        *bytes_written = 2;
    } else if (cp < 0x10000) {
        out[0] = (char)(0xE0 | (cp >> 12));
        out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (char)(0x80 | (cp & 0x3F));
        *bytes_written = 3;
    } else {
        out[0] = (char)(0xF0 | (cp >> 18));
        out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
        out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[3] = (char)(0x80 | (cp & 0x3F));
        *bytes_written = 4;
    }
    return out;
}

char *str_decode_html_basic(const char *s, size_t len)
{
    /* Allocate generous buffer: worst case same length + some slack */
    size_t cap = len + 4;
    char *result = malloc(cap);
    assert(result);
    size_t j = 0;

    for (size_t i = 0; i < len; ) {
        if (s[i] != '&') {
            result[j++] = s[i++];
            continue;
        }
        /* Find closing semicolon */
        size_t k = i + 1;
        while (k < len && s[k] != ';' && s[k] != '&' && s[k] != '\n') k++;
        if (k >= len || s[k] != ';') {
            /* No closing semicolon — emit literally */
            result[j++] = s[i++];
            continue;
        }
        const char *ref  = s + i + 1;
        size_t      rlen = k - i - 1;

        if (rlen > 0 && ref[0] == '#') {
            /* Numeric character reference */
            uint32_t cp = 0;
            bool is_hex = (rlen > 1 && (ref[1] == 'x' || ref[1] == 'X'));
            const char *digits = ref + (is_hex ? 2 : 1);
            size_t dlen = rlen - (is_hex ? 2 : 1);
            for (size_t d = 0; d < dlen; d++) {
                char dc = digits[d];
                if (is_hex) {
                    if (dc >= '0' && dc <= '9') cp = cp * 16 + (uint32_t)(dc - '0');
                    else if (dc >= 'a' && dc <= 'f') cp = cp * 16 + (uint32_t)(dc - 'a' + 10);
                    else if (dc >= 'A' && dc <= 'F') cp = cp * 16 + (uint32_t)(dc - 'A' + 10);
                    else { cp = 0; break; }
                } else {
                    if (dc >= '0' && dc <= '9') cp = cp * 10 + (uint32_t)(dc - '0');
                    else { cp = 0; break; }
                }
            }
            if (cp > 0 && cp <= 0x10FFFF) {
                char tmp[4];
                int nb = 0;
                encode_codepoint(cp, tmp, &nb);
                /* Grow result if needed */
                while (j + (size_t)nb + 1 > cap) { cap *= 2; result = realloc(result, cap); assert(result); }
                memcpy(result + j, tmp, (size_t)nb);
                j += (size_t)nb;
                i = k + 1;
                continue;
            }
        } else {
            /* Possibly a named entity */
            /* We do a case-insensitive search in our small table */
            char lower_ref[16] = {0};
            size_t copy_len = rlen < 15 ? rlen : 15;
            for (size_t d = 0; d < copy_len; d++)
                lower_ref[d] = (char)tolower((unsigned char)ref[d]);

            bool found = false;
            for (int n = 0; n < HTML_NAMES_COUNT; n++) {
                if (strcmp(lower_ref, HTML_NAMES[n].name) == 0) {
                    result[j++] = HTML_NAMES[n].ch;
                    i = k + 1;
                    found = true;
                    break;
                }
            }
            if (found) continue;
        }
        /* Unknown or invalid entity — emit literally */
        result[j++] = s[i++];
    }
    result[j] = '\0';
    return result;
}

/* ── restore ────────────────────────────────────────────────────────────── */
/*
 * JS: s.replace(/\0(\d+)\x7F/gu, (_, p1) => stack[p1])
 */
char *str_restore(const char *s, size_t len,
                  const char **stack, size_t stack_count,
                  const size_t *stack_lengths,
                  size_t *out_len)
{
    /* Two-pass: first count output size, then emit */
    size_t cap = len * 2 + 1;
    char *result = malloc(cap);
    assert(result);
    size_t j = 0;

    for (size_t i = 0; i < len; ) {
        if ((unsigned char)s[i] == '\0') {
            /* Try to parse \0<digits>\x7F */
            size_t k = i + 1;
            while (k < len && s[k] >= '0' && s[k] <= '9') k++;
            if (k < len && k > i + 1 && (unsigned char)s[k] == '\x7F') {
                /* Parse the index */
                size_t idx = 0;
                for (size_t d = i + 1; d < k; d++) {
                    idx = idx * 10 + (size_t)(s[d] - '0');
                }
                if (idx < stack_count && stack[idx]) {
                    const char *rep = stack[idx];
                    /* Use stored length when available (binary-safe for entries
                     * that contain embedded NUL bytes from token sentinels). */
                    size_t replen = (stack_lengths && stack_lengths[idx])
                                    ? stack_lengths[idx]
                                    : strlen(rep);
                    while (j + replen + 1 > cap) {
                        cap *= 2;
                        result = realloc(result, cap);
                        assert(result);
                    }
                    memcpy(result + j, rep, replen);
                    j += replen;
                    i = k + 1;
                    continue;
                }
            }
        }
        if (j + 2 > cap) { cap *= 2; result = realloc(result, cap); assert(result); }
        result[j++] = s[i++];
    }
    result[j] = '\0';
    if (out_len) *out_len = j;
    return result;
}

/* ── UTF-8 helpers ───────────────────────────────────────────────────────── */

int utf8_char_len(unsigned char c)
{
    if (c < 0x80)  return 1;
    if (c < 0xE0)  return 2;
    if (c < 0xF0)  return 3;
    return 4;
}

uint32_t utf8_tolower_codepoint(uint32_t cp)
{
    /* Fast path for ASCII */
    if (cp < 128) return (uint32_t)tolower((int)cp);
    /* Full Unicode folding would require ICU; return as-is for non-ASCII. */
    return cp;
}

/* ── str_istr ────────────────────────────────────────────────────────────── */

const char *str_istr(const char *haystack, size_t hlen,
                     const char *needle,   size_t nlen)
{
    if (nlen == 0) return haystack;
    if (nlen > hlen) return NULL;
    for (size_t i = 0; i <= hlen - nlen; i++) {
        bool match = true;
        for (size_t j = 0; j < nlen; j++) {
            if (tolower((unsigned char)haystack[i + j]) !=
                tolower((unsigned char)needle[j])) {
                match = false;
                break;
            }
        }
        if (match) return haystack + i;
    }
    return NULL;
}

char *str_extract_interwiki(const char *s, size_t len, const ParserConfig *cfg, size_t *consumed)
{
    if (!s || len == 0 || !cfg) {
        if (consumed) *consumed = 0;
        return NULL;
    }
    if (cfg->interwiki.count == 0) {
        if (consumed) *consumed = 0;
        return NULL;
    }

    /* Build normalized buffer (underscores -> spaces) and a map from temp
     * index to original raw byte index so we can compute consumed bytes. */
    char *temp = malloc(len + 1);
    assert(temp);
    size_t *pos_map = malloc((len + 1) * sizeof(size_t));
    assert(pos_map);
    size_t tlen = 0;
    for (size_t i = 0; i < len; i++) {
        temp[tlen] = (s[i] == '_') ? ' ' : s[i];
        pos_map[tlen] = i;
        tlen++;
    }
    temp[tlen] = '\0';

    /* Trim leading whitespace */
    size_t start = 0;
    while (start < tlen && isspace((unsigned char)temp[start])) start++;

    /* Optional leading colon (force prefix removal) */
    if (start < tlen && temp[start] == ':') {
        start++;
        while (start < tlen && isspace((unsigned char)temp[start])) start++;
    }

    for (size_t k = 0; k < cfg->interwiki.count; k++) {
        const char *iw = cfg->interwiki.items[k];
        if (!iw) continue;
        size_t iwlen = strlen(iw);
        if (start + iwlen > tlen) continue;
        bool match = true;
        for (size_t j = 0; j < iwlen; j++) {
            if (tolower((unsigned char)temp[start + j]) != tolower((unsigned char)iw[j])) { match = false; break; }
        }
        if (!match) continue;
        size_t j = start + iwlen;
        while (j < tlen && isspace((unsigned char)temp[j])) j++;
        if (j < tlen && temp[j] == ':') {
            /* matched prefix + optional spaces + ':'; consumed bytes end at raw index of ':' + 1 */
            size_t raw_consumed = pos_map[j] + 1;
            char *out = strdup(iw);
            if (out) {
                for (char *p = out; *p; ++p) *p = (char)tolower((unsigned char)*p);
            }
            free(temp);
            free(pos_map);
            if (consumed) *consumed = raw_consumed;
            return out;
        }
    }

    free(temp);
    free(pos_map);
    if (consumed) *consumed = 0;
    return NULL;
}
