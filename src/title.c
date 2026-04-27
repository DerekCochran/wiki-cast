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

static char *strndup0(const char *s, size_t len)
{
    char *out = malloc(len + 1);
    if (!out) return NULL;
    if (len > 0) memcpy(out, s, len);
    out[len] = '\0';
    return out;
}

static char *trim_lc_n(const char *s, size_t len)
{
    size_t start = 0;
    size_t end = len;
    while (start < end && isspace((unsigned char)s[start])) start++;
    while (end > start && isspace((unsigned char)s[end - 1])) end--;
    char *out = malloc(end - start + 1);
    if (!out) return NULL;
    for (size_t i = start; i < end; i++) {
        out[i - start] = (char)tolower((unsigned char)s[i]);
    }
    out[end - start] = '\0';
    return out;
}

static const char *title_namespace_name(const ParserConfig *cfg, int ns)
{
    if (!cfg) return "";
    for (size_t i = 0; i < cfg->ns_count; i++) {
        if (cfg->namespaces[i].num == ns && cfg->namespaces[i].name) {
            return cfg->namespaces[i].name;
        }
    }
    return "";
}

static int title_lookup_namespace(const ParserConfig *cfg, const char *s, size_t len)
{
    if (!cfg || !s) return 0;
    char *key = trim_lc_n(s, len);
    if (!key) return 0;
    int ns = 0;
    for (size_t i = 0; i < cfg->ns_count; i++) {
        const char *name = cfg->namespaces[i].name;
        if (name && strcasecmp(name, key) == 0) {
            ns = cfg->namespaces[i].num;
            break;
        }
    }
    free(key);
    return ns;
}

static void title_replace_1e9a(char *s)
{
    if (!s) return;
    size_t len = strlen(s);
    for (size_t i = 0; i + 2 < len; i++) {
        unsigned char b0 = (unsigned char)s[i];
        unsigned char b1 = (unsigned char)s[i + 1];
        unsigned char b2 = (unsigned char)s[i + 2];
        if (b0 == 0xE1 && b1 == 0xBA && b2 == 0x9A) {
            s[i] = 'A';
            s[i + 1] = (char)0xCA;
            s[i + 2] = (char)0xBE;
        }
    }
}

static size_t utf8_encode_codepoint(uint32_t cp, char *out)
{
    if (cp < 0x80) {
        out[0] = (char)cp;
        return 1;
    }
    if (cp < 0x800) {
        out[0] = (char)(0xC0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3F));
        return 2;
    }
    if (cp < 0x10000) {
        out[0] = (char)(0xE0 | (cp >> 12));
        out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (char)(0x80 | (cp & 0x3F));
        return 3;
    }
    out[0] = (char)(0xF0 | (cp >> 18));
    out[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
    out[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
    out[3] = (char)(0x80 | (cp & 0x3F));
    return 4;
}

static size_t title_uppercase_first_codepoint(char *s, size_t len)
{
    if (len == 0) return 0;
    int char_len = utf8_char_len((unsigned char)s[0]);
    if (char_len <= 0 || (size_t)char_len > len) return len;

    uint32_t cp = 0;
    if (char_len == 1) {
        cp = (unsigned char)s[0];
    } else if (char_len == 2) {
        cp = ((uint32_t)(s[0] & 0x1F) << 6)
           | ((uint32_t)(unsigned char)s[1] & 0x3F);
    } else if (char_len == 3) {
        cp = ((uint32_t)(s[0] & 0x0F) << 12)
           | ((uint32_t)(unsigned char)s[1] & 0x3F) << 6
           | ((uint32_t)(unsigned char)s[2] & 0x3F);
    } else {
        cp = ((uint32_t)(s[0] & 0x07) << 18)
           | ((uint32_t)(unsigned char)s[1] & 0x3F) << 12
           | ((uint32_t)(unsigned char)s[2] & 0x3F) << 6
           | ((uint32_t)(unsigned char)s[3] & 0x3F);
    }

    if (cp == 0x00DF) {
        const char buf[2] = {'S', 'S'};
        size_t wrote = 2;
        size_t new_len = len - (size_t)char_len + wrote;
        memmove(s + wrote, s + char_len, len - (size_t)char_len + 1);
        memcpy(s, buf, wrote);
        return new_len;
    }

    uint32_t up = utf8_toupper_codepoint(cp);
    if (up == cp) return len;

    char buf[4];
    size_t wrote = utf8_encode_codepoint(up, buf);
    size_t new_len = len - (size_t)char_len + wrote;
    memmove(s + wrote, s + char_len, len - (size_t)char_len + 1);
    memcpy(s, buf, wrote);
    return new_len;
}

static char *title_main_from_text(const char *s, size_t len)
{
    char *out = strndup0(s, len);
    if (!out) return NULL;
    size_t in = 0, out_i = 0;
    while (out[in] && (out[in] == '_' || isspace((unsigned char)out[in]))) in++;
    bool last_space = false;
    for (; out[in]; in++) {
        char c = out[in];
        if (c == '_') c = ' ';
        if (isspace((unsigned char)c)) {
            if (!last_space) out[out_i++] = ' ';
            last_space = true;
        } else {
            out[out_i++] = c;
            last_space = false;
        }
    }
    while (out_i > 0 && out[out_i - 1] == ' ') out_i--;
    out[out_i] = '\0';
    title_replace_1e9a(out);
    out_i = title_uppercase_first_codepoint(out, out_i);
    out[out_i] = '\0';
    return out;
}

static char *title_compose_resolved(const Title *t, const char *page)
{
    if (!t || !t->main || !t->prefix || !t->interwiki) return NULL;
    size_t pre_len = strlen(t->interwiki);
    size_t ns_len = strlen(t->prefix);
    size_t main_len = strlen(t->main);
    size_t base_len = pre_len + (pre_len ? 1 : 0) + ns_len + (ns_len ? 1 : 0) + main_len;
    char *base = malloc(base_len + 1);
    if (!base) return NULL;
    size_t pos = 0;
    if (pre_len > 0) {
        memcpy(base + pos, t->interwiki, pre_len);
        pos += pre_len;
        base[pos++] = ':';
    }
    if (ns_len > 0) {
        memcpy(base + pos, t->prefix, ns_len);
        pos += ns_len;
        base[pos++] = ':';
    }
    memcpy(base + pos, t->main, main_len);
    pos += main_len;
    base[pos] = '\0';
    for (size_t i = 0; i < pos; i++) {
        if (base[i] == ' ') base[i] = '_';
    }

    if (base[0] == '/') {
        size_t page_len = page ? strlen(page) : 0;
        while (pos > 0 && base[pos - 1] == '/') pos--;
        char *resolved = malloc(page_len + pos + 1);
        if (!resolved) {
            free(base);
            return NULL;
        }
        if (page_len > 0) memcpy(resolved, page, page_len);
        memcpy(resolved + page_len, base, pos);
        resolved[page_len + pos] = '\0';
        free(base);
        return resolved;
    }

    if (strncmp(base, "../", 3) == 0 && page && strchr(page, '/')) {
        size_t level = 0;
        const char *sub = base;
        while (strncmp(sub, "../", 3) == 0) {
            level++;
            sub += 3;
        }
        size_t page_len = strlen(page);
        size_t dir_count = 1;
        for (size_t i = 0; i < page_len; i++) {
            if (page[i] == '/') dir_count++;
        }
        if (dir_count > level) {
            size_t keep = page_len;
            size_t drops = level;
            while (keep > 0 && drops > 0) {
                keep--;
                if (page[keep] == '/') drops--;
            }
            while (keep > 0 && page[keep - 1] != '/') keep--;
            size_t sub_len = strlen(sub);
            char *resolved = malloc(keep + (sub_len ? 1 : 0) + sub_len + 1);
            if (!resolved) {
                free(base);
                return NULL;
            }
            memcpy(resolved, page, keep);
            size_t out = keep;
            if (sub_len > 0 && out > 0 && resolved[out - 1] != '/') resolved[out++] = '/';
            if (sub_len > 0) {
                memcpy(resolved + out, sub, sub_len);
                out += sub_len;
            }
            resolved[out] = '\0';
            free(base);
            return resolved;
        }
    }

    return base;
}

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
    Title *t = title_parse_half_parsed(raw, raw_len, 0, cfg, true, "");
    bool valid = t && t->valid;
    title_free(t);
    return valid;
}

Title *title_parse_half_parsed(const char *raw, size_t raw_len,
                               int default_ns,
                               const ParserConfig *cfg,
                               bool self_link,
                               const char *page)
{
    if (!raw) return NULL;

    Title *t = calloc(1, sizeof(*t));
    if (!t) return NULL;
    t->prefix = strdup("");
    t->interwiki = strdup("");
    if (!t->prefix || !t->interwiki) {
        title_free(t);
        return NULL;
    }

    size_t t0 = 0, t1 = raw_len;
    while (t0 < t1 && isspace((unsigned char)raw[t0])) t0++;
    while (t1 > t0 && isspace((unsigned char)raw[t1 - 1])) t1--;
    bool subpage = (t1 - t0 >= 3
                    && raw[t0] == '.'
                    && raw[t0 + 1] == '.'
                    && raw[t0 + 2] == '/');
    bool page_subpage = (page && *page && raw_len - t0 >= 1 && raw[t0] == '/');

    size_t decoded_len = 0;
    char *pct_decoded = title_try_percent_decode(raw, raw_len, &decoded_len);
    if (!pct_decoded) {
        title_free(t);
        return NULL;
    }

    char *html_decoded = str_decode_html_basic(pct_decoded, decoded_len);
    free(pct_decoded);
    if (!html_decoded) {
        title_free(t);
        return NULL;
    }

    size_t html_len = strlen(html_decoded);
    char *norm = malloc(html_len + 1);
    if (!norm) {
        free(html_decoded);
        title_free(t);
        return NULL;
    }

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

    int ns = default_ns;
    const char *title = norm + start;
    size_t title_len = end - start;

    if (subpage || page_subpage) {
        ns = 0;
    } else {
        if (title_len > 0 && title[0] == ':') {
            ns = 0;
            title++;
            title_len--;
            while (title_len > 0 && isspace((unsigned char)title[0])) {
                title++;
                title_len--;
            }
        }

        if (default_ns == 0) {
            size_t iw_consumed = 0;
            char *iw = str_extract_interwiki(title, title_len, cfg, &iw_consumed);
            if (iw) {
                free(t->interwiki);
                t->interwiki = iw;
                if (iw_consumed <= title_len) {
                    title += iw_consumed;
                    title_len -= iw_consumed;
                }
            }
        }

        const char *colon = memchr(title, ':', title_len);
        if (colon) {
            int found = title_lookup_namespace(cfg, title, (size_t)(colon - title));
            if (found) {
                ns = found;
                title_len -= (size_t)(colon + 1 - title);
                title = colon + 1;
                while (title_len > 0 && isspace((unsigned char)title[0])) {
                    title++;
                    title_len--;
                }
            }
        }
    }
    t->ns = ns;

    size_t hash = title_len;
    for (size_t i = 0; i < title_len; i++) {
        if (title[i] == '#') {
            hash = i;
            break;
        }
    }
    if (hash < title_len) {
        const char *fragment = title + hash + 1;
        size_t fragment_len = title_len - hash - 1;
        while (fragment_len > 0 && isspace((unsigned char)fragment[fragment_len - 1])) fragment_len--;
        size_t frag_dec_len = 0;
        char *frag_pct = title_try_percent_decode(fragment, fragment_len, &frag_dec_len);
        if (frag_pct) {
            char *frag_html = str_decode_html_basic(frag_pct, frag_dec_len);
            free(frag_pct);
            if (frag_html) {
                size_t flen = strlen(frag_html);
                while (flen > 0 && isspace((unsigned char)frag_html[flen - 1])) flen--;
                for (size_t i = 0; i < flen; i++) {
                    if (frag_html[i] == ' ') frag_html[i] = '_';
                }
                frag_html[flen] = '\0';
                t->fragment = frag_html;
            }
        }
        title_len = hash;
        while (title_len > 0 && isspace((unsigned char)title[title_len - 1])) title_len--;
    }

    t->main = title_main_from_text(title, title_len);
    if (!t->main) {
        free(norm);
        title_free(t);
        return NULL;
    }

    free(t->prefix);
    t->prefix = strdup(title_namespace_name(cfg, ns));
    if (!t->prefix) {
        free(norm);
        title_free(t);
        return NULL;
    }

    size_t level = 0;
    const char *sub = title;
    if (subpage) {
        while (title_len >= (size_t)((level + 1) * 3) && strncmp(sub, "../", 3) == 0) {
            level++;
            sub += 3;
            title_len -= 3;
        }
    }

    char *decoded_again = str_decode_html_basic(title, title_len);
    bool html_idempotent = decoded_again
        && strlen(decoded_again) == title_len
        && memcmp(decoded_again, title, title_len) == 0;
    free(decoded_again);

    bool page_ok = true;
    if (level > 0 && page != NULL) {
        size_t page_parts = 1;
        for (const char *p = page; *p; ++p) {
            if (*p == '/') page_parts++;
        }
        page_ok = page_parts > level;
    }

    t->valid = (t->main[0] != '\0'
                || t->interwiki[0] != '\0'
                || (self_link && t->ns == 0 && t->fragment != NULL))
        && html_idempotent
        && page_ok
        && !title_has_invalid_chars(sub, strlen(sub));

    t->title = title_compose_resolved(t, page);
    if (!t->title) {
        free(norm);
        title_free(t);
        return NULL;
    }

    free(norm);
    return t;
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
    out = title_uppercase_first_codepoint(result, out);

    size_t cap_at = 0;
    for (size_t i = 0; i < out; i++) {
        if (result[i] == ':') {
            cap_at = i + 1;
            break;
        }
    }
    if (cap_at < out) {
        out = cap_at + title_uppercase_first_codepoint(result + cap_at, out - cap_at);
    }

    return result;
}

void title_free(Title *t)
{
    if (!t) return;
    free(t->main);
    free(t->prefix);
    free(t->fragment);
    free(t->interwiki);
    free(t->title);
    free(t);
}
