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

/*
 * Stage 4: horizontal rules (lines with 4+ dashes) and double-underscore
 * magic words like __TOC__, __NOTOC__, etc.  This implementation uses
 * callback scanners (parser_scan) with ParserRules for HR and double-underscore
 * detection, then validates double-underscore keys against the ParserConfig
 * lists before creating tokens.
 *
 * Heading finalization uses a line-at-a-time forward scan
 * (heading_line_parse_full) instead of PCRE.
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

/* Returns bytes consumed if *p starts a \x00\d+[cn]\x7F sentinel, else 0. */
static size_t skip_cn_sentinel(const char *p, size_t remaining) {
    if(!p || remaining < 4 || (unsigned char)p[0] != 0) return 0;
    size_t j = 1;
    if(j >= remaining || p[j] < '0' || p[j] > '9') return 0;
    while(j < remaining && p[j] >= '0' && p[j] <= '9') j++;
    if(j >= remaining) return 0;
    char t = p[j];
    if(t != 'c' && t != 'n') return 0;
    if(j + 1 >= remaining || (unsigned char)p[j + 1] != 0x7F) return 0;
    return j + 2;
}

typedef struct {
    const char *lead;    size_t lead_len;    /* group 1: \x00\d+[cn]\x7F prefix */
    const char *eq;      size_t eq_count;    /* group 2: opening = chars (1–6)  */
    const char *content; size_t content_len; /* group 3: inner heading text     */
    const char *trail;   size_t trail_len;   /* group 4: trailing ws/sentinels  */
} HdLineResult;

/*
 * heading_line_parse_full — forward-scan replacement for:
 *   /^((?:\x00\d+[cn]\x7F)*)(={1,6})(.+)\2((?:\s|\x00\d+[cn]\x7F)*)$/
 *
 * Applied to a single line (no embedded \n).
 * Returns true and populates *out on match; false otherwise.
 */
static bool heading_line_parse_full(const char *s, size_t len, HdLineResult *out) {
    if(!s || len == 0 || !out) return false;
    const char *p = s, *end = s + len;

    const char *lead = p;
    while(p < end) {
        size_t sc = skip_cn_sentinel(p, (size_t)(end - p));
        if(sc == 0) break;
        p += sc;
    }
    size_t lead_len = (size_t)(p - lead);

	const char *eq_start = p;
	size_t open_run = 0;
	while(p < end && *p == '=' && open_run < 6) { p++; open_run++; }
	if(open_run == 0) return false;

    const char *trail_end = end, *trail_start = end;
    bool changed = true;
	while(changed && trail_start > eq_start + 1) {
        changed = false;
        if(isspace((unsigned char)*(trail_start - 1))) { trail_start--; changed = true; continue; }
		if((unsigned char)*(trail_start - 1) == 0x7F && trail_start - 2 >= eq_start) {
            const char *type_p = trail_start - 2;
            if(*type_p == 'c' || *type_p == 'n') {
                const char *q = type_p - 1;
                size_t digit_count = 0;
				while(q > eq_start && *q >= '0' && *q <= '9') { q--; digit_count++; }
				if(digit_count >= 1 && (unsigned char)*q == 0 && q >= eq_start) {
                    trail_start = q; changed = true; continue;
                }
            }
        }
    }

	/* Regex parity: backtrack (={1,6}) from max to 1 until closing run matches. */
	for(size_t eq_count = open_run; eq_count > 0; eq_count--) {
		const char *content_start = eq_start + eq_count;
		if(content_start >= trail_start) continue;
		if((size_t)(trail_start - content_start) < eq_count + 1) continue;

		bool close_ok = true;
		for(size_t i = 0; i < eq_count; i++) {
			if(*(trail_start - 1 - i) != '=') {
				close_ok = false;
				break;
			}
		}
		if(!close_ok) continue;

		const char *content_end = trail_start - eq_count;
		if(content_end <= content_start) continue;

		out->lead = lead;             out->lead_len    = lead_len;
		out->eq   = eq_start;         out->eq_count    = eq_count;
		out->content = content_start; out->content_len = (size_t)(content_end - content_start);
		out->trail   = trail_start;   out->trail_len   = (size_t)(trail_end - trail_start);
		return true;
	}

	return false;
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
		sz_ptr_t it;
		sz_size_t it_len;
		sz_string_range(&sl->items[i], &it, &it_len);
		if(!it) continue;
		if(it_len == len && memcmp(it, s, len) == 0) return 1;
	}
	return 0;
}

static int strlist_has_lower(const StrList *sl, const char *s, size_t len) {
	if(!sl) return 0;
	for(size_t i= 0; i < sl->count; i++) {
		sz_ptr_t it;
		sz_size_t it_len;
		sz_string_range(&sl->items[i], &it, &it_len);
		if(!it) continue;
		if(it_len != len) continue;
		int ok= 1;
		for(size_t k= 0; k < len; k++) {
			if(tolower((unsigned char)((const char *)it)[k]) != tolower((unsigned char)s[k])) {
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
		sz_ptr_t key_start;
		sz_size_t key_len;
		sz_string_range(&m->keys[i], &key_start, &key_len);
		if(key_start && key_len == strlen(key) && memcmp(key_start, key, key_len) == 0) {
			sz_ptr_t val_start;
			sz_size_t val_len;
			sz_string_range(&m->values[i], &val_start, &val_len);
			return (const char *)val_start;
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

	bool skip_heading_for_param_ctx= root_type == TOKEN_PLAIN
		&& root_name
		&& (strcmp(root_name, "parameter-value") == 0
			|| strcmp(root_name, "parameter-key") == 0
			|| strcmp(root_name, "attr-value") == 0);

	/* Heading finalization: line-at-a-time forward scan */
	if(!config_excluded(cfg, "heading") && !skip_heading_for_param_ctx) {
		size_t out_cap2 = tb->len * 2 + 64;
		char *out2 = malloc(out_cap2);
		if(!out2) { log_fatal("OOM in heading finalization"); abort(); }
		size_t out2_len = 0;
#define GROW_OUT2(need) do { \
		while(out2_len + (need) >= out_cap2) { \
			if(out_cap2 > SIZE_MAX / 2) { log_fatal("buffer size overflow in heading finalization"); abort(); } \
			out_cap2 *= 2; \
			char *_grow_tmp = realloc(out2, out_cap2); \
			if(!_grow_tmp) { free(out2); log_fatal("OOM in realloc"); abort(); } \
			out2 = _grow_tmp; \
		} \
	} while(0)
		const char *buf2 = tb->buf;
		size_t buf2_len  = tb->len;
		size_t cursor = 0;

		while(cursor < buf2_len) {
			size_t line_start = cursor;
			size_t line_end = line_start;
			while(line_end < buf2_len && buf2[line_end] != '\n') line_end++;
			const char *line = buf2 + line_start;
			size_t line_len  = line_end - line_start;

			HdLineResult hr;
			if(heading_line_parse_full(line, line_len, &hr)) {
				if(getenv("WTC_DEBUG_STAGE_4")) {
					size_t preview_n = line_len < 96 ? line_len : 96;
					char preview[256];
					size_t pp = 0;
					for(size_t k = 0; k < preview_n && pp + 1 < sizeof(preview); k++) {
						unsigned char c = (unsigned char)line[k];
						if(c >= 0x20 && c < 0x7F) {
							preview[pp++] = (char)c;
						} else if(c == 0x00 && pp + 2 < sizeof(preview)) {
							preview[pp++] = '\\';
							preview[pp++] = '0';
						} else {
							preview[pp++] = '.';
						}
					}
					preview[pp] = '\0';
					log_debug_env_token(
						"WTC_DEBUG_STAGE_4", NULL,
						"heading-match root_type=%d root_name=%s line_len=%zu level=%zu preview=%s",
						(int)root_type,
						root_name ? root_name : "(null)",
						line_len,
						hr.eq_count,
						preview
					);
				}
				/* JS parity for /...((?:\s|\0\d+[cn]\x7F)*)$/gmu:
				 * consume maximal whitespace/cn-sentinel run after the heading,
				 * but end match at a line boundary (before '\n' or EOS). */
				size_t match_end = line_end;
				size_t scan = line_end;
				size_t last_boundary = line_end;
				while(scan < buf2_len) {
					size_t sc = skip_cn_sentinel(buf2 + scan, buf2_len - scan);
					if(sc > 0) {
						scan += sc;
						if(scan == buf2_len || buf2[scan] == '\n') last_boundary = scan;
						continue;
					}
					if(isspace((unsigned char)buf2[scan])) {
						scan++;
						if(scan == buf2_len || buf2[scan] == '\n') last_boundary = scan;
						continue;
					}
					break;
				}
				match_end = last_boundary;

				/* 1. Emit lead sentinels verbatim */
				if(hr.lead_len > 0) {
					GROW_OUT2(hr.lead_len);
					memcpy(out2 + out2_len, hr.lead, hr.lead_len);
					out2_len += hr.lead_len;
				}

				/* 2. Map hr.* to buf2-relative offsets (same variable names as old ov[]) */
				size_t eq_s    = (size_t)(hr.eq      - buf2), eq_e    = eq_s + hr.eq_count;
				size_t text_s  = (size_t)(hr.content - buf2), text_e  = text_s + hr.content_len;
				size_t trail_s = (size_t)(hr.trail   - buf2), trail_e = trail_s + hr.trail_len;
				size_t extra_trail_len = match_end > line_end ? (match_end - line_end) : 0;

				/* 3. Build heading token */
				int level = (int)(eq_e > eq_s ? eq_e - eq_s : 0);
				const char *h_inner = (text_e > text_s) ? (buf2 + text_s) : "";
				size_t h_inner_len = (text_e > text_s) ? (text_e - text_s) : 0;
				const char *h_trail = (trail_e > trail_s) ? (buf2 + trail_s) : "";
				size_t h_trail_len = (trail_e > trail_s) ? (trail_e - trail_s) : 0;

				Token *t = token_new(TOKEN_HEADING, "heading");
				if(t) {
					t->data.heading.level = level;

					Token *title_tok = token_new(TOKEN_PLAIN, "heading-title");
					if(title_tok) {
						if(h_inner_len > 0) {
							const char *title_view = wiki_thread_buf_append_to_tokens(h_inner, h_inner_len);
							if(title_view) token_append_text_n(title_tok, title_view, h_inner_len);
						} else {
							token_append_text_n(title_tok, "", 0);
						}
						token_append_child(t, title_tok);
					}

					Token *trail_tok = token_new(TOKEN_SYNTAX, "heading-trail");
					if(trail_tok) {
						size_t trail_total_len = h_trail_len + extra_trail_len;
						if(trail_total_len > 0) {
							if(h_trail_len > 0 && extra_trail_len > 0) {
								ThreadBuf *tmp_trail = wiki_thread_buf_acquire_scratch();
								if(!tmp_trail) { log_fatal("thread_buffer: failed to acquire scratch in heading trail build"); abort(); }
								tmp_trail->len = 0;
								wiki_thread_buf_append(tmp_trail, (sz_string_view_t){ .start = h_trail, .length = h_trail_len });
								wiki_thread_buf_append(tmp_trail, (sz_string_view_t){ .start = buf2 + line_end, .length = extra_trail_len });
								const char *trail_view = wiki_thread_buf_append_to_tokens(tmp_trail->buf, tmp_trail->len);
								if(trail_view) token_append_text_n(trail_tok, trail_view, tmp_trail->len);
								else token_append_text_n(trail_tok, "", 0);
								wiki_thread_buf_release_scratch(tmp_trail);
							} else if(h_trail_len > 0) {
								const char *trail_view = wiki_thread_buf_append_to_tokens(h_trail, h_trail_len);
								if(trail_view) token_append_text_n(trail_tok, trail_view, h_trail_len);
								else token_append_text_n(trail_tok, "", 0);
							} else {
								const char *trail_view = wiki_thread_buf_append_to_tokens(buf2 + line_end, extra_trail_len);
								if(trail_view) token_append_text_n(trail_tok, trail_view, extra_trail_len);
								else token_append_text_n(trail_tok, "", 0);
							}
						} else {
							token_append_text_n(trail_tok, "", 0);
						}
						token_append_child(t, trail_tok);
					}

					accum_push(accum, t);
					char sent[64];
					size_t slen = 0;
					work_str_sentinel(accum->count - 1, 'h', sent, &slen);
					GROW_OUT2(slen);
					memcpy(out2 + out2_len, sent, slen);
					out2_len += slen;

					/* Continue from match end (newline boundary char is not consumed). */
					cursor = match_end;
					continue;
				}

				/* OOM fallback: leave current line unchanged */
				size_t copy_len = (line_end < buf2_len) ? line_len + 1 : line_len;
				GROW_OUT2(copy_len);
				memcpy(out2 + out2_len, buf2 + line_start, copy_len);
				out2_len += copy_len;
				cursor = (line_end < buf2_len) ? (line_end + 1) : line_end;
				continue;
			}

			/* Not a heading: copy line + optional '\n' verbatim */
			size_t copy_len = (line_end < buf2_len) ? line_len + 1 : line_len;
			GROW_OUT2(copy_len);
			memcpy(out2 + out2_len, buf2 + line_start, copy_len);
			out2_len += copy_len;
			cursor = (line_end < buf2_len) ? (line_end + 1) : line_end;
		}
		out2[out2_len] = '\0';
		wiki_thread_buf_set(tb, out2, out2_len);
		free(out2);
#undef GROW_OUT2
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
