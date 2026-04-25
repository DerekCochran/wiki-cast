#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

#include "parser/braces.h"
#include "string_util.h"
#include "token.h"
#include "log.h"
#include "title.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <stdio.h>

static bool str_list_contains_ci(const StrList *sl, const char *needle)
{
    if (!sl || !needle) return false;
    for (size_t i = 0; i < sl->count; i++) {
        if (sl->items[i] && strcasecmp(sl->items[i], needle) == 0) return true;
    }
    return false;
}

/* Build a regex subject buffer with identical length/offsets where embedded
 * NUL bytes are replaced by SOH so PCRE lookaheads like \n(?![=\x00]) do not
 * spuriously reject lines that only contain internal sentinels. */
static char *braces_make_match_subject(const char *buf, size_t len)
{
    if (!buf || len == 0) return NULL;
    char *subject = malloc(len);
    if (!subject) return NULL;
    memcpy(subject, buf, len);
    for (size_t i = 0; i < len; i++) {
        if ((unsigned char)subject[i] == '\0') {
            subject[i] = '\x01';
        }
    }
    return subject;
}

static const char *str_map_get_exact(const StrMap *m, const char *key)
{
    if (!m || !key) return NULL;
    for (size_t i = 0; i < m->count; i++) {
        if (m->keys[i] && m->values[i] && strcmp(m->keys[i], key) == 0) {
            return m->values[i];
        }
    }
    return NULL;
}

/* JS parity: parser/braces.js getSymbol() for {{...}} replacements. */
static char braces_get_symbol(const char *name, size_t len,
                              const ParserConfig *cfg,
                              bool *is_magic_out)
{
    if (is_magic_out) *is_magic_out = false;
    if (!name || len == 0) return 't';

    /* JS parity: getSymbol(trimLc(removeComment(s))).
     * removeComment removes \0\d+[cn]\x7F markers before matching. */
    size_t cleaned_len = 0;
    char *cleaned = str_remove_comment(name, len, &cleaned_len);
    if (!cleaned) return 't';

    /* trim ASCII whitespace */
    size_t i = 0, j = cleaned_len;
    while (i < cleaned_len && isspace((unsigned char)cleaned[i])) i++;
    while (j > i && isspace((unsigned char)cleaned[j - 1])) j--;
    if (j <= i) { free(cleaned); return 't'; }

    size_t n = j - i;
    char *trimmed = malloc(n + 1);
    if (!trimmed) { free(cleaned); return 't'; }
    memcpy(trimmed, cleaned + i, n);
    trimmed[n] = '\0';

    char *lc = malloc(n + 1);
    if (!lc) { free(cleaned); free(trimmed); return 't'; }
    for (size_t k = 0; k < n; k++) {
        lc[k] = (char)tolower((unsigned char)cleaned[i + k]);
    }
    lc[n] = '\0';
    free(cleaned);

    const char *canonical = NULL;
    const char *base_orig = trimmed;
    size_t base_orig_len = n;
    const char *colon_orig = memchr(trimmed, ':', n);
    if (colon_orig && colon_orig > trimmed) {
        base_orig_len = (size_t)(colon_orig - trimmed);
    }

    char *base_orig_buf = NULL;
    if (base_orig_len != n) {
        base_orig_buf = malloc(base_orig_len + 1);
        if (base_orig_buf) {
            memcpy(base_orig_buf, trimmed, base_orig_len);
            base_orig_buf[base_orig_len] = '\0';
            base_orig = base_orig_buf;
        }
    }

    const char *base_lc = lc;
    size_t base_lc_len = n;
    const char *colon = strchr(lc, ':');
    if (colon && colon > lc) {
        base_lc_len = (size_t)(colon - lc);
    }

    char *base_buf = NULL;
    if (base_lc_len != n) {
        base_buf = malloc(base_lc_len + 1);
        if (base_buf) {
            memcpy(base_buf, lc, base_lc_len);
            base_buf[base_lc_len] = '\0';
            base_lc = base_buf;
        }
    }

    if (cfg) {
        canonical = str_map_get_exact(&cfg->parser_function_sensitive, trimmed);
        if (!canonical) {
            canonical = str_map_get_exact(&cfg->parser_function_insensitive, lc);
        }
        if (!canonical && base_orig && base_orig[0]) {
            canonical = str_map_get_exact(&cfg->parser_function_sensitive, base_orig);
        }
        if (!canonical && base_lc && base_lc[0]) {
            canonical = str_map_get_exact(&cfg->parser_function_insensitive, base_lc);
        }
    }

    char out = 't';
    if (strcmp(lc, "!") == 0) {
        out = '!';
        if (is_magic_out) *is_magic_out = true;
    } else if (strcmp(lc, "!!") == 0) {
        out = '+';
        if (is_magic_out) *is_magic_out = true;
    } else if (strcmp(lc, "(!") == 0) {
        out = '{';
        if (is_magic_out) *is_magic_out = true;
    } else if (strcmp(lc, "!)") == 0) {
        out = '}';
        if (is_magic_out) *is_magic_out = true;
    } else if (strcmp(lc, "!-") == 0) {
        out = '-';
        if (is_magic_out) *is_magic_out = true;
    } else if (strcmp(lc, "=") == 0) {
        out = '~';
        if (is_magic_out) *is_magic_out = true;
    } else if (strcmp(lc, "server") == 0) {
        out = 'm';
        if (is_magic_out) *is_magic_out = true;
    } else if ((strncmp(lc, "filepath:", 9) == 0 && n > 9)
            || (strncmp(lc, "fullurl:", 8) == 0 && n > 8)
            || (strncmp(lc, "fullurle:", 9) == 0 && n > 9)
            || (strncmp(lc, "canonicalurl:", 13) == 0 && n > 13)
            || (strncmp(lc, "canonicalurle:", 14) == 0 && n > 14)) {
        out = 'm';
        if (is_magic_out) *is_magic_out = true;
    } else if (strncmp(lc, "#vardefine:", 11) == 0 && n > 11) {
        out = 'n';
        if (is_magic_out) *is_magic_out = true;
    } else if (lc[0] == '#') {
        if (is_magic_out) *is_magic_out = true;
    } else if (cfg && canonical && canonical[0]) {
        if (is_magic_out) *is_magic_out = true;
    } else if (cfg && base_lc && base_lc[0]) {
        const char *base_canonical = str_map_get_exact(&cfg->parser_function_insensitive, base_lc);
        if (base_canonical) {
            if (is_magic_out) *is_magic_out = true;
        }
    }

    free(base_buf);
    free(base_orig_buf);
    free(trimmed);
    free(lc);
    return out;
}

/*
 * parseBraces — stage 1 of the wikitext parsing pipeline.
 *
 * Handles: {{{arg}}}, {{template}}, [[link]], -{converter}-
 * For templates/args: builds a token and pushes a sentinel into the working string.
 * For links/converters: parks the full matched text in a link-stack and emits a
 *   numeric-only sentinel; these are restored before each convergence check so
 *   later stages (parseLinks, parseConverter) see them intact.
 */

static char *trim_copy(const char *s, size_t len)
{
    if (!s) return NULL;
    size_t i = 0, j = len;
    while (i < j && isspace((unsigned char)s[i])) i++;
    while (j > i && isspace((unsigned char)s[j - 1])) j--;
    size_t n = j - i;
    char *out = malloc(n + 1);
    if (!out) return NULL;
    memcpy(out, s + i, n);
    out[n] = '\0';
    return out;
}

/* Build a JS-shaped transclude/arg token and push to accum. */
static Token *build_template_token(char **parts_restored, const size_t *parts_lens,
                                    size_t parts_count,
                                    bool is_arg, const ParserConfig *cfg, Accum *accum)
{
    Token *t = token_new(is_arg ? TOKEN_ARG : TOKEN_TRANSCLUDE,
                         is_arg ? "arg" : "template");
    if (!t) return NULL;

    /* ArgToken: [arg-name, arg-default?, hidden*] */
    if (is_arg) {
        if (parts_count > 0 && parts_restored[0]) {
            Token *name_tok = token_new(TOKEN_ATOM, "arg-name");
            if (!name_tok) { token_free(t); return NULL; }
            token_append_text_n(name_tok, parts_restored[0], parts_lens[0]);
            token_append_child(t, name_tok);

            char *nm = trim_copy(parts_restored[0], parts_lens[0]);
            if (nm) t->name = nm;
        }

        if (parts_count > 1 && parts_restored[1]) {
            Token *def_tok = token_new(TOKEN_PLAIN, "arg-default");
            if (def_tok) {
                token_append_text_n(def_tok, parts_restored[1], parts_lens[1]);
                token_append_child(t, def_tok);
            }
        }

        for (size_t k = 2; k < parts_count; k++) {
            if (!parts_restored[k]) continue;
            Token *hidden = token_new(TOKEN_HIDDEN, "hidden");
            if (!hidden) continue;
            token_append_text_n(hidden, parts_restored[k], parts_lens[k]);
            token_append_child(t, hidden);
        }
        accum_push(accum, t);
        return t;
    }

    /* JS parity: TranscludeToken throws for invalid template names.
     * In this stage implementation, at minimum reject empty names after
     * removeComment()+trim so {{}} and {{   }} are not tokenized. */
    if (parts_count > 0 && parts_restored[0]) {
        size_t cleaned_len = 0;
        char *cleaned = str_remove_comment(parts_restored[0], parts_lens[0], &cleaned_len);
        if (!cleaned) {
            token_free(t);
            return NULL;
        }
        size_t i = 0, j = cleaned_len;
        while (i < cleaned_len && isspace((unsigned char)cleaned[i])) i++;
        while (j > i && isspace((unsigned char)cleaned[j - 1])) j--;
        free(cleaned);
        if (j <= i) {
            token_free(t);
            return NULL;
        }
    }

    bool transclude_is_magic = false;
    size_t magic_title_len = 0;
    const char *magic_first_arg = NULL;
    size_t magic_first_arg_len = 0;

    if (parts_count > 0 && parts_restored[0]) {
        size_t p0_len = parts_lens[0];
        bool magic = false;
        (void)braces_get_symbol(parts_restored[0], p0_len, cfg, &magic);
        if (magic) {
            transclude_is_magic = true;
            magic_title_len = p0_len;
            const char *colon = memchr(parts_restored[0], ':', p0_len);
            if (colon) {
                magic_title_len = (size_t)(colon - parts_restored[0]);
                magic_first_arg = colon + 1;
                magic_first_arg_len = p0_len - magic_title_len - 1;
            }

            free(t->type_name);
            t->type_name = strdup("magic-word");
            char *nm = trim_copy(parts_restored[0], p0_len);
            if (nm) {
                for (char *p = nm; *p; p++) {
                    *p = (char)tolower((unsigned char)*p);
                }
                char *colon = strchr(nm, ':');
                if (colon && colon > nm) {
                    *colon = '\0';
                }
                t->name = nm;
            }

            Token *mw_name = token_new(TOKEN_SYNTAX, "magic-word-name");
            if (mw_name) {
                token_append_text_n(mw_name, parts_restored[0], magic_title_len);
                token_append_child(t, mw_name);
            }
        } else {
            Token *tpl_name = token_new(TOKEN_ATOM, "template-name");
            if (tpl_name) {
                token_append_text_n(tpl_name, parts_restored[0], p0_len);
                token_append_child(t, tpl_name);
            }

            char *trimmed_name = trim_copy(parts_restored[0], p0_len);
            const char *name_src = trimmed_name ? trimmed_name : parts_restored[0];
            size_t name_len = trimmed_name ? strlen(trimmed_name) : p0_len;
            char *norm = title_normalize(name_src, name_len);
            if (norm && norm[0]) {
                size_t nn = strlen(norm);
                char *full = malloc(nn + 10);
                if (full) {
                    memcpy(full, "Template:", 9);
                    memcpy(full + 9, norm, nn + 1);
                    t->name = full;
                }
            }
            free(trimmed_name);
            free(norm);
        }
    }

    size_t positional = 1;
    if (transclude_is_magic && magic_first_arg) {
        const char *part = magic_first_arg;
        size_t part_len = magic_first_arg_len;
        const char *eq = memchr(part, '=', part_len);

        Token *param = token_new(TOKEN_PARAMETER, "parameter");
        if (param) {
            param->sep = '\0';

            Token *key_tok = token_new(TOKEN_PLAIN, "parameter-key");
            Token *val_tok = token_new(TOKEN_PLAIN, "parameter-value");
            if (key_tok && val_tok) {
                if (eq) {
                    size_t key_len = (size_t)(eq - part);
                    size_t val_len = part_len - key_len - 1;
                    token_append_text_n(key_tok, part, key_len);
                    token_append_text_n(val_tok, eq + 1, val_len);
                    token_append_child(param, key_tok);
                    token_append_child(param, val_tok);

                    char *pname = trim_copy(part, key_len);
                    if (pname) param->name = pname;
                } else {
                    token_append_child(param, key_tok);
                    token_append_text_n(val_tok, part, part_len);
                    token_append_child(param, val_tok);

                    char *pname = strdup("1");
                    if (pname) param->name = pname;
                    positional = 2;
                }
                token_append_child(t, param);
            } else {
                if (key_tok) token_free(key_tok);
                if (val_tok) token_free(val_tok);
                token_free(param);
            }
        }
    }

    for (size_t k = 1; k < parts_count; k++) {
        if (!parts_restored[k]) continue;

        const char *part = parts_restored[k];
        size_t part_len = parts_lens[k];
        const char *eq = memchr(part, '=', part_len);

        Token *param = token_new(TOKEN_PARAMETER, "parameter");
        if (!param) continue;
        param->sep = '\0';

        Token *key_tok = token_new(TOKEN_PLAIN, "parameter-key");
        Token *val_tok = token_new(TOKEN_PLAIN, "parameter-value");
        if (!key_tok || !val_tok) {
            if (key_tok) token_free(key_tok);
            if (val_tok) token_free(val_tok);
            token_free(param);
            continue;
        }

        if (eq) {
            size_t key_len = (size_t)(eq - part);
            size_t val_len = part_len - key_len - 1;
            token_append_text_n(key_tok, part, key_len);
            token_append_text_n(val_tok, eq + 1, val_len);
            token_append_child(param, key_tok);
            token_append_child(param, val_tok);

            char *pname = trim_copy(part, key_len);
            if (pname) param->name = pname;
        } else {
            token_append_child(param, key_tok);
            token_append_text_n(val_tok, part, part_len);
            token_append_child(param, val_tok);

            char idx_buf[32];
            int n = snprintf(idx_buf, sizeof(idx_buf), "%zu", positional++);
            if (n > 0) {
                char *pname = malloc((size_t)n + 1);
                if (pname) {
                    memcpy(pname, idx_buf, (size_t)n + 1);
                    param->name = pname;
                }
            }
        }
        token_append_child(t, param);
    }

    accum_push(accum, t);
    return t;
}

/* Helper: split inner content on '|', restore link-stack entries in each part,
 * and build a template/arg token.  Uses do-while to handle trailing '|'. */
static Token *build_from_inner(const char *inner, size_t inner_len,
                                bool is_arg,
                                char **link_stack, size_t link_count,
                                const size_t *link_stack_lens,
                                const ParserConfig *cfg,
                                Accum *accum)
{
    size_t part_cap = 8, part_count = 0;
    char **parts = malloc(part_cap * sizeof(char *));
    size_t *plens = malloc(part_cap * sizeof(size_t));
    assert(parts && plens);

    size_t si = 0;
    do {
        size_t j = si;
        while (j < inner_len && inner[j] != '|') j++;
        size_t plen = j - si;
        char *tmp = malloc(plen + 1);
        memcpy(tmp, inner + si, plen);
        tmp[plen] = '\0';
        size_t restored_len = 0;
        char *restored = str_restore(tmp, plen, (const char **)link_stack, link_count, link_stack_lens, &restored_len);
        free(tmp);
        if (part_count >= part_cap) {
            part_cap *= 2;
            parts = realloc(parts, part_cap * sizeof(char *));
            plens = realloc(plens, part_cap * sizeof(size_t));
            assert(parts && plens);
        }
        parts[part_count] = restored;
        plens[part_count] = restored_len;
        part_count++;
        if (j >= inner_len) break;
        si = j + 1;
    } while (1);

    Token *tok = build_template_token(parts, plens, part_count, is_arg, cfg, accum);
    for (size_t p = 0; p < part_count; p++) free(parts[p]);
    free(parts);
    free(plens);
    return tok;
}

/* JS parity for triple-brace close path:
 * if removeComment(argParts[1]).trim().endsWith(':') and base is in subst list,
 * sentinel is 's' instead of 'a'. */
static char braces_arg_symbol(const char *inner, size_t inner_len, const ParserConfig *cfg)
{
    if (!inner || inner_len == 0 || !cfg) return 'a';

    /* Extract the second arg part (between first and second '|'). */
    size_t p = 0;
    while (p < inner_len && inner[p] != '|') p++;
    if (p >= inner_len) return 'a';
    size_t vstart = p + 1;
    size_t vend = vstart;
    while (vend < inner_len && inner[vend] != '|') vend++;
    if (vend <= vstart) return 'a';

    size_t cleaned_len = 0;
    char *cleaned = str_remove_comment(inner + vstart, vend - vstart, &cleaned_len);
    if (!cleaned) return 'a';

    /* trim */
    size_t i = 0, j = cleaned_len;
    while (i < j && isspace((unsigned char)cleaned[i])) i++;
    while (j > i && isspace((unsigned char)cleaned[j - 1])) j--;
    if (j <= i) { free(cleaned); return 'a'; }

    if (cleaned[j - 1] != ':') { free(cleaned); return 'a'; }

    size_t base_len = j - i - 1;
    if (base_len == 0) { free(cleaned); return 'a'; }

    char *base = malloc(base_len + 1);
    if (!base) { free(cleaned); return 'a'; }
    for (size_t k = 0; k < base_len; k++) {
        base[k] = (char)tolower((unsigned char)cleaned[i + k]);
    }
    base[base_len] = '\0';
    free(cleaned);

    char sym = 'a';
    for (size_t n = 0; n < cfg->parser_function_subst.count; n++) {
        const char *s = cfg->parser_function_subst.items[n];
        if (!s) continue;
        if (strcmp(base, s) == 0) { sym = 's'; break; }
    }
    free(base);
    return sym;
}

/* Pre-pass for simple innermost {{{...}}} arguments.
 * Mirrors the JS reReplace behavior for non-nested triple-brace arguments. */
static void parse_simple_args(ThreadBuf *tb, const ParserConfig *cfg, Accum *accum)
{
    const char *pattern_with_lb =
        "(?<!\\{)\\{\\{\\{((?:[^\\n{}\\[]|\\[(?!\\[)|\\n(?![=\\x00]))*)\\}\\}\\}(?!\\})";
    const char *pattern_fallback =
        "\\{\\{\\{((?:[^\\n{}\\[]|\\[(?!\\[)|\\n(?![=\\x00]))*)\\}\\}\\}(?!\\})";

    PCRE2_SIZE err_offset;
    int err_code;
    pcre2_code *re = pcre2_compile((PCRE2_SPTR)pattern_with_lb, PCRE2_ZERO_TERMINATED,
                                   PCRE2_UTF, &err_code, &err_offset, NULL);
    if (!re) {
        re = pcre2_compile((PCRE2_SPTR)pattern_fallback, PCRE2_ZERO_TERMINATED,
                           PCRE2_UTF, &err_code, &err_offset, NULL);
    }
    if (!re) return;

    char *prev = NULL;
    size_t prev_len = 0;

    while (1) {
        pcre2_match_data *md = pcre2_match_data_create_from_pattern(re, NULL);
        if (!md) break;

        size_t out_cap = tb->len * 2 + 64;
        char *out = malloc(out_cap);
        assert(out);
        size_t out_len = 0;
        size_t search_at = 0;
        char *match_subject = braces_make_match_subject(tb->buf, tb->len);
        const char *subject = match_subject ? match_subject : tb->buf;

#define ENSURE_ARG_CAP(need) do { \
    while (out_len + (need) >= out_cap) { \
        out_cap *= 2; \
        out = realloc(out, out_cap); \
        assert(out); \
    } \
} while (0)

        while (search_at <= tb->len) {
            int rc = pcre2_match(re, (PCRE2_SPTR)subject, tb->len, search_at, 0, md, NULL);
            if (rc <= 0) {
                size_t rest = tb->len - search_at;
                ENSURE_ARG_CAP(rest + 1);
                memcpy(out + out_len, tb->buf + search_at, rest);
                out_len += rest;
                break;
            }

            PCRE2_SIZE *ov = pcre2_get_ovector_pointer(md);
            size_t ms = ov[0], me = ov[1];
            size_t cs = (ov[2] != PCRE2_UNSET) ? ov[2] : 0;
            size_t ce = (ov[3] != PCRE2_UNSET) ? ov[3] : 0;

            size_t before = ms - search_at;
            ENSURE_ARG_CAP(before + 32);
            memcpy(out + out_len, tb->buf + search_at, before);
            out_len += before;

            const char *inner = (cs < ce) ? tb->buf + cs : "";
            size_t inner_len = (cs < ce) ? (ce - cs) : 0;

            if (memchr(inner, '\0', inner_len)) {
                ENSURE_ARG_CAP(me - ms);
                memcpy(out + out_len, tb->buf + ms, me - ms);
                out_len += me - ms;
            } else {
                Token *tok = build_from_inner(inner, inner_len, true,
                                              NULL, 0, NULL, cfg, accum);
                if (tok) {
                    size_t idx = accum->count - 1;
                    char sent[64];
                    size_t slen;
                    char sym = braces_arg_symbol(inner, inner_len, cfg);
                    work_str_sentinel(idx, sym, sent, &slen);
                    ENSURE_ARG_CAP(slen);
                    memcpy(out + out_len, sent, slen);
                    out_len += slen;
                } else {
                    ENSURE_ARG_CAP(me - ms);
                    memcpy(out + out_len, tb->buf + ms, me - ms);
                    out_len += me - ms;
                }
            }

            search_at = me;
            if (me == ms) search_at++;
        }

#undef ENSURE_ARG_CAP

        out[out_len] = '\0';

        if (prev && prev_len == out_len && memcmp(prev, out, out_len) == 0) {
            free(out);
            free(match_subject);
            pcre2_match_data_free(md);
            break;
        }

        wiki_thread_buf_set(tb, out, out_len);
        free(out);

        free(prev);
        prev = malloc(out_len + 1);
        assert(prev);
        memcpy(prev, tb->buf, out_len);
        prev[out_len] = '\0';
        prev_len = out_len;
        free(match_subject);

        pcre2_match_data_free(md);
    }

    free(prev);
    pcre2_code_free(re);
}

/* Main parse function */
void parse_braces(ThreadBuf *tb, const ParserConfig *cfg, Accum *accum)
{
    if (!tb || !tb->buf) return;

    /* First, replace simple innermost triple-brace args. */
    parse_simple_args(tb, cfg, accum);

    /*
     * JS parity for reReplace in parser/braces.js:
     *   /(?<!\{)\{\{((?:[^\n{}[]|\[(?!\[)|\n(?![=\0]))*)\}\}
     *   |\{\{((?:[^\n{}[]|\[(?!\[)|\n(?![=\0]))*)\}\}(?!\})
     *   |\[\[(?:[^\n[\]{]|\n(?![=\0]))*\]\]
     *   |-\{(?:[^\n{}[]|\[(?!\[)|\n(?![=\0]))*\}-/gu
     *
     * Fallback (no lookbehind):
     *   /\{\{((?:[^\n{}[]|\[(?!\[)|\n(?![=\0]))*)\}\}(?!\})
     *   |\[\[(?:[^\n[\]{]|\n(?![=\0]))*\]\]
     *   |-\{(?:[^\n{}[]|\[(?!\[)|\n(?![=\0]))*\}-/gu
     */
    const char *pattern_with_lb =
        "(?<!\\{)\\{\\{((?:[^\\n{}\\[]|\\[(?!\\[)|\\n(?![=\\x00]))*)\\}\\}"
        "|\\{\\{((?:[^\\n{}\\[]|\\[(?!\\[)|\\n(?![=\\x00]))*)\\}\\}(?!\\})"
        "|\\[\\[(?:[^\\n\\[\\]\\{]|\\n(?![=\\x00]))*\\]\\]"
        "|-\\{(?:[^\\n{}\\[]|\\[(?!\\[)|\\n(?![=\\x00]))*\\}-";
    const char *pattern_fallback =
        "\\{\\{((?:[^\\n{}\\[]|\\[(?!\\[)|\\n(?![=\\x00]))*)\\}\\}(?!\\})"
        "|\\[\\[(?:[^\\n\\[\\]\\{]|\\n(?![=\\x00]))*\\]\\]"
        "|-\\{(?:[^\\n{}\\[]|\\[(?!\\[)|\\n(?![=\\x00]))*\\}-";

    if (!cfg->regex_braces) {
        ParserConfig *m = (ParserConfig *)cfg;
        PCRE2_SIZE err_offset;
        int err_code;
        m->regex_braces = (ParserConfigRegex *)pcre2_compile(
            (PCRE2_SPTR)pattern_with_lb, PCRE2_ZERO_TERMINATED,
            PCRE2_UTF,
            &err_code, &err_offset, NULL);
        if (!m->regex_braces) {
            m->regex_braces = (ParserConfigRegex *)pcre2_compile(
                (PCRE2_SPTR)pattern_fallback, PCRE2_ZERO_TERMINATED,
                PCRE2_UTF,
                &err_code, &err_offset, NULL);
        }
        if (!m->regex_braces) {
            PCRE2_UCHAR8 err_buf[256];
            pcre2_get_error_message(err_code, err_buf, sizeof(err_buf));
            log_error("braces regex compile error at %zu: %s",
                      err_offset, err_buf);
            return;
        }
    }
    pcre2_code *re = (pcre2_code *)cfg->regex_braces;

    /* linkStack: temporarily holds [[...]] and -{...}- text so brace matching
     * can proceed without those patterns interfering.  Stores the FULL matched
     * text (including delimiters) so it can be restored verbatim.
     * link_stack_lens[i] stores the binary-safe byte length of link_stack[i],
     * required because entries may contain embedded NUL bytes from token sentinels. */
    size_t link_cap = 16, link_count = 0;
    char   **link_stack      = malloc(link_cap * sizeof(char *));
    size_t  *link_stack_lens = malloc(link_cap * sizeof(size_t));
    assert(link_stack && link_stack_lens);

    char  *prev_buf     = NULL;
    size_t prev_buf_len = 0;

    while (1) {
        pcre2_match_data *md = pcre2_match_data_create_from_pattern(re, NULL);
        if (!md) { free(link_stack); free(link_stack_lens); return; }

        size_t out_cap = tb->len * 2 + 64;
        char  *out_buf = malloc(out_cap);
        assert(out_buf);
        size_t out_len    = 0;
        size_t search_at  = 0;
        char  *match_subject = braces_make_match_subject(tb->buf, tb->len);
        const char *subject = match_subject ? match_subject : tb->buf;

#define ENSURE_CAP(need) do { \
    while (out_len + (need) >= out_cap) { \
        out_cap *= 2; \
        out_buf = realloc(out_buf, out_cap); \
        assert(out_buf); \
    } \
} while (0)

        while (search_at <= tb->len) {
            int rc = pcre2_match(re, (PCRE2_SPTR)subject, tb->len,
                                 search_at, 0, md, NULL);
            if (rc <= 0) {
                size_t rest = tb->len - search_at;
                ENSURE_CAP(rest + 1);
                memcpy(out_buf + out_len, tb->buf + search_at, rest);
                out_len += rest;
                break;
            }

            PCRE2_SIZE *ov   = pcre2_get_ovector_pointer(md);
            size_t      mstart = ov[0];
            size_t      mend   = ov[1];

            /* Copy verbatim text before the match */
            size_t before = mstart - search_at;
            ENSURE_CAP(before + 8);
            memcpy(out_buf + out_len, tb->buf + search_at, before);
            out_len += before;

            /* Dispatch on which alternative matched. */
            if (ov[2] != PCRE2_UNSET || ov[4] != PCRE2_UNSET) {
                /* {{...}} branch matched with captured inner in group 1 or 2. */
                PCRE2_SIZE cstart = (ov[2] != PCRE2_UNSET) ? ov[2] : ov[4];
                PCRE2_SIZE cend   = (ov[2] != PCRE2_UNSET) ? ov[3] : ov[5];
                const char *inner = tb->buf + cstart;
                size_t inner_len = cend - cstart;

                /* Keep nested token markers intact; this parser stage does not
                 * resolve \0N<type>\x7F inside template parts. */
                /*
                 * NOTE: Do NOT skip if inner contains \0 bytes -- those are
                 * sentinel markers from prior iterations (e.g. {{Inner}} already
                 * tokenized). The build_from_inner/str_restore pipeline handles
                 * them correctly. Only skip if the regex pattern itself is unstable
                 * (i.e., inner would expand infinitely), which the convergence check
                 * handles via the prev_buf comparison.
                 */

                Token *tok = build_from_inner(inner, inner_len,
                                              false, link_stack, link_count,
                                              link_stack_lens,
                                              cfg,
                                              accum);
                if (tok) {
                    size_t tok_idx = accum->count - 1;
                    char sent[64]; size_t slen;
                    char sym = 't';
                    if (inner_len > 0) {
                        size_t p0_end = 0;
                        while (p0_end < inner_len && inner[p0_end] != '|') p0_end++;
                        sym = braces_get_symbol(inner, p0_end, cfg, NULL);
                    }
                    work_str_sentinel(tok_idx, sym, sent, &slen);
                    ENSURE_CAP(slen);
                    memcpy(out_buf + out_len, sent, slen);
                    out_len += slen;
                } else {
                    ENSURE_CAP(mend - mstart);
                    memcpy(out_buf + out_len, tb->buf + mstart, mend - mstart);
                    out_len += mend - mstart;
                }

            } else {
                /* [[...]] and -{...}- branches are parked and restored later. */
                size_t llen = mend - mstart;
                char  *tmp  = malloc(llen + 1);
                memcpy(tmp, tb->buf + mstart, llen);
                tmp[llen] = '\0';
                /* Restore any nested link-stack entries embedded in this match */
                size_t restored_llen = 0;
                char *restored = str_restore(tmp, llen,
                                             (const char **)link_stack, link_count,
                                             link_stack_lens,
                                             &restored_llen);
                free(tmp);

                if (link_count >= link_cap) {
                    link_cap  *= 2;
                    link_stack      = realloc(link_stack,      link_cap * sizeof(char *));
                    link_stack_lens = realloc(link_stack_lens, link_cap * sizeof(size_t));
                    assert(link_stack && link_stack_lens);
                }
                link_stack[link_count]      = restored;
                link_stack_lens[link_count] = restored_llen;
                size_t link_idx = link_count++;

                /* Numeric-only sentinel \0<N>\x7F — matched by str_restore */
                char mark[64];
                int n = snprintf(mark + 1, sizeof(mark) - 2, "%zu", link_idx);
                mark[0] = '\0';
                mark[1 + n] = '\x7F';
                size_t mlen = (size_t)(n + 2);
                ENSURE_CAP(mlen);
                memcpy(out_buf + out_len, mark, mlen);
                out_len += mlen;

            }

            search_at = mend;
            if (mend == mstart) search_at++;
        }

#undef ENSURE_CAP

        out_buf[out_len] = '\0';  /* NUL-terminate for safety */

        /* Binary-safe convergence check on the unresolved placeholder form. */
        if (prev_buf
            && prev_buf_len == out_len
            && memcmp(prev_buf, out_buf, out_len) == 0) {
            free(out_buf);
            free(prev_buf);
            free(match_subject);
            pcre2_match_data_free(md);
            break;
        }

        /* Keep parked placeholders for the next pass; restore only once at end. */
        wiki_thread_buf_set(tb, out_buf, out_len);

        free(prev_buf);
        prev_buf = malloc(out_len + 1);
        memcpy(prev_buf, out_buf, out_len);
        prev_buf[out_len] = '\0';
        prev_buf_len = out_len;
        free(out_buf);
        free(match_subject);

        pcre2_match_data_free(md);
    }

    /* Final restoration of parked [[...]] / -{...}- placeholders. */
    {
        size_t restored_len = 0;
        char *restored_all = str_restore(tb->buf, tb->len,
                                         (const char **)link_stack, link_count,
                                         link_stack_lens,
                                         &restored_len);
        wiki_thread_buf_set(tb, restored_all, restored_len);
        free(restored_all);
    }

    /* Cleanup link_stack */
    for (size_t i = 0; i < link_count; i++) free(link_stack[i]);
    free(link_stack);
    free(link_stack_lens);
}
