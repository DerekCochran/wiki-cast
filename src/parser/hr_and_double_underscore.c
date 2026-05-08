#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

#include "util/log.h"
#include "parser/hr_and_double_underscore.h"
#include "util/string_util.h"
#include "util/thread_buffer.h"
#include "token.h"
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "util/pcre_cache.h"

/*
 * Stage 4: horizontal rules (lines with 4+ dashes) and double-underscore
 * magic words like __TOC__, __NOTOC__, etc.  This implementation uses a
 * PCRE2 regex to find candidate substrings, then validates double-underscore
 * keys against the ParserConfig lists before creating tokens.
 */

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

/* Lowercase ASCII-only copy */
static char *lower_copy(const char *s, size_t len) {
	char *out= malloc(len + 1);
	assert(out);
	for(size_t i= 0; i < len; i++) out[i]= (char)tolower((unsigned char)s[i]);
	out[len]= '\0';
	return out;
}

static int is_fullwidth_wrapped_dunder(const char *s) {
	static const char fw[]= "\xEF\xBC\xBF"; /* U+FF3F FULLWIDTH LOW LINE */
	size_t fwl= sizeof(fw) - 1U;
	size_t len= s ? strlen(s) : 0;
	if(len < 4U * fwl + 1U) return 0;
	return memcmp(s, fw, fwl) == 0 &&
			 memcmp(s + fwl, fw, fwl) == 0 &&
			 memcmp(s + len - fwl, fw, fwl) == 0 &&
			 memcmp(s + len - 2U * fwl, fw, fwl) == 0;
}

static void pattern_append(char **buf, size_t *cap, size_t *len, const char *s) {
	size_t add= strlen(s);
	if(*len + add + 1 > *cap) {
		while(*len + add + 1 > *cap) *cap*= 2;
		*buf= realloc(*buf, *cap);
		assert(*buf);
	}
	memcpy(*buf + *len, s, add);
	*len+= add;
	(*buf)[*len]= '\0';
}

static void pattern_append_n(char **buf, size_t *cap, size_t *len, const char *s, size_t n) {
	if(*len + n + 1 > *cap) {
		while(*len + n + 1 > *cap) *cap*= 2;
		*buf= realloc(*buf, *cap);
		assert(*buf);
	}
	memcpy(*buf + *len, s, n);
	*len+= n;
	(*buf)[*len]= '\0';
}




static char *build_hr_and_dunder_pattern(const ParserConfig *cfg) {
	static const char fw[]= "\xEF\xBC\xBF"; /* U+FF3F FULLWIDTH LOW LINE */
	size_t cap= 256;
	size_t len= 0;
	char *pattern= malloc(cap);
	assert(pattern);
	pattern[0]= '\0';

	/* Mirrors JS: ^((?:\0\d+[cno]\x7F)*)(-{4,})|__(${underscore})__|＿{2}(${fullwidth.slice(2,-2)})＿{2} */
	pattern_append(&pattern, &cap, &len, "^((?:\\x00\\d+[cno]\\x7F)*)(-{4,})|__(");

	int first= 1;
	for(int list= 0; list < 2; list++) {
		const StrList *sl= &cfg->double_underscore[list];
		for(size_t i= 0; i < sl->count; i++) {
			const char *it= sl->items[i];
			if(!it || is_fullwidth_wrapped_dunder(it)) continue;
			if(!first) pattern_append(&pattern, &cap, &len, "|");
			pattern_append(&pattern, &cap, &len, it);
			first= 0;
		}
	}

	pattern_append(&pattern, &cap, &len, ")__|");
	pattern_append(&pattern, &cap, &len, fw);
	pattern_append(&pattern, &cap, &len, "{2}(");

	first= 1;
	for(int list= 0; list < 2; list++) {
		const StrList *sl= &cfg->double_underscore[list];
		for(size_t i= 0; i < sl->count; i++) {
			const char *it= sl->items[i];
			size_t it_len;
			if(!it || !is_fullwidth_wrapped_dunder(it)) continue;
			it_len= strlen(it);
			if(!first) pattern_append(&pattern, &cap, &len, "|");
			pattern_append_n(&pattern, &cap, &len,
								 it + 2U * (sizeof(fw) - 1U),
								 it_len - 4U * (sizeof(fw) - 1U));
			first= 0;
		}
	}

	pattern_append(&pattern, &cap, &len, ")");
	pattern_append(&pattern, &cap, &len, fw);
	pattern_append(&pattern, &cap, &len, "{2}");

	return pattern;
}

static pcre2_code *compile_regex(const ParserConfig *cfg) {
	/* Lazily build and cache the HR / double-underscore pattern string
	 * in the ParserConfig to avoid per-parse realloc/snprintf work. */
	const char *pattern = cfg->pattern_hr_and_dunder;
	if(!pattern || !pattern[0]) {
		char *pat = build_hr_and_dunder_pattern(cfg);
		((ParserConfig *)cfg)->pattern_hr_and_dunder = pat;
		pattern = pat;
	}
	return pcre_cache_get(pattern, PCRE2_UTF | PCRE2_MULTILINE | PCRE2_CASELESS);
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

	/* Use process-wide cached regex for this call */
	pcre2_code *re = compile_regex(cfg);
	if(!re) return;
	pcre2_match_data *md = pcre2_match_data_create_from_pattern(re, NULL);
	if(!md) return;

	size_t out_cap= tb->len * 2 + 64;
	char *out_buf= malloc(out_cap);
	assert(out_buf);
	size_t out_len= 0;
	size_t search_at= 0;

#define ENSURE_CAP(need)                  \
	do {                                    \
		while(out_len + (need) >= out_cap) {  \
			out_cap*= 2;                        \
			out_buf= realloc(out_buf, out_cap); \
			assert(out_buf);                    \
		}                                     \
	} while(0)

	while(search_at <= tb->len) {
		int rc= pcre2_match(re, (PCRE2_SPTR)tb->buf, tb->len, search_at, 0, md, NULL);
		if(rc <= 0) {
			size_t rest= tb->len - search_at;
			ENSURE_CAP(rest + 1);
			memcpy(out_buf + out_len, tb->buf + search_at, rest);
			out_len+= rest;
			break;
		}

		PCRE2_SIZE *ov= pcre2_get_ovector_pointer(md);
		size_t mstart= ov[0], mend= ov[1];

		/* copy before match */
		size_t before= mstart - search_at;
		ENSURE_CAP(before + 32);
		memcpy(out_buf + out_len, tb->buf + search_at, before);
		out_len+= before;

		/* Groups: 1 = lead sentinels, 2 = hr dashes, 3 = __...__, 4 = ＿＿...＿＿ */
		size_t g1s= (rc > 1 && ov[2] != PCRE2_UNSET) ? ov[2] : 0;
		size_t g1e= (rc > 1 && ov[3] != PCRE2_UNSET) ? ov[3] : 0;
		size_t g2s= (rc > 2 && ov[4] != PCRE2_UNSET) ? ov[4] : 0;
		size_t g2e= (rc > 2 && ov[5] != PCRE2_UNSET) ? ov[5] : 0;
		size_t g3s= (rc > 3 && ov[6] != PCRE2_UNSET) ? ov[6] : 0;
		size_t g3e= (rc > 3 && ov[7] != PCRE2_UNSET) ? ov[7] : 0;
		size_t g4s= (rc > 4 && ov[8] != PCRE2_UNSET) ? ov[8] : 0;
		size_t g4e= (rc > 4 && ov[9] != PCRE2_UNSET) ? ov[9] : 0;

		if(g2e > g2s) {
			/* HR matched (group 2) */
			/* copy leading sentinel markers group1 (if any) */
			if(g1e > g1s) {
				size_t leadlen= g1e - g1s;
				ENSURE_CAP(leadlen);
				memcpy(out_buf + out_len, tb->buf + g1s, leadlen);
				out_len+= leadlen;
			}

			Token *t= token_new(TOKEN_HR, "hr");
			if(t) {
				/* Store the dash sequence for round-trip toString */
				size_t dashlen= g2e - g2s;
				if(dashlen > 0) {
					const char *dash_view = wiki_thread_buf_append_to_tokens(tb->buf + g2s, dashlen);
					if(dash_view) token_append_text_n(t, dash_view, dashlen);
				}
				accum_push(accum, t);
			}
			size_t tok_idx= accum->count ? accum->count - 1 : 0;
			char sent[64];
			size_t slen;
			work_str_sentinel(tok_idx, 'r', sent, &slen);
			ENSURE_CAP(slen);
			memcpy(out_buf + out_len, sent, slen);
			out_len+= slen;

		} else if(g3e > g3s || g4e > g4s) {
			/* Double-underscore candidate: validate against config lists */
			size_t ks= (g3e > g3s) ? g3s : g4s;
			size_t ke= (g3e > g3s) ? g3e : g4e;
			const char *key_ptr= tb->buf + ks;
			size_t key_len= ke - ks;

			/* Check case-sensitive list (index 1) for exact match */
			int case_sensitive= strlist_has_exact(&cfg->double_underscore[1], key_ptr, key_len);
			/* Check case-insensitive list (index 0) by lowercasing */
			int case_insensitive= strlist_has_lower(&cfg->double_underscore[0], key_ptr, key_len);

			if(case_sensitive || case_insensitive) {
				/* Build DoubleUnderscore token */
				Token *t= token_new(TOKEN_DOUBLE_UNDERSCORE, "double-underscore");
				if(t) {
					t->data.dunder.case_sensitive= case_sensitive != 0;
					t->data.dunder.fullwidth= (g4e > g4s);
					char *lc= lower_copy(key_ptr, key_len);
					const char *alias= NULL;
					if(case_sensitive) {
						char *raw= malloc(key_len + 1);
						assert(raw);
						memcpy(raw, key_ptr, key_len);
						raw[key_len]= '\0';
						alias= strmap_get_exact(&cfg->double_underscore_alias[1], raw);
						free(raw);
					} else if(lc) {
						alias= strmap_get_exact(&cfg->double_underscore_alias[0], lc);
					}
					if(alias && alias[0]) {
						size_t alen= strlen(alias);
						t->name= lower_copy(alias, alen);
						free(lc);
					} else {
						t->name= lc;
					}
					/* inner text: original matched word */
					if(key_len > 0) {
						const char *key_view = wiki_thread_buf_append_to_tokens(key_ptr, key_len);
						if(key_view) token_append_text_n(t, key_view, key_len);
					}
					accum_push(accum, t);
					size_t tok_idx= accum->count ? accum->count - 1 : 0;
					char sent[64];
					size_t slen;
					/* Special-case: __TOC__ → sentinel 'u' when insensitive and canonical == "toc" */
					char ch= 'n';
					if(case_insensitive) {
						char *lcname= t->name ? t->name : NULL;
						if(lcname && strcmp(lcname, "toc") == 0) ch= 'u';
					}
					work_str_sentinel(tok_idx, ch, sent, &slen);
					ENSURE_CAP(slen);
					memcpy(out_buf + out_len, sent, slen);
					out_len+= slen;
				} else {
					/* fallback: copy original match */
					ENSURE_CAP(mend - mstart);
					memcpy(out_buf + out_len, tb->buf + mstart, mend - mstart);
					out_len+= mend - mstart;
				}
			} else {
				/* Not a registered magic word: copy original match */
				ENSURE_CAP(mend - mstart);
				memcpy(out_buf + out_len, tb->buf + mstart, mend - mstart);
				out_len+= mend - mstart;
			}

		} else {
			/* No recognized capture — copy raw substring */
			ENSURE_CAP(mend - mstart);
			memcpy(out_buf + out_len, tb->buf + mstart, mend - mstart);
			out_len+= mend - mstart;
		}

		search_at= mend;
		if(mend == mstart) search_at++;
	}

	out_buf[out_len]= '\0';
	wiki_thread_buf_set(tb, out_buf, out_len);
	free(out_buf);

	pcre2_match_data_free(md);

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
