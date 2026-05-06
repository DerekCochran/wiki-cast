#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

#include "log.h"
#include "parser/list.h"
#include "string_util.h"
#include "token.h"
#include "thread_buffer.h"
#include "util/pcre_cache.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Helper: compute common prefix length as per JS util/html.getCommon */
static size_t get_common_prefix_len(const char *prefix, size_t plen, const char *last) {
	if(!last) return 0;
	size_t last_len= strlen(last);
	if(last_len == 0) return 0;
	if(plen >= last_len && strncmp(prefix, last, last_len) == 0) return last_len;
	for(size_t i= 0; i < last_len; i++) {
		if(i >= plen) return i;
		if(prefix[i] != last[i]) return i;
	}
	return last_len;
}

/* Build a simple list token: stores the raw part text as a single text child */
static Token *make_list_token(const char *part, size_t part_len, Accum *accum) {
	Token *t= token_new(TOKEN_LIST, "list");
	if(!t) return NULL;
	if(part_len > 0) {
		const char *part_view = wiki_thread_buf_append_to_tokens(part, part_len);
		if(part_view) token_append_text_n(t, part_view, part_len);
	} else {
		token_append_text_n(t, NULL, 0);
	}
	accum_push(accum, t);
	return t;
	accum_push(accum, t);
	return t;
}

/* Build a dd token (definition item marker) */
static Token *make_dd_token(const char *syntax, size_t syntax_len, Accum *accum) {
	Token *t= token_new(TOKEN_DD, "dd");
	if(!t) return NULL;
	if(syntax_len > 0) {
		const char *syn_view = wiki_thread_buf_append_to_tokens(syntax, syntax_len);
		if(syn_view) token_append_text_n(t, syn_view, syntax_len);
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
static char **split_on_semicolon_lookahead(const char *s, size_t len, size_t *out_count) {
	size_t cap= 8, count= 0;
	char **parts= malloc(cap * sizeof(char *));
	assert(parts);
	size_t start= 0;
	for(size_t i= 0; i < len; i++) {
		if(s[i] == ';' && i != start) {
			size_t plen= i - start;
			char *p= malloc(plen + 1);
			assert(p);
			memcpy(p, s + start, plen);
			p[plen]= '\0';
			if(count >= cap) {
				cap*= 2;
				parts= realloc(parts, cap * sizeof(char *));
				assert(parts);
			}
			parts[count++]= p;
			start= i;
		}
	}
	/* last part */
	size_t plen= (len >= start) ? (len - start) : 0;
	char *p= malloc(plen + 1);
	assert(p);
	if(plen > 0) memcpy(p, s + start, plen);
	p[plen]= '\0';
	if(count >= cap) {
		cap*= 2;
		parts= realloc(parts, cap * sizeof(char *));
		assert(parts);
	}
	parts[count++]= p;
	*out_count= count;
	return parts;
}

/* free array returned by split_on_semicolon_lookahead */
static void free_parts(char **parts, size_t count) {
	for(size_t i= 0; i < count; i++) free(parts[i]);
	free(parts);
}

/* Main parse_list implementation */
void parse_list(ThreadBuf *tb, const ParserConfig *cfg, Accum *accum) {
	if(!tb || !tb->buf) return;
	if(config_excluded(cfg, "list")) return;

	/* Compile or reuse cached regexes: prefix, full, brace */
	const char *prefix_pat= "^((?:\\x00\\d+[cno]\\x7F)*)([;:*#]+)(\\s*)";
	const char *full_pat= ":+|\\-\\{|\\x00\\d+[xq]\\x7F";
	const char *brace_pat= "\\-\\{|\\}-";

	pcre2_code *re_prefix = pcre_cache_get(prefix_pat, PCRE2_UTF);
	pcre2_code *re_full = pcre_cache_get(full_pat, PCRE2_UTF);
	pcre2_code *re_brace = pcre_cache_get(brace_pat, PCRE2_UTF);

	pcre2_match_data *md_pref= pcre2_match_data_create_from_pattern(re_prefix, NULL);
	pcre2_match_data *md_full= pcre2_match_data_create_from_pattern(re_full, NULL);
	pcre2_match_data *md_br= pcre2_match_data_create_from_pattern(re_brace, NULL);

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
		if(llen) memcpy(line, buf + pos, llen);
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

	/* Process each line (root: start at 0) */
	for(size_t li= 0; li < line_count; li++) {
		char *line= lines[li];
		size_t line_len= lines_len[li];

		/* Run prefix regex anchored at start */
		int rc= pcre2_match(re_prefix, (PCRE2_SPTR)line, line_len, 0, 0, md_pref, NULL);
		if(rc <= 0) {
			/* No match: reset lastPrefix and keep line unchanged */
			if(lastPrefix) {
				free(lastPrefix);
				lastPrefix= strdup("");
			}
			continue;
		}

		PCRE2_SIZE *ov= pcre2_get_ovector_pointer(md_pref);
		size_t match_end= ov[1];

		/* group offsets: group1=(ov[2],ov[3]) group2=(ov[4],ov[5]) group3=(ov[6],ov[7]) */
		size_t g1s= (rc >= 2 && ov[2] != PCRE2_UNSET) ? ov[2] : 0;
		size_t g1e= (rc >= 2 && ov[3] != PCRE2_UNSET) ? ov[3] : g1s;
		size_t g2s= (rc >= 3 && ov[4] != PCRE2_UNSET) ? ov[4] : 0;
		size_t g2e= (rc >= 3 && ov[5] != PCRE2_UNSET) ? ov[5] : g2s;
		size_t g3s= (rc >= 4 && ov[6] != PCRE2_UNSET) ? ov[6] : 0;
		size_t g3e= (rc >= 4 && ov[7] != PCRE2_UNSET) ? ov[7] : g3s;

		/* Extract comment (may contain sentinels), prefix and space */
		const char *comment= (g1e > g1s) ? line + g1s : "";
		size_t comment_len= (g1e > g1s) ? (g1e - g1s) : 0;
		const char *prefix= (g2e > g2s) ? line + g2s : "";
		size_t prefix_len= (g2e > g2s) ? (g2e - g2s) : 0;
		const char *space= (g3e > g3s) ? line + g3s : "";
		size_t space_len= (g3e > g3s) ? (g3e - g3s) : 0;

		/* Build prefix2 = prefix with ';' -> ':' */
		char *prefix2= malloc(prefix_len + 1);
		assert(prefix2);
		for(size_t i= 0; i < prefix_len; i++) prefix2[i]= (prefix[i] == ';') ? ':' : prefix[i];
		prefix2[prefix_len]= '\0';

		size_t common= get_common_prefix_len(prefix2, prefix_len, lastPrefix ? lastPrefix : "");

		/* Build combined = ((common>1)?prefix.slice(common-1):prefix) + space */
		size_t start_slice= (common > 1) ? (common - 1) : 0;
		size_t combined_len= (prefix_len > start_slice ? prefix_len - start_slice : 0) + space_len;
		char *combined= malloc(combined_len + 1);
		assert(combined);
		if(prefix_len > start_slice) memcpy(combined, prefix + start_slice, prefix_len - start_slice);
		if(space_len) memcpy(combined + (prefix_len > start_slice ? prefix_len - start_slice : 0), space, space_len);
		combined[combined_len]= '\0';

		/* Split combined into parts using lookahead semicolon split */
		size_t parts_count= 0;
		char **parts= split_on_semicolon_lookahead(combined, combined_len, &parts_count);

		bool isDt= (parts_count > 0 && parts[0][0] == ';');
		int dt= (int)parts_count - (isDt ? 0 : 1);

		/* If common > 1, handle commonPrefix adjustments */
		if(common > 1) {
			size_t cp_len= common - 1;
			char *commonPrefix= malloc(cp_len + 1);
			assert(commonPrefix);
			memcpy(commonPrefix, prefix, cp_len);
			commonPrefix[cp_len]= '\0';
			if(isDt) {
				size_t cp_parts= 0;
				char **cp_list= split_on_semicolon_lookahead(commonPrefix, cp_len, &cp_parts);
				/* prepend cp_list to parts */
				size_t new_count= parts_count + cp_parts;
				char **new_parts= malloc(new_count * sizeof(char *));
				assert(new_parts);
				size_t pi= 0;
				for(size_t k= 0; k < cp_parts; k++) new_parts[pi++]= strdup(cp_list[k]);
				for(size_t k= 0; k < parts_count; k++) new_parts[pi++]= strdup(parts[k]);
				free_parts(cp_list, cp_parts);
				free_parts(parts, parts_count);
				parts= new_parts;
				parts_count= new_count;
				if(strchr(commonPrefix, ';')) dt+= (int)cp_parts;
			} else {
				/* prepend commonPrefix to first part */
				size_t a_len= strlen(parts[0]);
				char *merged= malloc(cp_len + a_len + 1);
				assert(merged);
				memcpy(merged, commonPrefix, cp_len);
				memcpy(merged + cp_len, parts[0], a_len);
				merged[cp_len + a_len]= '\0';
				free(parts[0]);
				parts[0]= merged;
			}
			free(commonPrefix);
		}

		/* Update lastPrefix */
		if(lastPrefix) free(lastPrefix);
		lastPrefix= prefix2; /* ownership transferred */

		/* Build text = comment + sentinel markers for each part + rest-of-line */
		size_t base_idx= accum->count;
		/* compute approximate out length */
		size_t out_cap= comment_len + parts_count * 16 + (line_len - (match_end)) + 8;
		char *out= malloc(out_cap);
		assert(out);
		size_t out_len= 0;
		if(comment_len) {
			memcpy(out + out_len, comment, comment_len);
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
			memcpy(out + out_len, marker, mlen);
			out_len+= mlen;
		}
		/* Append rest of line after the matched prefix */
		size_t rest_len= (match_end < line_len) ? (line_len - match_end) : 0;
		if(out_len + rest_len + 1 > out_cap) {
			out_cap= out_len + rest_len + 1;
			out= realloc(out, out_cap);
			assert(out);
		}
		if(rest_len) memcpy(out + out_len, line + match_end, rest_len);
		out_len+= rest_len;
		out[out_len]= '\0';

		/* Create ListToken for each part (order matters) */
		for(size_t i= 0; i < parts_count; i++) {
			make_list_token(parts[i], strlen(parts[i]), accum);
		}

		/* If no dt (definition count), replace line with out and continue */
		if(dt == 0) {
			free(line);
			lines[li]= out; /* adopt out as new line */
			lines_len[li]= out_len;
			free_parts(parts, parts_count);
			free(combined);
			continue;
		}

		/* dt > 0: need to handle ':' tokens that may become dd tokens.
         * Iterate over matches of full_pat in `out`.  Switch to brace_pat
         * when encountering -{ / }- sequences.  Maintain lt/lb/li/lc state. */
		pcre2_code *re_cur= re_full;
		pcre2_match_data *md_cur= md_full;
		size_t search_at= 0;
		int lt= 0;
		int lc= 0;
		int lb= 0;
		int li_flag= 0;

		while(search_at <= out_len && dt > 0) {
			int rc2= pcre2_match(re_cur, (PCRE2_SPTR)out, out_len, search_at, 0, md_cur, NULL);
			if(rc2 <= 0) break;
			PCRE2_SIZE *ov2= pcre2_get_ovector_pointer(md_cur);
			size_t mstart= ov2[0];
			size_t mend= ov2[1];
			size_t slen= mend - mstart;
			/* Simple cases */
			if(slen == 2 && out[mstart] == '-' && out[mstart + 1] == '{') {
				if(!lc) {
					/* switch to brace regex */
					re_cur= re_brace;
					md_cur= md_br;
				}
				lc++;
				search_at= mend;
				continue;
			}
			if(slen == 2 && out[mstart] == '}' && out[mstart + 1] == '-') {
				lc--;
				if(!lc) {
					re_cur= re_full;
					md_cur= md_full;
				}
				search_at= mend;
				continue;
			}

			/* Check for sentinel: begins with NUL and ends with 0x7F */
			if((unsigned char)out[mstart] == 0 && (unsigned char)out[mend - 1] == 0x7F) {
				/* parse index and type char */
				if(slen < 3) {
					search_at= mend;
					continue;
				}
				char typech= out[mend - 2];
				/* parse decimal index between mstart+1 .. mend-2 */
				size_t idx= 0;
				for(size_t k= mstart + 1; k < mend - 2; k++) {
					if(out[k] < '0' || out[k] > '9') {
						idx= SIZE_MAX;
						break;
					}
					idx= idx * 10 + (size_t)(out[k] - '0');
				}
				if(idx == SIZE_MAX) {
					search_at= mend;
					continue;
				}

				if(typech == 'x') {
					Token *ht= accum_get(accum, idx);
					const char *name= ht ? ht->name : NULL;
					bool closing= ht ? ht->data.html.closing : false;
					bool selfClosing= ht ? ht->data.html.self_closing : false;
					/* normalTags = cfg->html[0], voidTags = cfg->html[2] */
					bool is_normal= false;
					for(size_t ni= 0; ni < cfg->html[0].count; ni++)
						if(cfg->html[0].items[ni] && name && strcmp(cfg->html[0].items[ni], name) == 0) {
							is_normal= true;
							break;
						}
					bool is_void= false;
					for(size_t vi= 0; vi < cfg->html[2].count; vi++)
						if(cfg->html[2].items[vi] && name && strcmp(cfg->html[2].items[vi], name) == 0) {
							is_void= true;
							break;
						}
					if(is_normal || (!selfClosing && !is_void)) {
						if(!closing)
							lt++;
						else if(lt)
							lt--;
					}
				} else if(typech == 'q') {
					Token *qt= accum_get(accum, idx);
					bool bold= qt ? qt->data.quote.bold : false;
					bool italic= qt ? qt->data.quote.italic : false;
					if(bold) {
						if(!lb)
							lt++;
						else if(lt)
							lt--;
						lb= !lb;
					}
					if(italic) {
						if(!li_flag)
							lt++;
						else if(lt)
							lt--;
						li_flag= !li_flag;
					}
				}
				search_at= mend;
				continue;
			}

			/* syntax is a sequence of ':' (colon) */
			if(out[mstart] == ':') {
				/* count number of ':' characters in the match */
				size_t colons= slen; /* pattern ':+', so group length equals match length */
				if(colons >= (size_t)dt && lt == 0) {
					/* create a dd token for the first dt colons and return early */
					size_t take= (size_t)dt;
					make_dd_token(out + mstart, take, accum);
					/* build new string: out[0..mstart) + sentinel_of_new_dd + out[mstart + take ..] */
					char mark[64];
					size_t mlen= 0;
					work_str_sentinel(accum->count - 1, 'd', mark, &mlen);
					size_t new_len= mstart + mlen + (out_len - (mstart + take));
					char *new_out= malloc(new_len + 1);
					assert(new_out);
					memcpy(new_out, out, mstart);
					memcpy(new_out + mstart, mark, mlen);
					memcpy(new_out + mstart + mlen, out + mstart + take, out_len - (mstart + take));
					new_out[new_len]= '\0';
					free(out);
					/* adopt and finish processing this line */
					free(line);
					lines[li]= new_out;
					lines_len[li]= new_len;
					free_parts(parts, parts_count);
					free(combined);
					goto next_line;
				}
				if(lt == 0) {
					/* create dd for the matched syntax, reduce dt and continue scanning */
					make_dd_token(out + mstart, slen, accum);
					char mark[64];
					size_t mlen= 0;
					work_str_sentinel(accum->count - 1, 'd', mark, &mlen);
					/* replace in-place: out = out[0..mstart) + mark + out[mend..] */
					size_t new_len= mstart + mlen + (out_len - mend);
					if(new_len + 1 > out_cap) {
						out= realloc(out, new_len + 1);
						assert(out);
						out_cap= new_len + 1;
					}
					memmove(out + mstart + mlen, out + mend, out_len - mend);
					memcpy(out + mstart, mark, mlen);
					out_len= new_len;
					dt-= (int)slen;
					/* continue scanning after the inserted marker */
					search_at= mstart + mlen;
					continue;
				}
			}

			search_at= mend;
		}

		/* finished dd processing for this line: adopt out */
		free(line);
		lines[li]= out;
		lines_len[li]= out_len;
		free_parts(parts, parts_count);
		free(combined);
	next_line:;
	}

	/* Join lines back with '\n' and set ws */
	size_t total_len= 0;
	for(size_t i= 0; i < line_count; i++) total_len+= lines_len[i];
	total_len+= (line_count > 0 ? (line_count - 1) : 0); /* newlines between lines */
	char *joined= malloc(total_len + 1);
	assert(joined);
	size_t p= 0;
	for(size_t i= 0; i < line_count; i++) {
		if(lines_len[i] > 0) memcpy(joined + p, lines[i], lines_len[i]);
		p+= lines_len[i];
		if(i + 1 < line_count) {
			joined[p++]= '\n';
		}
		free(lines[i]);
	}
	joined[p]= '\0';

	wiki_thread_buf_set(tb, joined, p);
	free(joined);
	free(lines);
	free(lines_len);
	if(lastPrefix) free(lastPrefix);

	pcre2_match_data_free(md_pref);
	pcre2_match_data_free(md_full);
	pcre2_match_data_free(md_br);
}
