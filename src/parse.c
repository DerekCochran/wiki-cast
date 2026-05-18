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
 *   Stage  3: parseTable
 *   Stage  4: parseHrAndDoubleUnderscore
 *   Stage  5: parseLinks
 *   Stage  6: parseQuotes
 *   Stage  7: parseExternalLinks
 *   Stage  8: parseMagicLinks
 *   Stage  9: parseList
 *   Stage 10: parseConverter
 *
 * After all requested stages, build() expands sentinel markers into the
 * child token tree.
 */
#include "parse.h"
#include "accum.h"
#include "build.h"
#include "config.h"
#include "util/log.h"
#include "util/env_cache.h"
#include "parser/braces.h"
#include "parser/converter.h"
#include "parser/links.h"
#include "parser/list.h"
#include "parser/quotes.h"
#include "parser/redirect.h"
#include "parser/table.h"
#include "util/string_util.h"
#include "util/thread_buffer.h"
#include "token.h"

/* build() is declared in build.h */
#include "parser/comment_and_ext.h"
#include "parser/hr_and_double_underscore.h"
#include "parser/html.h"
#include "parser/magic_links.h"

#include "parser/external_links.h"
#include <stringzilla/stringzilla.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

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
																size_t *count, size_t *cap) {
	if(!t) return;
	if(*count >= *cap) {
		*cap*= 2;
		*arr= realloc(*arr, *cap * sizeof(Token *));
		assert(*arr);
	}
	(*arr)[(*count)++]= (Token *)t;
	for(size_t i= 0; i < t->child_count; i++) {
		if(!t->children[i].is_text)
			collect_tree_tokens(t->children[i].token, arr, count, cap);
	}
}

static int cmp_token_ptr(const void *a, const void *b) {
	/* Compare token pointers numerically for qsort / bsearch */
	uintptr_t pa= (uintptr_t)*(const Token *const *)a;
	uintptr_t pb= (uintptr_t)*(const Token *const *)b;
	return (pa > pb) - (pa < pb);
}

static void free_accum_orphans(const Token *root, Accum *accum) {
	size_t cap= 64 + accum->count;
	size_t count= 0;
	Token **live= malloc(cap * sizeof(Token *));
	if(!live) return;

	collect_tree_tokens(root, &live, &count, &cap); // Collect live tokens from the tree
	qsort(live, count, sizeof(Token *), cmp_token_ptr);

	for(size_t i= 0; i < accum->count; i++) {
		Token *t= accum->tokens[i];
		if(!t) continue;

		/* Binary search in sorted live set */
		size_t lo= 0, hi= count;
		bool found= false;
		while(lo < hi) {
			size_t mid= (lo + hi) / 2;
			if(live[mid] == t) {
				found= true;
				break;
			}
			if((uintptr_t)live[mid] < (uintptr_t)t)
				lo= mid + 1;
			else
				hi= mid;
		}
		if(!found) {
			/* Orphan: free shallowly — token children are separate accum
             * entries and are freed when their own slot is encountered. */
			token_free_shallow(t);
		}
	}
	free(live);
}

static bool mem_has(const char *s, size_t len, const char *needle) {
	size_t nlen= needle ? strlen(needle) : 0;
	if(!s || nlen == 0 || len < nlen) return false;
	return sz_find(s, len, needle, nlen) != NULL;
}

/* Write a JSON-escaped string of given length to fp (surrounded by quotes). */
static void json_write_escaped_len(const char *s, size_t len, FILE *fp) {
	if(!fp) return;
	fputc('"', fp);
	for(size_t i= 0; i < len; i++) {
		unsigned char c= (unsigned char)s[i];
		if(c == '"')
			fputs("\\\"", fp);
		else if(c == '\\')
			fputs("\\\\", fp);
		else if(c == '\n')
			fputs("\\n", fp);
		else if(c == '\r')
			fputs("\\r", fp);
		else if(c == '\t')
			fputs("\\t", fp);
		else if(c < 0x20)
			fprintf(fp, "\\u%04x", c);
		else
			fputc(c, fp);
	}
	fputc('"', fp);
}

static void stage_json_write_token(const Token *t, FILE *fp, const Accum *accum);

static bool stage_json_parse_sentinel(const char *s, size_t len, size_t *pos, size_t *idx_out) {
	if(!s || !pos || !idx_out) {
		log_debug_env_token("WTC_DEBUG_STAGE_1", NULL,
			"[C stage_json_parse_sentinel] invalid args pos_ptr=%p", (void*)pos);
		return false;
	}
	if(*pos >= len) {
		log_debug_env_token("WTC_DEBUG_STAGE_1", NULL,
			"[C stage_json_parse_sentinel] pos >= len: pos=%zu len=%zu", *pos, len);
		return false;
	}
	if((unsigned char)s[*pos] != '\0') {
		log_debug_env_token("WTC_DEBUG_STAGE_1", NULL,
			"[C stage_json_parse_sentinel] not NUL at pos=%zu byte=%02X", *pos, (unsigned char)s[*pos]);
		return false;
	}

	size_t p = *pos + 1; /* first digit */
	if(p >= len) {
		log_debug_env_token("WTC_DEBUG_STAGE_1", NULL,
			"[C stage_json_parse_sentinel] no room for digit at p=%zu len=%zu", p, len);
		return false;
	}
	if(!isdigit((unsigned char)s[p])) {
		log_debug_env_token("WTC_DEBUG_STAGE_1", NULL,
			"[C stage_json_parse_sentinel] first char after NUL not digit p=%zu byte=%02X", p, (unsigned char)s[p]);
		return false;
	}

	size_t idx = 0;
	while(p < len && isdigit((unsigned char)s[p])) {
		idx = idx * 10 + (size_t)(s[p] - '0');
		p++;
	}

	/* p now points at the type char (should exist) */
	if(p >= len) {
		log_debug_env_token("WTC_DEBUG_STAGE_1", NULL,
			"[C stage_json_parse_sentinel] ran off end after digits p=%zu len=%zu", p, len);
		return false;
	}

	unsigned char type_ch = (unsigned char)s[p];
	if(p + 1 >= len) {
		log_debug_env_token("WTC_DEBUG_STAGE_1", NULL,
			"[C stage_json_parse_sentinel] missing DEL after type at p=%zu type=%02X len=%zu", p, type_ch, len);
		return false;
	}
	if((unsigned char)s[p + 1] != 0x7F) {
		log_debug_env_token("WTC_DEBUG_STAGE_1", NULL,
			"[C stage_json_parse_sentinel] trailing byte not DEL at del_idx=%zu byte=%02X", p + 1, (unsigned char)s[p + 1]);
		return false;
	}

	/* Success: advance pos to after the DEL */
	*idx_out = idx;
	*pos = p + 2;
	log_debug_env_token("WTC_DEBUG_STAGE_1", NULL,
		"[C stage_json_parse_sentinel] OK parsed idx=%zu type=%02X next_pos=%zu", idx, type_ch, *pos);
	return true;
}

static void stage_json_write_text(const char *s, size_t len, FILE *fp) {
	fputs("{\"type\":\"text\",\"data\":", fp);
	json_write_escaped_len(s, len, fp);
	fputc('}', fp);
}

static void stage_json_write_text_segments(const char *s, size_t len,
														 FILE *fp,
														 const Accum *accum,
														 bool *first) {
	if(!s || len == 0) return;

	size_t pos= 0;
	while(pos < len) {
		if((unsigned char)s[pos] == '\0') {
			/* Debug: log hex around this NUL so we can see sentinel bytes */
			{
				char hexbuf[128];
				size_t hexpos = 0;
				size_t lookahead = len - pos;
				if(lookahead > 12) lookahead = 12;
				for(size_t i = 0; i < lookahead && hexpos + 3 < sizeof(hexbuf); i++) {
					int wn = snprintf(hexbuf + hexpos, sizeof(hexbuf) - hexpos, "%02X", (unsigned char)s[pos + i]);
					if(wn > 0) hexpos += (size_t)wn;
					if(i + 1 < lookahead && hexpos + 1 < sizeof(hexbuf)) hexbuf[hexpos++] = ' ';
				}
				hexbuf[hexpos] = '\0';
				log_debug_env_token("WTC_DEBUG_STAGE_1", NULL,
					"[C stage_json_write_text_segments] NUL at pos=%zu len=%zu lookahead_hex=%s",
					pos, len, hexbuf);
			}
			size_t idx= 0;
			size_t p= pos;
			bool _sp_ok = stage_json_parse_sentinel(s, len, &p, &idx);
			log_debug_env_token("WTC_DEBUG_STAGE_1", NULL,
				"[C stage_json_write_text_segments] parse_sentinel=%d idx=%zu accum_count=%zu tok=%p",
				(int)_sp_ok, idx,
				accum ? accum->count : (size_t)-1,
				(void*)(accum && idx < accum->count ? accum->tokens[idx] : NULL));
			if(_sp_ok && accum && idx < accum->count && accum->tokens[idx]) {
				if(!*first) fputc(',', fp);
				*first= false;
				stage_json_write_token(accum->tokens[idx], fp, accum);
				pos= p;
				continue;
			}
		}

		size_t start= pos;
		while(pos < len && (unsigned char)s[pos] != '\0') pos++;
		if(pos > start) {
			if(!*first) fputc(',', fp);
			*first= false;
			stage_json_write_text(s + start, pos - start, fp);
		}
	}
}

static void stage_json_write_token(const Token *t, FILE *fp, const Accum *accum) {
	if(!t) {
		fputs("null", fp);
		return;
	}

	fputs("{\"type\":", fp);
	json_write_escaped_len(t->type_name ? t->type_name : "", t->type_name ? strlen(t->type_name) : 0, fp);

	if(t->name) {
		fputs(",\"name\":", fp);
		json_write_escaped_len(t->name, strlen(t->name), fp);
	}

	bool has_stage_children= false;
	for(size_t i= 0; i < t->child_count; i++) {
		const Child *c= &t->children[i];
		if(c->is_text) {
			if(c->text_len > 0) {
				has_stage_children= true;
				break;
			}
		} else {
			has_stage_children= true;
			break;
		}
	}

	if(has_stage_children) {
		fputs(",\"childNodes\":[", fp);
		bool first= true;
		for(size_t i= 0; i < t->child_count; i++) {
			const Child *c= &t->children[i];
			if(c->is_text) {
				{
					char _hbuf[128]; size_t _hp = 0;
					size_t _ls = (c->text_len > 86) ? 86 : 0;
					size_t _le = (_ls + 6 < c->text_len) ? _ls + 6 : c->text_len;
					for(size_t _qi = _ls; _qi < _le && _hp + 3 < sizeof(_hbuf); _qi++) {
						int _wn = snprintf(_hbuf+_hp, sizeof(_hbuf)-_hp, "%02X", (unsigned char)c->text[_qi]);
						if(_wn > 0) _hp += (size_t)_wn;
						if(_qi+1 < _le && _hp < sizeof(_hbuf)) _hbuf[_hp++] = ' ';
					}
					_hbuf[_hp] = '\0';
					log_debug_env_token("WTC_DEBUG_STAGE_1", NULL,
						"[C write_tok_child] tok_type=%s text_len=%zu ptr=%p bytes_at[%zu..%zu]=%s",
						t->type_name ? t->type_name : "?", c->text_len, (void*)c->text,
						_ls, _le, _hbuf);
				}
				stage_json_write_text_segments(c->text, c->text_len, fp, accum, &first);
			} else {
				if(!first) fputc(',', fp);
				first= false;
				stage_json_write_token(c->token, fp, accum);
			}
		}
		fputs("]", fp);
	}

	fputc('}', fp);
}

/* Append a JSON snapshot representing the current root content (ws)
 * to <stage_log_dir>/native-stage.log. The ws buffer is scanned for
 * sentinel markers (\0<digits><ch>\x7F) and token entries from the
 * accumulator are embedded via json_stringify_wikiparser_node(). */
static void append_native_stage_json(const char *stage_log_dir, int stage, ThreadBuf *ws, Accum *accum) {
	if(!stage_log_dir || !ws) return;
	/* Also emit human-readable dumps of the stage and tokens arena to stdout
	 * when requested via env var WTC_DEBUG_STAGE_DUMP. This helps narrow which
	 * stage produces or corrupts sentinel markers. */
	wiki_thread_buf_log_state(NULL, ws);
	char pathbuf[1024];
	snprintf(pathbuf, sizeof(pathbuf), "%s/native-stage.log", stage_log_dir);
	FILE *f= fopen(pathbuf, "a");
	if(!f) return;
	fprintf(f, "Stage %d: ", stage);
	/* Emit a root object with childNodes array */
	fputs("{\"type\":\"root\",\"childNodes\":[", f);

	bool first= true;
	size_t pos= 0;
	while(pos < ws->len) {
		if((unsigned char)ws->buf[pos] == '\0') {
			{
				char hexbuf[128];
				size_t hexpos = 0;
				size_t lookahead = ws->len - pos;
				if(lookahead > 8) lookahead = 8;
				for(size_t i = 0; i < lookahead && hexpos + 3 < sizeof(hexbuf); i++) {
					int wn = snprintf(hexbuf + hexpos, sizeof(hexbuf) - hexpos, "%02X", (unsigned char)ws->buf[pos + i]);
					if(wn > 0) hexpos += (size_t)wn;
					if(i + 1 < lookahead && hexpos + 1 < sizeof(hexbuf)) hexbuf[hexpos++] = ' ';
				}
				hexbuf[hexpos] = '\0';
				log_debug_env_token("WTC_DEBUG_STAGE_1", NULL,
					"[C append_native_stage_json] NUL at pos=%zu lookahead=%zu hex=%s",
					pos, lookahead, hexbuf);
			}
			/* sentinel: \0<digits><ch>\x7F */
			pos++;
			size_t numStart= pos;
			while(pos < ws->len && isdigit((unsigned char)ws->buf[pos])) pos++;
			size_t numLen= pos - numStart;
			if(numLen == 0) continue;
			char numbuf[32];
			if(numLen >= sizeof(numbuf)) continue;
			sz_copy(numbuf, ws->buf + numStart, numLen);
			numbuf[numLen]= '\0';
			long idx= strtol(numbuf, NULL, 10);
			/* skip the sentinel char and the trailing 0x7F if present */
			if(pos < ws->len) pos++;
			if(pos < ws->len && (unsigned char)ws->buf[pos] == 0x7F) pos++;

			if(!first) fputc(',', f);
			first= false;

			if(idx >= 0 && (size_t)idx < accum->count && accum->tokens[idx]) {
				stage_json_write_token(accum->tokens[idx], f, accum);
			} else {
				fputs("null", f);
			}
		} else {
			size_t start= pos;
			while(pos < ws->len && (unsigned char)ws->buf[pos] != '\0') pos++;
			size_t seglen= pos - start;
			if(!first) fputc(',', f);
			first= false;
			stage_json_write_text(ws->buf + start, seglen, f);
		}
	}

	fputs("]}\n", f);
	fclose(f);
}

static void parse_list_skip_first_line(ThreadBuf *scratch, const ParserConfig *cfg, Accum *accum) {
	if(!scratch || !scratch->buf || scratch->len == 0) return;

	const char nl= '\n';
	const char *nl_pos= sz_find_byte(scratch->buf, scratch->len, &nl);
	if(!nl_pos || nl_pos + 1 >= scratch->buf + scratch->len) return;
	size_t newline_index= (size_t)(nl_pos - scratch->buf);

	size_t prefix_len= newline_index + 1;
	size_t rest_len= scratch->len - prefix_len;
 	/* Try to use a temporary scratch buffer for prefix+processed-rest assembly. */
 	const char *orig_buf = scratch->buf;
 	const char *rest = orig_buf + prefix_len;

 	ThreadBuf *tmp = wiki_thread_buf_acquire_scratch();
 	if(!tmp) {
 		log_fatal("parse_list_skip_first_line: failed to acquire scratch");
 		abort();
 	}

	/* Copy prefix into tmp, then parse the rest into `scratch` and append. */
	wiki_thread_buf_reserve(tmp, prefix_len + rest_len + 1);
	sz_copy(tmp->buf, orig_buf, prefix_len);
	tmp->len = prefix_len;

	wiki_thread_buf_set(scratch, rest, rest_len);
	parse_list(scratch, cfg, accum);

	/* Ensure tmp can hold prefix + processed-rest and append. */
	wiki_thread_buf_reserve(tmp, prefix_len + scratch->len + 1);
	sz_copy(tmp->buf + prefix_len, scratch->buf, scratch->len);
	tmp->len = prefix_len + scratch->len;
	tmp->buf[tmp->len] = '\0';

	wiki_thread_buf_set(scratch, tmp->buf, tmp->len);
	wiki_thread_buf_release_scratch(tmp);
	return;
}

static bool should_postprocess_plain(const Token *t) {
	if(!t || !(t->type == TOKEN_PLAIN || t->type == TOKEN_EXT_INNER) || !t->type_name) return false;
	return strcmp(t->type_name, "td-inner") == 0 || strcmp(t->type_name, "table-inter") == 0 || strcmp(t->type_name, "ext-inner") == 0 || strcmp(t->type_name, "heading-title") == 0;
}

static bool ext_inner_allows_nested_parse(const char *name) {
	if(!name || !*name) return false;

	/* JS ExtToken parity: only specific ext tags parse inner wikitext.
     * Unlisted tags (for example score/syntaxhighlight/math) are nowiki-like. */
	return strcmp(name, "indicator") == 0 || strcmp(name, "poem") == 0 || strcmp(name, "ref") == 0 || strcmp(name, "option") == 0 || strcmp(name, "combooption") == 0 || strcmp(name, "tab") == 0 || strcmp(name, "tabs") == 0 || strcmp(name, "poll") == 0 || strcmp(name, "seo") == 0 || strcmp(name, "langconvert") == 0 || strcmp(name, "phonos") == 0 || strcmp(name, "dynamicpagelist") == 0 || strcmp(name, "inputbox") == 0 || strcmp(name, "references") == 0 || strcmp(name, "choose") == 0 || strcmp(name, "combobox") == 0 || strcmp(name, "gallery") == 0 || strcmp(name, "imagemap") == 0 || strcmp(name, "categorytree") == 0;
}

static void postprocess_nested_plain(Token *t, const ParserConfig *cfg, Accum *accum,
																 const char *page);

static void parse_quotes_stage6_per_line(ThreadBuf *ws, const ParserConfig *cfg, Accum *accum);

static Token *make_empty_noinclude(Accum *accum) {
	Token *n= token_new(TOKEN_NOINCLUDE, "noinclude");
	if(!n) return NULL;
	token_append_text_n(n, "", 0);
	accum_push(accum, n);
	return n;
}

static Token *parse_single_link_token(const char *s, size_t len,
																	const ParserConfig *cfg, Accum *accum,
																	const char *page) {
	if(!s) return NULL;

	ThreadBuf *scratch = wiki_thread_buf_acquire_scratch();
	if(!scratch) return NULL;
	/* Reserve space and build wrapped string into scratch */
	wiki_thread_buf_reserve(scratch, len + 4);
	scratch->buf[0]= '[';
	scratch->buf[1]= '[';
	sz_copy(scratch->buf + 2, s, len);
	scratch->buf[2 + len]= ']';
	scratch->buf[3 + len]= ']';
	scratch->buf[4 + len]= '\0';
	scratch->len= len + 4;

	parse_links(scratch, cfg, accum, page, false);

	Token *tmp= token_new(TOKEN_PLAIN, "imagemap-link-inner");
	if(!tmp) {
		wiki_thread_buf_release_scratch(scratch);
		return NULL;
	}
	build_from_str(tmp, scratch->buf, scratch->len, accum);
	build_token_recursive(tmp, accum, cfg);

	Token *out= NULL;
	if(tmp->child_count == 1 && !tmp->children[0].is_text && tmp->children[0].token && tmp->children[0].token->type == TOKEN_LINK) {
		out= tmp->children[0].token;
		tmp->children[0].token= NULL;
	}

	token_free_shallow(tmp);
	wiki_thread_buf_release_scratch(scratch);
	return out;
}

static Token *parse_gallery_image_line(const char *line, size_t line_len,
																			 const ParserConfig *cfg, Accum *accum,
																			 const char *page) {
	if(!line || line_len == 0) return NULL;

	ThreadBuf *pre_text_tb= NULL;
	const char pipe_ch = '|';
	const char *pipe_ptr = sz_find_byte(line, line_len, &pipe_ch);
	size_t pipe_idx= SIZE_MAX;
	if(pipe_ptr) {
		pipe_idx= (size_t)(pipe_ptr - line);
		if(pipe_idx + 1 < line_len) {
			pre_text_tb= wiki_thread_buf_acquire_scratch_from_data(line + pipe_idx + 1, line_len - (pipe_idx + 1));
			if(!pre_text_tb) { log_fatal("parse_gallery_image_line: failed to acquire scratch for pre_text"); abort(); }
			/* JS parity: gallery-image text is pre-parsed through inline-link stages
			 * before FileToken-style parameter splitting. */
			parse_comment_and_ext(pre_text_tb, cfg, accum, false);
			parse_braces(pre_text_tb, cfg, accum);
			parse_links(pre_text_tb, cfg, accum, page, false);
			parse_external_links(pre_text_tb, cfg, accum, false);
			parse_magic_links(pre_text_tb, cfg, accum);
		}
	}

	ThreadBuf *scratch = wiki_thread_buf_acquire_scratch();
	if(!scratch) return NULL;
	if(pre_text_tb && pipe_idx != SIZE_MAX) {
		size_t lhs_len= pipe_idx + 1; /* include the first '|' */
		size_t wrapped_len= 2 + lhs_len + pre_text_tb->len + 2; /* [[ + lhs + pre + ]] */
		wiki_thread_buf_reserve(scratch, wrapped_len + 1);
		scratch->buf[0]= '[';
		scratch->buf[1]= '[';
		sz_copy(scratch->buf + 2, line, lhs_len);
		if(pre_text_tb->len > 0) {
			sz_copy(scratch->buf + 2 + lhs_len, pre_text_tb->buf, pre_text_tb->len);
		}
		size_t tail= 2 + lhs_len + pre_text_tb->len;
		scratch->buf[tail]= ']';
		scratch->buf[tail + 1]= ']';
		scratch->buf[tail + 2]= '\0';
		scratch->len= tail + 2;
	} else {
		wiki_thread_buf_reserve(scratch, line_len + 4);
		scratch->buf[0]= '[';
		scratch->buf[1]= '[';
		sz_copy(scratch->buf + 2, line, line_len);
		scratch->buf[2 + line_len]= ']';
		scratch->buf[3 + line_len]= ']';
		scratch->buf[4 + line_len]= '\0';
		scratch->len= line_len + 4;
	}

	/* JS parity: braces are parsed before links, which protects pipes inside templates. */
	parse_braces(scratch, cfg, accum);
	parse_links(scratch, cfg, accum, page, false);

	Token *tmp= token_new(TOKEN_PLAIN, "gallery-line");
	if(!tmp) {
		if(pre_text_tb) wiki_thread_buf_release_scratch(pre_text_tb);
		wiki_thread_buf_release_scratch(scratch);
		return NULL;
	}
	build_from_str(tmp, scratch->buf, scratch->len, accum);
	build_token_recursive(tmp, accum, cfg);

	Token *out= NULL;
	if(tmp->child_count == 1 && !tmp->children[0].is_text && tmp->children[0].token && tmp->children[0].token->type == TOKEN_FILE) {
		out= tmp->children[0].token;
		tmp->children[0].token= NULL;
		if(out->type_name) free(out->type_name);
		out->type_name= strdup("gallery-image");
		/* JS parity: GalleryImageToken keeps link=... as a link parameter. */
		for(size_t ci= 1; ci < out->child_count; ci++) {
			if(out->children[ci].is_text || !out->children[ci].token) continue;
			Token *param= out->children[ci].token;
			if(param->type != TOKEN_PLAIN || !param->type_name || strcmp(param->type_name, "image-parameter") != 0) continue;
			if(!param->name || strcmp(param->name, "caption") != 0 || param->child_count == 0) continue;
			Child *first= &param->children[0];
			if(!first->is_text || !first->text || first->text_len < 5) continue;

			size_t p= 0;
			while(p < first->text_len && (first->text[p] == ' ' || first->text[p] == '\t')) p++;
			if(p + 5 > first->text_len || strncmp(first->text + p, "link=", 5) != 0) continue;

			char *new_name= strdup("link");
			if(!new_name) continue;
			free(param->name);
			param->name= new_name;
			free(param->data.image_param.raw_syntax);
			param->data.image_param.raw_syntax= malloc(p + 8);
			if(param->data.image_param.raw_syntax) {
				if(p > 0) memcpy(param->data.image_param.raw_syntax, first->text, p);
				memcpy(param->data.image_param.raw_syntax + p, "link=$1", 7);
				param->data.image_param.raw_syntax[p + 7]= '\0';
			}

			size_t prefix_len= p + 5;
			size_t new_len= first->text_len - prefix_len;
			const char *view= wiki_thread_buf_append_to_tokens(first->text + prefix_len, new_len);
			if(first->text_owned && first->text) free((void *)first->text);
			first->text= view;
			first->text_len= new_len;
			first->text_owned= false;
		}
	}
	if(!out) {
		size_t non_ws= 0;
		while(non_ws < line_len && isspace((unsigned char)line[non_ws])) non_ws++;
		if(non_ws < line_len && sz_find(line, line_len, "[[", 2) == NULL) {
			Token *fallback= token_new(TOKEN_FILE, "gallery-image");
			if(fallback) {
				Token *target= token_new(TOKEN_ATOM, "link-target");
				if(target) {
					const char *view= wiki_thread_buf_append_to_tokens(line, line_len);
					token_append_text_n(target, view, line_len);
					accum_push(accum, target);
					token_append_child(fallback, target);
				}
				accum_push(accum, fallback);
				out= fallback;
			}
		}
	}

	token_free_shallow(tmp);
	if(pre_text_tb) wiki_thread_buf_release_scratch(pre_text_tb);
	wiki_thread_buf_release_scratch(scratch);
	return out;
}

static Token *parse_imagemap_image_line(const char *line, size_t line_len,
																				const ParserConfig *cfg, Accum *accum,
																				const char *page) {
	if(!line || line_len == 0) return NULL;

	ThreadBuf *scratch = wiki_thread_buf_acquire_scratch();
	if(!scratch) return NULL;
	wiki_thread_buf_reserve(scratch, line_len + 4);
	scratch->buf[0]= '[';
	scratch->buf[1]= '[';
	memcpy(scratch->buf + 2, line, line_len);
	scratch->buf[2 + line_len]= ']';
	scratch->buf[3 + line_len]= ']';
	scratch->buf[4 + line_len]= '\0';
	scratch->len= line_len + 4;

	/* JS parity: braces are parsed before links, which protects pipes inside templates. */
	parse_braces(scratch, cfg, accum);
	parse_links(scratch, cfg, accum, page, false);

	Token *tmp= token_new(TOKEN_PLAIN, "imagemap-image-line");
	if(!tmp) {
		wiki_thread_buf_release_scratch(scratch);
		return NULL;
	}
	build_from_str(tmp, scratch->buf, scratch->len, accum);
	build_token_recursive(tmp, accum, cfg);

	Token *out= NULL;
	if(tmp->child_count == 1 && !tmp->children[0].is_text && tmp->children[0].token && tmp->children[0].token->type == TOKEN_FILE) {
		out= tmp->children[0].token;
		tmp->children[0].token= NULL;
		if(out->type_name) free(out->type_name);
		out->type_name= strdup("imagemap-image");
	}

	token_free_shallow(tmp);
	wiki_thread_buf_release_scratch(scratch);
	return out;
}

static Token *parse_imagemap_link_line(const char *line, size_t line_len,
																			 const ParserConfig *cfg, Accum *accum,
																			 const char *page) {
	if(!line || line_len == 0) return NULL;

	const char open_pat[] = "[[";
	const char *open_ptr= sz_find(line, line_len, open_pat, 2);
	if(!open_ptr) return NULL;
	size_t open= (size_t)(open_ptr - line);

	const char close_pat[] = "]]";
	const char *close_ptr= sz_find(line + open + 2, line_len - (open + 2), close_pat, 2);
	if(!close_ptr) return NULL;
	size_t close= (size_t)(close_ptr - line);
	if(close <= open + 1) return NULL;

	Token *t= token_new(TOKEN_PLAIN, "imagemap-link");
	if(!t) return NULL;
	accum_push(accum, t);

	if(open > 0) {
			const char *view = wiki_thread_buf_append_to_tokens(line, open);
			token_append_text_n(t, view, open);
	} else {
		token_append_text_n(t, "", 0);
	}

	const char *inner= line + open + 2;
	size_t inner_len= close - (open + 2);
	Token *link= parse_single_link_token(inner, inner_len, cfg, accum, page);
	if(link) {
		token_append_child(t, link);
	} else {
			const char *view = wiki_thread_buf_append_to_tokens(line + open, (close + 2) - open);
			token_append_text_n(t, view, (close + 2) - open);
	}

	if(close + 2 < line_len) {
		const char *view = wiki_thread_buf_append_to_tokens(line + close + 2, line_len - (close + 2));
		token_append_text_n(t, view, line_len - (close + 2));
	}

	Token *tail= make_empty_noinclude(accum);
	if(tail) token_append_child(t, tail);

	return t;
}

static void postprocess_gallery_ext_inner(Token *t, const ParserConfig *cfg, Accum *accum,
																		const char *page) {
	if(!t || !t->type_name || strcmp(t->type_name, "ext-inner") != 0 || !t->name || strcmp(t->name, "gallery") != 0) return;

	bool has_non_text= false;
	size_t src_len= 0;
	for(size_t i= 0; i < t->child_count; i++) {
		if(!t->children[i].is_text) {
			has_non_text= true;
		} else {
			src_len+= t->children[i].text_len;
		}
	}
	if(has_non_text || src_len == 0) return;

	/* Try to assemble the concatenated child-text into a temporary scratch buffer. */
	ThreadBuf *tmp = wiki_thread_buf_acquire_scratch();
	if(tmp) {
		wiki_thread_buf_reserve(tmp, src_len + 1);
		size_t pos = 0;
		for(size_t i= 0; i < t->child_count; i++) {
			sz_copy(tmp->buf + pos, t->children[i].text, t->children[i].text_len);
			pos += t->children[i].text_len;
		}
		tmp->len = pos;
		tmp->buf[tmp->len] = '\0';

		for(size_t i= 0; i < t->child_count; i++) {
			if(t->children[i].is_text) {
				if(t->children[i].text_owned && t->children[i].text) free((void*)t->children[i].text);
			}
		}
		t->child_count= 0;

		size_t line_start= 0;
		while(line_start < tmp->len) {
			const char nl= '\n';
			const char *eol= sz_find_byte(tmp->buf + line_start, tmp->len - line_start, &nl);
			size_t line_len = eol ? (size_t)(eol - (tmp->buf + line_start)) : tmp->len - line_start;
			const char *line_ptr= tmp->buf + line_start;

			Token *img= parse_gallery_image_line(line_ptr, line_len, cfg, accum, page);
			if(img) {
				token_append_child(t, img);
			} else {
				const char *view = wiki_thread_buf_append_to_tokens(line_ptr, line_len);
				token_append_text_n(t, view, line_len);
			}

			line_start= eol ? (size_t)(eol - tmp->buf) + 1 : tmp->len;
		}

		for(size_t i= 0; i < t->child_count; i++) {
			if(!t->children[i].is_text && t->children[i].token) {
				postprocess_nested_plain(t->children[i].token, cfg, accum, page);
			}
		}

		wiki_thread_buf_release_scratch(tmp);
		return;
	}

	/* acquisition failure is fatal — do not fall back to heap */
	log_fatal("postprocess_gallery_ext_inner: failed to acquire scratch");
	abort();
}

static void postprocess_imagemap_ext_inner(Token *t, const ParserConfig *cfg, Accum *accum,
																		 const char *page) {
	if(!t || !t->type_name || strcmp(t->type_name, "ext-inner") != 0 || !t->name || strcmp(t->name, "imagemap") != 0) return;

	bool has_non_text= false;
	size_t src_len= 0;
	for(size_t i= 0; i < t->child_count; i++) {
		if(!t->children[i].is_text) {
			has_non_text= true;
		} else {
			src_len+= t->children[i].text_len;
		}
	}
	if(has_non_text || src_len == 0) return;

	/* Try to assemble child text into a scratch buffer to avoid heap allocs. */
	ThreadBuf *tmp = wiki_thread_buf_acquire_scratch();
	if(tmp) {
		wiki_thread_buf_reserve(tmp, src_len + 1);
		size_t pos = 0;
		for(size_t i= 0; i < t->child_count; i++) {
			sz_copy(tmp->buf + pos, t->children[i].text, t->children[i].text_len);
			pos += t->children[i].text_len;
		}
		tmp->len = pos;
		tmp->buf[tmp->len] = '\0';

		for(size_t i= 0; i < t->child_count; i++) {
			if(t->children[i].is_text) {
				if(t->children[i].text_owned && t->children[i].text) free((void*)t->children[i].text);
			}
		}
		t->child_count= 0;

		bool image_seen= false;
		size_t line_start= 0;
		while(line_start < tmp->len) {
			const char nl= '\n';
			const char *eol= sz_find_byte(tmp->buf + line_start, tmp->len - line_start, &nl);
			size_t line_len = eol ? (size_t)(eol - (tmp->buf + line_start)) : tmp->len - line_start;
			const char *line_ptr= tmp->buf + line_start;

			if(line_len == 0) {
				Token *n= make_empty_noinclude(accum);
				if(n) token_append_child(t, n);
			} else {
				Token *tok= NULL;
				if(!image_seen) {
					tok= parse_imagemap_image_line(line_ptr, line_len, cfg, accum, page);
					if(tok) image_seen= true;
				}
				if(!tok) {
					tok= parse_imagemap_link_line(line_ptr, line_len, cfg, accum, page);
				}
				if(tok) {
					token_append_child(t, tok);
				} else {
					const char *view = wiki_thread_buf_append_to_tokens(line_ptr, line_len);
					token_append_text_n(t, view, line_len);
				}
			}

			line_start= eol ? (size_t)(eol - tmp->buf) + 1 : tmp->len;
		}

		for(size_t i= 0; i < t->child_count; i++) {
			if(!t->children[i].is_text && t->children[i].token) {
				postprocess_nested_plain(t->children[i].token, cfg, accum, page);
			}
		}

		wiki_thread_buf_release_scratch(tmp);
		return;
	}

	/* acquisition failure is fatal — do not fall back to heap */
	log_fatal("postprocess_imagemap_ext_inner: failed to acquire scratch");
	abort();
}

static void run_nested_plain_pipeline(ThreadBuf *scratch,
																			bool is_td_inner,
																			bool is_ext_inner,
																			bool is_heading_title,
																			Token *t,
																			const ParserConfig *cfg,
																					Accum *accum,
																					const char *page) {
	if(is_ext_inner) {
		parse_comment_and_ext(scratch, cfg, accum, false);
	}

	if(!is_heading_title) {
		parse_braces(scratch, cfg, accum);
	}

	if(is_td_inner || is_ext_inner) {
		bool is_poem_ext_inner= is_ext_inner && t && t->name && strcmp(t->name, "poem") == 0;
		bool ext_inner_has_bang= is_ext_inner && sz_find(scratch->buf, scratch->len, "!\x7F", 2) != NULL;
		bool ext_inner_has_sentinel = false;
		if (is_ext_inner) {
			const char _zn_run = '\0';
			ext_inner_has_sentinel = sz_find_byte(scratch->buf, scratch->len, &_zn_run) != NULL;
		}

		if(!ext_inner_has_bang) {
			/* JS parity: td-inner parsing starts from stage 4, so HTML (stage 2)
             * must not run before links; otherwise links spanning inline HTML split. */
			if(is_ext_inner) {
				parse_html(scratch, cfg, accum);
			}
			TokenType hr_root_type= t->type;
			if(ext_inner_has_sentinel) {
				hr_root_type= TOKEN_PLAIN;
			}
			parse_hr_and_double_underscore(scratch, cfg, accum, hr_root_type, t->type_name);
			const ParserConfig *links_cfg= cfg;
			ParserConfig cfg_local;
			if(is_ext_inner && cfg) {
				cfg_local= *cfg;
				cfg_local.in_ext= true;
				links_cfg= &cfg_local;
			}
			parse_links(scratch, links_cfg, accum, page, false);
			parse_quotes_stage6_per_line(scratch, cfg, accum);
			parse_external_links(scratch, cfg, accum, false);
			parse_magic_links(scratch, cfg, accum);
			if(is_td_inner) {
				parse_list_skip_first_line(scratch, cfg, accum);
			} else if(is_ext_inner) {
				if(is_poem_ext_inner) {
					parse_list(scratch, cfg, accum);
				} else {
					parse_list_skip_first_line(scratch, cfg, accum);
				}
			}
			parse_converter(scratch, cfg, accum);
		} else {
			parse_html(scratch, cfg, accum);
		}
	} else if(is_heading_title) {
		parse_html(scratch, cfg, accum);
		parse_links(scratch, cfg, accum, page, false);
		parse_quotes_stage6_per_line(scratch, cfg, accum);
		parse_external_links(scratch, cfg, accum, false);
		parse_magic_links(scratch, cfg, accum);
	}
}

static void postprocess_nested_plain(Token *t, const ParserConfig *cfg, Accum *accum,
																 const char *page) {
	if(!t) return;

	for(size_t i= 0; i < t->child_count; i++) {
		if(!t->children[i].is_text && t->children[i].token) {
			postprocess_nested_plain(t->children[i].token, cfg, accum, page);
		}
	}

	if(!should_postprocess_plain(t)) return;
	if(t->type == TOKEN_EXT_INNER && t->name && strcmp(t->name, "nowiki") == 0) return;
	if(t->type == TOKEN_EXT_INNER && !ext_inner_allows_nested_parse(t->name)) return;

	if(t->type_name && strcmp(t->type_name, "ext-inner") == 0 && t->name && strcmp(t->name, "gallery") == 0) {
		postprocess_gallery_ext_inner(t, cfg, accum, page);
		return;
	}

	if(t->type_name && strcmp(t->type_name, "ext-inner") == 0 && t->name && strcmp(t->name, "imagemap") == 0) {
		postprocess_imagemap_ext_inner(t, cfg, accum, page);
		return;
	}

	bool is_td_inner= strcmp(t->type_name, "td-inner") == 0 || strcmp(t->type_name, "table-inter") == 0;
	bool is_ext_inner= strcmp(t->type_name, "ext-inner") == 0;
	bool is_heading_title= strcmp(t->type_name, "heading-title") == 0;

	ThreadBuf *scratch= wiki_thread_buf_acquire_scratch();

	bool has_non_text= false;
	size_t txt_len= 0;
	for(size_t i= 0; i < t->child_count; i++) {
		if(!t->children[i].is_text) {
			has_non_text= true;
		} else {
			txt_len+= t->children[i].text_len;
		}
	}
	if(txt_len == 0) {
		wiki_thread_buf_release_scratch(scratch);
		return;
	}

		if(has_non_text) {
			if(is_td_inner || is_ext_inner || is_heading_title) {
				size_t ser_cap= txt_len + 64;
				ThreadBuf *tmp_ser = wiki_thread_buf_acquire_scratch();
				if(tmp_ser) {
					wiki_thread_buf_reserve(tmp_ser, ser_cap);
					size_t ser_len= 0;
					bool serializable= true;

					for(size_t i= 0; i < t->child_count; i++) {
						Child cur= t->children[i];
						if(cur.is_text) {
							if(ser_len + cur.text_len + 1 >= tmp_ser->cap) {
								wiki_thread_buf_reserve(tmp_ser, (ser_len + cur.text_len + 1) * 2);
							}
							sz_copy(tmp_ser->buf + ser_len, cur.text, cur.text_len);
							ser_len+= cur.text_len;
							tmp_ser->len = ser_len;
							continue;
						}

						Token *ctok= cur.token;
						size_t tok_idx= SIZE_MAX;
						for(size_t ai= 0; ai < accum->count; ai++) {
							if(accum->tokens[ai] == ctok) {
								tok_idx= ai;
								break;
							}
						}
						char sym= token_sentinel_char(ctok ? ctok->type : TOKEN_TEXT);
						if(tok_idx == SIZE_MAX || sym == '\0') {
							serializable= false;
							break;
						}

						char marker[64];
						size_t mlen= 0;
						work_str_sentinel(tok_idx, sym, marker, &mlen);

						if(ser_len + mlen + 1 >= tmp_ser->cap) {
							wiki_thread_buf_reserve(tmp_ser, (ser_len + mlen + 1) * 2);
						}
						sz_copy(tmp_ser->buf + ser_len, marker, mlen);
						ser_len+= mlen;
						tmp_ser->len = ser_len;
					}

					if(serializable) {
						/* Use the tmp_ser scratch directly for nested parsing and building. */
						run_nested_plain_pipeline(tmp_ser, is_td_inner, is_ext_inner, is_heading_title, t, cfg, accum, page);

						Token *tmp= token_new(TOKEN_PLAIN, t->type_name);
						if(tmp) {
							build_from_str(tmp, tmp_ser->buf, tmp_ser->len, accum);
							build_token_recursive(tmp, accum, cfg);

							for(size_t i= 0; i < t->child_count; i++) {
								if(t->children[i].is_text && t->children[i].text_owned && t->children[i].text) free((void*)t->children[i].text);
							}
							free(t->children);

							t->children= tmp->children;
							t->child_count= tmp->child_count;
							t->child_cap= tmp->child_cap;

							tmp->children= NULL;
							tmp->child_count= 0;
							tmp->child_cap= 0;
							token_free_shallow(tmp);

							for(size_t i= 0; i < t->child_count; i++) {
								if(!t->children[i].is_text && t->children[i].token) {
									postprocess_nested_plain(t->children[i].token, cfg, accum, page);
								}
							}

							wiki_thread_buf_release_scratch(tmp_ser);
							wiki_thread_buf_release_scratch(scratch);
							return;
						}
						/* If build failed, fall through to heap fallback by releasing tmp_ser. */
						wiki_thread_buf_release_scratch(tmp_ser);
					} else {
						/* tmp_ser couldn't be populated cleanly; fall back to heap-based serializing. */
						wiki_thread_buf_release_scratch(tmp_ser);
					}
				}
				/* acquisition failure is fatal — do not fall back to heap */
				log_fatal("postprocess_nested_plain: failed to acquire scratch for serializing");
				abort();
			}

		Child *old_children= t->children;
		size_t old_count= t->child_count;
		size_t new_cap= old_count ? old_count : 1;
		Child *new_children= malloc(new_cap * sizeof(Child));
		if(!new_children) {
			wiki_thread_buf_release_scratch(scratch);
			return;
		}
		size_t new_count= 0;

		for(size_t i= 0; i < old_count; i++) {
			Child cur= old_children[i];
			if(!cur.is_text) {
				if(new_count >= new_cap) {
					new_cap*= 2;
					Child *grown= realloc(new_children, new_cap * sizeof(Child));
					assert(grown);
					new_children= grown;
				}
				new_children[new_count++]= cur;
				continue;
			}

			const char *txt= cur.text;
			size_t cur_len= cur.text_len;
			ThreadBuf *tmp_tb = wiki_thread_buf_acquire_scratch_from_data(txt, cur_len);
			if(!tmp_tb) {
				log_fatal("postprocess_nested_plain: failed to acquire scratch for fragment");
				abort();
			}
			run_nested_plain_pipeline(tmp_tb, is_td_inner, is_ext_inner, is_heading_title, t, cfg, accum, page);

			const char *used_buf = tmp_tb->buf;
			size_t used_len = tmp_tb->len;
			bool unchanged = (used_len == cur_len && sz_equal(used_buf, txt, cur_len));
			const char _zn1 = '\0';
			bool has_marker = sz_find_byte(used_buf, used_len, &_zn1) != NULL;
			if(unchanged && !has_marker) {
				if(new_count >= new_cap) {
					new_cap*= 2;
					Child *grown= realloc(new_children, new_cap * sizeof(Child));
					assert(grown);
					new_children= grown;
				}
				new_children[new_count++]= cur;
				if(tmp_tb) wiki_thread_buf_release_scratch(tmp_tb);
				continue;
			}

			if(cur.text_owned && cur.text) free((void*)cur.text);

			Token *tmp= token_new(TOKEN_PLAIN, t->type_name);
			if(!tmp) {
				if(tmp_tb) wiki_thread_buf_release_scratch(tmp_tb);
				log_fatal("postprocess_nested_plain: token_new() returned NULL");
				abort();
			}

			build_from_str(tmp, used_buf, used_len, accum);
			build_token_recursive(tmp, accum, cfg);

			for(size_t j= 0; j < tmp->child_count; j++) {
				if(new_count >= new_cap) {
					new_cap*= 2;
					Child *grown= realloc(new_children, new_cap * sizeof(Child));
					assert(grown);
					new_children= grown;
				}
				new_children[new_count++]= tmp->children[j];
			}

			free(tmp->children);
			tmp->children= NULL;
			tmp->child_count= 0;
			tmp->child_cap= 0;
			token_free_shallow(tmp);
				if(tmp_tb) wiki_thread_buf_release_scratch(tmp_tb);
		}

		free(old_children);
		t->children= new_children;
		t->child_count= new_count;
		t->child_cap= new_cap;
		for(size_t i= 0; i < t->child_count; i++) {
			if(!t->children[i].is_text && t->children[i].token) {
				postprocess_nested_plain(t->children[i].token, cfg, accum, page);
			}
		}
		wiki_thread_buf_release_scratch(scratch);
		return;
	}

	/* Assemble joined child text into the existing scratch buffer. */
	wiki_thread_buf_reserve(scratch, txt_len + 1);
	size_t pos= 0;
	for(size_t i= 0; i < t->child_count; i++) {
		sz_copy(scratch->buf + pos, t->children[i].text, t->children[i].text_len);
		pos+= t->children[i].text_len;
	}
	scratch->len = pos;
	scratch->buf[pos]= '\0';

	/* Try to run the nested pipeline on a fresh scratch copy so we can
	 * compare the processed result to the original joined text. */
	ThreadBuf *tmp_tb = wiki_thread_buf_acquire_scratch_from_data(scratch->buf, txt_len);
	if(!tmp_tb) {
		/* Fallback to heap-based join (matches original behavior). */
		char *joined= malloc(txt_len + 1);
		if(!joined) {
			wiki_thread_buf_release_scratch(scratch);
			return;
		}
		size_t p2= 0;
		for(size_t i= 0; i < t->child_count; i++) {
			sz_copy(joined + p2, t->children[i].text, t->children[i].text_len);
			p2+= t->children[i].text_len;
		}
		joined[txt_len]= '\0';

		wiki_thread_buf_set(scratch, joined, txt_len);
		run_nested_plain_pipeline(scratch, is_td_inner, is_ext_inner, is_heading_title, t, cfg, accum, page);

		if(scratch->len == txt_len && sz_equal(scratch->buf, joined, txt_len)) {
			free(joined);
			wiki_thread_buf_release_scratch(scratch);
			return;
		}
		build_from_str(t, scratch->buf, scratch->len, accum);
		build_token_recursive(t, accum, cfg);
		for(size_t i= 0; i < t->child_count; i++) {
			if(!t->children[i].is_text && t->children[i].token) {
				postprocess_nested_plain(t->children[i].token, cfg, accum, page);
			}
		}
		free(joined);
		wiki_thread_buf_release_scratch(scratch);
		return;
	}

	run_nested_plain_pipeline(tmp_tb, is_td_inner, is_ext_inner, is_heading_title, t, cfg, accum, page);

	if(tmp_tb->len == txt_len && sz_equal(tmp_tb->buf, scratch->buf, txt_len)) {
		wiki_thread_buf_release_scratch(tmp_tb);
		wiki_thread_buf_release_scratch(scratch);
		return;
	}
	build_from_str(t, tmp_tb->buf, tmp_tb->len, accum);
	build_token_recursive(t, accum, cfg);
	for(size_t i= 0; i < t->child_count; i++) {
		if(!t->children[i].is_text && t->children[i].token) {
			postprocess_nested_plain(t->children[i].token, cfg, accum, page);
		}
	}
	wiki_thread_buf_release_scratch(tmp_tb);
	wiki_thread_buf_release_scratch(scratch);
}

static void postprocess_root_braces_fallback(Token *root, const ParserConfig *cfg, Accum *accum) {
	if(!root || root->type != TOKEN_ROOT) return;
	if(root->child_count != 1 || !root->children[0].is_text) return;

	const char *txt= root->children[0].text;
	size_t txt_len= root->children[0].text_len;
	if(!txt || txt_len == 0 || sz_find(txt, txt_len, "{{", 2) == NULL) return;

	ThreadBuf *scratch = wiki_thread_buf_acquire_scratch_from_data(txt, txt_len);
	parse_braces(scratch, cfg, accum);

	if(scratch->len == txt_len && sz_equal(scratch->buf, txt, txt_len)) {
		wiki_thread_buf_release_scratch(scratch);
		return;
	}
	build_from_str(root, scratch->buf, scratch->len, accum);
	wiki_thread_buf_release_scratch(scratch);
}

typedef enum {
	ATTR_VALUE_PARSE_NONE= 0,
	ATTR_VALUE_PARSE_CONVERTER_ONLY,
	ATTR_VALUE_PARSE_RICH_INLINE,
} AttrValueParseMode;

static AttrValueParseMode classify_attr_value_parse_mode(const Token *parent,
																								 const Token *grandparent) {
	if(!parent || !grandparent || parent->type != TOKEN_EXT_ATTR) return ATTR_VALUE_PARSE_NONE;
	if(!parent->name || !grandparent->name || !grandparent->type_name) return ATTR_VALUE_PARSE_NONE;

	if(strcmp(grandparent->type_name, "ext-attrs") != 0 &&
		 strcmp(grandparent->type_name, "html-attrs") != 0 &&
		 strcmp(grandparent->type_name, "table-attrs") != 0) {
		return ATTR_VALUE_PARSE_NONE;
	}

	const char *key= parent->name;
	const char *tag= grandparent->name;

	if(strcmp(key, "title") == 0 || (strcmp(tag, "img") == 0 && strcmp(key, "alt") == 0)) {
		return ATTR_VALUE_PARSE_CONVERTER_ONLY;
	}

	if((strcmp(tag, "gallery") == 0 && strcmp(key, "caption") == 0) ||
		 (strcmp(tag, "ref") == 0 && strcmp(key, "details") == 0) ||
		 ((strcmp(tag, "mapframe") == 0 || strcmp(tag, "maplink") == 0) && strcmp(key, "text") == 0) ||
		 (strcmp(tag, "choose") == 0 && (strcmp(key, "before") == 0 || strcmp(key, "after") == 0))) {
		return ATTR_VALUE_PARSE_RICH_INLINE;
	}

	return ATTR_VALUE_PARSE_NONE;
}

static void postprocess_parameter_value_inline_impl(Token *t, const ParserConfig *cfg, Accum *accum,
																				const char *page, const Token *parent,
																				const Token *grandparent) {
	if(!t) return;

	log_debug_env_token("DEBUG_PARAM_VALUE", t, "postprocess_parameter_value_inline_impl start");

	for(size_t i= 0; i < t->child_count; i++) {
		if(!t->children[i].is_text && t->children[i].token) {
			postprocess_parameter_value_inline_impl(t->children[i].token, cfg, accum, page, t, parent);
		}
	}

	bool is_parameter_value= false;
	bool is_arg_default= false;
	bool is_attr_value= (t->type == TOKEN_ATTR_VALUE);
	AttrValueParseMode attr_mode= ATTR_VALUE_PARSE_NONE;

	if(is_attr_value) {
		attr_mode= classify_attr_value_parse_mode(parent, grandparent);
		if(attr_mode == ATTR_VALUE_PARSE_NONE) {
			return;
		}
	}

	if(!is_attr_value) {
		if(t->type != TOKEN_PLAIN || !t->type_name) {
			return;
		}
		is_parameter_value= strcmp(t->type_name, "parameter-value") == 0;
		is_arg_default= strcmp(t->type_name, "arg-default") == 0;
		if(!is_parameter_value && !is_arg_default) {
			return;
		}
	}

	ThreadBuf *scratch= wiki_thread_buf_acquire_scratch();

	Child *old_children= t->children;
	size_t old_count= t->child_count;
	size_t new_cap= old_count ? old_count : 1;
	Child *new_children= malloc(new_cap * sizeof(Child));
	if(!new_children) {
		wiki_thread_buf_release_scratch(scratch);
		return;
	}
	size_t new_count= 0;

	for(size_t i= 0; i < old_count; i++) {
		Child cur= old_children[i];

		if(!cur.is_text) {
			if(new_count >= new_cap) {
				new_cap*= 2;
				Child *grown= realloc(new_children, new_cap * sizeof(Child));
				assert(grown);
				new_children= grown;
			}
			new_children[new_count++]= cur;
			continue;
		}

		const char *txt= cur.text;
		size_t txt_len= cur.text_len;
		wiki_thread_buf_set(scratch, txt, txt_len);

		if(is_attr_value) {
			if(attr_mode == ATTR_VALUE_PARSE_RICH_INLINE) {
				parse_braces(scratch, cfg, accum);
				bool has_bang_sentinel= mem_has(scratch->buf, scratch->len, "!\x7F");
				if(!has_bang_sentinel) {
					parse_links(scratch, cfg, accum, page, false);
					parse_quotes_stage6_per_line(scratch, cfg, accum);
					parse_external_links(scratch, cfg, accum, false);
					parse_magic_links(scratch, cfg, accum);
				}
				parse_converter(scratch, cfg, accum);
			} else if(attr_mode == ATTR_VALUE_PARSE_CONVERTER_ONLY) {
				parse_converter(scratch, cfg, accum);
			}
		} else {
			parse_comment_and_ext(scratch, cfg, accum, false);
			parse_braces(scratch, cfg, accum);
			parse_html(scratch, cfg, accum);
			parse_table(scratch, cfg, accum);
			parse_hr_and_double_underscore(scratch, cfg, accum, TOKEN_PLAIN, is_attr_value ? "attr-value" : "parameter-value");
			bool has_bang_sentinel= mem_has(scratch->buf, scratch->len, "!\x7F");
			if(!has_bang_sentinel) {
				parse_links(scratch, cfg, accum, page, false);
				parse_quotes_stage6_per_line(scratch, cfg, accum);
				parse_external_links(scratch, cfg, accum, false);
				parse_magic_links(scratch, cfg, accum);
				parse_list_skip_first_line(scratch, cfg, accum);
				parse_converter(scratch, cfg, accum);
			}
		}


		bool unchanged = (scratch->len == txt_len && sz_equal(scratch->buf, txt, txt_len));
		const char _zn2 = '\0';
		bool has_marker = sz_find_byte(scratch->buf, scratch->len, &_zn2) != NULL;
			if(unchanged && !has_marker) {
				if(new_count >= new_cap) {
					new_cap*= 2;
					Child *grown= realloc(new_children, new_cap * sizeof(Child));
					assert(grown);
					new_children= grown;
				}
				new_children[new_count++]= cur;
				continue;
			}

			if(cur.text_owned && cur.text) free((void*)cur.text);

		Token *tmp= token_new(is_attr_value ? TOKEN_ATTR_VALUE : TOKEN_PLAIN,
			is_attr_value ? "attr-value" : t->type_name);
		if(!tmp) {
			wiki_thread_buf_release_scratch(scratch);
			log_fatal("postprocess_parameter_value_inline_impl: token_new() returned NULL");
			abort();
		}

		build_from_str(tmp, scratch->buf, scratch->len, accum);
		build_token_recursive(tmp, accum, cfg);

		for(size_t j= 0; j < tmp->child_count; j++) {
			if(new_count >= new_cap) {
				new_cap*= 2;
				Child *grown= realloc(new_children, new_cap * sizeof(Child));
				assert(grown);
				new_children= grown;
			}
			new_children[new_count++]= tmp->children[j];
		}

		free(tmp->children);
		tmp->children= NULL;
		tmp->child_count= 0;
		tmp->child_cap= 0;
		token_free_shallow(tmp);
	}

	free(old_children);
	t->children= new_children;
	t->child_count= new_count;
	t->child_cap= new_cap;
	wiki_thread_buf_release_scratch(scratch);

	/* JS parity: any sub-token (e.g. ExtToken with ext-inner) that was
     * built from this parameter-value text must run the nested-plain pass
     * so its ext-inner content goes through stages 5..10 just like JS
     * Token.parseOnce would do for tokens added to the accum. */
	for(size_t i= 0; i < t->child_count; i++) {
		if(!t->children[i].is_text && t->children[i].token) {
			postprocess_nested_plain(t->children[i].token, cfg, accum, page);
		}
	}
	log_debug_env_token("DEBUG_PARAM_VALUE", t, "postprocess_parameter_value_inline_impl end");
}

static void postprocess_parameter_value_inline(Token *t, const ParserConfig *cfg, Accum *accum,
																			const char *page) {
	postprocess_parameter_value_inline_impl(t, cfg, accum, page, NULL, NULL);
}

static void finalize_gallery_and_link_names(Token *t, const ParserConfig *cfg,
																			const char *page) {
	if(!t || !cfg) return;

	for(size_t i= 0; i < t->child_count; i++) {
		if(!t->children[i].is_text && t->children[i].token) {
			finalize_gallery_and_link_names(t->children[i].token, cfg, page);
		}
	}

	if(t->type == TOKEN_EXT_INNER && t->name && strcmp(t->name, "gallery") == 0) {
		bool has_leading_empty= (t->child_count > 0 && t->children[0].is_text && t->children[0].text_len == 0);
		if(!has_leading_empty) {
			if(t->child_count >= t->child_cap) {
				t->child_cap= t->child_cap ? t->child_cap * 2 : 4;
				t->children= realloc(t->children, t->child_cap * sizeof(Child));
				assert(t->children);
			}
			memmove(t->children + 1, t->children, t->child_count * sizeof(Child));
			Child *c= &t->children[0];
			c->is_text= true;
			c->text_len= 0;
			c->text= strdup("");
			c->token= NULL;
			t->child_count++;
		}
	}

	if((t->type == TOKEN_LINK || t->type == TOKEN_FILE || t->type == TOKEN_CATEGORY) && (!t->name || t->name[0] == '\0')) {
		if(t->child_count > 0 && !t->children[0].is_text && t->children[0].token) {
			Token *target= t->children[0].token;
			if(target->child_count > 0) {
				ThreadBuf *name_buf= wiki_thread_buf_acquire_scratch();
				const char *raw= NULL;
				size_t raw_len= 0;
				if(name_buf) {
					token_to_string(target, name_buf);
					raw= name_buf->buf;
					raw_len= name_buf->len;
				}
				int def_ns= (t->type == TOKEN_FILE) ? 6 : (t->type == TOKEN_CATEGORY ? 14 : 0);
				Title *tt= raw ? title_parse_half_parsed(raw, raw_len, def_ns, cfg, true, page) : NULL;
				if(tt && tt->valid && tt->title) {
					free(t->name);
					t->name= strdup(tt->title);
				} else if(raw && raw_len > 0) {
					char *norm= title_normalize(raw, raw_len);
					if(norm) {
						free(t->name);
						t->name= norm;
					}
				}
				title_free(tt);
				if(name_buf) wiki_thread_buf_release_scratch(name_buf);
			}
		}
	}
}

static void parse_quotes_stage6_per_line(ThreadBuf *ws, const ParserConfig *cfg, Accum *accum) {
	if(!ws || !ws->buf) return;

	ThreadBuf *scratch= wiki_thread_buf_acquire_scratch();

	size_t out_cap= ws->len * 2 + 64;
	char *out= malloc(out_cap);
	assert(out);
	size_t out_len= 0;

	size_t line_start= 0;
	while(line_start < ws->len) {
		const char nl= '\n';
		const char *eol= sz_find_byte(ws->buf + line_start, ws->len - line_start, &nl);
		size_t line_len = eol ? (size_t)(eol - (ws->buf + line_start)) : ws->len - line_start;
		wiki_thread_buf_set(scratch, ws->buf + line_start, line_len);
		parse_quotes(scratch, cfg, accum, false);

		while(out_len + scratch->len + 2 >= out_cap) {
			out_cap*= 2;
			out= realloc(out, out_cap);
			assert(out);
		}
		if(scratch->len > 0) {
			sz_copy(out + out_len, scratch->buf, scratch->len);
			out_len+= scratch->len;
		}
		if(eol) {
			out[out_len++]= '\n';
		}

		line_start= eol ? (size_t)(eol - ws->buf) + 1 : ws->len;
	}

	out[out_len]= '\0';
	wiki_thread_buf_set(ws, out, out_len);

	free(out);
	wiki_thread_buf_release_scratch(scratch);
}

static void stage1_parse_braces_on_accum(const ParserConfig *cfg, Accum *accum) {
	if(!cfg || !accum) return;

	for(size_t ai= 0; ai < accum->count; ai++) {
		Token *tok= accum->tokens[ai];
		if(!tok) continue;
		if(tok->type != TOKEN_EXT_INNER || !ext_inner_allows_nested_parse(tok->name)) continue;

		/* JS parseOnce parity: only plain single-text tokens are reparsed. */
		if(tok->child_count != 1 || !tok->children[0].is_text) continue;

		const char *txt= tok->children[0].text;
		size_t txt_len= tok->children[0].text_len;
		if(!txt || txt_len == 0 || sz_find(txt, txt_len, "{{", 2) == NULL) continue;

		ThreadBuf *scratch = wiki_thread_buf_acquire_scratch();
		if(!scratch) continue;
		wiki_thread_buf_reserve(scratch, txt_len);
		sz_copy(scratch->buf, txt, txt_len);
		scratch->buf[txt_len]= '\0';
		scratch->len= txt_len;

		parse_braces(scratch, cfg, accum);
		if(!(scratch->len == txt_len && sz_equal(scratch->buf, txt, txt_len))) {
			build_from_str(tok, scratch->buf, scratch->len, accum);
		}
		wiki_thread_buf_release_scratch(scratch);
	}
}

static void stage0_parse_comment_and_ext_on_accum(const ParserConfig *cfg, Accum *accum) {
	if(!cfg || !accum) return;

	for(size_t ai= 0; ai < accum->count; ai++) {
		Token *tok= accum->tokens[ai];
		if(!tok) continue;
		if(tok->type != TOKEN_EXT_INNER || !ext_inner_allows_nested_parse(tok->name)) continue;

		/* JS parseOnce parity: only plain single-text tokens are reparsed. */
		if(tok->child_count != 1 || !tok->children[0].is_text) continue;

		const char *txt= tok->children[0].text;
		size_t txt_len= tok->children[0].text_len;
		if(!txt || txt_len == 0) continue;
		const char _lt = '<';
		if(sz_find_byte(txt, txt_len, &_lt) == NULL) continue;

		ThreadBuf *scratch = wiki_thread_buf_acquire_scratch();
		if(!scratch) continue;
		wiki_thread_buf_reserve(scratch, txt_len);
		sz_copy(scratch->buf, txt, txt_len);
		scratch->buf[txt_len]= '\0';
		scratch->len= txt_len;

		parse_comment_and_ext(scratch, cfg, accum, false);
		if(!(scratch->len == txt_len && sz_equal(scratch->buf, txt, txt_len))) {
			char *repl= malloc(scratch->len + 1);
			if(repl) {
				sz_copy(repl, scratch->buf, scratch->len);
				repl[scratch->len]= '\0';
				if(tok->children[0].text_owned && tok->children[0].text) free((void*)tok->children[0].text);
				tok->children[0].text= repl;
				tok->children[0].text_len= scratch->len;
				tok->children[0].text_owned = true;
			}
		}
		wiki_thread_buf_release_scratch(scratch);
	}
}

Token *wiki_parse_with_page(const char *wikitext, size_t input_len, const ParserConfig *cfg,
												 bool include, int max_stage,
												 const char *page) {
	if(!wikitext) {
		log_error("Wikitext is null");
		return NULL;
	}
	if(!cfg) {
		log_error("cfg is null");
		return NULL;
	}

	// /* Log StringZilla capabilities and dispatch info once when the parser is first used. */
	// static int sz_caps_logged = 0;
	// if(!sz_caps_logged) {
	// 	sz_caps_logged = 1;
	// 	sz_capability_t caps = sz_capabilities();
	// 	const char *caps_str = sz_capabilities_to_string(caps);
	// 	int dynamic = sz_dynamic_dispatch();

	// 	if(caps_str && *caps_str) {
	// 		/* Pick the final capability name as the best-guessed backend. */
	// 		const char *last = strrchr(caps_str, ',');
	// 		const char *backend = last && *(last + 1) ? last + 1 : caps_str;
	// 		log_info("StringZilla dynamic_dispatch=%d; capabilities: %s; chosen backend: %s", dynamic, caps_str, backend);
	// 	} else {
	// 		log_info("StringZilla dynamic_dispatch=%d; capabilities: (none)", dynamic);
	// 	}
	// }

	/* ── Grab a thread-local snapshot of the input ─────────────────────────
	 * The caller's string may be modified by another thread while we are
	 * executing.  We copy it into the thread's pre-allocated stage buffer
	 * (avoiding a per-call malloc) */
	ThreadBuffers *tbufs= wiki_thread_buf_get();

	/* Apply shrink/grow policy for this input size, then copy-and-tidy.
     * wiki_thread_buf_reserve() is the sole resize authority for ThreadBufs;
     * it guarantees the buffer can hold input_len+1 bytes before we hand
     * the pointer to str_tidy_into(), which never allocates. */
	wiki_thread_buf_reserve(&tbufs->stage, input_len);

	/* Pre-reserve the tokens arena to multiple times the input length and
	 * reset it. This is a temporary workaround to avoid reallocations of
	 * the tokens arena during parsing which would invalidate previously
	 * returned text pointers. The correct fix (see TODO.md) is to avoid
	 * appending sentinel-containing inner text into the arena. */
	if(tbufs->tokens.cap < input_len * 4 + 1)
		wiki_thread_buf_reserve(&tbufs->tokens, input_len * 4);
	tbufs->tokens.len= 0;

	size_t tidy_len= 0;
	str_tidy_into(wikitext, input_len, tbufs->stage.buf, tbufs->stage.cap, &tidy_len);
	tbufs->stage.len= tidy_len;

	/* ── Working string (mutated by each stage) ─────────────────────────── */
	ThreadBuf *ws= &tbufs->stage;

	/* Optional stage logging directory (set via env WIKI_STAGE_LOG_DIR). */
	const char *stage_log_dir = env_get("WIKI_STAGE_LOG_DIR");
	char runid[64]= "";
	if(stage_log_dir) {
		static int _run_counter= 0;
		_run_counter++;
		pid_t pid= getpid();
		long ts= (long)time(NULL);
		snprintf(runid, sizeof(runid), "%d-%ld-%d", (int)pid, ts, _run_counter);
		/* try to create directory if it doesn't exist */
		if(mkdir(stage_log_dir, 0777) != 0 && errno != EEXIST) {
			/* non-fatal; best-effort */
		}
	}

	/* ── Accumulator (holds extracted tokens) ─────────────────────────── */
	Accum accum;
	accum_init(&accum);

	/* ── Create root token ────────────────────────────────────────────────── */
	Token *root= token_new(TOKEN_ROOT, "root");
	if(!root) {
		/* no-op: thread buffer owned by TLS */
		accum_free(&accum);
		return NULL;
	}
	root->stage= -1;
	root->include= include;

	/* ── Run stages 0 .. max_stage ─────────────────────────────────────── */
	for(int stage= 0; stage <= max_stage && stage <= 10; stage++) {
		switch(stage) {
		case 0:
			/* parseRedirect only runs on the root token */
			parse_redirect(ws, cfg, &accum);
			/* parseCommentAndExt always runs at stage 0 */
			parse_comment_and_ext(ws, cfg, &accum, include);
			stage0_parse_comment_and_ext_on_accum(cfg, &accum);
			break;

		/* Stage 1: parseBraces */
		case 1:
			parse_braces(ws, cfg, &accum);
			stage1_parse_braces_on_accum(cfg, &accum);
			break;

		case 2: /* parseHtml */
			parse_html(ws, cfg, &accum);
			break;
		case 3: /* parseTable */
			parse_table(ws, cfg, &accum);
			break;
		case 4: /* parseHrAndDoubleUnderscore */
			parse_hr_and_double_underscore(ws, cfg, &accum, TOKEN_ROOT, "root");
			break;
		case 5: /* parseLinks */
			parse_links(ws, cfg, &accum, page, false);
			break;
		case 6: /* parseQuotes */
			parse_quotes_stage6_per_line(ws, cfg, &accum);
			break;
		case 7: /* parseExternalLinks */
			parse_external_links(ws, cfg, &accum, false);
			break;
		case 8: /* parseMagicLinks */
			parse_magic_links(ws, cfg, &accum);
			break;
		case 9: /* parseList */
			parse_list(ws, cfg, &accum);
			break;
		case 10: /* parseConverter */
			parse_converter(ws, cfg, &accum);
			break;
		}

		/* If stage logging enabled, append a JSON snapshot to native-stage.log */
		if(stage_log_dir) {
			append_native_stage_json(stage_log_dir, stage, ws, &accum);
		}
	}

	/* ── build phase 1: expand root-level sentinels into the tree ───────── */
	build_from_str(root, ws->buf, ws->len, &accum);

	/* JS parity: run inline stages (parse_links etc.) on parameter-value raw
     * text BEFORE build_token_recursive expands sub-token sentinels.
     * In JS, parseOnce(stage) is called on every accum token while each
     * parameter-value still has its single raw text child containing embedded
     * sentinels (e.g. \0Nt\x7F for a nested template).  parse_links can then
     * see the full "[[Target|sentinel]]" as an unbroken string and produce the
     * correct link token.  If we wait until after build_token_recursive the
     * sentinel has already been replaced by a real token child, splitting the
     * text that parse_links needs to match.
     * Walk accum directly (JS parity: JS calls parseOnce(n) on every accum
     * token, not just root-reachable tokens). This ensures we also process
     * parameter-value tokens embedded inside sentinels of tokens not yet
     * linked into the root tree (e.g. templates inside table-attr-dirty). */
	for(size_t _ai= 0; _ai < accum.count; _ai++) {
		if(accum.tokens[_ai]) {
			postprocess_parameter_value_inline(accum.tokens[_ai], cfg, &accum, page);
		}
	}

	/* ── build phase 2: recursively expand remaining sentinels ───────────── */
	build_token_recursive(root, &accum, cfg);

	/* JS parity: AttributesToken.afterBuild() sets table-attrs name to the
     * cell subtype (td/th/caption), including sibling inheritance for inline
     * continuation cells (||/!!). Must run AFTER full build. */
	propagate_table_subtypes(root);

	/* JS parity for nested plain regions that still contain parseable syntax. */
	postprocess_nested_plain(root, cfg, &accum, page);
	postprocess_root_braces_fallback(root, cfg, &accum);

	/* JS parity: run inline stages again for any new text children created
     * during build_token_recursive (e.g. ext-inner content). */
	postprocess_parameter_value_inline(root, cfg, &accum, page);
	finalize_gallery_and_link_names(root, cfg, page);

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

Token *wiki_parse(const char *wikitext, size_t input_len, const ParserConfig *cfg,
									bool include, int max_stage) {
	return wiki_parse_with_page(wikitext, input_len, cfg, include, max_stage, NULL);
}
