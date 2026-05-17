#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

#include "util/log.h"
#include "parser/hr_and_double_underscore.h"
#include "util/string_util.h"
#include "util/thread_buffer.h"
#include "util/wiki_parser_rules.h"
#include "token.h"
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "util/pcre_cache.h"

/*
 * Stage 4: horizontal rules (lines with 4+ dashes) and double-underscore
 * magic words like __TOC__, __NOTOC__, etc.  This implementation uses
 * callback scanners (parser_scan) with ParserRules for HR and double-underscore
 * detection, then validates double-underscore keys against the ParserConfig
 * lists before creating tokens.
 *
 * Heading finalization still uses PCRE (to be migrated later as B.2).
 */

/* Lowercase ASCII-only copy */
static char *lower_copy(const char *s, size_t len) {
	char *out= malloc(len + 1);
	assert(out);
	for(size_t i= 0; i < len; i++) out[i]= (char)tolower((unsigned char)s[i]);
	out[len]= '\0';
	return out;
}

/* Forward declarations for helper functions used in dunder_cb */
static int strlist_has_exact(const StrList *sl, const char *s, size_t len);
static int strlist_has_lower(const StrList *sl, const char *s, size_t len);
static const char *strmap_get_exact(const StrMap *m, const char *key);

/* Skip a CNO sentinel: \0\d+[cn]\x7F */
static size_t skip_cno_sentinel(const char *p, size_t rem) {
	if(!p || rem < 4 || (unsigned char)p[0] != 0) return 0;
	size_t j = 1;
	if(j >= rem || p[j] < '0' || p[j] > '9') return 0;
	while(j < rem && p[j] >= '0' && p[j] <= '9') j++;
	if(j + 1 >= rem) return 0;
	if((p[j] != 'c' && p[j] != 'n' && p[j] != 'o') || (unsigned char)p[j + 1] != 0x7F) return 0;
	return j + 2;
}

/* ── HR pass: detect lines with 4+ dashes after optional CNO sentinels ──── */

static void parse_hr_pass(ThreadBuf *tb, Accum *accum) {
	size_t out_cap = tb->len * 2 + 64;
	char *out = malloc(out_cap);
	if(!out) { log_fatal("OOM in parse_hr_pass"); abort(); }
	size_t out_len = 0;
	size_t i = 0;

#define ENSURE_OUT(N) do { \
	while(out_len + (N) + 1 >= out_cap) { \
		if(out_cap > SIZE_MAX / 2) { log_fatal("buffer size overflow in parse_hr_pass"); abort(); } \
		out_cap *= 2; \
		char *_hr_tmp = realloc(out, out_cap); \
		if(!_hr_tmp) { free(out); log_fatal("OOM in realloc"); abort(); } \
		out = _hr_tmp; \
	} \
} while(0)

	while(i < tb->len) {
		size_t line_start = i;
		size_t line_end = i;
		while(line_end < tb->len && tb->buf[line_end] != '\n') line_end++;

		size_t p = line_start;
		while(p < line_end) {
			size_t sc = skip_cno_sentinel(tb->buf + p, line_end - p);
			if(sc == 0) break;
			ENSURE_OUT(sc);
			memcpy(out + out_len, tb->buf + p, sc);
			out_len += sc;
			p += sc;
		}

		size_t dash = p;
		while(dash < line_end && tb->buf[dash] == '-') dash++;
		if(dash - p >= 4) {
			Token *t = token_new(TOKEN_HR, "hr");
			if(t) {
				const char *v = wiki_thread_buf_append_to_tokens(tb->buf + p, dash - p);
				if(v) token_append_text_n(t, v, dash - p);
				accum_push(accum, t);
				char sent[64]; size_t slen = 0;
				work_str_sentinel(accum->count - 1, 'r', sent, &slen);
				ENSURE_OUT(slen + (line_end - dash));
				memcpy(out + out_len, sent, slen); out_len += slen;
				if(line_end > dash) { memcpy(out + out_len, tb->buf + dash, line_end - dash); out_len += line_end - dash; }
			}
		} else {
			ENSURE_OUT(line_end - p);
			memcpy(out + out_len, tb->buf + p, line_end - p);
			out_len += line_end - p;
		}

		if(line_end < tb->len) { ENSURE_OUT(1); out[out_len++] = '\n'; }
		i = (line_end < tb->len) ? (line_end + 1) : line_end;
	}

	out[out_len] = '\0';
	wiki_thread_buf_set(tb, out, out_len);
	free(out);
#undef ENSURE_OUT
}

/* ── Double-underscore pass ──────────────────────────────────────────────── */

typedef struct {
	ThreadBuf *out;
	const ParserConfig *cfg;
	Accum *accum;
	bool fullwidth;
} DunderCtx;

static void dunder_emit_raw(DunderCtx *ctx, const char *inner, size_t len) {
	static const char fw[] = "\xEF\xBC\xBF\xEF\xBC\xBF"; /* ＿＿ */
	const char *delim = ctx->fullwidth ? fw : "__";
	size_t dlen = ctx->fullwidth ? 6 : 2;
	wiki_thread_buf_append(ctx->out, (sz_string_view_t){ .start = delim, .length = dlen });
	if(len) wiki_thread_buf_append(ctx->out, (sz_string_view_t){ .start = inner, .length = len });
	wiki_thread_buf_append(ctx->out, (sz_string_view_t){ .start = delim, .length = dlen });
}

static void dunder_cb(const char *seg, size_t len, ParserSegmentKind kind, void *ud) {
	DunderCtx *ctx = (DunderCtx *)ud;
	if(kind == PARSER_SEG_TEXT) {
		wiki_thread_buf_append(ctx->out, (sz_string_view_t){ .start = seg, .length = len });
		return;
	}

	int case_sensitive = strlist_has_exact(&ctx->cfg->double_underscore[1], seg, len);
	int case_insensitive = strlist_has_lower(&ctx->cfg->double_underscore[0], seg, len);
	if(!(case_sensitive || case_insensitive)) {
		dunder_emit_raw(ctx, seg, len);
		return;
	}

	Token *t = token_new(TOKEN_DOUBLE_UNDERSCORE, "double-underscore");
	if(!t) {
		dunder_emit_raw(ctx, seg, len);
		return;
	}

	t->data.dunder.case_sensitive = case_sensitive != 0;
	t->data.dunder.fullwidth = ctx->fullwidth;

	char *lc = lower_copy(seg, len);
	const char *alias = NULL;
	if(case_sensitive) {
		char *raw = malloc(len + 1);
		if(!raw) { log_fatal("OOM in dunder_cb"); abort(); }
		memcpy(raw, seg, len);
		raw[len] = '\0';
		alias = strmap_get_exact(&ctx->cfg->double_underscore_alias[1], raw);
		free(raw);
	} else if(lc) {
		alias = strmap_get_exact(&ctx->cfg->double_underscore_alias[0], lc);
	}

	if(alias && alias[0]) {
		size_t alen = strlen(alias);
		t->name = lower_copy(alias, alen);
		free(lc);
	} else {
		t->name = lc;
	}

	if(len > 0) {
		const char *view = wiki_thread_buf_append_to_tokens(seg, len);
		if(view) token_append_text_n(t, view, len);
	}

	accum_push(ctx->accum, t);

	char ch = 'n';
	if(case_insensitive && t->name && strcmp(t->name, "toc") == 0) ch = 'u';

	char sent[64]; size_t slen = 0;
	work_str_sentinel(ctx->accum->count - 1, ch, sent, &slen);
	wiki_thread_buf_append(ctx->out, (sz_string_view_t){ .start = sent, .length = slen });
}

static void parse_dunder_pass(ThreadBuf *tb, const ParserRules *rule,
							  bool fullwidth, const ParserConfig *cfg, Accum *accum) {
	ThreadBuf *out = wiki_thread_buf_acquire_scratch();
	if(!out) { log_fatal("thread_buffer: failed to acquire scratch in parse_dunder_pass"); abort(); }
	out->len = 0;

	DunderCtx ctx = { .out = out, .cfg = cfg, .accum = accum, .fullwidth = fullwidth };
	parser_scan(tb->buf, tb->len, rule, dunder_cb, &ctx);

	out->buf[out->len] = '\0';
	wiki_thread_buf_set(tb, out->buf, out->len);
	wiki_thread_buf_release_scratch(out);
}

/* ── Helper functions for dunder validation ─────────────────────────────── */

static int strlist_has_exact(const StrList *sl, const char *s, size_t len) {
	if(!sl) return 0;
	for(size_t i= 0; i < sl->count; i++) {
		const char *it= sl->items[i];
		if(!it) continue;
		if(strlen(it) == len && strncmp(it, s, len) == 0) return 1;
	}
	return 0;
}

static int strlist_has_lower(const StrList *sl, const char *s, size_t len) {
	if(!sl) return 0;
	for(size_t i= 0; i < sl->count; i++) {
		const char *it= sl->items[i];
		if(!it) continue;
		size_t il= strlen(it);
		if(il != len) continue;
		int ok= 1;
		for(size_t k= 0; k < len; k++) {
			if(tolower((unsigned char)it[k]) != tolower((unsigned char)s[k])) {
				ok= 0;
				break;
			}
		}
		if(ok) return 1;
	}
	return 0;
}

static const char *strmap_get_exact(const StrMap *m, const char *key) {
	if(!m || !key) return NULL;
	for(size_t i= 0; i < m->count; i++) {
		if(m->keys[i] && m->values[i] && strcmp(m->keys[i], key) == 0) {
			return m->values[i];
		}
	}
	return NULL;
}

void parse_hr_and_double_underscore(ThreadBuf *tb, const ParserConfig *cfg, Accum *accum,
									TokenType root_type, const char *root_name) {
	if(!tb || !tb->buf) return;

	bool prefixed= root_type != TOKEN_ROOT && !(root_type == TOKEN_EXT_INNER && root_name && strcmp(root_name, "poem") == 0);
	if(prefixed) {
		char *pref= malloc(tb->len + 1);
		assert(pref);
		pref[0]= '\0';
		if(tb->len > 0) {
			memcpy(pref + 1, tb->buf, tb->len);
		}
		wiki_thread_buf_set(tb, pref, tb->len + 1);
		free(pref);
	}

	/* New callback-based passes for HR and double-underscore */
	parse_hr_pass(tb, accum);
	parse_dunder_pass(tb, &wiki_rule_dunder_ascii, false, cfg, accum);
	parse_dunder_pass(tb, &wiki_rule_dunder_fullwidth, true, cfg, accum);

	/* Heading finalization: turn lines like "== Title ==" into heading tokens */
	{
		/* Use cached heading regex compiled once per process. */
		const char *hpat= "^((?:\\x00\\d+[cn]\\x7F)*)(={1,6})(.+)\\2((?:\\s|\\x00\\d+[cn]\\x7F)*)$";
			pcre2_code *hre = pcre_cache_get(hpat,
						PCRE2_UTF | PCRE2_MULTILINE | PCRE2_UCP);
		pcre2_match_data *hmd = pcre2_match_data_create_from_pattern(hre, NULL);
		if(!hmd) {
			log_error("hr_and_double_underscore: failed to create match data");
			return;
		}

		size_t out_cap2= tb->len * 2 + 64;
		char *out2= malloc(out_cap2);
		assert(out2);
		size_t out2_len= 0;
		size_t search2= 0;

		while(search2 <= tb->len) {
			int rc= pcre2_match(hre, (PCRE2_SPTR)tb->buf, tb->len, search2, 0, hmd, NULL);
			if(rc <= 0) {
				size_t rest= tb->len - search2;
				if(out2_len + rest + 1 > out_cap2) {
					out_cap2= out2_len + rest + 1;
					out2= realloc(out2, out_cap2);
					assert(out2);
				}
				memcpy(out2 + out2_len, tb->buf + search2, rest);
				out2_len+= rest;
				break;
			}

			PCRE2_SIZE *ov= pcre2_get_ovector_pointer(hmd);
			size_t ms= ov[0], me= ov[1];
			size_t before= ms - search2;
			if(out2_len + before + 32 > out_cap2) {
				out_cap2= out2_len + before + 32;
				out2= realloc(out2, out_cap2);
				assert(out2);
			}
			memcpy(out2 + out2_len, tb->buf + search2, before);
			out2_len+= before;

			size_t eq_s= (rc > 2 && ov[4] != PCRE2_UNSET) ? ov[4] : 0;
			size_t eq_e= (rc > 2 && ov[5] != PCRE2_UNSET) ? ov[5] : 0;
			size_t text_s= (rc > 3 && ov[6] != PCRE2_UNSET) ? ov[6] : 0;
			size_t text_e= (rc > 3 && ov[7] != PCRE2_UNSET) ? ov[7] : 0;
			size_t trail_s= (rc > 4 && ov[8] != PCRE2_UNSET) ? ov[8] : 0;
			size_t trail_e= (rc > 4 && ov[9] != PCRE2_UNSET) ? ov[9] : 0;

			/* Build heading token: level = length of eq (eq_e - eq_s) */
			int level= (int)(eq_e > eq_s ? eq_e - eq_s : 0);
			/* Extract heading inner text and trailing text */
			const char *h_inner= (text_e > text_s) ? tb->buf + text_s : "";
			size_t h_inner_len= (text_e > text_s) ? text_e - text_s : 0;
			const char *h_trail= (trail_e > trail_s) ? tb->buf + trail_s : "";
			size_t h_trail_len= (trail_e > trail_s) ? trail_e - trail_s : 0;
			size_t post_trail_len= 0;
			if(h_trail_len > 0) {
				bool only_line_endings= true;
				for(size_t ti= 0; ti < h_trail_len; ti++) {
					if(h_trail[ti] != '\n' && h_trail[ti] != '\r') {
						only_line_endings= false;
						break;
					}
				}
				if(only_line_endings && root_type != TOKEN_ROOT) {
					post_trail_len= h_trail_len;
					h_trail_len= 0;
				}
			}

			Token *t= token_new(TOKEN_HEADING, "heading");
			if(t) {
				t->data.heading.level= level;
				/* Build heading-title child token (TOKEN_PLAIN "heading-title") */
				Token *title_tok= token_new(TOKEN_PLAIN, "heading-title");
				if(title_tok) {
					if(h_inner_len) {
						const char *title_view = wiki_thread_buf_append_to_tokens(h_inner, h_inner_len);
						if(title_view) token_append_text_n(title_tok, title_view, h_inner_len);
					}
					token_append_child(t, title_tok);
				}
				/* Build heading-trail child token (TOKEN_SYNTAX "heading-trail") */
				Token *trail_tok= token_new(TOKEN_SYNTAX, "heading-trail");
				if(trail_tok) {
					/* Always append a text child (even if empty), mirroring JS which
					 * always creates an AstText("") inside heading-trail. */
					if(h_trail_len > 0) {
						const char *trail_view = wiki_thread_buf_append_to_tokens(h_trail, h_trail_len);
						if(trail_view) token_append_text_n(trail_tok, trail_view, h_trail_len);
					} else {
						token_append_text_n(trail_tok, NULL, 0);
					}
					token_append_child(t, trail_tok);
				}
				accum_push(accum, t);
				size_t tok_idx= accum->count ? accum->count - 1 : 0;
				char sent[64];
				size_t slen;
				work_str_sentinel(tok_idx, 'h', sent, &slen);
				if(out2_len + slen > out_cap2) {
					out_cap2= out2_len + slen + 16;
					out2= realloc(out2, out_cap2);
					assert(out2);
				}
				memcpy(out2 + out2_len, sent, slen);
				out2_len+= slen;
				if(post_trail_len > 0) {
					if(out2_len + post_trail_len + 1 > out_cap2) {
						out_cap2= out2_len + post_trail_len + 16;
						out2= realloc(out2, out_cap2);
						assert(out2);
					}
					memcpy(out2 + out2_len, h_trail, post_trail_len);
					out2_len+= post_trail_len;
				}
			} else {
				/* fallback: copy original match */
				if(out2_len + (me - ms) > out_cap2) {
					out_cap2= out2_len + (me - ms) + 16;
					out2= realloc(out2, out_cap2);
					assert(out2);
				}
				memcpy(out2 + out2_len, tb->buf + ms, me - ms);
				out2_len+= me - ms;
			}

			search2= me;
			if(me == ms) search2++;
		}

		out2[out2_len]= '\0';
		wiki_thread_buf_set(tb, out2, out2_len);
		free(out2);

		pcre2_match_data_free(hmd);
		/* Compiled pattern owned by pcre_cache; do NOT free `hre` */
	}

	if(prefixed && tb->len > 0) {
		size_t unpref_len= tb->len - 1;
		char *tmp= malloc(unpref_len + 1);
		assert(tmp);
		if(unpref_len > 0) {
			memcpy(tmp, tb->buf + 1, unpref_len);
		}
		tmp[unpref_len]= '\0';
		wiki_thread_buf_set(tb, tmp, unpref_len);
		free(tmp);
	}
}
