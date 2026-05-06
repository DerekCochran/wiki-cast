#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

#include "log.h"
#include "parser/converter.h"
#include "string_util.h"
#include "token.h"
#include "thread_buffer.h"
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define CONVERTER_ESC_NUL '\x02'

/*
 * parse_converter — simplified C implementation mirroring JS parseConverter.
 *
 * This implementation locates -{ ... }- fragments in the working string
 * and replaces each with a sentinel marker referencing a ConverterToken
 * pushed onto the accumulator. It supports flags before a '|' and
 * multiple rules separated by top-level semicolons. HTML entity semicolons
 * are temporarily masked during splitting.
 */

/* Helper: trim whitespace from ends; returns newly-allocated string */
static char *trim_copy(const char *s, size_t len) {
	size_t i= 0, j= len;
	while(i < j && isspace((unsigned char)s[i])) i++;
	while(j > i && isspace((unsigned char)s[j - 1])) j--;
	size_t n= j - i;
	char *r= malloc(n + 1);
	assert(r);
	memcpy(r, s + i, n);
	r[n]= '\0';
	return r;
}

static bool variant_in_config(const ParserConfig *cfg, const char *s, size_t len) {
	if(!cfg || !s) return false;
	char *trimmed= trim_copy(s, len);
	if(!trimmed) return false;

	for(size_t i= 0; i < cfg->variants.count; i++) {
		if(strcasecmp(trimmed, cfg->variants.items[i]) == 0) {
			free(trimmed);
			return true;
		}
	}

	free(trimmed);
	return false;
}

static bool token_append_text_decoded_nul(Token *t, const char *s, size_t len) {
	if(len == 0) {
		token_append_text_n(t, NULL, 0);
		return true;
	}
	ThreadBuf *scratch = wiki_thread_buf_acquire_scratch();
	wiki_thread_buf_set(scratch, s, len);
	/* decode placeholder into NUL bytes in scratch (safe: tokens must not reference scratch) */
	for(size_t i= 0; i < len; i++) {
		if(scratch->buf[i] == CONVERTER_ESC_NUL) scratch->buf[i]= '\0';
	}
	const char *view = wiki_thread_buf_append_to_tokens(scratch->buf, len);
	wiki_thread_buf_release_scratch(scratch);
	if(view) token_append_text_n(t, view, len);
	return view != NULL;
}

static Token *build_converter_rule_token(const char *rule, bool has_colon, const ParserConfig *cfg) {
	Token *r= token_new(TOKEN_PLAIN, "converter-rule");
	if(!r) return NULL;

	const char *colon= strchr(rule, ':');
	if(has_colon && colon) {
		size_t head_len= (size_t)(colon - rule);

		const char *arrow= NULL;
		for(size_t i= 0; i + 1 < head_len; i++) {
			if(rule[i] == '=' && rule[i + 1] == '>') {
				arrow= rule + i;
				break;
			}
		}

		const char *variant_ptr= arrow ? (arrow + 2) : rule;
		size_t variant_len= (size_t)(colon - variant_ptr);

		if(variant_in_config(cfg, variant_ptr, variant_len)) {
			if(arrow) {
				Token *from_tok= token_new(TOKEN_PLAIN, "converter-rule-from");
				if(!from_tok) {
					token_free(r);
					return NULL;
				}
				if(!token_append_text_decoded_nul(from_tok, rule, (size_t)(arrow - rule))) {
					token_free(from_tok);
					token_free(r);
					return NULL;
				}
				token_append_child(r, from_tok);
			}

			Token *variant_tok= token_new(TOKEN_PLAIN, "converter-rule-variant");
			if(!variant_tok) {
				token_free(r);
				return NULL;
			}
			if(!token_append_text_decoded_nul(variant_tok, variant_ptr, variant_len)) {
				token_free(variant_tok);
				token_free(r);
				return NULL;
			}
			token_append_child(r, variant_tok);

			Token *to_tok= token_new(TOKEN_PLAIN, "converter-rule-to");
			if(!to_tok) {
				token_free(r);
				return NULL;
			}
			if(!token_append_text_decoded_nul(to_tok, colon + 1, strlen(colon + 1))) {
				token_free(to_tok);
				token_free(r);
				return NULL;
			}
			token_append_child(r, to_tok);

			return r;
		}
	}

	Token *to_tok= token_new(TOKEN_PLAIN, "converter-rule-to");
	if(!to_tok) {
		token_free(r);
		return NULL;
	}
	if(!token_append_text_decoded_nul(to_tok, rule, strlen(rule))) {
		token_free(to_tok);
		token_free(r);
		return NULL;
	}
	token_append_child(r, to_tok);
	return r;
}

/* Build a minimal converter token and push to accum.
 * flags: array of strings (NULL-terminated)
 * rules: array of strings (NULL-terminated)
 */
static Token *build_converter_token(char **flags, char **rules, const ParserConfig *cfg, Accum *accum) {
	Token *t= token_new(TOKEN_CONVERTER, "converter");
	if(!t) return NULL;

	/* Flags token (always present) */
	Token *flags_tok= token_new(TOKEN_PLAIN, "converter-flags");
	if(!flags_tok) {
		token_free(t);
		return NULL;
	}
	for(size_t i= 0; flags && flags[i]; i++) {
		Token *f= token_new(TOKEN_PLAIN, "converter-flag");
		if(!f) {
			token_free(flags_tok);
			token_free(t);
			return NULL;
		}
		size_t flen = strlen(flags[i]);
		if(flen > 0) {
			const char *fview = wiki_thread_buf_append_to_tokens(flags[i], flen);
			if(fview) token_append_text_n(f, fview, flen);
		} else {
			token_append_text_n(f, NULL, 0);
		}
		token_append_child(flags_tok, f);
	}
	token_append_child(t, flags_tok);

	/* Rules tokens: decompose each rule into from/variant/to children like JS. */
	for(size_t i= 0; rules && rules[i]; i++) {
		bool has_colon= strchr(rules[i], ':') != NULL;
		Token *r= build_converter_rule_token(rules[i], has_colon, cfg);
		if(!r) {
			token_free(t);
			return NULL;
		}
		token_append_child(t, r);
	}

	accum_push(accum, t);
	return t;
}

/* Mask HTML-like entities of the form &[#a-z0-9]+; by replacing trailing
 * semicolon with \x01. Returns newly-allocated string. */
static char *mask_entities(const char *s, size_t len, size_t *out_len) {
	size_t cap= len + 8;
	char *out= malloc(cap);
	assert(out);
	size_t j= 0;

	for(size_t i= 0; i < len;) {
		if(s[i] == '&') {
			size_t k= i + 1;
			if(k < len && (s[k] == '#' || isalpha((unsigned char)s[k]))) {
				/* scan until semicolon or break */
				while(k < len && (isalnum((unsigned char)s[k]) || s[k] == '#' || s[k] == 'x' || s[k] == 'X')) k++;
				if(k < len && s[k] == ';') {
					/* copy until the semicolon, but write placeholder instead */
					size_t need= (k - i + 1);
					if(j + need + 1 > cap) {
						cap= (cap + need) * 2;
						out= realloc(out, cap);
						assert(out);
					}
					memcpy(out + j, s + i, k - i);
					j+= (k - i);
					out[j++]= '\x01'; /* placeholder for ';' */
					i= k + 1;
					continue;
				}
			}
		}
		if(j + 2 > cap) {
			cap*= 2;
			out= realloc(out, cap);
			assert(out);
		}
		out[j++]= (s[i] == '\0') ? CONVERTER_ESC_NUL : s[i];
		i++;
	}
	out[j]= '\0';
	if(out_len) *out_len= j;
	return out;
}

/* Restore placeholder \x01 back to ';' in-place (returns newly-allocated copy) */
static char *unmask_entities(const char *s, size_t len) {
	char *r= malloc(len + 1);
	assert(r);
	for(size_t i= 0; i < len; i++) r[i]= s[i] == '\x01' ? ';' : s[i];
	r[len]= '\0';
	return r;
}

/* Escape a config variant value so it is safe in a regex alternation. */
static void append_regex_escaped(char **buf, size_t *cap, size_t *len, const char *s) {
	for(const char *p= s; *p; p++) {
		unsigned char c= (unsigned char)*p;
		bool meta= (c < 0x80) && (c == '\\' || c == '.' || c == '^' || c == '$' || c == '|' || c == '?' || c == '*' || c == '+' || c == '(' || c == ')' || c == '[' || c == ']' || c == '{' || c == '}');
		size_t need= meta ? 2 : 1;
		if(*len + need + 1 > *cap) {
			*cap= (*cap + need + 64) * 2;
			*buf= realloc(*buf, *cap);
			assert(*buf);
		}
		if(meta) (*buf)[(*len)++]= '\\';
		(*buf)[(*len)++]= (char)c;
	}
	(*buf)[*len]= '\0';
}

/* JS parity:
 * new RegExp(String.raw`;(?=(?:[^;]*?=>)?\s*(?:${variants.join('|')})\s*:|(?:\s|\0\d+[cn]\x7F)*$)`, 'iu')
 */
static pcre2_code *compile_converter_split_regex(const ParserConfig *cfg) {
	if(!cfg || cfg->variants.count == 0) return NULL;

	size_t cap= 256;
	char *pat= malloc(cap);
	assert(pat);
	size_t len= 0;

	const char *prefix= ";(?=(?:[^;]*?=>)?\\s*(?:";
	size_t prefix_len= strlen(prefix);
	if(len + prefix_len + 1 > cap) {
		cap= (len + prefix_len + 64) * 2;
		pat= realloc(pat, cap);
		assert(pat);
	}
	memcpy(pat + len, prefix, prefix_len);
	len+= prefix_len;
	pat[len]= '\0';

	for(size_t i= 0; i < cfg->variants.count; i++) {
		if(i > 0) {
			if(len + 2 > cap) {
				cap= (cap + 64) * 2;
				pat= realloc(pat, cap);
				assert(pat);
			}
			pat[len++]= '|';
			pat[len]= '\0';
		}
		append_regex_escaped(&pat, &cap, &len, cfg->variants.items[i]);
	}

	const char *suffix= ")\\s*:|(?:\\s|\\x00\\d+[cn]\\x7F)*$)";
	size_t suffix_len= strlen(suffix);
	if(len + suffix_len + 1 > cap) {
		cap= (len + suffix_len + 64) * 2;
		pat= realloc(pat, cap);
		assert(pat);
	}
	memcpy(pat + len, suffix, suffix_len);
	len+= suffix_len;
	pat[len]= '\0';

	PCRE2_SIZE err_offset;
	int err_code;
	pcre2_code *re= pcre2_compile(
	(PCRE2_SPTR)pat, PCRE2_ZERO_TERMINATED,
	PCRE2_CASELESS | PCRE2_UTF,
	&err_code, &err_offset, NULL);
	if(!re) {
		PCRE2_UCHAR8 err_buf[256];
		pcre2_get_error_message(err_code, err_buf, sizeof(err_buf));
		log_error("converter split regex compile error at %zu: %s Pattern: %.200s",
							(size_t)err_offset, (char *)err_buf, pat);
	}

	free(pat);
	return re;
}

void parse_converter(ThreadBuf *tb, const ParserConfig *cfg, Accum *accum) {
	if(!tb || !tb->buf) return;
	if(!cfg || cfg->variants.count == 0) return; /* no variants configured */

	if(!cfg->regex_converter) {
		ParserConfig *m= (ParserConfig *)cfg;
		m->regex_converter= (ParserConfigRegex *)compile_converter_split_regex(cfg);
		if(!m->regex_converter) return;
	}
	pcre2_code *re_split= (pcre2_code *)cfg->regex_converter;

	size_t *stack= NULL;
	size_t stack_cap= 0;
	size_t stack_len= 0;

	bool scan_closes= false; /* false => regex1 /-\{/ ; true => regex2 /-\{|\}-/ */
	size_t cursor= 0;

	while(cursor + 1 < tb->len) {
		bool found= false;
		bool is_close= false;
		size_t index= 0;

		for(size_t i= cursor; i + 1 < tb->len; i++) {
			if(tb->buf[i] == '-' && tb->buf[i + 1] == '{') {
				found= true;
				is_close= false;
				index= i;
				break;
			}
			if(scan_closes && tb->buf[i] == '}' && tb->buf[i + 1] == '-') {
				found= true;
				is_close= true;
				index= i;
				break;
			}
		}

		if(!found) break;

		if(!is_close) {
			if(stack_len >= stack_cap) {
				stack_cap= stack_cap ? stack_cap * 2 : 16;
				stack= realloc(stack, stack_cap * sizeof(size_t));
				assert(stack);
			}
			stack[stack_len++]= index;
			scan_closes= true;
			cursor= index + 2;
			continue;
		}

		if(stack_len == 0) {
			cursor= index + 2;
			continue;
		}

		size_t open_idx= stack[--stack_len];
		size_t tok_idx= accum_len(accum);

		size_t inner_start= open_idx + 2;
		size_t inner_len= index - inner_start;
		const char *inner_ptr= tb->buf + inner_start;

		ssize_t pipe_at= -1;
		for(size_t k= 0; k < inner_len; k++) {
			if(inner_ptr[k] == '|') {
				pipe_at= (ssize_t)k;
				break;
			}
		}

		char **flags= NULL;
		size_t flags_count= 0;
		if(pipe_at != -1) {
			size_t flags_len= (size_t)pipe_at;
			size_t segs= 1;
			for(size_t k= 0; k < flags_len; k++) {
				if(inner_ptr[k] == ';') segs++;
			}
			flags= malloc((segs + 1) * sizeof(char *));
			assert(flags);
			size_t start= 0;
			for(size_t k= 0; k <= flags_len; k++) {
				if(k == flags_len || inner_ptr[k] == ';') {
					size_t n= k - start;
					char *seg= malloc(n + 1);
					assert(seg);
					if(n > 0) memcpy(seg, inner_ptr + start, n);
					seg[n]= '\0';
					flags[flags_count++]= seg;
					start= k + 1;
				}
			}
			flags[flags_count]= NULL;
		} else {
			flags= malloc(sizeof(char *));
			assert(flags);
			flags[0]= NULL;
		}

		const char *raw_ptr= (pipe_at == -1) ? inner_ptr : inner_ptr + pipe_at + 1;
		size_t raw_len= (pipe_at == -1) ? inner_len : inner_len - (size_t)pipe_at - 1;

		size_t masked_len= 0;
		char *masked= mask_entities(raw_ptr, raw_len, &masked_len);

		char **rules= NULL;
		size_t rule_cap= 0;
		size_t rule_count= 0;

		pcre2_match_data *split_md= pcre2_match_data_create_from_pattern(re_split, NULL);
		assert(split_md);

		size_t split_cur= 0;
		while(split_cur <= masked_len) {
			int rc= pcre2_match(re_split, (PCRE2_SPTR)masked, masked_len,
													split_cur, 0, split_md, NULL);
			if(rc <= 0) break;

			PCRE2_SIZE *ov= pcre2_get_ovector_pointer(split_md);
			size_t ms= ov[0], me= ov[1];
			if(ms < split_cur || me < ms) break;

			size_t seg_len= ms - split_cur;
			char *seg= malloc(seg_len + 1);
			assert(seg);
			if(seg_len > 0) memcpy(seg, masked + split_cur, seg_len);
			seg[seg_len]= '\0';

			char *restored= unmask_entities(seg, seg_len);
			free(seg);

			if(rule_count >= rule_cap) {
				rule_cap= rule_cap ? rule_cap * 2 : 8;
				rules= realloc(rules, rule_cap * sizeof(char *));
				assert(rules);
			}
			rules[rule_count++]= restored;

			split_cur= me;
			if(me == ms) split_cur++;
		}

		pcre2_match_data_free(split_md);

		{
			size_t seg_len= masked_len - split_cur;
			char *seg= malloc(seg_len + 1);
			assert(seg);
			if(seg_len > 0) memcpy(seg, masked + split_cur, seg_len);
			seg[seg_len]= '\0';
			char *restored= unmask_entities(seg, seg_len);
			free(seg);

			if(rule_count >= rule_cap) {
				rule_cap= rule_cap ? rule_cap * 2 : 8;
				rules= realloc(rules, rule_cap * sizeof(char *));
				assert(rules);
			}
			rules[rule_count++]= restored;
		}

		rules= realloc(rules, (rule_count + 1) * sizeof(char *));
		assert(rules);
		rules[rule_count]= NULL;

		build_converter_token(flags, rules, cfg, accum);

		char marker[64];
		size_t marker_len= 0;
		work_str_sentinel(tok_idx, 'v', marker, &marker_len);

		size_t prefix_len= open_idx;
		size_t suffix_start= index + 2;
		size_t suffix_len= tb->len - suffix_start;
		size_t new_len= prefix_len + marker_len + suffix_len;

		char *new_buf= malloc(new_len + 1);
		assert(new_buf);
		if(prefix_len > 0) memcpy(new_buf, tb->buf, prefix_len);
		memcpy(new_buf + prefix_len, marker, marker_len);
		if(suffix_len > 0) memcpy(new_buf + prefix_len + marker_len, tb->buf + suffix_start, suffix_len);
		new_buf[new_len]= '\0';

		wiki_thread_buf_set(tb, new_buf, new_len);
		free(new_buf);

		for(size_t fi= 0; fi < flags_count; fi++) free(flags[fi]);
		free(flags);
		for(size_t ri= 0; ri < rule_count; ri++) free(rules[ri]);
		free(rules);
		free(masked);

		if(stack_len == 0) {
			scan_closes= false;
		}
		cursor= open_idx + marker_len;
	}

	free(stack);
}
