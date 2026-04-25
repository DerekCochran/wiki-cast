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
            parse_html(scratch, cfg, accum);
            TokenType hr_root_type = t->type;
            if (ext_inner_has_sentinel) {
                hr_root_type = TOKEN_PLAIN;
            }
            parse_hr_and_double_underscore(scratch, cfg, accum, hr_root_type, t->type_name);
            parse_links(scratch, cfg, accum, NULL, false);
            parse_quotes(scratch, cfg, accum, false);
            parse_external_links(scratch, cfg, accum, false);
            parse_magic_links(scratch, cfg, accum);
            if (is_td_inner) {
                parse_list(scratch, cfg, accum);
            }
            parse_converter(scratch, cfg, accum);
        } else {
            parse_html(scratch, cfg, accum);
        }
    } else if (is_heading_title) {
        parse_html(scratch, cfg, accum);
        parse_links(scratch, cfg, accum, NULL, false);
        parse_quotes(scratch, cfg, accum, false);
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

    bool is_td_inner = strcmp(t->type_name, "td-inner") == 0;
    bool is_ext_inner = strcmp(t->type_name, "ext-inner") == 0;
    bool is_heading_title = strcmp(t->type_name, "heading-title") == 0;

    ThreadBuffers *tbufs = wiki_thread_buf_get();
    ThreadBuf *scratch = &tbufs->scratch;

    bool has_non_text = false;
    size_t txt_len = 0;
    for (size_t i = 0; i < t->child_count; i++) {
        if (!t->children[i].is_text) {
            has_non_text = true;
        } else {
            txt_len += t->children[i].text_len;
        }
    }
    if (txt_len == 0) return;

    if (has_non_text) {
        Child *old_children = t->children;
        size_t old_count = t->child_count;
        size_t new_cap = old_count ? old_count : 1;
        Child *new_children = malloc(new_cap * sizeof(Child));
        if (!new_children) return;
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
        return;
    }

    char *joined = malloc(txt_len + 1);
    if (!joined) return;
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
        return;
    }
    build_from_str(t, scratch->buf, scratch->len, accum);
    build_token_recursive(t, accum);
    free(joined);
}

static void postprocess_root_braces_fallback(Token *root, const ParserConfig *cfg, Accum *accum)
{
    if (!root || root->type != TOKEN_ROOT) return;
    if (root->child_count != 1 || !root->children[0].is_text) return;

    const char *txt = root->children[0].text;
    size_t txt_len = root->children[0].text_len;
    if (!txt || txt_len == 0 || !mem_has(txt, txt_len, "{{")) return;

    ThreadBuffers *tbufs = wiki_thread_buf_get();
    ThreadBuf *scratch = &tbufs->scratch;
    wiki_thread_buf_set(scratch, txt, txt_len);
    parse_braces(scratch, cfg, accum);

    if (scratch->len == txt_len && memcmp(scratch->buf, txt, txt_len) == 0) return;
    build_from_str(root, scratch->buf, scratch->len, accum);
}

static void postprocess_parameter_value_inline(Token *t, const ParserConfig *cfg, Accum *accum)
{
    if (!t) return;

    for (size_t i = 0; i < t->child_count; i++) {
        if (!t->children[i].is_text && t->children[i].token) {
            postprocess_parameter_value_inline(t->children[i].token, cfg, accum);
        }
    }

    if (t->type != TOKEN_PLAIN || !t->type_name || strcmp(t->type_name, "parameter-value") != 0) {
        return;
    }

    ThreadBuffers *tbufs = wiki_thread_buf_get();
    ThreadBuf *scratch = &tbufs->scratch;

    Child *old_children = t->children;
    size_t old_count = t->child_count;
    size_t new_cap = old_count ? old_count : 1;
    Child *new_children = malloc(new_cap * sizeof(Child));
    if (!new_children) return;
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
        parse_hr_and_double_underscore(scratch, cfg, accum, TOKEN_PLAIN, "parameter-value");
        bool has_bang_sentinel = mem_has(scratch->buf, scratch->len, "!\x7F");
        if (!has_bang_sentinel) {
            parse_links(scratch, cfg, accum, NULL, false);
            parse_quotes(scratch, cfg, accum, false);
            parse_external_links(scratch, cfg, accum, false);
            parse_magic_links(scratch, cfg, accum);
            parse_list_skip_first_line(scratch, cfg, accum);
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

        Token *tmp = token_new(TOKEN_PLAIN, "parameter-value");
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
                    parse_quotes(ws, cfg, &accum, false);
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
