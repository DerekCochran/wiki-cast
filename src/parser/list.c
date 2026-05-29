#include "util/log.h"
#include "parser/list.h"
#include "util/string_util.h"
#include "stringzilla/stringzilla.h"
#include "token.h"
#include "util/thread_buffer.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── List-prefix helpers (replace regex ^((?:\x00\d+[cno]\x7F)*)([;:*#]+)(\s*)) */
typedef struct {
	const char *sentinels;
	size_t      sentinels_len;
	const char *markers;
	size_t      markers_len;
	const char *trailing_ws;
	size_t      trailing_ws_len;
} ListPrefixResult;

typedef enum {
	FULL_MATCH_COLON     = 0,
	FULL_MATCH_CONVERTER = 1,
	FULL_MATCH_SENTINEL  = 2,
} FullMatchKind;

typedef struct {
	FullMatchKind kind;
	size_t        pos;
	size_t        len;
	size_t        colon_count;   /* FULL_MATCH_COLON only  */
	size_t        sentinel_n;    /* FULL_MATCH_SENTINEL    */
	char          sentinel_type; /* 'x' or 'q'             */
} FullMatch;

/* JS parity helper for Unicode Zs consumed by the JS list-prefix whitespace regex. */
static size_t consume_js_zs_list(const char *s, size_t len, size_t i) {
	if(i >= len) return 0;
	unsigned char c0 = (unsigned char)s[i];
	if(c0 == 0x20) return 1; /* U+0020 */
	if(i + 1 < len && c0 == 0xC2 && (unsigned char)s[i + 1] == 0xA0) return 2; /* U+00A0 */
	if(i + 2 < len && c0 == 0xEF && (unsigned char)s[i + 1] == 0xBB && (unsigned char)s[i + 2] == 0xBF) return 3; /* U+FEFF */
	if(i + 2 < len && c0 == 0xE1 && (unsigned char)s[i + 1] == 0x9A && (unsigned char)s[i + 2] == 0x80) return 3; /* U+1680 */
	if(i + 2 < len && c0 == 0xE2 && (unsigned char)s[i + 1] == 0x80 &&
	   (unsigned char)s[i + 2] >= 0x80 && (unsigned char)s[i + 2] <= 0x8A) return 3; /* U+2000..U+200A */
	if(i + 2 < len && c0 == 0xE2 && (unsigned char)s[i + 1] == 0x80 &&
	   ((unsigned char)s[i + 2] == 0xA8 || (unsigned char)s[i + 2] == 0xA9)) return 3; /* U+2028/U+2029 */
	if(i + 2 < len && c0 == 0xE2 && (unsigned char)s[i + 1] == 0x80 && (unsigned char)s[i + 2] == 0xAF) return 3; /* U+202F */
	if(i + 2 < len && c0 == 0xE2 && (unsigned char)s[i + 1] == 0x81 && (unsigned char)s[i + 2] == 0x9F) return 3; /* U+205F */
	if(i + 2 < len && c0 == 0xE3 && (unsigned char)s[i + 1] == 0x80 && (unsigned char)s[i + 2] == 0x80) return 3; /* U+3000 */
	return 0;
}

static size_t consume_list_space(const char *s, size_t len, size_t i) {
	if(i >= len) return 0;
	unsigned char c = (unsigned char)s[i];
	if(c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v') return 1;
	return consume_js_zs_list(s, len, i);
}

/* Find the first match of  :+ | -{ | \x00\d+[xq]\x7F  starting at or after
 * `start_pos` in `buf`. Returns true and fills *out on success. */
static bool full_scan_first(const char *buf, size_t len, size_t start_pos, FullMatch *out) {
	if(!buf || start_pos >= len || !out) return false;
	size_t cur = start_pos;
	/* candidate bytes: ':', '-', '\0' */
	char cand[3]; cand[0]=':'; cand[1]='-'; cand[2]='\0';
	while(cur < len) {
		const char *found = sz_find_byte_from(buf + cur, len - cur, cand, 3);
		if(!found) return false;
		size_t p = (size_t)(found - buf);
		unsigned char c = (unsigned char)buf[p];

		if(c == ':') {
			/* count run of ':' */
			size_t i = p;
			while(i < len && buf[i] == ':') i++;
			out->kind = FULL_MATCH_COLON;
			out->pos = p;
			out->len = i - p;
			out->colon_count = out->len;
			out->sentinel_n = 0;
			out->sentinel_type = '\0';
			return true;
		}

		if(c == '-') {
			/* check for '-{' */
			if(p + 1 < len && buf[p + 1] == '{') {
				out->kind = FULL_MATCH_CONVERTER;
				out->pos = p;
				out->len = 2;
				out->colon_count = 0;
				out->sentinel_n = 0;
				out->sentinel_type = '\0';
				return true;
			}
			/* not a match; continue scanning after this byte */
			cur = p + 1;
			continue;
		}

		if(c == '\0') {
			/* try to parse sentinel starting at p: \0 <digits> <type> \x7F */
			size_t j = p + 1;
			if(j >= len) { cur = p + 1; continue; }
			if(!(buf[j] >= '0' && buf[j] <= '9')) { cur = p + 1; continue; }
			size_t k = j;
			while(k < len && buf[k] >= '0' && buf[k] <= '9') k++;
			if(k >= len) { cur = p + 1; continue; }
			char t = buf[k];
			if(k + 1 >= len) { cur = p + 1; continue; }
			if((unsigned char)buf[k + 1] != (unsigned char)0x7F) { cur = p + 1; continue; }
			/* Only accept type 'x' or 'q' for this pattern */
			if(!(t == 'x' || t == 'q')) { cur = p + 1; continue; }
			/* parse decimal number */
			size_t n = 0;
			for(size_t d = j; d < k; ++d) n = n * 10 + (size_t)(buf[d] - '0');
			out->kind = FULL_MATCH_SENTINEL;
			out->pos = p;
			out->len = (k + 2) - p;
			out->colon_count = 0;
			out->sentinel_n = n;
			out->sentinel_type = t;
			return true;
		}

		/* fallback: continue scanning after this character */
		cur = p + 1;
	}
	return false;
}

static size_t skip_cno_sentinel(const char *p, size_t remaining) {
	if(!p || remaining < 4) return 0;
	if((unsigned char)p[0] != '\0') return 0;
	size_t j = 1;
	const char *not_digit = sz_find_byte_not_from(p + j, remaining - j, "0123456789", 10);
	j = not_digit ? (size_t)(not_digit - p) : remaining;
	if(j == 1 || j + 1 >= remaining) return 0; /* need at least one digit and a type+DEL */
	char t = p[j];
	if(!(t == 'c' || t == 'n' || t == 'o')) return 0;
	if((unsigned char)p[j + 1] != '\x7F') return 0;
	return (j + 2); /* total bytes consumed: NUL + digits + type + DEL */
}

static bool list_prefix_parse(const char *line, size_t len, ListPrefixResult *out) {
	if(!line || len == 0 || !out) return false;
	size_t pos = 0;

	/* 1) consume zero-or-more c/n/o sentinels */
	while(pos < len) {
		size_t s = skip_cno_sentinel(line + pos, len - pos);
		if(s == 0) break;
		pos += s;
	}

	/* 2) require at least one list marker */
	if(pos >= len) return false;
	const char *markers_start = line + pos;
	size_t mlen = 0;
	while(pos < len) {
		char ch = line[pos];
		if(!(ch == ';' || ch == ':' || ch == '*' || ch == '#')) break;
		pos++; mlen++;
	}
	if(mlen == 0) return false;

	/* 3) trailing whitespace with JS \s* semantics. */
	const char *ws_start = line + pos;
	size_t ws_len = 0;
	while(pos < len) {
		size_t ws = consume_list_space(line, len, pos);
		if(ws == 0) break;
		pos += ws;
		ws_len += ws;
	}

	out->sentinels = line;
	out->sentinels_len = (size_t)(markers_start - line);
	out->markers = markers_start;
	out->markers_len = mlen;
	out->trailing_ws = ws_start;
	out->trailing_ws_len = ws_len;
	return true;
}

/* Helper: compute common prefix length as per JS util/html.getCommon */
static size_t get_common_prefix_len(const char *prefix, size_t plen, const char *last, size_t last_len) {
	if(!last) return 0;
	if(last_len == 0) return 0;
	size_t common_len = plen < last_len ? plen : last_len;
	if(common_len > 0 && sz_equal(prefix, last, common_len) == sz_true_k) return common_len;
	for(size_t i= 0; i < common_len; i++) {
		if(prefix[i] != last[i]) return i;
	}
	return common_len;
}

/* Build a simple list token: stores the raw part text as a single text child */
static Token *make_list_token(const char *part, size_t part_len, Accum *accum) {
	Token *t= token_new(TOKEN_LIST, "list");
	if(!t) return NULL;
	if(part_len > 0) {
		token_append_text_n(t, part, part_len);
	} else {
		token_append_text_n(t, NULL, 0);
	}
	accum_push(accum, t);
	return t;
}

/* Build a dd token (definition item marker) */
static Token *make_dd_token(const char *syntax, size_t syntax_len, Accum *accum) {
	Token *t= token_new(TOKEN_DD, "dd");
	if(!t) return NULL;
	if(syntax_len > 0) {
		token_append_text_n(t, syntax, syntax_len);
	} else {
		token_append_text_n(t, NULL, 0);
	}
	accum_push(accum, t);
	return t;
}

/* split a string `s` of length `len` using the JS-style /(?=;)/ split: i.e.
 * split at positions where ';' appears, keeping the leading ';' on later parts.
 * Returns an array of allocated strings (caller frees), and sets out_count.
 */
static char **split_on_semicolon_lookahead_with_len(const char *s, size_t len,
																											 size_t *out_count,
																											 size_t **out_lens);

static char **split_on_semicolon_lookahead(const char *s, size_t len, size_t *out_count) {
	return split_on_semicolon_lookahead_with_len(s, len, out_count, NULL);
}

static char **split_on_semicolon_lookahead_with_len(const char *s, size_t len,
																											 size_t *out_count,
																											 size_t **out_lens) {
	size_t cap= 8, count= 0;
	char **parts= malloc(cap * sizeof(char *));
	size_t *lens= out_lens ? malloc(cap * sizeof(size_t)) : NULL;
	assert(parts);
	if(out_lens) assert(lens);
	size_t start= 0;
	for(size_t i= 0; i < len; i++) {
		if(s[i] == ';' && i != start) {
			size_t plen= i - start;
			char *p= malloc(plen + 1);
			assert(p);
			sz_copy(p, s + start, plen);
			p[plen]= '\0';
			if(count >= cap) {
				cap*= 2;
				parts= realloc(parts, cap * sizeof(char *));
				if(out_lens) lens= realloc(lens, cap * sizeof(size_t));
				assert(parts);
				if(out_lens) assert(lens);
			}
			parts[count++]= p;
			if(out_lens) lens[count - 1]= plen;
			start= i;
		}
	}
	/* last part */
	size_t plen= (len >= start) ? (len - start) : 0;
	char *p= malloc(plen + 1);
	assert(p);
	if(plen > 0) sz_copy(p, s + start, plen);
	p[plen]= '\0';
	if(count >= cap) {
		cap*= 2;
		parts= realloc(parts, cap * sizeof(char *));
		if(out_lens) lens= realloc(lens, cap * sizeof(size_t));
		assert(parts);
		if(out_lens) assert(lens);
	}
	parts[count++]= p;
	if(out_lens) lens[count - 1]= plen;
	*out_count= count;
	if(out_lens) *out_lens= lens;
	return parts;
}

/* free array returned by split_on_semicolon_lookahead */
static void free_parts(char **parts, size_t *lens, size_t count) {
	for(size_t i= 0; i < count; i++) free(parts[i]);
	free(parts);
	free(lens);
}

/* Main parse_list implementation */
void parse_list(ThreadBuf *tb, const ParserConfig *cfg, Accum *accum) {
	if(!tb || !tb->buf) return;
	if(config_excluded(cfg, "list")) return;

	/* Split tb->buf into lines (preserve empty final line semantics) */
	const char *buf= tb->buf;
	size_t blen= tb->len;

	/* Collect lines into array of (ptr,len) by scanning for '\n' */
	size_t line_cap= 32, line_count= 0;
	char **lines= malloc(line_cap * sizeof(char *));
	size_t *lines_len= malloc(line_cap * sizeof(size_t));
	assert(lines && lines_len);

	size_t pos= 0;
	while(pos <= blen) {
		size_t next= pos;
		while(next < blen && buf[next] != '\n') next++;
		size_t llen= next - pos;
		char *line= malloc(llen + 1);
		assert(line);
		if(llen) sz_copy(line, buf + pos, llen);
		line[llen]= '\0';
		if(line_count >= line_cap) {
			line_cap*= 2;
			lines= realloc(lines, line_cap * sizeof(char *));
			lines_len= realloc(lines_len, line_cap * sizeof(size_t));
			assert(lines && lines_len);
		}
		lines[line_count]= line;
		lines_len[line_count]= llen;
		line_count++;
		pos= (next < blen) ? next + 1 : next + 1; /* move past '\n' or end */
		if(next >= blen) break;
	}

	/* State: lastPrefix string (malloc'd when set) */
	char *lastPrefix= NULL;
	size_t lastPrefixLen= 0;

	/* Process each line (root: start at 0) */
	for(size_t li= 0; li < line_count; li++) {
		char *line= lines[li];
		size_t line_len= lines_len[li];

		/* Parse list prefix (leading sentinels, marker run, trailing ws) */
		ListPrefixResult lpr;
		if(!list_prefix_parse(line, line_len, &lpr)) {
			/* No match: reset lastPrefix and keep line unchanged */
			if(lastPrefix) {
				free(lastPrefix);
				lastPrefix= NULL;
				lastPrefixLen= 0;
			}
			continue;
		}

		const char *comment = lpr.sentinels;
		size_t comment_len = lpr.sentinels_len;
		const char *prefix = lpr.markers;
		size_t prefix_len = lpr.markers_len;
		const char *space = lpr.trailing_ws;
		size_t space_len = lpr.trailing_ws_len;
		size_t match_end = comment_len + prefix_len + space_len;

		/* Build prefix2 = prefix with ';' -> ':' */
		char *prefix2= malloc(prefix_len + 1);
		assert(prefix2);
		for(size_t i= 0; i < prefix_len; i++) prefix2[i]= (prefix[i] == ';') ? ':' : prefix[i];
		prefix2[prefix_len]= '\0';

		size_t common= get_common_prefix_len(prefix2, prefix_len, lastPrefix, lastPrefixLen);

		/* Build combined = ((common>1)?prefix.slice(common-1):prefix) + space */
		size_t start_slice= (common > 1) ? (common - 1) : 0;
		size_t combined_len= (prefix_len > start_slice ? prefix_len - start_slice : 0) + space_len;
		char *combined= malloc(combined_len + 1);
		assert(combined);
		if(prefix_len > start_slice) sz_copy(combined, prefix + start_slice, prefix_len - start_slice);
		if(space_len) sz_copy(combined + (prefix_len > start_slice ? prefix_len - start_slice : 0), space, space_len);
		combined[combined_len]= '\0';

		/* Split combined into parts using lookahead semicolon split */
		size_t parts_count= 0;
		size_t *parts_len= NULL;
		char **parts= split_on_semicolon_lookahead_with_len(combined, combined_len, &parts_count, &parts_len);

		bool isDt= (parts_count > 0 && parts[0][0] == ';');
		int dt= (int)parts_count - (isDt ? 0 : 1);

		/* If common > 1, handle commonPrefix adjustments */
		if(common > 1) {
			size_t cp_len= common - 1;
			char *commonPrefix= malloc(cp_len + 1);
			assert(commonPrefix);
			sz_copy(commonPrefix, prefix, cp_len);
			commonPrefix[cp_len]= '\0';
			if(isDt) {
				size_t cp_parts= 0;
				size_t *cp_lens= NULL;
				char **cp_list= split_on_semicolon_lookahead_with_len(commonPrefix, cp_len, &cp_parts, &cp_lens);
				/* prepend cp_list to parts */
				size_t new_count= parts_count + cp_parts;
				char **new_parts= malloc(new_count * sizeof(char *));
				size_t *new_lens= malloc(new_count * sizeof(size_t));
				assert(new_parts);
				assert(new_lens);
				size_t pi= 0;
				for(size_t k= 0; k < cp_parts; k++) { new_parts[pi]= cp_list[k]; new_lens[pi]= cp_lens[k]; pi++; }
				for(size_t k= 0; k < parts_count; k++) { new_parts[pi]= parts[k]; new_lens[pi]= parts_len[k]; pi++; }
				free(cp_list);
				free(cp_lens);
				free(parts);
				free(parts_len);
				parts= new_parts;
				parts_len= new_lens;
				parts_count= new_count;
				if(strchr(commonPrefix, ';')) dt+= (int)cp_parts;
			} else {
				/* prepend commonPrefix to first part */
				size_t a_len= parts_len[0];
				char *merged= malloc(cp_len + a_len + 1);
				assert(merged);
				sz_copy(merged, commonPrefix, cp_len);
				sz_copy(merged + cp_len, parts[0], a_len);
				merged[cp_len + a_len]= '\0';
				free(parts[0]);
				parts[0]= merged;
				parts_len[0]= cp_len + a_len;
			}
			free(commonPrefix);
		}

		/* Update lastPrefix */
		if(lastPrefix) free(lastPrefix);
		lastPrefix= prefix2; /* ownership transferred */
		lastPrefixLen= prefix_len;

		/* Build text = comment + sentinel markers for each part + rest-of-line */
		size_t base_idx= accum->count;
		/* compute approximate out length */
		size_t out_cap= comment_len + parts_count * 16 + (line_len - (match_end)) + 8;
		char *out= malloc(out_cap);
		assert(out);
		size_t out_len= 0;
		if(comment_len) {
			sz_copy(out + out_len, comment, comment_len);
			out_len+= comment_len;
		}

		/* Append sentinel markers for each part (indices base_idx + i) */
		for(size_t i= 0; i < parts_count; i++) {
			char marker[64];
			size_t mlen= 0;
			work_str_sentinel(base_idx + i, 'd', marker, &mlen);
			if(out_len + mlen + 1 > out_cap) {
				out_cap= (out_len + mlen + 1) * 2;
				out= realloc(out, out_cap);
				assert(out);
			}
			sz_copy(out + out_len, marker, mlen);
			out_len+= mlen;
		}
		/* Append rest of line after the matched prefix */
		size_t rest_len= (match_end < line_len) ? (line_len - match_end) : 0;
		if(out_len + rest_len + 1 > out_cap) {
			out_cap= out_len + rest_len + 1;
			out= realloc(out, out_cap);
			assert(out);
		}
		if(rest_len) sz_copy(out + out_len, line + match_end, rest_len);
		out_len+= rest_len;
		out[out_len]= '\0';

		/* Create ListToken for each part (order matters) */
		for(size_t i= 0; i < parts_count; i++) {
			make_list_token(parts[i], parts_len[i], accum);
		}

		if(dt == 0) {
			free(line);
			lines[li]= out; /* adopt out as new line */
			lines_len[li]= out_len;
			free_parts(parts, parts_len, parts_count);
			free(combined);
			continue;
		}

		/* dt > 0: need to handle ':' tokens that may become dd tokens.
		 * Manual scanner finds the next special item among:
		 *  - a run of ':' characters
		 *  - the sequence '-{' or '}-'
		 *  - a sentinel of type 'x' or 'q'
		 */
		size_t search_at = 0;
		int lt = 0;
		int lc = 0;
		int lb = 0;
		int li_flag = 0;

		while(search_at <= out_len && dt > 0) {
			/* find candidate positions for next specials (-{ and }-) */
			size_t next_nbrace_pos = SIZE_MAX; /* "-{" */
			size_t next_cbrace_pos = SIZE_MAX; /* "}-" */

			/* find next -{ sequence */
			for(size_t p = search_at; p + 1 < out_len; ) {
				const char *h = memchr(out + p, '-', out_len - p);
				if(!h) break;
				size_t ip = (size_t)(h - out);
				if(ip + 1 < out_len && out[ip + 1] == '{') { next_nbrace_pos = ip; break; }
				p = ip + 1;
			}
			/* find next }- sequence */
			for(size_t p = search_at; p + 1 < out_len; ) {
				const char *h = memchr(out + p, '}', out_len - p);
				if(!h) break;
				size_t ip = (size_t)(h - out);
				if(ip + 1 < out_len && out[ip + 1] == '-') { next_cbrace_pos = ip; break; }
				p = ip + 1;
			}

			enum { MT_NONE=0, MT_COLON, MT_NBRACE, MT_CBRACE, MT_SENTINEL } mt = MT_NONE;
			size_t mpos = SIZE_MAX;
			size_t sentinel_total_len = 0;
			size_t sentinel_idx = 0;
			char sentinel_type = 0;

			if(lc == 0) {
				/* Use scanner to locate the next colon run, -{, or sentinel */
				FullMatch fm;
				if(!full_scan_first(out, out_len, search_at, &fm)) break;
				switch(fm.kind) {
					case FULL_MATCH_COLON:
						mt = MT_COLON;
						mpos = fm.pos;
						break;
					case FULL_MATCH_CONVERTER:
						mt = MT_NBRACE;
						mpos = fm.pos;
						break;
					case FULL_MATCH_SENTINEL:
						mt = MT_SENTINEL;
						mpos = fm.pos;
						sentinel_total_len = fm.len;
						sentinel_idx = fm.sentinel_n;
						sentinel_type = fm.sentinel_type;
						break;
					default:
						break;
				}
			} else {
				if(next_nbrace_pos != SIZE_MAX) { mpos = next_nbrace_pos; mt = MT_NBRACE; }
				if(next_cbrace_pos != SIZE_MAX && (mt == MT_NONE || next_cbrace_pos < mpos)) { mpos = next_cbrace_pos; mt = MT_CBRACE; }
			}

			if(mt == MT_NONE) break;

			if(mt == MT_NBRACE) {
				size_t mend = mpos + 2; /* "-{" */
				if(!lc) {
					/* switching into brace mode */
				}
				lc++;
				search_at = mend;
				continue;
			}
			if(mt == MT_CBRACE) {
				size_t mend = mpos + 2; /* "}-" */
				if(lc > 0) lc--;
				search_at = mend;
				continue;
			}
			if(mt == MT_SENTINEL) {
				size_t mstart = mpos;
				size_t mend = mstart + sentinel_total_len;
				char typech = sentinel_type;
				size_t idx = sentinel_idx;
				if(typech == 'x') {
					Token *ht = accum_get(accum, idx);
							const char *name = ht ? ht->name : NULL;
					bool closing = ht ? ht->data.html.closing : false;
					bool selfClosing = ht ? ht->data.html.self_closing : false;

					size_t name_len = name ? strlen(name) : 0;
					bool is_normal = false;
				for(size_t ni =0; ni < cfg->html[0].count; ni++) {
					sz_ptr_t html_name;
					sz_size_t html_len;
					sz_string_range(&cfg->html[0].items[ni], &html_name, &html_len);
								if(html_name && name && html_len == name_len && sz_equal((const char *)html_name, name, name_len) == sz_true_k) { is_normal = true; break; }
				}
				bool is_void = false;
				for(size_t vi =0; vi < cfg->html[2].count; vi++) {
					sz_ptr_t html_name2;
					sz_size_t html_len2;
					sz_string_range(&cfg->html[2].items[vi], &html_name2, &html_len2);
								if(html_name2 && name && html_len2 == name_len && sz_equal((const char *)html_name2, name, name_len) == sz_true_k) { is_void = true; break; }
				}
					if(is_normal || (!selfClosing && !is_void)) {
						if(!closing) lt++; else if(lt) lt--;
					}
				} else if(typech == 'q') {
					Token *qt = accum_get(accum, idx);
					bool bold = qt ? qt->data.quote.bold : false;
					bool italic = qt ? qt->data.quote.italic : false;
					if(bold) {
						if(!lb) lt++; else if(lt) lt--;
						lb = !lb;
					}
					if(italic) {
						if(!li_flag) lt++; else if(lt) lt--;
						li_flag = !li_flag;
					}
				}
				search_at = mend;
				continue;
			}

			/* mt == MT_COLON */
			if(mt == MT_COLON) {
				size_t mstart = mpos;
				size_t i2 = mstart;
				while(i2 < out_len && out[i2] == ':') i2++;
				size_t slen = i2 - mstart;
				if(slen >= (size_t)dt && lt == 0) {
					size_t take = (size_t)dt;
					make_dd_token(out + mstart, take, accum);
					char mark[64]; size_t mlen = 0;
					work_str_sentinel(accum->count - 1, 'd', mark, &mlen);
					size_t new_len = mstart + mlen + (out_len - (mstart + take));
					char *new_out = malloc(new_len + 1);
					assert(new_out);
					sz_copy(new_out, out, mstart);
					sz_copy(new_out + mstart, mark, mlen);
					sz_copy(new_out + mstart + mlen, out + mstart + take, out_len - (mstart + take));
					new_out[new_len] = '\0';
					free(out);
					free(line);
					lines[li] = new_out;
					lines_len[li] = new_len;
					free_parts(parts, parts_len, parts_count);
					free(combined);
					goto next_line;
				}
				if(lt == 0) {
					make_dd_token(out + mstart, slen, accum);
					char mark[64]; size_t mlen = 0;
					work_str_sentinel(accum->count - 1, 'd', mark, &mlen);
					size_t mend = mstart + slen;
					size_t new_len = mstart + mlen + (out_len - mend);
					if(new_len + 1 > out_cap) {
						out = realloc(out, new_len + 1);
						assert(out);
						out_cap = new_len + 1;
					}
					memmove(out + mstart + mlen, out + mend, out_len - mend);
					sz_copy(out + mstart, mark, mlen);
					out_len = new_len;
					dt -= (int)slen;
					search_at = mstart + mlen;
					continue;
				}
				search_at = i2;
				continue;
			}
		}

		/* finished dd processing for this line: adopt out */
		free(line);
		lines[li]= out;
		lines_len[li]= out_len;
		free_parts(parts, parts_len, parts_count);
		free(combined);
	next_line:;
	}

	/* Join lines back with '\n' and set ws */
	size_t total_len= 0;
	for(size_t i= 0; i < line_count; i++) total_len+= lines_len[i];
	total_len+= (line_count > 0 ? (line_count - 1) : 0); /* newlines between lines */
	ThreadBuf *join_tb = wiki_thread_buf_acquire_scratch();
	if(!join_tb) { log_fatal("parse_list: failed to acquire scratch for join"); abort(); }
	wiki_thread_buf_reserve(join_tb, total_len + 1);
	join_tb->len = 0;
	for(size_t i= 0; i < line_count; i++) {
		if(lines_len[i] > 0) {
			sz_copy(join_tb->buf + join_tb->len, lines[i], lines_len[i]);
			join_tb->len += lines_len[i];
		}
		if(i + 1 < line_count) {
			join_tb->buf[join_tb->len++]= '\n';
		}
		free(lines[i]);
	}
	join_tb->buf[join_tb->len]= '\0';

	wiki_thread_buf_set(tb, join_tb->buf, join_tb->len);
	wiki_thread_buf_release_scratch(join_tb);
	free(lines);
	free(lines_len);
	if(lastPrefix) free(lastPrefix);

	/* md_pref was removed (prefix parsing handled inline) */
}
