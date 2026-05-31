#include "util/log.h"
#include "util/env_cache.h"
#include "parser/hr_and_double_underscore.h"
#include "util/string_util.h"
#include "util/thread_buffer.h"
#include "util/wiki_parser_rules.h"
#include "token.h"
#include "stringzilla/stringzilla.h"
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

/* Lowercase ASCII-only copy using the LUT-based path */
static char *lower_copy(const char *s, size_t len) {
	char *out= malloc(len + 1);
	assert(out);
	if(len > 0) sz_lookup(out, len, s, (const char *)fast_tolower_table());
	out[len]= '\0';
	return out;
}

/* Forward declarations for helper functions used in dunder_cb */
static int strlist_has_exact(const StrList *sl, const char *s, size_t len);
static int strlist_has_lower(const StrList *sl, const char *s, size_t len);
static const char *strmap_get_exact(const StrMap *m, const char *key, size_t key_len);

/* Skip a CNO sentinel: \0\d+[cno]\x7F
 * Uses sz_find_byte to jump to the \x7F terminal, then validates backward. */
static size_t skip_cno_sentinel(const char *p, size_t rem) {
	if(!p || rem < 4 || (unsigned char)p[0] != 0) return 0;
	if(p[1] < '0' || p[1] > '9') return 0;
	static const char del_ch = '\x7F';
	const char *del = sz_find_byte(p + 1, rem - 1, &del_ch);
	if(!del) return 0;
	size_t end_pos = (size_t)(del - p); /* position of \x7F */
	if(end_pos < 3) return 0;           /* need: NUL digit+ type DEL */
	char type = p[end_pos - 1];
	if(type != 'c' && type != 'n' && type != 'o') return 0;
	for(size_t k = 1; k < end_pos - 1; k++) {
		if(p[k] < '0' || p[k] > '9') return 0;
	}
	return end_pos + 1;
}

/* Returns bytes consumed if *p starts a \x00\d+[cn]\x7F sentinel, else 0.
 * Uses sz_find_byte to jump to the \x7F terminal, then validates backward. */
static size_t skip_cn_sentinel(const char *p, size_t remaining) {
	if(!p || remaining < 4 || (unsigned char)p[0] != 0) return 0;
	if(p[1] < '0' || p[1] > '9') return 0;
	static const char del_ch = '\x7F';
	const char *del = sz_find_byte(p + 1, remaining - 1, &del_ch);
	if(!del) return 0;
	size_t end_pos = (size_t)(del - p); /* position of \x7F */
	if(end_pos < 3) return 0;
	char type = p[end_pos - 1];
	if(type != 'c' && type != 'n') return 0;
	for(size_t k = 1; k < end_pos - 1; k++) {
		if(p[k] < '0' || p[k] > '9') return 0;
	}
	return end_pos + 1;
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
	const char *first_non_eq = sz_find_byte_not_from(p, (size_t)(end - p), "=", 1);
	size_t open_run = first_non_eq ? (size_t)(first_non_eq - p) : (size_t)(end - p);
	if(open_run > 6) open_run = 6;
	p += open_run;
	if(open_run == 0) return false;

    const char *trail_end = end, *trail_start = end;
    bool changed = true;
	while(changed && trail_start > eq_start + 1) {
        changed = false;
		size_t ws_suffix = str_js_trim_ws_suffix_len(eq_start, (size_t)(trail_start - eq_start));
		if(ws_suffix > 0) { trail_start -= ws_suffix; changed = true; continue; }
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
	ThreadBuf *out = wiki_thread_buf_acquire_scratch();
	if(!out) { log_fatal("OOM in parse_hr_pass"); abort(); }
	out->len = 0;
	wiki_thread_buf_reserve(out, tb->len * 2 + 64);

	size_t i = 0;
	while(i < tb->len) {
		size_t line_start = i;
		size_t line_end = i;
		while(line_end < tb->len && tb->buf[line_end] != '\n') line_end++;

		size_t p = line_start;
		while(p < line_end) {
			size_t sc = skip_cno_sentinel(tb->buf + p, line_end - p);
			if(sc == 0) break;
			wiki_thread_buf_append(out, (sz_string_view_t){ tb->buf + p, sc });
			p += sc;
		}

		size_t dash = p;
		while(dash < line_end && tb->buf[dash] == '-') dash++;
		if(dash - p >= 4) {
			Token *t = token_new(TOKEN_HR, "hr");
			if(t) {
				token_append_text_n(t, tb->buf + p, dash - p);
				accum_push(accum, t);
				char sent[64]; size_t slen = 0;
				work_str_sentinel(accum->count - 1, 'r', sent, &slen);
				wiki_thread_buf_append(out, (sz_string_view_t){ sent, slen });
				if(line_end > dash) {
					wiki_thread_buf_append(out, (sz_string_view_t){ tb->buf + dash, line_end - dash });
				}
			}
		} else {
			wiki_thread_buf_append(out, (sz_string_view_t){ tb->buf + p, line_end - p });
		}

		if(line_end < tb->len) wiki_thread_buf_putc(out, '\n');
		i = (line_end < tb->len) ? (line_end + 1) : line_end;
	}

	out->buf[out->len] = '\0';
	wiki_thread_buf_set(tb, out->buf, out->len);
	wiki_thread_buf_release_scratch(out);
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
		/* Pass seg and len directly — avoids malloc+copy just for the key lookup */
		alias = strmap_get_exact(&ctx->cfg->double_underscore_alias[1], seg, len);
	} else if(lc) {
		alias = strmap_get_exact(&ctx->cfg->double_underscore_alias[0], lc, len);
	}

	if(alias && alias[0]) {
		size_t alen = strlen(alias);
			t->name = lower_copy(alias, alen);
		free(lc);
		lc = NULL;
	} else {
			t->name = lc;
		lc = NULL;
	}

	if(len > 0) {
		token_append_text_n(t, seg, len);
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
		if(it_len == len && sz_equal((const char *)it, s, len) == sz_true_k) return 1;
	}
	return 0;
}

static int strlist_has_lower(const StrList *sl, const char *s, size_t len) {
	if(!sl) return 0;
	for(size_t i= 0; i < sl->count; i++) {
		sz_ptr_t it;
		sz_size_t it_len;
		sz_string_range(&sl->items[i], &it, &it_len);
		if(!it || it_len != len) continue;
		if(str_ci_eq_n((const char *)it, s, len)) return 1;
	}
	return 0;
}

static const char *strmap_get_exact(const StrMap *m, const char *key, size_t key_cmp_len) {
	if(!m || !key) return NULL;
	for(size_t i= 0; i < m->count; i++) {
		sz_ptr_t key_start;
		sz_size_t key_len;
		sz_string_range(&m->keys[i], &key_start, &key_len);
		if(key_start && key_len == key_cmp_len && sz_equal((const char *)key_start, key, key_cmp_len) == sz_true_k) {
			sz_ptr_t val_start;
			sz_size_t val_len;
			sz_string_range(&m->values[i], &val_start, &val_len);
			return (const char *)val_start;
		}
	}
	return NULL;
}

void parse_hr_and_double_underscore(ThreadBuf *tb, const ParserConfig *cfg, Accum *accum,
															TokenType root_type, const char *root_name,
															bool allow_heading) {
	if(!tb || !tb->buf) return;

	bool poem_ctx= root_name && strcmp(root_name, "poem") == 0;
	bool prefixed= root_type != TOKEN_ROOT && !(root_type == TOKEN_EXT_INNER && poem_ctx);
	if(prefixed) {
		/* Prepend a NUL byte in-place with memmove — avoids malloc+copy+free */
		wiki_thread_buf_reserve(tb, tb->len + 1);
		if(tb->len > 0) memmove(tb->buf + 1, tb->buf, tb->len);
		tb->buf[0]= '\0';
		tb->len += 1;
		tb->buf[tb->len]= '\0';
	}

	/* New callback-based passes for HR and double-underscore */
	parse_hr_pass(tb, accum);
	parse_dunder_pass(tb, &wiki_rule_dunder_ascii, false, cfg, accum);
	parse_dunder_pass(tb, &wiki_rule_dunder_fullwidth, true, cfg, accum);

	bool skip_heading_for_param_ctx= root_type == TOKEN_PLAIN
		&& root_name
		&& (strcmp(root_name, "parameter-key") == 0
			|| strcmp(root_name, "attr-value") == 0);
	bool skip_heading_for_references_ctx= root_name
		&& strcmp(root_name, "references") == 0;

	/* Heading finalization: line-at-a-time forward scan */
	if(allow_heading && !config_excluded(cfg, "heading") && !skip_heading_for_param_ctx && !skip_heading_for_references_ctx && !poem_ctx) {
		bool allow_crossline_heading_trail = !(root_name && strcmp(root_name, "parameter-value") == 0);
		ThreadBuf *out2_tb = wiki_thread_buf_acquire_scratch();
		if(!out2_tb) { log_fatal("OOM in heading finalization"); abort(); }
		out2_tb->len = 0;
		wiki_thread_buf_reserve(out2_tb, tb->len * 2 + 64);
#define GROW_OUT2_APPEND(ptr, n) wiki_thread_buf_append(out2_tb, (sz_string_view_t){ (ptr), (n) })
		const char *buf2 = tb->buf;
		size_t buf2_len  = tb->len;
		size_t cursor = 0;

		while(cursor < buf2_len) {
			size_t line_start = cursor;
			size_t line_end = line_start;
			while(line_end < buf2_len && buf2[line_end] != '\n') line_end++;
			const char *line = buf2 + line_start;
			size_t line_len  = line_end - line_start;
			const char eq_ch = '=';
			if(line_len == 0 || sz_find_byte(line, line_len, &eq_ch) == NULL) {
				if(line_len > 0) {
					GROW_OUT2_APPEND(line, line_len);
				}
				if(line_end < buf2_len) {
					GROW_OUT2_APPEND("\n", 1);
					cursor = line_end + 1;
				} else {
					cursor = line_end;
				}
				continue;
			}

			HdLineResult hr;
			if(heading_line_parse_full(line, line_len, &hr)) {
				if(env_set("WTC_DEBUG_STAGE_4")) {
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
				if(allow_crossline_heading_trail) {
					bool consumed_any = false;
					bool seen_non_newline = false;
					while(scan < buf2_len) {
						size_t sc = skip_cn_sentinel(buf2 + scan, buf2_len - scan);
						if(sc > 0) {
							scan += sc;
							if(scan == buf2_len || buf2[scan] == '\n') last_boundary = scan;
							consumed_any = true;
							seen_non_newline = true;
							continue;
						}
						if(buf2[scan] == '\n' || buf2[scan] == '\r') {
							if(seen_non_newline) {
								break;
							}
							if(consumed_any) {
								size_t look= scan;
								while(look < buf2_len && (buf2[look] == '\n' || buf2[look] == '\r')) {
									look++;
								}
								size_t look_sc= (look < buf2_len)
									? skip_cn_sentinel(buf2 + look, buf2_len - look)
									: 0;
								if(look_sc == 0) {
									break;
								}
							}
							scan++;
							if(scan == buf2_len || buf2[scan] == '\n') last_boundary = scan;
							consumed_any = true;
							continue;
						}
						size_t ws = str_js_trim_ws_len_at(buf2, buf2_len, scan);
						if(ws > 0) {
							consumed_any = true;
							seen_non_newline = true;
							scan += ws;
							if(scan == buf2_len || buf2[scan] == '\n') last_boundary = scan;
							continue;
						}
						break;
					}
					match_end = consumed_any ? last_boundary : line_end;
				}

				/* 1. Emit lead sentinels verbatim */
				if(hr.lead_len > 0) {
					GROW_OUT2_APPEND(hr.lead, hr.lead_len);
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
							token_append_text_n(title_tok, h_inner, h_inner_len);
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
								token_append_text_n(trail_tok, tmp_trail->buf, tmp_trail->len);
								wiki_thread_buf_release_scratch(tmp_trail);
							} else if(h_trail_len > 0) {
								token_append_text_n(trail_tok, h_trail, h_trail_len);
							} else {
								token_append_text_n(trail_tok, buf2 + line_end, extra_trail_len);
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
					GROW_OUT2_APPEND(sent, slen);

					/* Continue from match end (newline boundary char is not consumed). */
					cursor = match_end;
					continue;
				}

				/* OOM fallback: leave current line unchanged */
				size_t copy_len = (line_end < buf2_len) ? line_len + 1 : line_len;
				GROW_OUT2_APPEND(buf2 + line_start, copy_len);
				cursor = (line_end < buf2_len) ? (line_end + 1) : line_end;
				continue;
			}

			/* Not a heading: copy line + optional '\n' verbatim */
			size_t copy_len = (line_end < buf2_len) ? line_len + 1 : line_len;
			GROW_OUT2_APPEND(buf2 + line_start, copy_len);
			cursor = (line_end < buf2_len) ? (line_end + 1) : line_end;
		}
		out2_tb->buf[out2_tb->len] = '\0';
		wiki_thread_buf_set(tb, out2_tb->buf, out2_tb->len);
		wiki_thread_buf_release_scratch(out2_tb);
#undef GROW_OUT2_APPEND
	}

	if(prefixed && tb->len > 0) {
		/* Remove the NUL prefix in-place with memmove — avoids malloc+copy+free */
		size_t unpref_len= tb->len - 1;
		if(unpref_len > 0) memmove(tb->buf, tb->buf + 1, unpref_len);
		tb->len= unpref_len;
		tb->buf[tb->len]= '\0';
	}
}
