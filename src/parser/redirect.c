#include "util/log.h"
#include "parser/redirect.h"
#include "util/string_util.h"
#include "title.h"
#include "util/thread_buffer.h"
#include "util/wiki_parser_rules.h"
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Helper functions for redirect prefix parsing ──────────────────────── */

static inline bool is_ws_char(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
}

static bool ci_starts_with_n(const char *s, size_t slen, const char *pat, size_t plen) {
    if(plen > slen) return false;
    return str_ci_eq_n(s, pat, plen);
}

static bool parse_redirect_prefix(const char *s, size_t len, const ParserConfig *cfg,
                                  size_t *pre_e, size_t *syn_s, size_t *syn_e,
                                  size_t *link_s, size_t *link_e,
                                  size_t *disp_s, size_t *disp_e,
                                  size_t *post_s, size_t *post_e,
                                  size_t *full_end) {
    if(!s || !cfg) return false;
    size_t p = 0;
    while(p < len && is_ws_char((unsigned char)s[p])) p++;
    *pre_e = p;

    size_t kw_len = 0;
    bool matched_kw = false;
    for(size_t i = 0; i < cfg->redirection.count; i++) {
        sz_ptr_t kw;
        sz_size_t klen;
        sz_string_range(&cfg->redirection.items[i], &kw, &klen);
        if(!kw) continue;
        if(ci_starts_with_n(s + p, len - p, (const char *)kw, klen)) {
            kw_len = klen;
            matched_kw = true;
            break; /* left-to-right alternation parity */
        }
    }
    if(!matched_kw) return false;

    *syn_s = p;
    p += kw_len;
    while(p < len && is_ws_char((unsigned char)s[p])) p++;
    if(p < len && s[p] == ':') {
        p++;
        while(p < len && is_ws_char((unsigned char)s[p])) p++;
    }
    *syn_e = p;

    if(p + 1 >= len || s[p] != '[' || s[p + 1] != '[') return false;
    p += 2;
    *link_s = p;
    while(p < len && s[p] != '\n' && s[p] != '|' && s[p] != ']') p++;
    if(p <= *link_s) return false;
    *link_e = p;

    *disp_s = SIZE_MAX;
    *disp_e = SIZE_MAX;
    if(p < len && s[p] == '|') {
        *disp_s = p;
        p++;
        /* JS capture is (\|.*?)? with no DOTALL, so display text cannot cross '\n'.
         * Stop at first "]]" or newline, whichever comes first. */
        while(p + 1 < len && s[p] != '\n' && !(s[p] == ']' && s[p + 1] == ']')) p++;
        *disp_e = p;
    }

    if(p + 1 >= len || s[p] != ']' || s[p + 1] != ']') return false;
    p += 2;

    *post_s = p;
    while(p < len && is_ws_char((unsigned char)s[p])) p++;
    *post_e = p;
    *full_end = p;
    return true;
}

/* ── Token constructors ──────────────────────────────────────────────────── */

static Token *build_redirect_token(
const char *pre, size_t pre_len,
const char *syn, size_t syn_len,
const char *link, size_t link_len,
const char *disp, size_t disp_len,
const char *post, size_t post_len,
const ParserConfig *cfg,
Accum *accum) {
	Token *syn_tok= token_new(TOKEN_REDIRECT_SYNTAX, "redirect-syntax");
	if(!syn_tok) return NULL;
	if(syn_len > 0) {
		const char *syn_view = wiki_thread_buf_append_to_tokens(syn, syn_len);
		if(syn_view) token_append_text_n(syn_tok, syn_view, syn_len);
	} else {
		token_append_text_n(syn_tok, NULL, 0);
	}
	accum_push(accum, syn_tok);

	Token *link_atom= token_new(TOKEN_ATOM, "link-target");
	if(!link_atom) return NULL;
	if(link_len > 0) {
		const char *link_view = wiki_thread_buf_append_to_tokens(link, link_len);
		if(link_view) token_append_text_n(link_atom, link_view, link_len);
	} else {
		token_append_text_n(link_atom, NULL, 0);
	}
	accum_push(accum, link_atom);

	Token *target_tok= token_new(TOKEN_REDIRECT_TARGET, "redirect-target");
	if(!target_tok) return NULL;
	size_t link_main_len= link_len;
	for(size_t i= 0; i < link_len; i++) {
		if(link[i] == '#') {
			link_main_len= i;
			break;
		}
	}
	while(link_main_len > 0 && isspace((unsigned char)link[link_main_len - 1])) {
		link_main_len--;
	}
	Title *parsed= title_parse_half_parsed(link, link_main_len, 0, cfg, true, "");
	if(parsed && parsed->title) {
		target_tok->name.start= strdup(parsed->title);
	}
	title_free(parsed);
	token_append_child(target_tok, link_atom);

	/* JS parity: RedirectTargetToken creates NoincludeToken whenever text?.slice(1)
     * is not undefined — i.e. whenever the '|' delimiter was present in the input,
     * even if the display text is empty (e.g. [[target|]]).
     * disp is non-NULL iff the '|' group was captured by the regex. */
	if(disp) {
		Token *noinclude= token_new(TOKEN_NOINCLUDE, "noinclude");
		if(!noinclude) return NULL;
		if(disp_len > 0) {
			const char *disp_view = wiki_thread_buf_append_to_tokens(disp, disp_len);
			if(disp_view) token_append_text_n(noinclude, disp_view, disp_len);
		}
		accum_push(accum, noinclude);
		token_append_child(target_tok, noinclude);
	}
	accum_push(accum, target_tok);

	Token *redir= token_new(TOKEN_REDIRECT, "redirect");
	if(!redir) return NULL;
	char *pre_owned= malloc(pre_len + 1);
	sz_copy(pre_owned, pre, pre_len);
	pre_owned[pre_len]= '\0';
	redir->data.redirect.pre = (sz_string_view_t){ .start = pre_owned, .length = pre_len };
	if(post && post_len > 0) {
		char *post_owned= malloc(post_len + 1);
		sz_copy(post_owned, post, post_len);
		post_owned[post_len]= '\0';
		redir->data.redirect.post = (sz_string_view_t){ .start = post_owned, .length = post_len };
	} else {
		char *post_owned= strdup("");
		redir->data.redirect.post = (sz_string_view_t){ .start = post_owned, .length = 0 };
	}
	char *link_owned= malloc(link_len + 1);
	sz_copy(link_owned, link, link_len);
	link_owned[link_len]= '\0';
	redir->data.redirect.link = (sz_string_view_t){ .start = link_owned, .length = link_len };

	token_append_child(redir, syn_tok);
	token_append_child(redir, target_tok);
	accum_push(accum, redir);

	return redir;
}

bool parse_redirect(ThreadBuf *tb, const ParserConfig *cfg, Accum *accum) {
	if(!tb || !tb->buf || !cfg || !accum) return false;

	size_t pre_e = 0, syn_s = 0, syn_e = 0;
	size_t link_s = 0, link_e = 0;
	size_t disp_s = SIZE_MAX, disp_e = SIZE_MAX;
	size_t post_s = 0, post_e = 0, full_end = 0;

	if(!parse_redirect_prefix(tb->buf, tb->len, cfg,
							  &pre_e, &syn_s, &syn_e,
							  &link_s, &link_e,
							  &disp_s, &disp_e,
							  &post_s, &post_e,
							  &full_end)) {
		return false;
	}

	const char *link_ptr = tb->buf + link_s;
	size_t link_len = link_e - link_s;
	if(!title_is_valid_half_parsed(link_ptr, link_len, cfg)) return false;

	const char *disp_ptr = NULL;
	size_t disp_len = 0;
	if(disp_s != SIZE_MAX && disp_e != SIZE_MAX && disp_e >= disp_s + 1) {
		disp_ptr = tb->buf + disp_s + 1;
		disp_len = disp_e - disp_s - 1;
	}

	const char *post_ptr = (post_e > post_s) ? (tb->buf + post_s) : NULL;
	size_t post_len = (post_e > post_s) ? (post_e - post_s) : 0;

	Token *redir = build_redirect_token(
		tb->buf, pre_e,
		tb->buf + syn_s, syn_e - syn_s,
		link_ptr, link_len,
		disp_ptr, disp_len,
		post_ptr, post_len,
		cfg, accum);
	if(!redir) return false;

	/* The redirect token was the last item pushed to accum */
	size_t sent_idx = accum->count - 1;

	char sent_buf[32];
	size_t sent_len = 0;
	work_str_sentinel(sent_idx, 'o', sent_buf, &sent_len);

	size_t rest_len = tb->len - full_end;
	size_t new_len = sent_len + rest_len;
	wiki_thread_buf_reserve(tb, new_len + 1);
	/* Assemble sentinel + remaining content directly into tb */
	sz_copy(tb->buf, sent_buf, sent_len);
	if(rest_len > 0) sz_copy(tb->buf + sent_len, tb->buf + full_end, rest_len);
	tb->buf[new_len] = '\0';
	tb->len = new_len;
	return true;
}
