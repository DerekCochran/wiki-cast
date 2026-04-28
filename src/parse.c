/*
 * parse.c — Main entry point: wiki_parse()
 *
 * Drives the 11-stage pipeline and returns the root Token.
 *
 * Stage pipeline (mirrors Token.prototype.parse / parseOnce in JS):
 *
 *   Stage -1 (pre-parse): tidy \0 and \x7F from input
 *   Stage  0: parseRedirect (root only) then parseCommentAndExt
 *   Stage  1: parseBraces
 *   Stage  2: parseHtml
 *   Stage  3: parseTable           (not yet implemented — stub)
 *   Stage  4: parseHrAndDoubleUnderscore (stub)
 *   Stage  5: parseLinks           (stub)
 *   Stage  6: parseQuotes          (stub)
 *   Stage  7: parseExternalLinks   (stub)
 *   Stage  8: parseMagicLinks      (stub)
 *   Stage  9: parseList            (stub)
 *   Stage 10: parseConverter       (stub)
 *
 * After all requested stages, build() expands sentinel markers into the
 * child token tree.
 */
#include "parse.h"
#include "token.h"
#include "config.h"
#include "accum.h"
#include "string_util.h"
#include "thread_buffer.h"
#include "build.h"
#include "log.h"
#include "parser/redirect.h"
#include "parser/braces.h"
#include "parser/links.h"
#include "parser/quotes.h"
#include "parser/converter.h"
#include "parser/list.h"
#include "parser/table.h"

/* build() is declared in build.h */
#include "parser/comment_and_ext.h"
#include "parser/hr_and_double_underscore.h"
#include "parser/html.h"
#include "parser/magic_links.h"

#include <stdlib.h>
#include <string.h>
#include "parser/external_links.h"
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <ctype.h>
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>

/* ── Orphan-token cleanup helpers ──────────────────────────────────────────
 *
 * After build(), any accumulator token that is not reachable from the root
 * tree is an orphan.  Orphans arise when a later-stage parser (e.g.
 * parse_braces, parse_links) stores content containing an earlier-stage
 * sentinel via a NUL-terminated string API (strdup / str_restore).  The
 * embedded \0 byte of the sentinel is silently truncated, so the sentinel
 * is never written into a token text child that build_from_str can expand.
 * The token is then in the accumulator but not in the tree → leak.
 *
 * Fix: walk the final tree, collect all live token pointers, then iterate
 * the accumulator and call token_free_shallow() on every non-live entry.
 * token_free_shallow() frees the token struct and its text children but
 * does NOT recurse into token children, so sub-tokens that each have their
 * own accum entry are freed individually without double-freeing.
 */
static void collect_tree_tokens(const Token *t, Token ***arr,
                                 size_t *count, size_t *cap)
{
    if (!t) return;
    if (*count >= *cap) {
        *cap *= 2;
        *arr  = realloc(*arr, *cap * sizeof(Token *));
        assert(*arr);
    }
    (*arr)[(*count)++] = (Token *)t;
    for (size_t i = 0; i < t->child_count; i++) {
        if (!t->children[i].is_text)
            collect_tree_tokens(t->children[i].token, arr, count, cap);
    }
}

static int cmp_token_ptr(const void *a, const void *b)
{
    /* Compare token pointers numerically for qsort / bsearch */
    uintptr_t pa = (uintptr_t)*(const Token * const *)a;
    uintptr_t pb = (uintptr_t)*(const Token * const *)b;
    return (pa > pb) - (pa < pb);
}

static void free_accum_orphans(const Token *root, Accum *accum)
{
    size_t cap   = 64 + accum->count;
    size_t count = 0;
    Token **live = malloc(cap * sizeof(Token *));
    if (!live) return;

    collect_tree_tokens(root, &live, &count, &cap);
    qsort(live, count, sizeof(Token *), cmp_token_ptr);

    for (size_t i = 0; i < accum->count; i++) {
        Token *t = accum->tokens[i];
        if (!t) continue;

        /* Binary search in sorted live set */
        size_t lo = 0, hi = count;
        bool found = false;
        while (lo < hi) {
            size_t mid = (lo + hi) / 2;
            if (live[mid] == t) { found = true; break; }
            if ((uintptr_t)live[mid] < (uintptr_t)t) lo = mid + 1;
            else hi = mid;
        }
        if (!found) {
            /* Orphan: free shallowly — token children are separate accum
             * entries and are freed when their own slot is encountered. */
            token_free_shallow(t);
        }
    }
    free(live);
}

static bool mem_has(const char *s, size_t len, const char *needle)
{
    size_t nlen = needle ? strlen(needle) : 0;
    if (!s || nlen == 0 || len < nlen) return false;
    for (size_t i = 0; i + nlen <= len; i++) {
        if (memcmp(s + i, needle, nlen) == 0) return true;
    }
    return false;
}

/* Write a JSON-escaped string of given length to fp (surrounded by quotes). */
static void json_write_escaped_len(const char *s, size_t len, FILE *fp)
{
    if (!fp) return;
    fputc('"', fp);
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)s[i];
        if (c == '"') fputs("\\\"", fp);
        else if (c == '\\') fputs("\\\\", fp);
        else if (c == '\n') fputs("\\n", fp);
        else if (c == '\r') fputs("\\r", fp);
        else if (c == '\t') fputs("\\t", fp);
        else if (c < 0x20) fprintf(fp, "\\u%04x", c);
        else fputc(c, fp);
    }
    fputc('"', fp);
}

/* Append a JSON snapshot representing the current root content (ws)
 * to <stage_log_dir>/native-stage.log. The ws buffer is scanned for
 * sentinel markers (\0<digits><ch>\x7F) and token entries from the
 * accumulator are embedded via token_to_json(). */
static void append_native_stage_json(const char *stage_log_dir, int stage, ThreadBuf *ws, Accum *accum)
{
    if (!stage_log_dir || !ws) return;
    char pathbuf[1024];
    snprintf(pathbuf, sizeof(pathbuf), "%s/native-stage.log", stage_log_dir);
    FILE *f = fopen(pathbuf, "a");
    if (!f) return;
    fprintf(f, "--- Stage %d --\n", stage);
    /* Emit a root object with childNodes array */
    fputs("{\"type\":\"root\",\"childNodes\":[", f);

    bool first = true;
    size_t pos = 0;
    while (pos < ws->len) {
        if ((unsigned char)ws->buf[pos] == '\0') {
            /* sentinel: \0<digits><ch>\x7F */
            pos++;
            size_t numStart = pos;
            while (pos < ws->len && isdigit((unsigned char)ws->buf[pos])) pos++;
            size_t numLen = pos - numStart;
            if (numLen == 0) continue;
            char numbuf[32];
            if (numLen >= sizeof(numbuf)) continue;
            memcpy(numbuf, ws->buf + numStart, numLen);
            numbuf[numLen] = '\0';
            long idx = strtol(numbuf, NULL, 10);
            /* skip the sentinel char and the trailing 0x7F if present */
            if (pos < ws->len) pos++;
            if (pos < ws->len && (unsigned char)ws->buf[pos] == 0x7F) pos++;

            if (!first) fputc(',', f);
            first = false;

            if (idx >= 0 && (size_t)idx < accum->count && accum->tokens[idx]) {
                token_to_json(accum->tokens[idx], f);
            } else {
                fputs("null", f);
            }
        } else {
            size_t start = pos;
            while (pos < ws->len && (unsigned char)ws->buf[pos] != '\0') pos++;
            size_t seglen = pos - start;
            if (!first) fputc(',', f);
            first = false;
            fputs("{\"type\":\"text\",\"data\":", f);
            json_write_escaped_len(ws->buf + start, seglen, f);
            fputc('}', f);
        }
    }

    fputs("]}\n\n", f);
    fclose(f);
}

static void parse_list_skip_first_line(ThreadBuf *scratch, const ParserConfig *cfg, Accum *accum)
{
    if (!scratch || !scratch->buf || scratch->len == 0) return;

    size_t nl = SIZE_MAX;
    for (size_t i = 0; i < scratch->len; i++) {
        if (scratch->buf[i] == '\n') {
            nl = i;
            break;
        }
    }
    if (nl == SIZE_MAX || nl + 1 >= scratch->len) return;

    size_t prefix_len = nl + 1;
    size_t rest_len = scratch->len - prefix_len;
    char *prefix = malloc(prefix_len + 1);
    if (!prefix) return;
    memcpy(prefix, scratch->buf, prefix_len);
    prefix[prefix_len] = '\0';

    const char *rest = scratch->buf + prefix_len;
    wiki_thread_buf_set(scratch, rest, rest_len);
    parse_list(scratch, cfg, accum);

    size_t out_len = prefix_len + scratch->len;
    char *out = malloc(out_len + 1);
    if (!out) {
        free(prefix);
        return;
    }
    memcpy(out, prefix, prefix_len);
    memcpy(out + prefix_len, scratch->buf, scratch->len);
    out[out_len] = '\0';

    wiki_thread_buf_set(scratch, out, out_len);
    free(out);
    free(prefix);
}

static bool should_postprocess_plain(const Token *t)
{
    if (!t || !(t->type == TOKEN_PLAIN || t->type == TOKEN_EXT_INNER) || !t->type_name) return false;
    return strcmp(t->type_name, "td-inner") == 0
    || strcmp(t->type_name, "ext-inner") == 0
        || strcmp(t->type_name, "heading-title") == 0;
}

static bool ext_inner_allows_nested_parse(const char *name)
{
    if (!name || !*name) return false;

    /* JS ExtToken parity: only specific ext tags parse inner wikitext.
     * Unlisted tags (for example score/syntaxhighlight/math) are nowiki-like. */
    return strcmp(name, "pre") == 0
        || strcmp(name, "indicator") == 0
        || strcmp(name, "poem") == 0
        || strcmp(name, "ref") == 0
        || strcmp(name, "option") == 0
        || strcmp(name, "combooption") == 0
        || strcmp(name, "tab") == 0
        || strcmp(name, "tabs") == 0
        || strcmp(name, "poll") == 0
        || strcmp(name, "seo") == 0
        || strcmp(name, "langconvert") == 0
        || strcmp(name, "phonos") == 0
        || strcmp(name, "dynamicpagelist") == 0
        || strcmp(name, "inputbox") == 0
        || strcmp(name, "references") == 0
        || strcmp(name, "choose") == 0
        || strcmp(name, "combobox") == 0
        || strcmp(name, "gallery") == 0
        || strcmp(name, "imagemap") == 0
        || strcmp(name, "hiero") == 0
        || strcmp(name, "categorytree") == 0;
}

    static void postprocess_nested_plain(Token *t, const ParserConfig *cfg, Accum *accum);

static void trim_view_local(const char **ptr, size_t *len)
{
    const char *s = *ptr;
    size_t l = *len;
    size_t a = 0;
    while (a < l && isspace((unsigned char)s[a])) a++;
    size_t b = l;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    *ptr = s + a;
    *len = b - a;
}

static int namespace_from_title(const char *title_ptr, size_t title_len, const ParserConfig *cfg)
{
    if (!title_ptr || !cfg) return 0;
    for (size_t i = 0; i < title_len; i++) {
        if (title_ptr[i] != ':') continue;
        size_t pre_len = i;
        for (size_t k = 0; k < cfg->ns_count; k++) {
            const char *nm = cfg->namespaces[k].name;
            if (!nm || strlen(nm) != pre_len) continue;
            if (strncasecmp(nm, title_ptr, pre_len) == 0) {
                return cfg->namespaces[k].num;
            }
        }
        return 0;
    }
    return 0;
}

static void parse_quotes_stage6_per_line(ThreadBuf *ws, const ParserConfig *cfg, Accum *accum);

static Token *parse_gallery_caption_fragment(const char *s, size_t len,
                                             const ParserConfig *cfg, Accum *accum)
{
    if (!s) return NULL;

    ThreadBuf *scratch = wiki_thread_buf_acquire_scratch();
    wiki_thread_buf_set(scratch, s, len);

    parse_comment_and_ext(scratch, cfg, accum, false);
    parse_braces(scratch, cfg, accum);
    parse_html(scratch, cfg, accum);
    parse_links(scratch, cfg, accum, NULL, false);
    parse_quotes_stage6_per_line(scratch, cfg, accum);
    parse_external_links(scratch, cfg, accum, true);
    parse_magic_links(scratch, cfg, accum);

    Token *inner = token_new(TOKEN_PLAIN, "text");
    if (!inner) {
        wiki_thread_buf_release_scratch(scratch);
        return NULL;
    }
    build_from_str(inner, scratch->buf, scratch->len, accum);
    build_token_recursive(inner, accum);
    wiki_thread_buf_release_scratch(scratch);
    return inner;
}

static Token *make_empty_noinclude(Accum *accum)
{
    Token *n = token_new(TOKEN_NOINCLUDE, "noinclude");
    if (!n) return NULL;
    token_append_text_n(n, "", 0);
    accum_push(accum, n);
    return n;
}

static Token *parse_single_link_token(const char *s, size_t len,
                                      const ParserConfig *cfg, Accum *accum)
{
    if (!s) return NULL;

    char *wrapped = malloc(len + 5);
    if (!wrapped) return NULL;
    wrapped[0] = '[';
    wrapped[1] = '[';
    memcpy(wrapped + 2, s, len);
    wrapped[2 + len] = ']';
    wrapped[3 + len] = ']';
    wrapped[4 + len] = '\0';

    ThreadBuf tmp_tb;
    tmp_tb.buf = wrapped;
    tmp_tb.len = len + 4;
    tmp_tb.cap = len + 5;
    tmp_tb.shrink_size = (size_t)-1;
    tmp_tb.target_size = tmp_tb.cap;

    parse_links(&tmp_tb, cfg, accum, NULL, false);

    Token *tmp = token_new(TOKEN_PLAIN, "imagemap-link-inner");
    if (!tmp) return NULL;
    build_from_str(tmp, tmp_tb.buf, tmp_tb.len, accum);
    build_token_recursive(tmp, accum);

    Token *out = NULL;
    if (tmp->child_count == 1 && !tmp->children[0].is_text && tmp->children[0].token
        && tmp->children[0].token->type == TOKEN_LINK) {
        out = tmp->children[0].token;
        tmp->children[0].token = NULL;
    }

    token_free_shallow(tmp);
    free(tmp_tb.buf);
    return out;
}

static void append_fragment_children(Token *dst, Token *frag)
{
    if (!dst || !frag) return;
    for (size_t ci = 0; ci < frag->child_count; ci++) {
        if (frag->children[ci].is_text) {
            token_append_text_n(dst, frag->children[ci].text, frag->children[ci].text_len);
        } else {
            token_append_child(dst, frag->children[ci].token);
            frag->children[ci].token = NULL;
        }
    }
}

static Token *parse_gallery_image_line(const char *line, size_t line_len,
                                       const ParserConfig *cfg, Accum *accum)
{
    if (!line || line_len == 0) return NULL;

    char *wrapped = malloc(line_len + 5);
    if (!wrapped) return NULL;
    wrapped[0] = '[';
    wrapped[1] = '[';
    memcpy(wrapped + 2, line, line_len);
    wrapped[2 + line_len] = ']';
    wrapped[3 + line_len] = ']';
    wrapped[4 + line_len] = '\0';

    ThreadBuf tmp_tb;
    tmp_tb.buf = wrapped;
    tmp_tb.len = line_len + 4;
    tmp_tb.cap = line_len + 5;
    tmp_tb.shrink_size = (size_t)-1;
    tmp_tb.target_size = tmp_tb.cap;

    /* JS parity: braces are parsed before links, which protects pipes inside templates. */
    parse_braces(&tmp_tb, cfg, accum);
    parse_links(&tmp_tb, cfg, accum, NULL, false);

    Token *tmp = token_new(TOKEN_PLAIN, "gallery-line");
    if (!tmp) return NULL;
    build_from_str(tmp, tmp_tb.buf, tmp_tb.len, accum);
    build_token_recursive(tmp, accum);

    Token *out = NULL;
    if (tmp->child_count == 1 && !tmp->children[0].is_text && tmp->children[0].token
        && tmp->children[0].token->type == TOKEN_FILE) {
        out = tmp->children[0].token;
        tmp->children[0].token = NULL;
        if (out->type_name) free(out->type_name);
        out->type_name = strdup("gallery-image");
    }

    token_free_shallow(tmp);
    free(tmp_tb.buf);
    return out;
}

static Token *parse_imagemap_image_line(const char *line, size_t line_len,
                                        const ParserConfig *cfg, Accum *accum)
{
    if (!line || line_len == 0) return NULL;

    char *wrapped = malloc(line_len + 5);
    if (!wrapped) return NULL;
    wrapped[0] = '[';
    wrapped[1] = '[';
    memcpy(wrapped + 2, line, line_len);
    wrapped[2 + line_len] = ']';
    wrapped[3 + line_len] = ']';
    wrapped[4 + line_len] = '\0';

    ThreadBuf tmp_tb;
    tmp_tb.buf = wrapped;
    tmp_tb.len = line_len + 4;
    tmp_tb.cap = line_len + 5;
    tmp_tb.shrink_size = (size_t)-1;
    tmp_tb.target_size = tmp_tb.cap;

    /* JS parity: braces are parsed before links, which protects pipes inside templates. */
    parse_braces(&tmp_tb, cfg, accum);
    parse_links(&tmp_tb, cfg, accum, NULL, false);

    Token *tmp = token_new(TOKEN_PLAIN, "imagemap-image-line");
    if (!tmp) return NULL;
    build_from_str(tmp, tmp_tb.buf, tmp_tb.len, accum);
    build_token_recursive(tmp, accum);

    Token *out = NULL;
    if (tmp->child_count == 1 && !tmp->children[0].is_text && tmp->children[0].token
        && tmp->children[0].token->type == TOKEN_FILE) {
        out = tmp->children[0].token;
        tmp->children[0].token = NULL;
        if (out->type_name) free(out->type_name);
        out->type_name = strdup("imagemap-image");
    }

    token_free_shallow(tmp);
    free(tmp_tb.buf);
    return out;
}

static Token *parse_imagemap_link_line(const char *line, size_t line_len,
                                       const ParserConfig *cfg, Accum *accum)
{
    if (!line || line_len == 0) return NULL;

    size_t open = SIZE_MAX;
    for (size_t i = 0; i + 1 < line_len; i++) {
        if (line[i] == '[' && line[i + 1] == '[') {
            open = i;
            break;
        }
    }
    if (open == SIZE_MAX) return NULL;

    size_t close = SIZE_MAX;
    for (size_t i = open + 2; i + 1 < line_len; i++) {
        if (line[i] == ']' && line[i + 1] == ']') {
            close = i;
            break;
        }
    }
    if (close == SIZE_MAX || close <= open + 1) return NULL;

    Token *t = token_new(TOKEN_PLAIN, "imagemap-link");
    if (!t) return NULL;
    accum_push(accum, t);

    if (open > 0) {
        token_append_text_n(t, line, open);
    } else {
        token_append_text_n(t, "", 0);
    }

    const char *inner = line + open + 2;
    size_t inner_len = close - (open + 2);
    Token *link = parse_single_link_token(inner, inner_len, cfg, accum);
    if (link) {
        token_append_child(t, link);
    } else {
        token_append_text_n(t, line + open, (close + 2) - open);
    }

    if (close + 2 < line_len) {
        token_append_text_n(t, line + close + 2, line_len - (close + 2));
    }

    Token *tail = make_empty_noinclude(accum);
    if (tail) token_append_child(t, tail);

    return t;
}

static void postprocess_gallery_ext_inner(Token *t, const ParserConfig *cfg, Accum *accum)
{
    if (!t || !t->type_name || strcmp(t->type_name, "ext-inner") != 0
        || !t->name || strcmp(t->name, "gallery") != 0) return;

    bool has_non_text = false;
    size_t src_len = 0;
    for (size_t i = 0; i < t->child_count; i++) {
        if (!t->children[i].is_text) {
            has_non_text = true;
        } else {
            src_len += t->children[i].text_len;
        }
    }
    if (has_non_text || src_len == 0) return;

    char *src = malloc(src_len + 1);
    if (!src) return;
    size_t pos = 0;
    for (size_t i = 0; i < t->child_count; i++) {
        memcpy(src + pos, t->children[i].text, t->children[i].text_len);
        pos += t->children[i].text_len;
    }
    src[src_len] = '\0';

    for (size_t i = 0; i < t->child_count; i++) {
        if (t->children[i].is_text) free(t->children[i].text);
    }
    t->child_count = 0;

    size_t line_start = 0;
    for (size_t i = 0; i <= src_len; i++) {
        if (i != src_len && src[i] != '\n') continue;

        size_t line_len = i - line_start;
        const char *line_ptr = src + line_start;

        Token *img = parse_gallery_image_line(line_ptr, line_len, cfg, accum);
        if (img) {
            token_append_child(t, img);
        } else {
            token_append_text_n(t, line_ptr, line_len);
        }

        line_start = i + 1;
    }

    for (size_t i = 0; i < t->child_count; i++) {
        if (!t->children[i].is_text && t->children[i].token) {
            postprocess_nested_plain(t->children[i].token, cfg, accum);
        }
    }

    free(src);
}

static void postprocess_imagemap_ext_inner(Token *t, const ParserConfig *cfg, Accum *accum)
{
    if (!t || !t->type_name || strcmp(t->type_name, "ext-inner") != 0
        || !t->name || strcmp(t->name, "imagemap") != 0) return;

    bool has_non_text = false;
    size_t src_len = 0;
    for (size_t i = 0; i < t->child_count; i++) {
        if (!t->children[i].is_text) {
            has_non_text = true;
        } else {
            src_len += t->children[i].text_len;
        }
    }
    if (has_non_text || src_len == 0) return;

    char *src = malloc(src_len + 1);
    if (!src) return;
    size_t pos = 0;
    for (size_t i = 0; i < t->child_count; i++) {
        memcpy(src + pos, t->children[i].text, t->children[i].text_len);
        pos += t->children[i].text_len;
    }
    src[src_len] = '\0';

    for (size_t i = 0; i < t->child_count; i++) {
        if (t->children[i].is_text) free(t->children[i].text);
    }
    t->child_count = 0;

    bool image_seen = false;
    size_t line_start = 0;
    for (size_t i = 0; i <= src_len; i++) {
        if (i != src_len && src[i] != '\n') continue;

        size_t line_len = i - line_start;
        const char *line_ptr = src + line_start;

        if (line_len == 0) {
            Token *n = make_empty_noinclude(accum);
            if (n) token_append_child(t, n);
        } else {
            Token *tok = NULL;
            if (!image_seen) {
                tok = parse_imagemap_image_line(line_ptr, line_len, cfg, accum);
                if (tok) image_seen = true;
            }
            if (!tok) {
                tok = parse_imagemap_link_line(line_ptr, line_len, cfg, accum);
            }
            if (tok) {
                token_append_child(t, tok);
            } else {
                token_append_text_n(t, line_ptr, line_len);
            }
        }

        line_start = i + 1;
    }

    for (size_t i = 0; i < t->child_count; i++) {
        if (!t->children[i].is_text && t->children[i].token) {
            postprocess_nested_plain(t->children[i].token, cfg, accum);
        }
    }

    free(src);
}

static void run_nested_plain_pipeline(ThreadBuf *scratch,
                                      bool is_td_inner,
                                      bool is_ext_inner,
                                      bool is_heading_title,
                                      Token *t,
                                      const ParserConfig *cfg,
                                      Accum *accum)
{
    if (is_ext_inner) {
        parse_comment_and_ext(scratch, cfg, accum, false);
    }

    parse_braces(scratch, cfg, accum);

    if (is_td_inner || is_ext_inner) {
        bool ext_inner_has_bang = is_ext_inner && mem_has(scratch->buf, scratch->len, "!\x7F");
        bool ext_inner_has_sentinel = is_ext_inner && memchr(scratch->buf, '\0', scratch->len) != NULL;

        if (!ext_inner_has_bang) {
            /* JS parity: td-inner parsing starts from stage 4, so HTML (stage 2)
             * must not run before links; otherwise links spanning inline HTML split. */
            if (is_ext_inner) {
                parse_html(scratch, cfg, accum);
            }
            TokenType hr_root_type = t->type;
            if (ext_inner_has_sentinel) {
                hr_root_type = TOKEN_PLAIN;
            }
            parse_hr_and_double_underscore(scratch, cfg, accum, hr_root_type, t->type_name);
            const ParserConfig *links_cfg = cfg;
            ParserConfig cfg_local;
            if (is_ext_inner && cfg) {
                cfg_local = *cfg;
                cfg_local.in_ext = true;
                links_cfg = &cfg_local;
            }
            parse_links(scratch, links_cfg, accum, NULL, false);
            parse_quotes_stage6_per_line(scratch, cfg, accum);
            parse_external_links(scratch, cfg, accum, false);
            parse_magic_links(scratch, cfg, accum);
            if (is_td_inner) {
                parse_list(scratch, cfg, accum);
            } else if (is_ext_inner) {
                parse_list_skip_first_line(scratch, cfg, accum);
            }
            parse_converter(scratch, cfg, accum);
        } else {
            parse_html(scratch, cfg, accum);
        }
    } else if (is_heading_title) {
        parse_html(scratch, cfg, accum);
        parse_links(scratch, cfg, accum, NULL, false);
        parse_quotes_stage6_per_line(scratch, cfg, accum);
        parse_external_links(scratch, cfg, accum, false);
        parse_magic_links(scratch, cfg, accum);
    }
}

static void postprocess_nested_plain(Token *t, const ParserConfig *cfg, Accum *accum)
{
    if (!t) return;

    for (size_t i = 0; i < t->child_count; i++) {
        if (!t->children[i].is_text && t->children[i].token) {
            postprocess_nested_plain(t->children[i].token, cfg, accum);
        }
    }

    if (!should_postprocess_plain(t)) return;
    if (t->type == TOKEN_EXT_INNER && t->name && strcmp(t->name, "nowiki") == 0) return;
    if (t->type == TOKEN_EXT_INNER && !ext_inner_allows_nested_parse(t->name)) return;

    if (t->type_name && strcmp(t->type_name, "ext-inner") == 0
        && t->name && strcmp(t->name, "gallery") == 0) {
        postprocess_gallery_ext_inner(t, cfg, accum);
        return;
    }

    if (t->type_name && strcmp(t->type_name, "ext-inner") == 0
        && t->name && strcmp(t->name, "imagemap") == 0) {
        postprocess_imagemap_ext_inner(t, cfg, accum);
        return;
    }

    bool is_td_inner = strcmp(t->type_name, "td-inner") == 0;
    bool is_ext_inner = strcmp(t->type_name, "ext-inner") == 0;
    bool is_heading_title = strcmp(t->type_name, "heading-title") == 0;

    ThreadBuf *scratch = wiki_thread_buf_acquire_scratch();

    bool has_non_text = false;
    size_t txt_len = 0;
    for (size_t i = 0; i < t->child_count; i++) {
        if (!t->children[i].is_text) {
            has_non_text = true;
        } else {
            txt_len += t->children[i].text_len;
        }
    }
    if (txt_len == 0) {
        wiki_thread_buf_release_scratch(scratch);
        return;
    }

    if (has_non_text) {
        if (is_td_inner) {
            size_t ser_cap = txt_len + 64;
            char *ser = malloc(ser_cap);
            if (ser) {
                size_t ser_len = 0;
                bool serializable = true;

                for (size_t i = 0; i < t->child_count; i++) {
                    Child cur = t->children[i];
                    if (cur.is_text) {
                        while (ser_len + cur.text_len + 1 >= ser_cap) {
                            ser_cap *= 2;
                            char *grown = realloc(ser, ser_cap);
                            if (!grown) {
                                serializable = false;
                                break;
                            }
                            ser = grown;
                        }
                        if (!serializable) break;
                        memcpy(ser + ser_len, cur.text, cur.text_len);
                        ser_len += cur.text_len;
                        continue;
                    }

                    Token *ctok = cur.token;
                    size_t tok_idx = SIZE_MAX;
                    for (size_t ai = 0; ai < accum->count; ai++) {
                        if (accum->tokens[ai] == ctok) {
                            tok_idx = ai;
                            break;
                        }
                    }
                    char sym = token_sentinel_char(ctok ? ctok->type : TOKEN_TEXT);
                    if (tok_idx == SIZE_MAX || sym == '\0') {
                        serializable = false;
                        break;
                    }

                    char marker[64];
                    size_t mlen = 0;
                    work_str_sentinel(tok_idx, sym, marker, &mlen);

                    while (ser_len + mlen + 1 >= ser_cap) {
                        ser_cap *= 2;
                        char *grown = realloc(ser, ser_cap);
                        if (!grown) {
                            serializable = false;
                            break;
                        }
                        ser = grown;
                    }
                    if (!serializable) break;
                    memcpy(ser + ser_len, marker, mlen);
                    ser_len += mlen;
                }

                if (serializable) {
                    wiki_thread_buf_set(scratch, ser, ser_len);
                    run_nested_plain_pipeline(scratch, is_td_inner, is_ext_inner, is_heading_title, t, cfg, accum);

                    Token *tmp = token_new(TOKEN_PLAIN, t->type_name);
                    if (tmp) {
                        build_from_str(tmp, scratch->buf, scratch->len, accum);
                        build_token_recursive(tmp, accum);

                        for (size_t i = 0; i < t->child_count; i++) {
                            if (t->children[i].is_text) free(t->children[i].text);
                        }
                        free(t->children);

                        t->children = tmp->children;
                        t->child_count = tmp->child_count;
                        t->child_cap = tmp->child_cap;

                        tmp->children = NULL;
                        tmp->child_count = 0;
                        tmp->child_cap = 0;
                        token_free_shallow(tmp);

                        free(ser);
                        wiki_thread_buf_release_scratch(scratch);
                        return;
                    }
                }

                free(ser);
            }
        }

        Child *old_children = t->children;
        size_t old_count = t->child_count;
        size_t new_cap = old_count ? old_count : 1;
        Child *new_children = malloc(new_cap * sizeof(Child));
        if (!new_children) {
            wiki_thread_buf_release_scratch(scratch);
            return;
        }
        size_t new_count = 0;

        for (size_t i = 0; i < old_count; i++) {
            Child cur = old_children[i];
            if (!cur.is_text) {
                if (new_count >= new_cap) {
                    new_cap *= 2;
                    Child *grown = realloc(new_children, new_cap * sizeof(Child));
                    assert(grown);
                    new_children = grown;
                }
                new_children[new_count++] = cur;
                continue;
            }

            const char *txt = cur.text;
            size_t cur_len = cur.text_len;
            wiki_thread_buf_set(scratch, txt, cur_len);
            run_nested_plain_pipeline(scratch, is_td_inner, is_ext_inner, is_heading_title, t, cfg, accum);

            bool unchanged = (scratch->len == cur_len && memcmp(scratch->buf, txt, cur_len) == 0);
            bool has_marker = memchr(scratch->buf, '\0', scratch->len) != NULL;
            if (unchanged && !has_marker) {
                if (new_count >= new_cap) {
                    new_cap *= 2;
                    Child *grown = realloc(new_children, new_cap * sizeof(Child));
                    assert(grown);
                    new_children = grown;
                }
                new_children[new_count++] = cur;
                continue;
            }

            free(cur.text);

            Token *tmp = token_new(TOKEN_PLAIN, t->type_name);
            if (!tmp) {
                if (new_count >= new_cap) {
                    new_cap *= 2;
                    Child *grown = realloc(new_children, new_cap * sizeof(Child));
                    assert(grown);
                    new_children = grown;
                }
                Child fallback;
                fallback.is_text = true;
                fallback.text_len = scratch->len;
                fallback.text = malloc(scratch->len + 1);
                assert(fallback.text);
                memcpy(fallback.text, scratch->buf, scratch->len);
                fallback.text[scratch->len] = '\0';
                new_children[new_count++] = fallback;
                continue;
            }

            build_from_str(tmp, scratch->buf, scratch->len, accum);
            build_token_recursive(tmp, accum);

            for (size_t j = 0; j < tmp->child_count; j++) {
                if (new_count >= new_cap) {
                    new_cap *= 2;
                    Child *grown = realloc(new_children, new_cap * sizeof(Child));
                    assert(grown);
                    new_children = grown;
                }
                new_children[new_count++] = tmp->children[j];
            }

            free(tmp->children);
            tmp->children = NULL;
            tmp->child_count = 0;
            tmp->child_cap = 0;
            token_free_shallow(tmp);
        }

        free(old_children);
        t->children = new_children;
        t->child_count = new_count;
        t->child_cap = new_cap;
        wiki_thread_buf_release_scratch(scratch);
        return;
    }

    char *joined = malloc(txt_len + 1);
    if (!joined) {
        wiki_thread_buf_release_scratch(scratch);
        return;
    }
    size_t pos = 0;
    for (size_t i = 0; i < t->child_count; i++) {
        memcpy(joined + pos, t->children[i].text, t->children[i].text_len);
        pos += t->children[i].text_len;
    }
    joined[txt_len] = '\0';
    const char *txt = joined;
    wiki_thread_buf_set(scratch, txt, txt_len);
    run_nested_plain_pipeline(scratch, is_td_inner, is_ext_inner, is_heading_title, t, cfg, accum);

    if (scratch->len == txt_len && memcmp(scratch->buf, txt, txt_len) == 0) {
        free(joined);
        wiki_thread_buf_release_scratch(scratch);
        return;
    }
    build_from_str(t, scratch->buf, scratch->len, accum);
    build_token_recursive(t, accum);
    free(joined);
    wiki_thread_buf_release_scratch(scratch);
}

static void postprocess_root_braces_fallback(Token *root, const ParserConfig *cfg, Accum *accum)
{
    if (!root || root->type != TOKEN_ROOT) return;
    if (root->child_count != 1 || !root->children[0].is_text) return;

    const char *txt = root->children[0].text;
    size_t txt_len = root->children[0].text_len;
    if (!txt || txt_len == 0 || !mem_has(txt, txt_len, "{{")) return;

    ThreadBuf *scratch = wiki_thread_buf_acquire_scratch();
    wiki_thread_buf_set(scratch, txt, txt_len);
    parse_braces(scratch, cfg, accum);

    if (scratch->len == txt_len && memcmp(scratch->buf, txt, txt_len) == 0) {
        wiki_thread_buf_release_scratch(scratch);
        return;
    }
    build_from_str(root, scratch->buf, scratch->len, accum);
    wiki_thread_buf_release_scratch(scratch);
}

static void postprocess_parameter_value_inline(Token *t, const ParserConfig *cfg, Accum *accum)
{
    if (!t) return;

    for (size_t i = 0; i < t->child_count; i++) {
        if (!t->children[i].is_text && t->children[i].token) {
            postprocess_parameter_value_inline(t->children[i].token, cfg, accum);
        }
    }

    if (t->type != TOKEN_PLAIN || !t->type_name) {
        return;
    }

    bool is_parameter_value = strcmp(t->type_name, "parameter-value") == 0;
    bool is_arg_default = strcmp(t->type_name, "arg-default") == 0;
    if (!is_parameter_value && !is_arg_default) {
        return;
    }

    ThreadBuf *scratch = wiki_thread_buf_acquire_scratch();

    Child *old_children = t->children;
    size_t old_count = t->child_count;
    size_t new_cap = old_count ? old_count : 1;
    Child *new_children = malloc(new_cap * sizeof(Child));
    if (!new_children) {
        wiki_thread_buf_release_scratch(scratch);
        return;
    }
    size_t new_count = 0;

    for (size_t i = 0; i < old_count; i++) {
        Child cur = old_children[i];

        if (!cur.is_text) {
            if (new_count >= new_cap) {
                new_cap *= 2;
                Child *grown = realloc(new_children, new_cap * sizeof(Child));
                assert(grown);
                new_children = grown;
            }
            new_children[new_count++] = cur;
            continue;
        }

        const char *txt = cur.text;
        size_t txt_len = cur.text_len;
        wiki_thread_buf_set(scratch, txt, txt_len);
        parse_comment_and_ext(scratch, cfg, accum, false);
        parse_braces(scratch, cfg, accum);
        parse_html(scratch, cfg, accum);
        if (is_parameter_value) {
            parse_hr_and_double_underscore(scratch, cfg, accum, TOKEN_PLAIN, "parameter-value");
            bool has_bang_sentinel = mem_has(scratch->buf, scratch->len, "!\x7F");
            if (!has_bang_sentinel) {
                parse_links(scratch, cfg, accum, NULL, false);
                parse_quotes_stage6_per_line(scratch, cfg, accum);
                parse_external_links(scratch, cfg, accum, false);
                parse_magic_links(scratch, cfg, accum);
                parse_list_skip_first_line(scratch, cfg, accum);
                parse_converter(scratch, cfg, accum);
            }
        }

        bool unchanged = (scratch->len == txt_len && memcmp(scratch->buf, txt, txt_len) == 0);
        bool has_marker = memchr(scratch->buf, '\0', scratch->len) != NULL;
        if (unchanged && !has_marker) {
            if (new_count >= new_cap) {
                new_cap *= 2;
                Child *grown = realloc(new_children, new_cap * sizeof(Child));
                assert(grown);
                new_children = grown;
            }
            new_children[new_count++] = cur;
            continue;
        }

        free(cur.text);

        Token *tmp = token_new(TOKEN_PLAIN, t->type_name);
        if (!tmp) {
            if (new_count >= new_cap) {
                new_cap *= 2;
                Child *grown = realloc(new_children, new_cap * sizeof(Child));
                assert(grown);
                new_children = grown;
            }
            Child fallback;
            fallback.is_text = true;
            fallback.text_len = scratch->len;
            fallback.text = malloc(scratch->len + 1);
            assert(fallback.text);
            memcpy(fallback.text, scratch->buf, scratch->len);
            fallback.text[scratch->len] = '\0';
            new_children[new_count++] = fallback;
            continue;
        }

        build_from_str(tmp, scratch->buf, scratch->len, accum);
        build_token_recursive(tmp, accum);

        for (size_t j = 0; j < tmp->child_count; j++) {
            if (new_count >= new_cap) {
                new_cap *= 2;
                Child *grown = realloc(new_children, new_cap * sizeof(Child));
                assert(grown);
                new_children = grown;
            }
            new_children[new_count++] = tmp->children[j];
        }

        free(tmp->children);
        tmp->children = NULL;
        tmp->child_count = 0;
        tmp->child_cap = 0;
        token_free_shallow(tmp);
    }

    free(old_children);
    t->children = new_children;
    t->child_count = new_count;
    t->child_cap = new_cap;
    wiki_thread_buf_release_scratch(scratch);

    /* JS parity: any sub-token (e.g. ExtToken with ext-inner) that was
     * built from this parameter-value text must run the nested-plain pass
     * so its ext-inner content goes through stages 5..10 just like JS
     * Token.parseOnce would do for tokens added to the accum. */
    for (size_t i = 0; i < t->child_count; i++) {
        if (!t->children[i].is_text && t->children[i].token) {
            postprocess_nested_plain(t->children[i].token, cfg, accum);
        }
    }
}

static void parse_quotes_stage6_per_line(ThreadBuf *ws, const ParserConfig *cfg, Accum *accum)
{
    if (!ws || !ws->buf) return;

    ThreadBuf *scratch = wiki_thread_buf_acquire_scratch();

    size_t out_cap = ws->len * 2 + 64;
    char *out = malloc(out_cap);
    assert(out);
    size_t out_len = 0;

    size_t line_start = 0;
    for (size_t i = 0; i <= ws->len; i++) {
        if (i != ws->len && ws->buf[i] != '\n') continue;

        size_t line_len = i - line_start;
        wiki_thread_buf_set(scratch, ws->buf + line_start, line_len);
        parse_quotes(scratch, cfg, accum, false);

        while (out_len + scratch->len + 2 >= out_cap) {
            out_cap *= 2;
            out = realloc(out, out_cap);
            assert(out);
        }
        if (scratch->len > 0) {
            memcpy(out + out_len, scratch->buf, scratch->len);
            out_len += scratch->len;
        }
        if (i != ws->len) {
            out[out_len++] = '\n';
        }

        line_start = i + 1;
    }

    out[out_len] = '\0';
    wiki_thread_buf_set(ws, out, out_len);

    free(out);
    wiki_thread_buf_release_scratch(scratch);
}

Token *wiki_parse(const char *wikitext, const ParserConfig *cfg,
                  bool include, int max_stage)
{
    if (!wikitext || !cfg) return NULL;

    /* ── Grab a thread-local snapshot of the input ─────────────────────────
     * The caller's string may be modified by another thread while we are
     * executing.  We copy it into the thread's pre-allocated main buffer
     * (avoiding a per-call malloc) */
    size_t input_len = strlen(wikitext);
    ThreadBuffers *tbufs = wiki_thread_buf_get();

    /* Apply shrink/grow policy for this input size, then copy-and-tidy.
     * wiki_thread_buf_reserve() is the sole resize authority for ThreadBufs;
     * it guarantees the buffer can hold input_len+1 bytes before we hand
     * the pointer to str_tidy_into(), which never allocates. */
    wiki_thread_buf_reserve(&tbufs->main, input_len);

    size_t tidy_len = 0;
    str_tidy_into(wikitext, input_len, tbufs->main.buf, tbufs->main.cap, &tidy_len);
    tbufs->main.len = tidy_len;

    /* ── Working string (mutated by each stage) ─────────────────────────── */
    ThreadBuf *ws = &tbufs->main;

    /* Optional stage logging directory (set via env WIKI_STAGE_LOG_DIR). */
    const char *stage_log_dir = getenv("WIKI_STAGE_LOG_DIR");
    char runid[64] = "";
    if (stage_log_dir) {
        static int _run_counter = 0;
        _run_counter++;
        pid_t pid = getpid();
        long ts = (long)time(NULL);
        snprintf(runid, sizeof(runid), "%d-%ld-%d", (int)pid, ts, _run_counter);
        /* try to create directory if it doesn't exist */
        if (mkdir(stage_log_dir, 0777) != 0 && errno != EEXIST) {
            /* non-fatal; best-effort */
        }
    }

    /* ── Accumulator (holds extracted tokens) ─────────────────────────── */
    Accum accum;
    accum_init(&accum);

    /* ── Create root token ────────────────────────────────────────────────── */
    Token *root = token_new(TOKEN_ROOT, "root");
    if (!root) {
        /* no-op: thread buffer owned by TLS */
        accum_free(&accum);
        return NULL;
    }
    root->stage   = -1;
    root->include = include;

    /* ── Run stages 0 .. max_stage ─────────────────────────────────────── */
    for (int stage = 0; stage <= max_stage && stage <= 10; stage++) {
        switch (stage) {
            case 0:
                /* parseRedirect only runs on the root token */
                parse_redirect(ws, cfg, &accum);
                /* parseCommentAndExt always runs at stage 0 */
                parse_comment_and_ext(ws, cfg, &accum, include);
                break;

            /* Stage 1: parseBraces */
            case 1:
                parse_braces(ws, cfg, &accum);
                break;

            case 2:  /* parseHtml */
                parse_html(ws, cfg, &accum);
                break;
            case 3:  /* parseTable */
                parse_table(ws, cfg, &accum);
                break;
            case 4:  /* parseHrAndDoubleUnderscore */
                parse_hr_and_double_underscore(ws, cfg, &accum, TOKEN_ROOT, "root");
                break;
                case 5:  /* parseLinks */
                    parse_links(ws, cfg, &accum, NULL, false);
                    break;
            case 6:  /* parseQuotes */
                    parse_quotes_stage6_per_line(ws, cfg, &accum);
                    break;
            case 7:  /* parseExternalLinks */
                parse_external_links(ws, cfg, &accum, false);
                    break;
            case 8:  /* parseMagicLinks */
                    parse_magic_links(ws, cfg, &accum);
                    break;
                case 9:  /* parseList */
                    parse_list(ws, cfg, &accum);
                    break;
            case 10: /* parseConverter */
                parse_converter(ws, cfg, &accum);
                break;
            }

            /* If stage logging enabled, append a JSON snapshot to native-stage.log */
            if (stage_log_dir) {
                append_native_stage_json(stage_log_dir, stage, ws, &accum);
            }
    }

    /* ── build(): expand sentinel markers into the tree ─────────────────── */
    build(root, ws, &accum);

    /* JS parity for nested plain regions that still contain parseable syntax. */
    postprocess_nested_plain(root, cfg, &accum);
    postprocess_root_braces_fallback(root, cfg, &accum);

    /* JS parity: inline parsing can appear inside parameter-value subtrees. */
    postprocess_parameter_value_inline(root, cfg, &accum);

    /* ── Debug: log the final token tree as JSON ─────────────────────────── */
    // if (log_get_level() <= LOG_DEBUG)
    //     token_log_json(root);

    /* ── Free orphan accum tokens ────────────────────────────────────────── */
    /* Tokens whose sentinel was inside content that a later-stage parser
     * stored via a NUL-terminated string, thereby losing the \0 byte of
     * the sentinel and preventing build() from linking them into the tree. */
    free_accum_orphans(root, &accum);

    /* ── Cleanup ─────────────────────────────────────────────────────────── */
    /* no-op: thread buffer owned by TLS */
    accum_free(&accum);

    return root;
}
