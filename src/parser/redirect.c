#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

#include "util/log.h"
#include "parser/redirect.h"
#include "string_util.h"
#include "title.h"
#include "thread_buffer.h"
#include "util/pcre_cache.h"
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── PCRE2 compile / free wrappers ──────────────────────────────────────── */

static pcre2_code *compile_redirect_regex(const ParserConfig *cfg) {
	/* Lazily build and cache the redirect pattern string in cfg. */
	if(!cfg->pattern_redirect || !cfg->pattern_redirect[0]) {
		size_t pattern_cap= 128;
		for(size_t i= 0; i < cfg->redirection.count; i++) {
			pattern_cap+= strlen(cfg->redirection.items[i]) * 2 + 4;
		}

		char *pattern= malloc(pattern_cap);
		assert(pattern);

		size_t pos= 0;
		pos+= (size_t)snprintf(pattern + pos, pattern_cap - pos,
							   "^(\\s*)((?:");
		for(size_t i= 0; i < cfg->redirection.count; i++) {
			if(i > 0) pattern[pos++]= '|';
			const char *kw= cfg->redirection.items[i];
			while(*kw) {
				unsigned char c= (unsigned char)*kw;
				if(c < 0x80 && !isalnum((int)c) && c != '_' && c != '-') {
					pattern[pos++]= '\\';
				}
				pattern[pos++]= (char)c;
				kw++;
			}
		}
		pos+= (size_t)snprintf(pattern + pos, pattern_cap - pos,
							   ")\\s*(?::\\s*)?)\\[\\[([^\\n|\\]]+)(\\|.*?)?\\]\\](\\s*)");

		((ParserConfig *)cfg)->pattern_redirect = pattern;
	}

	return pcre_cache_get(cfg->pattern_redirect, PCRE2_CASELESS | PCRE2_UTF);
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
		target_tok->name= strdup(parsed->title);
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
	redir->data.redirect.pre= malloc(pre_len + 1);
	memcpy(redir->data.redirect.pre, pre, pre_len);
	redir->data.redirect.pre[pre_len]= '\0';
	if(post && post_len > 0) {
		redir->data.redirect.post= malloc(post_len + 1);
		memcpy(redir->data.redirect.post, post, post_len);
		redir->data.redirect.post[post_len]= '\0';
	} else {
		redir->data.redirect.post= strdup("");
	}
	redir->data.redirect.link= malloc(link_len + 1);
	memcpy(redir->data.redirect.link, link, link_len);
	redir->data.redirect.link[link_len]= '\0';

	token_append_child(redir, syn_tok);
	token_append_child(redir, target_tok);
	accum_push(accum, redir);

	return redir;
}

bool parse_redirect(ThreadBuf *tb, const ParserConfig *cfg, Accum *accum) {
	pcre2_code *re= compile_redirect_regex(cfg);
	pcre2_match_data *md= pcre2_match_data_create_from_pattern(re, NULL);
	if(!md) return false;

	int rc= pcre2_match(re, (PCRE2_SPTR)tb->buf, tb->len, 0, 0, md, NULL);
	if(rc <= 0) {
		pcre2_match_data_free(md);
		return false;
	}

	PCRE2_SIZE *ovector= pcre2_get_ovector_pointer(md);
	size_t full_end= ovector[1];

	size_t pre_s= ovector[2], pre_e= ovector[3];
	size_t syn_s= ovector[4], syn_e= ovector[5];
	size_t link_s= ovector[6], link_e= ovector[7];
	size_t disp_s= (rc >= 5 && ovector[8] != PCRE2_UNSET) ? ovector[8] : 0;
	size_t disp_e= (rc >= 5 && ovector[9] != PCRE2_UNSET) ? ovector[9] : 0;

	const char *link_ptr= tb->buf + link_s;
	size_t link_len= link_e - link_s;

	if(!title_is_valid_half_parsed(link_ptr, link_len, cfg)) {
		pcre2_match_data_free(md);
		return false;
	}

	const char *disp_ptr= NULL;
	size_t disp_len= 0;
	if(disp_s < disp_e && disp_s != PCRE2_UNSET) {
		disp_ptr= tb->buf + disp_s + 1;
		disp_len= (disp_e - disp_s > 0) ? disp_e - disp_s - 1 : 0;
	}

	size_t post_s= (rc >= 6 && ovector[10] != PCRE2_UNSET) ? ovector[10] : 0;
	size_t post_e= (rc >= 6 && ovector[11] != PCRE2_UNSET) ? ovector[11] : 0;
	const char *post_ptr= NULL;
	size_t post_len= 0;
	if(post_e > post_s) {
		post_ptr= tb->buf + post_s;
		post_len= post_e - post_s;
	}

	Token *redir= build_redirect_token(
	tb->buf + pre_s, pre_e - pre_s,
	tb->buf + syn_s, syn_e - syn_s,
	link_ptr, link_len,
	disp_ptr, disp_len,
	post_ptr, post_len,
	cfg,
	accum);

	pcre2_match_data_free(md);
	if(!redir) return false;

	size_t sent_idx= 0;
	for(size_t i= 0; i < accum->count; i++) {
		if(accum->tokens[i] == redir) {
			sent_idx= i;
			break;
		}
	}

	size_t rest_len= tb->len - full_end;
	char sent_buf[32];
	size_t sent_len;
	work_str_sentinel(sent_idx, 'o', sent_buf, &sent_len);

	size_t new_len= sent_len + rest_len;
	char *new_buf= malloc(new_len + 1);
	assert(new_buf);
	memcpy(new_buf, sent_buf, sent_len);
	memcpy(new_buf + sent_len, tb->buf + full_end, rest_len);
	new_buf[new_len]= '\0';

	wiki_thread_buf_set(tb, new_buf, new_len);
	free(new_buf);
	return true;
}
