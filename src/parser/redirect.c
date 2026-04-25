#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

#include "parser/redirect.h"
#include "title.h"
#include "string_util.h"
#include "log.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <stdio.h>

/* ── PCRE2 compile / free wrappers ──────────────────────────────────────── */

static pcre2_code *compile_redirect_regex(const ParserConfig *cfg)
{
    size_t pattern_cap = 128;
    for (size_t i = 0; i < cfg->redirection.count; i++) {
        pattern_cap += strlen(cfg->redirection.items[i]) * 2 + 4;
    }

    char *pattern = malloc(pattern_cap);
    assert(pattern);

    size_t pos = 0;
    pos += (size_t)snprintf(pattern + pos, pattern_cap - pos,
                            "^(\\s*)((?:");
    for (size_t i = 0; i < cfg->redirection.count; i++) {
        if (i > 0) pattern[pos++] = '|';
        const char *kw = cfg->redirection.items[i];
        while (*kw) {
            unsigned char c = (unsigned char)*kw;
            if (c < 0x80 && !isalnum((int)c) && c != '_' && c != '-') {
                pattern[pos++] = '\\';
            }
            pattern[pos++] = (char)c;
            kw++;
        }
    }
    pos += (size_t)snprintf(pattern + pos, pattern_cap - pos,
                            ")\\s*(?::\\s*)?)\\[\\[([^\\n|\\]]+)(\\|.*?)?\\]\\](\\s*)");

    PCRE2_SIZE err_offset;
    int err_code;
    pcre2_code *re = pcre2_compile(
        (PCRE2_SPTR)pattern, PCRE2_ZERO_TERMINATED,
        PCRE2_CASELESS | PCRE2_UTF,
        &err_code, &err_offset, NULL);

    if (!re) {
        PCRE2_UCHAR8 err_buf[256];
        pcre2_get_error_message(err_code, err_buf, sizeof(err_buf));
        log_error("redirect regex compile error at %zu: %s Pattern: %s",
                  err_offset, err_buf, pattern);
    }
    free(pattern);
    return re;
}

/* ── Token constructors ──────────────────────────────────────────────────── */

static Token *build_redirect_token(
    const char *pre,  size_t pre_len,
    const char *syn,  size_t syn_len,
    const char *link, size_t link_len,
    const char *disp, size_t disp_len,
    const char *post, size_t post_len,
    Accum *accum)
{
    Token *syn_tok = token_new(TOKEN_REDIRECT_SYNTAX, "redirect-syntax");
    if (!syn_tok) return NULL;
    token_append_text_n(syn_tok, syn, syn_len);
    accum_push(accum, syn_tok);

    Token *link_atom = token_new(TOKEN_ATOM, "link-target");
    if (!link_atom) return NULL;
    token_append_text_n(link_atom, link, link_len);
    accum_push(accum, link_atom);

    Token *target_tok = token_new(TOKEN_REDIRECT_TARGET, "redirect-target");
    if (!target_tok) return NULL;
    size_t link_main_len = link_len;
    for (size_t i = 0; i < link_len; i++) {
        if (link[i] == '#') {
            link_main_len = i;
            break;
        }
    }
    while (link_main_len > 0 && isspace((unsigned char)link[link_main_len - 1])) {
        link_main_len--;
    }
    target_tok->name = title_normalize(link, link_main_len);
    token_append_child(target_tok, link_atom);

    if (disp && disp_len > 0) {
        Token *noinclude = token_new(TOKEN_NOINCLUDE, "noinclude");
        if (!noinclude) return NULL;
        token_append_text_n(noinclude, disp, disp_len);
        accum_push(accum, noinclude);
        token_append_child(target_tok, noinclude);
    }
    accum_push(accum, target_tok);

    Token *redir = token_new(TOKEN_REDIRECT, "redirect");
    if (!redir) return NULL;
    redir->data.redirect.pre  = malloc(pre_len + 1);
    memcpy(redir->data.redirect.pre, pre, pre_len);
    redir->data.redirect.pre[pre_len] = '\0';
    if (post && post_len > 0) {
        redir->data.redirect.post = malloc(post_len + 1);
        memcpy(redir->data.redirect.post, post, post_len);
        redir->data.redirect.post[post_len] = '\0';
    } else {
        redir->data.redirect.post = strdup("");
    }
    redir->data.redirect.link = malloc(link_len + 1);
    memcpy(redir->data.redirect.link, link, link_len);
    redir->data.redirect.link[link_len] = '\0';

    token_append_child(redir, syn_tok);
    token_append_child(redir, target_tok);
    accum_push(accum, redir);

    return redir;
}

bool parse_redirect(ThreadBuf *tb, const ParserConfig *cfg, Accum *accum)
{
    if (!cfg->regex_redirect) {
        ParserConfig *mutable_cfg = (ParserConfig *)cfg;
        mutable_cfg->regex_redirect = (ParserConfigRegex *)compile_redirect_regex(cfg);
        if (!mutable_cfg->regex_redirect) return false;
    }

    pcre2_code *re = (pcre2_code *)cfg->regex_redirect;
    pcre2_match_data *md = pcre2_match_data_create_from_pattern(re, NULL);
    if (!md) return false;

    int rc = pcre2_match(re, (PCRE2_SPTR)tb->buf, tb->len, 0, 0, md, NULL);
    if (rc <= 0) {
        pcre2_match_data_free(md);
        return false;
    }

    PCRE2_SIZE *ovector = pcre2_get_ovector_pointer(md);
    size_t full_end = ovector[1];

    size_t pre_s  = ovector[2], pre_e  = ovector[3];
    size_t syn_s  = ovector[4], syn_e  = ovector[5];
    size_t link_s = ovector[6], link_e = ovector[7];
    size_t disp_s = (rc >= 5 && ovector[8] != PCRE2_UNSET) ? ovector[8] : 0;
    size_t disp_e = (rc >= 5 && ovector[9] != PCRE2_UNSET) ? ovector[9] : 0;

    const char *link_ptr = tb->buf + link_s;
    size_t      link_len = link_e - link_s;

    if (!title_is_valid_half_parsed(link_ptr, link_len, cfg)) {
        pcre2_match_data_free(md);
        return false;
    }

    const char *disp_ptr = NULL;
    size_t      disp_len = 0;
    if (disp_s < disp_e && disp_s != PCRE2_UNSET) {
        disp_ptr = tb->buf + disp_s + 1;
        disp_len = (disp_e - disp_s > 0) ? disp_e - disp_s - 1 : 0;
    }

    size_t post_s = (rc >= 6 && ovector[10] != PCRE2_UNSET) ? ovector[10] : 0;
    size_t post_e = (rc >= 6 && ovector[11] != PCRE2_UNSET) ? ovector[11] : 0;
    const char *post_ptr = NULL;
    size_t post_len = 0;
    if (post_e > post_s) {
        post_ptr = tb->buf + post_s;
        post_len = post_e - post_s;
    }

    Token *redir = build_redirect_token(
        tb->buf + pre_s, pre_e - pre_s,
        tb->buf + syn_s, syn_e - syn_s,
        link_ptr,        link_len,
        disp_ptr,        disp_len,
        post_ptr,        post_len,
        accum);

    pcre2_match_data_free(md);
    if (!redir) return false;

    size_t sent_idx = 0;
    for (size_t i = 0; i < accum->count; i++) {
        if (accum->tokens[i] == redir) { sent_idx = i; break; }
    }

    size_t rest_len = tb->len - full_end;
    char sent_buf[32];
    size_t sent_len;
    work_str_sentinel(sent_idx, 'o', sent_buf, &sent_len);

    size_t new_len = sent_len + rest_len;
    char *new_buf  = malloc(new_len + 1);
    assert(new_buf);
    memcpy(new_buf,           sent_buf,           sent_len);
    memcpy(new_buf + sent_len, tb->buf + full_end, rest_len);
    new_buf[new_len] = '\0';

    wiki_thread_buf_set(tb, new_buf, new_len);
    free(new_buf);
    return true;
}
