#include "parser/braces.h"
#include "title.h"
#include "token.h"
#include "util/callback_parser.h"
#include "util/log.h"
#include "util/string_util.h"
#include "util/wiki_parser_rules.h"
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stringzilla/stringzilla.h>

/* ── brace-event scanner (wikitext-specific; lives here, not in callback_parser) ── */

typedef enum {
	BRACE_EVT_HEADING_OPEN= 0,
	BRACE_EVT_WIKILINK_OPEN= 1,
	BRACE_EVT_CONVERTER_OPEN= 2,
	BRACE_EVT_BRACE_OPEN= 3,
	BRACE_EVT_NEWLINE= 4,
	BRACE_EVT_PIPE= 5,
	BRACE_EVT_EQUALS= 6,
	BRACE_EVT_BRACE_CLOSE= 7,
	BRACE_EVT_CONVERTER_CLOSE= 8,
	BRACE_EVT_WIKILINK_CLOSE= 9,
} BraceEventKind;

/* Returns true and fills *out_len if buf[pos..] is a \0<digits><allowed_type>\x7F sentinel.
 * allowed_len is the length of allowed_types (known at all call sites, avoids repeated strlen). */
static bool parse_sentinel_at_allowed(const char *buf, size_t len, size_t pos,
											   const char *allowed_types, size_t allowed_len, size_t *out_len) {
	if(!buf || pos >= len) return false;
	if((unsigned char)buf[pos] != 0) return false;
	size_t j= pos + 1;
	if(j >= len || buf[j] < '0' || buf[j] > '9') return false;
	const char *not_digit = sz_find_byte_not_from(buf + j, len - j, "0123456789", 10);
	j = not_digit ? (size_t)(not_digit - buf) : len;
	if(j >= len) return false;
	char t= buf[j];
	if(allowed_types && sz_find_byte(allowed_types, allowed_len, &t) == NULL) return false;
	if(j + 1 >= len) return false;
	if((unsigned char)buf[j + 1] != (unsigned char)0x7F) return false;
	if(out_len) *out_len= (j + 2) - pos;
	return true;
}

/* Returns true and fills *out_len if buf[pos..] is a \0<digits><allowed_type>\x7F sentinel. */
/* Returns true and fills *out_len if buf[pos..] is a \0<digits><allowed_type>\x7F sentinel.
 * allowed_len is the length of the allowed_types string (known at all call sites). */

/*
 * Heading line result for the validator used in stage 4.
 */
typedef struct {
	const char *lead;         /* leading \x00\d+[cn]\x7F group; may be empty */
	size_t      lead_len;
	const char *open_eq;      /* pointer to first '=' of opening run         */
	size_t      eq_count;     /* 1–6                                          */
	const char *content;      /* (.+): inner heading text, non-empty          */
	size_t      content_len;
	const char *trail;        /* (\s|\x00\d+[cn]\x7F)*: trailing group        */
	size_t      trail_len;
} HeadingLineResult;

/*
 * heading_line_parse — forward-scan replacement for:
 *   /^(={1,6})(.+)\1((?:\s|\0\d+[cn]\x7F)*)$/
 *
 * Input `s[0..len]` must start with at least one '='.
 * Returns true and populates *out on match; false otherwise.
 * out->lead / out->lead_len are always set to NULL / 0 (no leading group).
 */
static bool heading_line_parse(const char *s, size_t len, HeadingLineResult *out) {
	if(!s || len == 0 || !out) return false;
	const char *end = s + len;

	/* Group 1: opening '=' run, 1–6 chars (regex backtracks this). */
	size_t open_run = 0;
	while(open_run < 6 && s + open_run < end && s[open_run] == '=') open_run++;
	if(open_run == 0 || s + open_run >= end) return false;

	/* Group 3: strip trailing ((\s|\0\d+[cn]\x7F)*) from the end */
	const char *trail_end   = end;
	const char *trail_start = end;
	bool changed = true;
	while(changed && trail_start > s + 1) {
		changed = false;
		/* whitespace */
		if(isspace((unsigned char)*(trail_start - 1))) {
			trail_start--; changed = true; continue;
		}
		/* \x00\d+[cn]\x7F sentinel — scan backwards */
		if((unsigned char)*(trail_start - 1) == (unsigned char)'\x7F'
		   && trail_start - 1 > s) {
			const char *type_p = trail_start - 2;
			if(*type_p == 'c' || *type_p == 'n') {
				const char *q = type_p - 1;
				size_t digit_count = 0;
				while(q > s && *q >= '0' && *q <= '9') {
					q--;
					digit_count++;
				}
				if(digit_count >= 1 && (unsigned char)*q == 0) {
					trail_start = q;
					changed = true;
					continue;
				}
			}
		}
	}

	/* JS/regex parity: backtrack opening run (={1,6}) from max to 1. */
	for(size_t eq_count = open_run; eq_count > 0; eq_count--) {
		const char *content_start = s + eq_count;
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

		/* JS regex parity: (.+) in /u mode does not match newlines. */
		char nl_ch = '\n';
		if(sz_find_byte(content_start, (size_t)(content_end - content_start), &nl_ch)) continue;

		out->lead        = NULL;
		out->lead_len    = 0;
		out->open_eq     = s;
		out->eq_count    = eq_count;
		out->content     = content_start;
		out->content_len = (size_t)(content_end - content_start);
		out->trail       = trail_start;
		out->trail_len   = (size_t)(trail_end - trail_start);
		return true;
	}

	return false;
}

/*
 * Advance *pos to the next brace-grammar event in buf[*pos .. len).
 * Returns true and fills the out-params on success; false when exhausted.
 *
 * Replaces the alternation regex:
 *   ^((?:\0\d+[cno]\x7F)*)={1,6}|\[\[|-\{(?!\{)|\{{2,}
 *   |\n(?!(?:[^\S\n]|\0\d+[cn]\x7F)*\n)|[|=]|\}{2,}|\}-|\]\]
 */
static bool __attribute__((unused))
brace_event_next(const char *buf, size_t len, size_t *pos,
					  BraceEventKind *kind_out, size_t *match_len_out,
					  size_t *brace_count_out, size_t *equals_count_out,
					  size_t *sentinel_len_out) {
	if(!buf || !pos || *pos >= len) return false;
	const char cand[]= {'\0', '[', '-', '{', '\n', '|', '=', '}', ']'};
	size_t i= *pos;
	while(i < len) {
		const char *found= (const char *)sz_find_byte_from(buf + i, len - i, cand, sizeof(cand));
		if(!found) return false;
		size_t p= (size_t)(found - buf);
		char ch= buf[p];

		if(ch == '\0') {
			size_t cur= p, total_sl= 0, sl= 0;
							while(cur < len && parse_sentinel_at_allowed(buf, len, cur, "cno", 3, &sl)) {
				total_sl+= sl;
				cur+= sl;
			}
			bool at_line= (p == 0) || (buf[p - 1] == '\n');
			if(total_sl > 0 && at_line) {
				size_t eqpos= p + total_sl, eqcount= 0;
				while(eqpos < len && buf[eqpos] == '=' && eqcount < 6) {
					eqpos++;
					eqcount++;
				}
				if(eqcount >= 1) {
					if(kind_out) *kind_out= BRACE_EVT_HEADING_OPEN;
					if(match_len_out) *match_len_out= total_sl + eqcount;
					if(brace_count_out) *brace_count_out= 0;
					if(equals_count_out) *equals_count_out= eqcount;
					if(sentinel_len_out) *sentinel_len_out= total_sl;
					*pos= p + total_sl + eqcount;
					return true;
				}
			}
			i= p + 1;
			continue;
		}
		if(ch == '[') {
			if(p + 1 < len && buf[p + 1] == '[') {
				if(kind_out) *kind_out= BRACE_EVT_WIKILINK_OPEN;
				if(match_len_out) *match_len_out= 2;
				if(brace_count_out) *brace_count_out= 0;
				if(equals_count_out) *equals_count_out= 0;
				if(sentinel_len_out) *sentinel_len_out= 0;
				*pos= p + 2;
				return true;
			}
			i= p + 1;
			continue;
		}
		if(ch == '-') {
			if(p + 1 < len && buf[p + 1] == '{' && !(p + 2 < len && buf[p + 2] == '{')) {
				if(kind_out) *kind_out= BRACE_EVT_CONVERTER_OPEN;
				if(match_len_out) *match_len_out= 2;
				if(brace_count_out) *brace_count_out= 0;
				if(equals_count_out) *equals_count_out= 0;
				if(sentinel_len_out) *sentinel_len_out= 0;
				*pos= p + 2;
				return true;
			}
			i= p + 1;
			continue;
		}
		if(ch == '{') {
			size_t cnt= 0;
			while(p + cnt < len && buf[p + cnt] == '{') cnt++;
			if(cnt >= 2) {
				size_t bc= cnt;
				if(kind_out) *kind_out= BRACE_EVT_BRACE_OPEN;
				if(match_len_out) *match_len_out= bc;
				if(brace_count_out) *brace_count_out= bc;
				if(equals_count_out) *equals_count_out= 0;
				if(sentinel_len_out) *sentinel_len_out= 0;
				*pos= p + bc;
				return true;
			}
			i= p + 1;
			continue;
		}
		if(ch == '\n') {
			/* Skip firing an event when followed by blank-line boundary
               ([^\S\n] | \0\d+[cn]\x7F)* then another \n. */
			size_t j= p + 1;
			while(j < len) {
				unsigned char cj= (unsigned char)buf[j];
				if(cj == '\0') {
					size_t sl= 0;
											if(parse_sentinel_at_allowed(buf, len, j, "cn", 2, &sl)) {
						j+= sl;
						continue;
					}
					break;
				}
				if(cj == ' ' || cj == '\t' || cj == '\v' || cj == '\f' || cj == '\r') {
					j++;
					continue;
				}
				break;
			}
			if(j < len && buf[j] == '\n') {
				i= p + 1;
				continue;
			}
			if(kind_out) *kind_out= BRACE_EVT_NEWLINE;
			if(match_len_out) *match_len_out= 1;
			if(brace_count_out) *brace_count_out= 0;
			if(equals_count_out) *equals_count_out= 0;
			if(sentinel_len_out) *sentinel_len_out= 0;
			*pos= p + 1;
			return true;
		}
		if(ch == '|') {
			if(kind_out) *kind_out= BRACE_EVT_PIPE;
			if(match_len_out) *match_len_out= 1;
			if(brace_count_out) *brace_count_out= 0;
			if(equals_count_out) *equals_count_out= 0;
			if(sentinel_len_out) *sentinel_len_out= 0;
			*pos= p + 1;
			return true;
		}
		if(ch == '=') {
			/* Heading-open has priority over inner '=' splitting at line start,
			 * mirroring the regex alternation that starts with ^...={1,6}. */
			bool at_line= (p == 0) || (buf[p - 1] == '\n');
			if(at_line) {
				size_t eqcount= 0;
				while(p + eqcount < len && buf[p + eqcount] == '=' && eqcount < 6) {
					eqcount++;
				}
				if(eqcount >= 1) {
					if(kind_out) *kind_out= BRACE_EVT_HEADING_OPEN;
					if(match_len_out) *match_len_out= eqcount;
					if(brace_count_out) *brace_count_out= 0;
					if(equals_count_out) *equals_count_out= eqcount;
					if(sentinel_len_out) *sentinel_len_out= 0;
					*pos= p + eqcount;
					return true;
				}
			}
			if(kind_out) *kind_out= BRACE_EVT_EQUALS;
			if(match_len_out) *match_len_out= 1;
			if(brace_count_out) *brace_count_out= 0;
			if(equals_count_out) *equals_count_out= 1;
			if(sentinel_len_out) *sentinel_len_out= 0;
			*pos= p + 1;
			return true;
		}
		if(ch == '}') {
			if(p + 1 < len && buf[p + 1] == '-') {
				if(kind_out) *kind_out= BRACE_EVT_CONVERTER_CLOSE;
				if(match_len_out) *match_len_out= 2;
				if(brace_count_out) *brace_count_out= 0;
				if(equals_count_out) *equals_count_out= 0;
				if(sentinel_len_out) *sentinel_len_out= 0;
				*pos= p + 2;
				return true;
			}
			size_t cnt= 0;
			while(p + cnt < len && buf[p + cnt] == '}') cnt++;
			if(cnt >= 2) {
				size_t bc= cnt;
				if(kind_out) *kind_out= BRACE_EVT_BRACE_CLOSE;
				if(match_len_out) *match_len_out= bc;
				if(brace_count_out) *brace_count_out= bc;
				if(equals_count_out) *equals_count_out= 0;
				if(sentinel_len_out) *sentinel_len_out= 0;
				*pos= p + bc;
				return true;
			}
			i= p + 1;
			continue;
		}
		if(ch == ']') {
			if(p + 1 < len && buf[p + 1] == ']') {
				if(kind_out) *kind_out= BRACE_EVT_WIKILINK_CLOSE;
				if(match_len_out) *match_len_out= 2;
				if(brace_count_out) *brace_count_out= 0;
				if(equals_count_out) *equals_count_out= 0;
				if(sentinel_len_out) *sentinel_len_out= 0;
				*pos= p + 2;
				return true;
			}
			i= p + 1;
			continue;
		}
		i= p + 1;
	}
	return false;
}

static bool str_list_contains_ci(const StrList *sl, const char *needle, size_t needle_len) {
	if(!sl || !needle) return false;
	for(size_t i= 0; i < sl->count; i++) {
		sz_ptr_t start;
		sz_size_t len;
		sz_string_range(&sl->items[i], &start, &len);
		if(start && len == needle_len && str_ci_eq_n((const char *)start, needle, len)) return true;
	}
	return false;
}

static const char *str_map_get_exact(const StrMap *m, const char *key, size_t key_cmp_len) {
	if(!m || !key) return NULL;
	for(size_t i= 0; i < m->count; i++) {
		sz_ptr_t key_start, val_start;
		sz_size_t key_len, val_len;
		sz_string_range(&m->keys[i], &key_start, &key_len);
		if(key_start && key_len == key_cmp_len && sz_equal(key_start, key, key_cmp_len) == sz_true_k) {
			sz_string_range(&m->values[i], &val_start, &val_len);
			return (const char *)val_start;
		}
	}
	return NULL;
}

/* Forward declaration: lower_copy is defined later but used above. */
static char *lower_copy(const char *s, size_t len);

/* JS parity: parser/braces.js getSymbol() for {{...}} replacements. */
static char braces_get_symbol(const char *name, size_t len,
										const ParserConfig *cfg, bool *is_magic_out) {
	if(is_magic_out) *is_magic_out= false;
	if(!name || len == 0) return 't';

	/* JS parity: getSymbol(trimLc(removeComment(s))).
     * removeComment removes \0\d+[cn]\x7F markers before matching. */
	size_t cleaned_len= 0;
	char *cleaned= str_remove_comment(name, len, &cleaned_len);
	if(!cleaned) return 't';

	/* trim ASCII whitespace */
	size_t i= 0, j= cleaned_len;
	while(i < cleaned_len && isspace((unsigned char)cleaned[i])) i++;
	while(j > i && isspace((unsigned char)cleaned[j - 1])) j--;
	if(j <= i) {
		free(cleaned);
		return 't';
	}

	size_t n= j - i;
	char *trimmed= malloc(n + 1);
	if(!trimmed) {
		free(cleaned);
		return 't';
	}
	sz_copy(trimmed, cleaned + i, n);
	trimmed[n]= '\0';

	char *lc= lower_copy(trimmed, n);
	if(!lc) {
		free(cleaned);
		free(trimmed);
		return 't';
	}
	free(cleaned);

	const char *canonical= NULL;
	const char *base_orig= trimmed;
	size_t base_orig_len= n;
	char colon_ch= ':';
	const char *colon_orig= sz_find_byte(trimmed, n, &colon_ch);
	bool has_function_colon= (colon_orig != NULL);
	if(!has_function_colon && n >= 3) {
		for(size_t ci= 0; ci + 2 < n; ci++) {
			if((unsigned char)trimmed[ci] == 0xEF &&
			   (unsigned char)trimmed[ci + 1] == 0xBC &&
			   (unsigned char)trimmed[ci + 2] == 0x9A) {
				has_function_colon= true;
				break;
			}
		}
	}
	if(colon_orig && colon_orig > trimmed) {
		base_orig_len= (size_t)(colon_orig - trimmed);
	}

	char *base_orig_buf= NULL;
	if(base_orig_len != n) {
		base_orig_buf= malloc(base_orig_len + 1);
		if(base_orig_buf) {
			sz_copy(base_orig_buf, trimmed, base_orig_len);
			base_orig_buf[base_orig_len]= '\0';
			base_orig= base_orig_buf;
		}
	}

	const char *base_lc= lc;
	size_t base_lc_len= n;
	const char *colon= sz_find_byte(lc, n, &colon_ch);
	if(colon && colon > lc) {
		base_lc_len= (size_t)(colon - lc);
	}

	char *base_buf= NULL;
	if(base_lc_len != n) {
		base_buf= malloc(base_lc_len + 1);
		if(base_buf) {
			sz_copy(base_buf, lc, base_lc_len);
			base_buf[base_lc_len]= '\0';
			base_lc= base_buf;
		}
	}

	if(cfg) {
		canonical= str_map_get_exact(&cfg->parser_function_sensitive, trimmed, n);
		if(!canonical) {
			canonical= str_map_get_exact(&cfg->parser_function_insensitive, lc, n);
		}
		if(!canonical && base_orig && base_orig[0]) {
			canonical= str_map_get_exact(&cfg->parser_function_sensitive, base_orig, base_orig_len);
		}
		if(!canonical && base_lc && base_lc[0]) {
			canonical= str_map_get_exact(&cfg->parser_function_insensitive, base_lc, base_lc_len);
		}
	}

	char out= 't';
	if(n == 1 && lc[0] == '!') {
		out= '!';
		if(is_magic_out) *is_magic_out= true;
	} else if(n == 2 && lc[0] == '!' && lc[1] == '!') {
		out= '+';
	} else if(n == 2 && lc[0] == '(' && lc[1] == '!') {
		out= '{';
	} else if(n == 2 && lc[0] == '!' && lc[1] == ')') {
		out= '}';
	} else if(n == 2 && lc[0] == '!' && lc[1] == '-') {
		out= '-';
	} else if(n == 1 && lc[0] == '=') {
		out= '~';
		if(is_magic_out) *is_magic_out= true;
	} else if(n == 6 && sz_equal(lc, "server", 6) == sz_true_k) {
		out= 'm';
		if(is_magic_out) *is_magic_out= true;
	} else if((n > 9 && sz_equal(lc, "filepath:", 9) == sz_true_k) || (n > 8 && sz_equal(lc, "fullurl:", 8) == sz_true_k) || (n > 9 && sz_equal(lc, "fullurle:", 9) == sz_true_k) || (n > 13 && sz_equal(lc, "canonicalurl:", 13) == sz_true_k) || (n > 14 && sz_equal(lc, "canonicalurle:", 14) == sz_true_k)) {
		out= 'm';
		if(is_magic_out) *is_magic_out= true;
	} else if(n > 11 && sz_equal(lc, "#vardefine:", 11) == sz_true_k) {
		out= 'n';
		if(is_magic_out) *is_magic_out= true;
	} else if(lc[0] == '#') {
		if(is_magic_out) {
			bool is_var_hash = (cfg && canonical && canonical[0] && str_list_contains_ci(&cfg->variable, canonical, strlen(canonical)));
			if(has_function_colon || is_var_hash) *is_magic_out= true;
		}
	} else if(cfg && canonical && canonical[0]) {
		bool is_var= str_list_contains_ci(&cfg->variable, canonical, strlen(canonical));
		if((has_function_colon || is_var) && is_magic_out) *is_magic_out= true;
	} else if(cfg && base_lc && base_lc[0]) {
		const char *base_canonical= str_map_get_exact(&cfg->parser_function_insensitive, base_lc, base_lc_len);
		if(base_canonical) {
			bool is_var= str_list_contains_ci(&cfg->variable, base_canonical, strlen(base_canonical));
			if((has_function_colon || is_var) && is_magic_out) *is_magic_out= true;
		}
	}

	if(is_magic_out) {
		log_debug_env_token("WTC_DEBUG_STAGE_1", NULL,
								  "Get braces symbol: out=%c base_lc=%s is_magic_out=%d", out, base_lc, *is_magic_out);
	} else {
		log_debug_env_token("WTC_DEBUG_STAGE_1", NULL,
								  "Get braces symbol: out=%c base_lc=%s is_magic_out=NULL", out, base_lc);
	}
	free(base_buf);
	free(base_orig_buf);
	free(trimmed);
	free(lc);
	return out;
}

/*
 * parseBraces — stage 1 of the wikitext parsing pipeline.
 *
 * Handles: {{{arg}}}, {{template}}, [[link]], -{converter}-
 * For templates/args: builds a token and pushes a sentinel into the working string.
 * For links/converters: parks the full matched text in a link-stack and emits a
 *   numeric-only sentinel; these are restored before each convergence check so
 *   later stages (parseLinks, parseConverter) see them intact.
 */

static char *trim_copy(const char *s, size_t len) {
	if(!s) return NULL;
	size_t i= 0, j= len;
	while(i < j && isspace((unsigned char)s[i])) i++;
	while(j > i && isspace((unsigned char)s[j - 1])) j--;
	size_t n= j - i;
	char *out= malloc(n + 1);
	if(!out) return NULL;
	if(n > 0) sz_copy(out, s + i, n);
	out[n]= '\0';
	return out;
}

static char *trim_copy_n(const char *s, size_t len, size_t *out_n) {
	if(!s) { if(out_n) *out_n = 0; return NULL; }
	size_t i= 0, j= len;
	while(i < j && isspace((unsigned char)s[i])) i++;
	while(j > i && isspace((unsigned char)s[j - 1])) j--;
	size_t n= j - i;
	char *out= malloc(n + 1);
	if(!out) { if(out_n) *out_n = 0; return NULL; }
	if(n > 0) sz_copy(out, s + i, n);
	out[n]= '\0';
	if(out_n) *out_n = n;
	return out;
}

static char *lower_copy(const char *s, size_t len) {
	if(!s) return NULL;

	/* Use a precomputed lookup table + Stringzilla's sz_lookup for faster
	 * bulk lowercase transformation. This preserves the byte-wise tolower()
	 * semantics used previously (C locale/unsigned-char based). */
	const unsigned char *lut = fast_tolower_table();
	char *out= malloc(len + 1);
	if(!out) return NULL;
	sz_lookup(out, len, s, (const char *)lut);
	out[len]= '\0';
	return out;
}

static const char *parser_function_canonical(const ParserConfig *cfg, const char *name, size_t len) {
	if(!cfg || !name || len == 0) return NULL;

	size_t trimmed_len = 0;
	char *trimmed= trim_copy_n(name, len, &trimmed_len);
	if(!trimmed || trimmed_len == 0) {
		free(trimmed);
		return NULL;
	}

	const char *canonical= str_map_get_exact(&cfg->parser_function_sensitive, trimmed, trimmed_len);
	if(!canonical) {
		char *lc= lower_copy(trimmed, trimmed_len);
		if(lc) {
			canonical= str_map_get_exact(&cfg->parser_function_insensitive, lc, trimmed_len);
			free(lc);
		}
	}

	free(trimmed);
	return canonical;
}

/* Build an owned, printable modifier string from title_part[0..mod_len),
 * expanding c/n/s sentinels via accum token toString (JS afterBuild parity). */
static char *build_transclude_modifier(const char *title_part, size_t title_part_len,
																 size_t mod_len, const Accum *accum, size_t *out_len) {
	if(!title_part || mod_len == 0 || mod_len > title_part_len) {
		if(out_len) *out_len = 0;
		return NULL;
	}

	ThreadBuf *mod_tb = wiki_thread_buf_acquire_scratch();
	if(!mod_tb) {
		if(out_len) *out_len = 0;
		return NULL;
	}
	mod_tb->len = 0;

	size_t i = 0;
	while(i < mod_len) {
		size_t sl = 0;
					if(parse_sentinel_at_allowed(title_part, mod_len, i, "cns", 3, &sl)) {
			size_t j = i + 1;
			size_t idx = 0;
			while(j < mod_len && title_part[j] >= '0' && title_part[j] <= '9') {
				idx = idx * 10 + (size_t)(title_part[j] - '0');
				j++;
			}

			Token *sent_tok = accum ? accum_get(accum, idx) : NULL;
			if(sent_tok) {
				ThreadBuf *tok_tb = wiki_thread_buf_acquire_scratch();
				if(tok_tb) {
					token_to_string(sent_tok, tok_tb);
					wiki_thread_buf_append(mod_tb, (sz_string_view_t){ .start = tok_tb->buf, .length = tok_tb->len });
					wiki_thread_buf_release_scratch(tok_tb);
				} else {
					wiki_thread_buf_append(mod_tb, (sz_string_view_t){ .start = title_part + i, .length = sl });
				}
			} else {
				wiki_thread_buf_append(mod_tb, (sz_string_view_t){ .start = title_part + i, .length = sl });
			}
			i += sl;
			continue;
		}

		wiki_thread_buf_putc(mod_tb, title_part[i]);
		i++;
	}

	char *modifier = malloc(mod_tb->len + 1);
	if(modifier) {
		sz_copy(modifier, mod_tb->buf, mod_tb->len);
		modifier[mod_tb->len] = '\0';
		if(out_len) *out_len = mod_tb->len;
	} else {
		if(out_len) *out_len = 0;
	}

	wiki_thread_buf_release_scratch(mod_tb);
	return modifier;
}

/* Build a JS-shaped transclude/arg token and push to accum. */
/* part_is_named[k]: if non-NULL, overrides memchr-based named-param detection.
 * NULL means use memchr for all parts (state-machine call site where parts are
 * already correctly split).  build_from_inner passes a pre-computed array based
 * on whether '=' appeared in the raw (pre-restore) part — JS parity for
 * part.indexOf('=') being called before restore(). */
static Token *build_template_token(const char **parts_restored, const size_t *parts_lens,
											  size_t parts_count,
											  bool is_arg, const ParserConfig *cfg, Accum *accum,
											  const bool *part_is_named) {
	Token *t= token_new(is_arg ? TOKEN_ARG : TOKEN_TRANSCLUDE,
							  is_arg ? "arg" : "template");
	if(!t) return NULL;

	/* ArgToken: [arg-name, arg-default?, hidden*] */
	if(is_arg) {
		if(parts_count > 0 && parts_restored[0]) {
			Token *name_tok= token_new(TOKEN_ATOM, "arg-name");
			if(!name_tok) {
				token_free(t);
				return NULL;
			}

			token_append_text_n(name_tok, parts_restored[0], parts_lens[0]);
			token_append_child(t, name_tok);

			char *nm= trim_copy(parts_restored[0], parts_lens[0]);
					if(nm) t->name= nm;
		}

		if(parts_count > 1 && parts_restored[1]) {
			Token *def_tok= token_new(TOKEN_PLAIN, "arg-default");
			if(def_tok) {
				token_append_text_n(def_tok, parts_restored[1], parts_lens[1]);
				token_append_child(t, def_tok);
			}
		}

		for(size_t k= 2; k < parts_count; k++) {
			if(!parts_restored[k]) continue;
			Token *hidden= token_new(TOKEN_HIDDEN, "hidden");
			if(!hidden) continue;
			token_append_text_n(hidden, parts_restored[k], parts_lens[k]);
			token_append_child(t, hidden);
		}
		accum_push(accum, t);
		return t;
	}

	/* JS parity: TranscludeToken throws for invalid template names.
     * In this stage implementation, at minimum reject empty names after
     * removeComment()+trim so {{}} and {{   }} are not tokenized. */
	if(parts_count > 0 && parts_restored[0]) {
		size_t cleaned_len= 0;
		char *cleaned= str_remove_comment(parts_restored[0], parts_lens[0], &cleaned_len);
		if(!cleaned) {
			token_free(t);
			return NULL;
		}
		size_t i= 0, j= cleaned_len;
		while(i < cleaned_len && isspace((unsigned char)cleaned[i])) i++;
		while(j > i && isspace((unsigned char)cleaned[j - 1])) j--;
		free(cleaned);
		if(j <= i) {
			token_free(t);
			return NULL;
		}
	}

	const char *title_part= (parts_count > 0) ? parts_restored[0] : NULL;
	size_t title_part_len= (parts_count > 0) ? parts_lens[0] : 0;

	if(title_part && cfg) {
		/* JS argSubst parity: /^(?:\s|\0\d+[cn]\x7F)*\0\d+s\x7F/ */
		size_t lead = 0;
		while(lead < title_part_len) {
			if(isspace((unsigned char)title_part[lead])) {
				lead++;
				continue;
			}
			size_t sl = 0;
							if(parse_sentinel_at_allowed(title_part, title_part_len, lead, "cn", 2, &sl)) {
				lead += sl;
				continue;
			}
			break;
		}
		size_t s_sl = 0;
					if(lead < title_part_len && parse_sentinel_at_allowed(title_part, title_part_len, lead, "s", 1, &s_sl)) {
			size_t mod_len = lead + s_sl;
			size_t modifier_len = 0;
			char *modifier = build_transclude_modifier(title_part, title_part_len, mod_len, accum, &modifier_len);
			t->data.transclude.modifier = (sz_string_view_t){ .start = modifier, .length = modifier ? modifier_len : 0 };
			title_part += mod_len;
			title_part_len -= mod_len;
		} else {
			char colon_ch= ':';
			const char *colon= sz_find_byte(title_part, title_part_len, &colon_ch);
			if(colon) {
				size_t prefix_len= (size_t)(colon - title_part);
				size_t prefix_str_len = 0;
				char *prefix= trim_copy_n(title_part, prefix_len, &prefix_str_len);
				if(prefix && prefix_str_len > 0 && (str_list_contains_ci(&cfg->parser_function_subst, prefix, prefix_str_len)
								 || str_list_contains_ci(&cfg->parser_function_raw, prefix, prefix_str_len))) {
					/* JS parity: consume leading whitespace and c/n sentinels from the
					 * first argument after the modifier colon into the modifier slice. */
					size_t mt_len= 0;
					while(prefix_len + 1 + mt_len < title_part_len) {
						size_t p= prefix_len + 1 + mt_len;
						if(isspace((unsigned char)title_part[p])) {
							mt_len++;
							continue;
						}
						size_t sl= 0;
													if(parse_sentinel_at_allowed(title_part, title_part_len, p, "cn", 2, &sl)) {
							mt_len+= sl;
							continue;
						}
						break;
					}
					size_t mod_len= prefix_len + 1 + mt_len;
					size_t modifier_len = 0;
					char *modifier= build_transclude_modifier(title_part, title_part_len, mod_len, accum, &modifier_len);
					t->data.transclude.modifier = (sz_string_view_t){ .start = modifier, .length = modifier ? modifier_len : 0 };
					title_part= title_part + mod_len;
					title_part_len-= mod_len;
				}
				free(prefix);
			}
		}
	}

	bool transclude_is_magic= false;
	bool invoke_magic= false;
	size_t magic_title_len= 0;
	const char *magic_first_arg= NULL;
	size_t magic_first_arg_len= 0;
	size_t params_start_idx= 1;

	if(title_part && title_part_len > 0) {
		size_t p0_len= title_part_len;
		bool magic= false;
		bool allow_magic= (parts_count <= 1);
		if(!allow_magic) {
			char colon_ch= ':';
			allow_magic= sz_find_byte(title_part, p0_len, &colon_ch) != NULL;
		}
		if(allow_magic) {
			(void)braces_get_symbol(title_part, p0_len, cfg, &magic);
		}
		if(magic) {
			transclude_is_magic= true;
			magic_title_len= p0_len;
			char colon_ch= ':';
			const char *colon= sz_find_byte(title_part, p0_len, &colon_ch);
			if(colon) {
				magic_title_len= (size_t)(colon - title_part);
				magic_first_arg= colon + 1;
				magic_first_arg_len= p0_len - magic_title_len - 1;
			}

			t->subtype= TOKEN_SUBTYPE_MAGIC_WORD;
			size_t magic_clean_len= 0;
			char *magic_clean= str_remove_comment(title_part, magic_title_len, &magic_clean_len);
			char *magic_raw_name= magic_clean ? trim_copy(magic_clean, magic_clean_len) : NULL;
			free(magic_clean);
			const char *canonical= magic_raw_name ? parser_function_canonical(cfg, magic_raw_name, strlen(magic_raw_name)) : NULL;
			if(magic_raw_name && magic_raw_name[0] == '#' && !canonical) {
				/* JS parity: unknown hash parser-functions (for example
				 * {{#vardefine:...}} on configs that do not define it) are
				 * treated as invalid template names and left as plain text. */
				free(magic_raw_name);
				token_free(t);
				return NULL;
			}
			if(canonical) {
							t->name= strdup(canonical);
			} else {
				char *nm= magic_raw_name;
					if(nm) {
						size_t nm_len= strlen(nm);
						sz_lookup(nm, nm_len, nm, (const char *)fast_tolower_table());
										t->name= nm;
					}
			}
			if(canonical && magic_raw_name) free(magic_raw_name);
					if(t->name && strcmp(t->name, "invoke") == 0) invoke_magic= true;

			Token *mw_name= token_new(TOKEN_SYNTAX, "magic-word-name");
			if(mw_name) {
				token_append_text_n(mw_name, title_part, magic_title_len);
				token_append_child(t, mw_name);
			}
		} else {
			/* JS parity: template names are validated with normalizeTitle(..., 10,
			 * {halfParsed:true, temporary:true}) and throw on invalid input. */
			if(cfg) {
				size_t cleaned_len= 0;
				char *cleaned= str_remove_comment(title_part, p0_len, &cleaned_len);
				if(!cleaned) {
					token_free(t);
					return NULL;
				}
				size_t b= 0, e= cleaned_len;
				while(b < e && isspace((unsigned char)cleaned[b])) b++;
				while(e > b && isspace((unsigned char)cleaned[e - 1])) e--;
				bool leading_placeholder= false;
				/* JS parity: Title validity rejects %HH in the title part, but
				 * not inside fragment text after '#'. */
				size_t percent_check_end= e;
				for(size_t p= b; p < e; p++) {
					if(cleaned[p] == '#') {
						percent_check_end= p;
						break;
					}
				}
				for(size_t p= b; p + 2 < percent_check_end; p++) {
					if(cleaned[p] == '%' &&
					   isxdigit((unsigned char)cleaned[p + 1]) &&
					   isxdigit((unsigned char)cleaned[p + 2])) {
						free(cleaned);
						token_free(t);
						return NULL;
					}
				}
				if(e > b && (unsigned char)cleaned[b] == '\0') {
					size_t k= b + 1;
					while(k < e && isdigit((unsigned char)cleaned[k])) k++;
					if(k > b + 1 && k + 1 < e && (unsigned char)cleaned[k + 1] == '\x7F') {
						leading_placeholder= true;
					}
				}
				Title *parsed= NULL;
				if(e > b) parsed= title_parse_half_parsed(cleaned + b, e - b, 10, cfg, true, NULL);
				free(cleaned);
				if(!parsed || !parsed->valid || !parsed->title || (!parsed->title[0] && !leading_placeholder)) {
					unsigned char t0= (parsed && parsed->title) ? (unsigned char)parsed->title[0] : 0;
					log_debug_env_token("WTC_DEBUG_STAGE_1", NULL,
						"[C build_template_token] reject template name: parsed=%p valid=%d title_ptr=%p title0=%02X leading_placeholder=%d len=%zu",
						(void *)parsed,
						parsed ? (int)parsed->valid : -1,
						(void *)(parsed ? parsed->title : NULL),
						t0,
						(int)leading_placeholder,
						(e > b) ? (e - b) : 0);
					title_free(parsed);
					token_free(t);
					return NULL;
				}
				title_free(parsed);
			}

			Token *tpl_name= token_new(TOKEN_ATOM, "template-name");
			if(tpl_name) {
				token_append_text_n(tpl_name, title_part, p0_len);
				token_append_child(t, tpl_name);
			}

			/* JS parity: template name is set in afterBuild(), not during parseBraces.
             * refresh_template_name() in build.c handles this after build(). */
		}
	}

	size_t positional= 1;
	if(transclude_is_magic && magic_first_arg) {
		if(invoke_magic) {
			Token *mod_tok= token_new(TOKEN_ATOM, "invoke-module");
			if(mod_tok) {
				token_append_text_n(mod_tok, magic_first_arg, magic_first_arg_len);
				token_append_child(t, mod_tok);
			}
			if(parts_count > 1 && parts_restored[1]) {
				Token *fn_tok= token_new(TOKEN_ATOM, "invoke-function");
				if(fn_tok) {
					token_append_text_n(fn_tok, parts_restored[1], parts_lens[1]);
					token_append_child(t, fn_tok);
				}
				params_start_idx= 2;
			}
		} else {
			const char *part= magic_first_arg;
			size_t part_len= magic_first_arg_len;

			Token *param= token_new(TOKEN_PARAMETER, "parameter");
			if(param) {
				param->sep= '\0';

				Token *key_tok= token_new(TOKEN_PLAIN, "parameter-key");
				Token *val_tok= token_new(TOKEN_PLAIN, "parameter-value");
				if(key_tok && val_tok) {
					/* JS parity: the first parser-function argument after ':' is
					 * always positional, even if it contains '='. */
					token_append_child(param, key_tok);
					token_append_text_n(val_tok, part, part_len);
					token_append_child(param, val_tok);

					char *pname= strdup("1");
								if(pname) param->name= pname;
					positional= 2;
					token_append_child(t, param);
				} else {
					if(key_tok) token_free(key_tok);
					if(val_tok) token_free(val_tok);
					token_free(param);
				}
			}
		}
	}

	for(size_t k= params_start_idx; k < parts_count; k++) {
		if(!parts_restored[k]) continue;

		const char *part= parts_restored[k];
		size_t part_len= parts_lens[k];

		/* JS parity: for magic words, parameter splitting differs from templates.
		 * In TranscludeToken constructor:
		 *   if (!(templateLike || name==='switch' && j>0 || name==='tag' && j>1))
		 *     part = [part.join('=')]
		 * So non-template magic words are positional by default, except:
		 *   - #switch: params with j>0 may be key=value
		 *   - #tag: params with j>1 may be key=value
		 * Here k maps to j with an offset of params_start_idx. */
		bool allow_named_split= true;
			if(transclude_is_magic && t->name) {
				if(strcmp(t->name, "invoke") == 0) {
				/* JS parity: #invoke behaves template-like for remaining args,
				 * so key=value stays named (for example x=y). */
				allow_named_split= true;
				} else if(strcmp(t->name, "switch") == 0) {
				allow_named_split= (k >= params_start_idx);
				} else if(strcmp(t->name, "tag") == 0) {
				allow_named_split= (k > params_start_idx);
			} else {
				allow_named_split= false;
			}
		}

		/* JS parity: use pre-determined named/positional flag when available.
         * part_is_named[k]==false means the raw part had no '=', so even if
         * the restored text contains '=' (e.g. from [[=]]), it is positional. */
		char eq_ch= '=';
		const char *eq= (!allow_named_split || (part_is_named && !part_is_named[k]))
							 ? NULL
							 : sz_find_byte(part, part_len, &eq_ch);

		Token *param= token_new(TOKEN_PARAMETER, "parameter");
		if(!param) continue;
		param->sep= '\0';

		Token *key_tok= token_new(TOKEN_PLAIN, "parameter-key");
		Token *val_tok= token_new(TOKEN_PLAIN, "parameter-value");
		if(!key_tok || !val_tok) {
			if(key_tok) token_free(key_tok);
			continue;
		}

		if(eq) {
			size_t key_len= (size_t)(eq - part);
			size_t val_len= part_len - key_len - 1;
			const char *key_view= part;
			const char *val_view= eq + 1;
			{
				char _vhbuf[128];
				size_t _vhp= 0;
				size_t _vls= (val_len > 86) ? 86 : 0;
				size_t _vle= (_vls + 6 < val_len) ? _vls + 6 : val_len;
				for(size_t _qi= _vls; _qi < _vle && _vhp + 3 < sizeof(_vhbuf); _qi++) {
					int _wn= snprintf(_vhbuf + _vhp, sizeof(_vhbuf) - _vhp, "%02X", (unsigned char)val_view[_qi]);
					if(_wn > 0) _vhp+= (size_t)_wn;
					if(_qi + 1 < _vle && _vhp < sizeof(_vhbuf)) _vhbuf[_vhp++]= ' ';
				}
				_vhbuf[_vhp]= '\0';
				log_debug_env_token("WTC_DEBUG_STAGE_1", NULL,
										  "[C build_tpl_named] val_view=%p val_len=%zu bytes_at[%zu..%zu]=%s",
										  (void *)val_view, val_len, _vls, _vle, _vhbuf);
			}
			token_append_text_n(key_tok, key_view, key_len);
			token_append_text_n(val_tok, val_view, val_len);
			token_append_child(param, key_tok);
			token_append_child(param, val_tok);

			size_t key_clean_len= 0;
			char *key_clean= str_remove_comment(part, key_len, &key_clean_len);
			char *pname= key_clean ? trim_copy(key_clean, key_clean_len) : NULL;
			free(key_clean);
				   if(pname) param->name= pname;
		} else {
			token_append_child(param, key_tok);
			token_append_text_n(val_tok, part, part_len);
			token_append_child(param, val_tok);

			char idx_buf[32];
			int n= snprintf(idx_buf, sizeof(idx_buf), "%zu", positional++);
			if(n > 0) {
				// strdup handles the malloc and the copy in one go.
				// It is safe because it only copies the exact length of the string.
				char *pname= strdup(idx_buf);
				if(pname) {
								param->name= pname;
				}
			}
		}
		token_append_child(t, param);
	}

	accum_push(accum, t);
	return t;
}

/* Helper: split inner content on '|', restore link-stack entries in each part,
 * and build a template/arg token.  Uses do-while to handle trailing '|'. */
static Token *build_from_inner(const char *inner, size_t inner_len,
										 bool is_arg,
										 char **link_stack, size_t link_count,
										 const size_t *link_stack_lens,
										 const ParserConfig *cfg,
										 Accum *accum) {
	size_t part_cap= 8, part_count= 0;
	char **parts= malloc(part_cap * sizeof(char *));
	size_t *plens= malloc(part_cap * sizeof(size_t));
	assert(parts && plens);

	/* JS parity: indexOf('=') on raw (pre-restore) part. Track per-part. */
	bool *parts_named= NULL;
	size_t parts_named_cap= 8;
	parts_named= malloc(parts_named_cap * sizeof(bool));
	assert(parts_named);

	size_t si= 0;
	do {
		size_t j= si;
		while(j < inner_len && inner[j] != '|') j++;
		size_t plen= j - si;
		const char *raw= inner + si;

		char *restored;
		size_t restored_len= 0;
		bool is_named= false;

		if(part_count > 0) {
			/* JS parity: part.indexOf('=') on the raw (sentinel-containing) part
             * before restore() so '=' inside [[=]] sentinels is never detected
             * as a named-parameter separator. */
			char eq_ch= '=';
			const char *eq_in_raw= sz_find_byte(raw, plen, &eq_ch);
			if(eq_in_raw) {
				is_named= true;
				/* Named param: restore key and value separately, join with '=' */
				size_t key_raw_len= (size_t)(eq_in_raw - raw);
				size_t val_raw_len= plen - key_raw_len - 1;
				size_t key_len= 0, val_len= 0;
				char *key= str_restore(raw, key_raw_len,
											  (const char **)link_stack, link_count, link_stack_lens, &key_len);
				char *val= str_restore(eq_in_raw + 1, val_raw_len,
											  (const char **)link_stack, link_count, link_stack_lens, &val_len);
				restored_len= key_len + 1 + val_len;
				restored= malloc(restored_len + 1);
				assert(restored);
				sz_copy(restored, key, key_len);
				restored[key_len]= '=';
				sz_copy(restored + key_len + 1, val, val_len);
				restored[restored_len]= '\0';
				free(key);
				free(val);
			} else {
				restored= str_restore(raw, plen, (const char **)link_stack, link_count, link_stack_lens, &restored_len);
			}
		} else {
			restored= str_restore(raw, plen, (const char **)link_stack, link_count, link_stack_lens, &restored_len);
		}

		if(part_count >= parts_named_cap) {
			parts_named_cap*= 2;
			parts_named= realloc(parts_named, parts_named_cap * sizeof(bool));
			assert(parts_named);
		}
		parts_named[part_count]= is_named;

		if(part_count >= part_cap) {
			part_cap*= 2;
			parts= realloc(parts, part_cap * sizeof(char *));
			plens= realloc(plens, part_cap * sizeof(size_t));
			assert(parts && plens);
		}
		/* Debug: log restored part bytes if it contains sentinel-like bytes */
		{
			char needle= '\x7F';
			const char *has_del= sz_find_byte(restored, restored_len, &needle);
			char nul= '\0';
			const char *has_nul= sz_find_byte(restored, restored_len, &nul);
			if(has_del || has_nul) {
				char hexbuf[256];
				size_t hexpos= 0;
				size_t look= restored_len > 64 ? 64 : restored_len;
				for(size_t ii= 0; ii < look && hexpos + 3 < sizeof(hexbuf); ii++) {
					int wn= snprintf(hexbuf + hexpos, sizeof(hexbuf) - hexpos, "%02X", (unsigned char)restored[ii]);
					if(wn > 0) hexpos+= (size_t)wn;
					if(ii + 1 < look && hexpos + 1 < sizeof(hexbuf)) hexbuf[hexpos++]= ' ';
				}
				hexbuf[hexpos]= '\0';
				log_debug_env_token("WTC_DEBUG_STAGE_1", NULL,
										  "[C build_from_inner] part %zu restored_len=%zu preview_hex=%s",
										  part_count, restored_len, hexbuf);
			}
		}
		parts[part_count]= restored;
		plens[part_count]= restored_len;
		part_count++;
		if(j >= inner_len) break;
		si= j + 1;
	} while(1);

	Token *tok= build_template_token((const char **)parts, plens, part_count, is_arg, cfg, accum, parts_named);
	free(parts_named);
	for(size_t p= 0; p < part_count; p++) free(parts[p]);
	free(parts);
	free(plens);
	return tok;
}

/* JS parity for triple-brace close path:
 * if removeComment(argParts[1]).trim().endsWith(':') and base is in subst list,
 * sentinel is 's' instead of 'a'. */
typedef struct {
	char **items;
	size_t *lens;
	bool *named;
	size_t count;
	size_t cap;
} PartList;

static bool parts_init(PartList *parts) {
	if(!parts) return false;
	parts->items= NULL;
	parts->lens= NULL;
	parts->named= NULL;
	parts->count= 0;
	parts->cap= 0;
	return true;
}

static void parts_free(PartList *parts) {
	if(!parts) return;
	for(size_t i= 0; i < parts->count; i++) {
		free(parts->items[i]);
	}
	free(parts->items);
	free(parts->lens);
	free(parts->named);
	parts->items= NULL;
	parts->lens= NULL;
	parts->named= NULL;
	parts->count= 0;
	parts->cap= 0;
}

static bool parts_grow(PartList *parts) {
	if(parts->count + 1 > parts->cap) {
		size_t cap= parts->cap ? parts->cap * 2 : 4;
		char **items= realloc(parts->items, cap * sizeof(char *));
		size_t *lens= realloc(parts->lens, cap * sizeof(size_t));
		bool *named= realloc(parts->named, cap * sizeof(bool));
		if(!items || !lens || !named) return false;
		parts->items= items;
		parts->lens= lens;
		parts->named= named;
		parts->cap= cap;
	}
	return true;
}

static bool parts_add_empty(PartList *parts) {
	if(!parts_grow(parts)) return false;
	parts->items[parts->count]= malloc(1);
	if(!parts->items[parts->count]) return false;
	parts->items[parts->count][0]= '\0';
	parts->lens[parts->count]= 0;
	parts->named[parts->count]= false;
	parts->count++;
	return true;
}

static bool parts_append_text(PartList *parts, const char *s, size_t len) {
	if(!parts) return false;
	if(parts->count == 0) {
		if(!parts_add_empty(parts)) return false;
	}
	size_t idx= parts->count - 1;
	size_t old_len= parts->lens[idx];
	char *new_buf= realloc(parts->items[idx], old_len + len + 1);
	if(!new_buf) return false;
	parts->items[idx]= new_buf;
	sz_copy(new_buf + old_len, s, len);
	new_buf[old_len + len]= '\0';
	parts->lens[idx]= old_len + len;
	return true;
}

typedef struct {
	char *open;
	size_t open_len;
	size_t index;
	size_t pos;
	bool find_equal;
	PartList parts;
	bool has_parts;
	size_t out_mark;    /* out_len corresponding to source offset `index` */
	bool has_out_mark;
} BraceFrame;

static bool brace_frame_init(BraceFrame *frame, const char *open, size_t open_len, size_t index, size_t pos, bool find_equal) {
	if(!frame) return false;
	frame->out_mark= 0;
	frame->has_out_mark= false;
	frame->open= malloc(open_len + 1);
	if(!frame->open) return false;
	sz_copy(frame->open, open, open_len);
	frame->open[open_len]= '\0';
	frame->open_len= open_len;
	frame->index= index;
	frame->pos= pos;
	frame->find_equal= find_equal;
	frame->has_parts= (open_len > 0 && open[0] == '{');
	if(frame->has_parts) {
		/* initialise parts and ensure we clean up on failure */
		parts_init(&frame->parts);
		if(!parts_add_empty(&frame->parts)) {
			/* parts_add_empty failed: free open and any partial parts allocations */
			free(frame->open);
			frame->open= NULL;
			parts_free(&frame->parts);
			return false;
		}
	} else {
		parts_init(&frame->parts);
	}
	return true;
}

static void brace_frame_free(BraceFrame *frame) {
	if(!frame) return;
	free(frame->open);
	frame->open= NULL;
	parts_free(&frame->parts);
}

static bool brace_push_part(BraceFrame *frame, const char *buf, size_t from, size_t to,
									 const char **link_stack, size_t link_count,
									 const size_t *link_stack_lens) {
	if(!frame || !frame->has_parts) return true;
	size_t len= to > from ? to - from : 0;
	if(len == 0) return true;
	size_t restored_len= 0;
	char *restored= str_restore(buf + from, len,
										 (const char **)link_stack, link_count,
										 link_stack_lens, &restored_len);
	if(!restored) return false;
	bool ok= parts_append_text(&frame->parts, restored, restored_len);
	free(restored);
	return ok;
}

static bool brace_frame_join_inner(const BraceFrame *frame, char **out, size_t *out_len) {
	if(!frame || !out || !out_len) return false;
	if(!frame->has_parts) return false;
	size_t total= 0;
	for(size_t i= 0; i < frame->parts.count; i++) {
		total+= frame->parts.lens[i];
		if(i + 1 < frame->parts.count) total+= 1;
	}
	char *buf= malloc(total + 1);
	if(!buf) return false;
	size_t pos= 0;
	for(size_t i= 0; i < frame->parts.count; i++) {
		sz_copy(buf + pos, frame->parts.items[i], frame->parts.lens[i]);
		pos+= frame->parts.lens[i];
		if(i + 1 < frame->parts.count) {
			buf[pos++]= '|';
		}
	}
	buf[pos]= '\0';
	*out= buf;
	*out_len= pos;
	return true;
}

static bool brace_frame_append_part(BraceFrame *frame) {
	if(!frame || !frame->has_parts) return false;
	return parts_add_empty(&frame->parts);
}

static void brace_frame_mark_named_current(BraceFrame *frame) {
	if(!frame || !frame->has_parts || frame->parts.count == 0) return;
	frame->parts.named[frame->parts.count - 1]= true;
}

static char braces_arg_symbol(const char *inner, size_t inner_len, const ParserConfig *cfg);

static bool braces_state_machine(ThreadBuf *tb, const ParserConfig *cfg,
											Accum *accum, char **link_stack, size_t link_count,
										const size_t *link_stack_lens,
										bool allow_heading) {
	if(!tb || !tb->buf) return false;
	BraceFrame *stack= malloc(64 * sizeof(BraceFrame));
	if(!stack) {
		return false;
	}
	size_t stack_cap= 64;
	size_t stack_len= 0;

	size_t search_at= 0;
	size_t next_write= 0;
	size_t out_cap= tb->len * 2 + 64;
	char *out= malloc(out_cap);
	assert(out);
	size_t out_len= 0;

#define ENSURE_OUT_CAP(need)               \
	do {                                    \
		while(out_len + (need) >= out_cap) { \
			out_cap*= 2;                      \
			out= realloc(out, out_cap);       \
			assert(out);                      \
		}                                    \
	} while(0)

	while(1) {
		/* Scan for the next brace-related event. */
		size_t event_pos = search_at;
		BraceEventKind evkind = 0;
		size_t match_len = 0, brace_count = 0, equals_count = 0, sentinel_len = 0;
		bool matched = brace_event_next(tb->buf, tb->len, &event_pos,
										&evkind, &match_len, &brace_count,
										&equals_count, &sentinel_len);

		/* JS parity: heading frames can close at EOF without a trailing newline. */
		if(!matched && stack_len > 0 && stack[stack_len - 1].open_len == 1 && stack[stack_len - 1].open[0] == '=') {
			matched= true;
			evkind= BRACE_EVT_NEWLINE;
			event_pos= tb->len;
			match_len= 0;
			brace_count= 0;
			equals_count= 0;
			sentinel_len= 0;
		}

		if(!matched) {
			break;
		}

		size_t cur_index;
		size_t syntax_start;
		size_t syntax_end;
		size_t match_start = 0;
		if(matched) {
			size_t match_end = event_pos;
			match_start = match_end - match_len;
			bool has_heading_prefix = (evkind == BRACE_EVT_HEADING_OPEN && sentinel_len > 0);
			if(has_heading_prefix) {
				syntax_start = match_start + sentinel_len;
				syntax_end = match_end;
				cur_index = syntax_start;
			} else {
				syntax_start = match_start;
				syntax_end = match_end;
				cur_index = match_start;
			}
		} else {
			cur_index = tb->len;
			syntax_start = cur_index;
			syntax_end = cur_index;
		}

		size_t syntax_len = syntax_end > syntax_start ? syntax_end - syntax_start : 0;
		const char *syntax = tb->buf + syntax_start;
		BraceFrame top;
		bool has_top = false;
		bool top_requeued = false;
		bool top_consumed = false;
		if(stack_len > 0) {
			top = stack[stack_len - 1];
			has_top = true;
			stack_len--;
		}

		/* JS parity: when a template/arg frame has findEqual=true, a line-start
		 * single '=' token is handled as an inner named-parameter separator,
		 * not as heading-open. Multi-'=' line-start sequences still open heading
		 * frames. */
		if(matched && evkind == BRACE_EVT_HEADING_OPEN && equals_count == 1 && has_top && top.find_equal) {
			evkind= BRACE_EVT_EQUALS;
		}

		if(matched && evkind == BRACE_EVT_WIKILINK_CLOSE) {
			/* ]] closes a [[ link frame; preserve any non-link frame below it */
			if(has_top && !(top.open_len >= 1 && top.open[0] == '[')) {
				stack[stack_len++]= top;
				top_requeued= true;
			}
		} else if(matched && evkind == BRACE_EVT_CONVERTER_CLOSE) {
			/* }- closes a -{ converter frame; preserve any non-converter frame below it */
			if(has_top && !(top.open_len >= 1 && top.open[0] == '-')) {
				stack[stack_len++]= top;
				top_requeued= true;
			}
		} else if(matched && evkind == BRACE_EVT_NEWLINE) {
			if(has_top && top.open_len == 1 && top.open[0] == '=') {
				/* Only materialize heading tokens when no outer frame is active.
				 * Nested template values are reparsed recursively later. */
				if(stack_len == 0) {
				const char *slice= tb->buf + top.index;
				size_t slice_len= cur_index - top.index;
				HeadingLineResult hr;
				if(heading_line_parse(slice, slice_len, &hr)) {
					size_t title_start = (size_t)(hr.content - slice);
					size_t title_len = hr.content_len;
					size_t trail_start = (size_t)(hr.trail - slice);
					size_t trail_len = hr.trail_len;
					char *title = str_restore(slice + title_start, title_len, (const char **)link_stack, link_count, link_stack_lens, &title_len);
					if(title) {
						char nl= '\n';
						bool restored_has_newline= sz_find_byte(title, title_len, &nl) != NULL;
						if(!restored_has_newline) {
							Token *heading_tok= token_new(TOKEN_HEADING, "heading");
							if(heading_tok) {
								heading_tok->data.heading.level= (int)hr.eq_count;
								Token *title_tok= token_new(TOKEN_PLAIN, "heading-title");
								if(title_tok) {
										token_append_text_n(title_tok, title, title_len);
									token_append_child(heading_tok, title_tok);
									Token *trail_tok= token_new(TOKEN_SYNTAX, "heading-trail");
									if(trail_tok) {
										if(trail_len > 0) {
												token_append_text_n(trail_tok, slice + trail_start, trail_len);
										} else {
											token_append_text_n(trail_tok, "", 0);
										}
										token_append_child(heading_tok, trail_tok);
									}
									accum_push(accum, heading_tok);
									size_t idx= accum->count - 1;
									char sent[64];
									size_t slen;
									work_str_sentinel(idx, 'h', sent, &slen);
									{
										char hexbuf[128];
										size_t hexpos= 0;
										for(size_t _i= 0; _i < slen && hexpos + 3 < sizeof(hexbuf); _i++) {
											int wn= snprintf(hexbuf + hexpos, sizeof(hexbuf) - hexpos, "%02X", (unsigned char)sent[_i]);
											if(wn > 0) hexpos+= (size_t)wn;
											if(_i + 1 < slen && hexpos + 1 < sizeof(hexbuf)) hexbuf[hexpos++]= ' ';
										}
										hexbuf[hexpos]= '\0';
										log_debug_env_token("WTC_DEBUG_STAGE_1", NULL,
														  "[C parseBraces] wrote sentinel idx=%zu type=%c slen=%zu hex=%s",
														  idx, 'h', slen, hexbuf);
									}
									ENSURE_OUT_CAP((top.index > next_write ? top.index - next_write : 0) + slen);
									if(top.index > next_write) {
										sz_copy(out + out_len, tb->buf + next_write, top.index - next_write);
										out_len+= top.index - next_write;
									}
									sz_copy(out + out_len, sent, slen);
									out_len+= slen;
									next_write= cur_index;
								} else {
									token_free(heading_tok);
								}
							}
						}
						free(title);
					}
				}
				}
			} else if(has_top) {
				/* \n only closes = heading frames; preserve other frames */
				stack[stack_len++]= top;
				top_requeued= true;
			}
		} else {
			/* JS parity: split on '|' and on a single '=' right after '|'
			 * (tracked by frame.find_equal), unless a heading frame intercepts. */
			bool inner_equal = matched && evkind == BRACE_EVT_EQUALS && has_top && top.find_equal;
			if(matched && (evkind == BRACE_EVT_PIPE || inner_equal)) {
				if(has_top && top.has_parts) {
					if(!brace_push_part(&top, tb->buf, top.pos, cur_index, (const char **)link_stack, link_count, link_stack_lens)) {
						;
					}
					if(evkind == BRACE_EVT_PIPE) {
						brace_frame_append_part(&top);
						top.find_equal= true;
					} else {
						parts_append_text(&top.parts, "=", 1);
						brace_frame_mark_named_current(&top);
						top.find_equal= false;
					}
					top.pos= cur_index + 1;
					/* Keep the updated template frame active for subsequent parts. */
					stack[stack_len++]= top;
					top_requeued= true;
				} else if(has_top) {
					/* Keep non-template frames (e.g. [[, -{, =) alive across '|'. */
					stack[stack_len++]= top;
					top_requeued= true;
				}
			} else if(matched && evkind == BRACE_EVT_BRACE_CLOSE) {
				if(has_top && top.has_parts) {
					if(!brace_push_part(&top, tb->buf, top.pos, cur_index, (const char **)link_stack, link_count, link_stack_lens)) {
						;
					}
					size_t close_len= brace_count;
					size_t max_close= top.open_len < 3 ? top.open_len : 3;
					if(close_len > max_close) close_len= max_close;
					/* JS parity: close.length is min(open.length, 3), and regex.lastIndex
					 * advances by that consumed length only. This lets any remaining '}'
					 * in an overlong run close outer frames in subsequent iterations. */
					syntax_end= cur_index + close_len;
					bool is_arg_close= (close_len == 3);
					size_t rest= top.open_len > close_len ? top.open_len - close_len : 0;
					char *inner= NULL;
					size_t inner_len= 0;
					if(!brace_frame_join_inner(&top, &inner, &inner_len)) {
						inner= NULL;
						inner_len= 0;
					}
					Token *tok= build_template_token((const char **)top.parts.items, top.parts.lens, top.parts.count, is_arg_close, cfg, accum, top.parts.named);
					if(!tok) {
						size_t p0_len= top.parts.count > 0 ? top.parts.lens[0] : 0;
						const char *p0= top.parts.count > 0 ? top.parts.items[0] : NULL;
						char nul= '\0';
						bool p0_has_nul= p0 && sz_find_byte(p0, p0_len, &nul) != NULL;
						log_debug_env_token("WTC_DEBUG_STAGE_1", NULL,
							"[C parseBraces] close rejected: is_arg_close=%d open_len=%zu parts_count=%zu p0_len=%zu p0_has_nul=%d",
							(int)is_arg_close, top.open_len, top.parts.count, p0_len, (int)p0_has_nul);
					}
					if(tok) {
						size_t tok_idx= accum->count - 1;
						char sent[64];
						size_t slen;
						char sym= 't';
						if(is_arg_close) {
							if(inner) sym= braces_arg_symbol(inner, inner_len, cfg);
						} else if(top.parts.count > 0) {
							sym= braces_get_symbol(top.parts.items[0], top.parts.lens[0], cfg, NULL);
						}
						work_str_sentinel(tok_idx, sym, sent, &slen);
						size_t rep_start= top.index + rest;
						size_t rep_end= cur_index + close_len;

						/* Nested close can overlap text already emitted by an inner close.
						 * Roll output back to the open-frame mark so outer replacement
						 * replaces rather than appends duplicated content. */
						if(rep_start < next_write && top.has_out_mark) {
							size_t rollback= top.out_mark;
							if(rollback > out_len) rollback= out_len;
							out_len= rollback;
							next_write= top.index;
							if(rep_start > next_write) {
								ENSURE_OUT_CAP(rep_start - next_write);
								sz_copy(out + out_len, tb->buf + next_write, rep_start - next_write);
								out_len+= rep_start - next_write;
								next_write= rep_start;
							} else {
								next_write= rep_start;
							}
						}

						ENSURE_OUT_CAP((rep_start > next_write ? rep_start - next_write : 0) + slen);
						if(rep_start > next_write) {
							sz_copy(out + out_len, tb->buf + next_write, rep_start - next_write);
							out_len+= rep_start - next_write;
						}
						sz_copy(out + out_len, sent, slen);
						out_len+= slen;
						next_write= rep_end;
						if(rest > 1) {
							BraceFrame child;
							if(brace_frame_init(&child, top.open, rest, top.index, rep_end, false)) {
								bool child_ok = true;
								/* JS parity: when an overlong opener (for example '{{{{{') closes
								 * as an inner token, the remaining outer frame starts with that
								 * emitted sentinel as its first part. */
								if(child.has_parts) {
									child_ok = parts_append_text(&child.parts, sent, slen);
								}
								if(child_ok) {
								if(top.has_out_mark) {
									child.out_mark= top.out_mark;
									child.has_out_mark= true;
								}
								stack[++stack_len - 1]= child;
								} else {
									brace_frame_free(&child);
								}
							}
						} else if(rest == 1 && top.index > 0 && tb->buf[top.index - 1] == '-') {
							BraceFrame child;
							if(brace_frame_init(&child, "-{", 2, top.index - 1, top.index + 1, false)) {
								stack[++stack_len - 1]= child;
							}
						}
					} else {
						/* Invalid {{...}} (e.g. empty template name): keep raw text and
						 * only consume as many closing braces as this frame owns so
						 * adjacent outer closes are still available to parse. */
						size_t close_consume= close_len;
						size_t rep_end= cur_index + close_consume;
						if(rep_end > next_write) {
							ENSURE_OUT_CAP(rep_end - next_write);
							sz_copy(out + out_len, tb->buf + next_write, rep_end - next_write);
							out_len+= rep_end - next_write;
						}
						next_write= rep_end;
						syntax_end= rep_end;
					}
					free(inner);
					top_consumed= true;
				} else if(has_top) {
					/* Keep non-template frames when encountering stray '}}'. */
					stack[stack_len++]= top;
					top_requeued= true;
				}
			}
			if(matched && evkind == BRACE_EVT_BRACE_OPEN) {
				if(cur_index > next_write) {
					ENSURE_OUT_CAP(cur_index - next_write);
					sz_copy(out + out_len, tb->buf + next_write, cur_index - next_write);
					out_len+= cur_index - next_write;
					next_write= cur_index;
				}
				size_t frame_out_mark= out_len;
				BraceFrame frame;
				if(brace_frame_init(&frame, syntax, syntax_len, cur_index, cur_index + syntax_len, false)) {
					frame.out_mark= frame_out_mark;
					frame.has_out_mark= true;
					if(has_top) {
						stack[++stack_len - 1]= top;
						top_requeued= true;
					}
					if(stack_len + 1 > stack_cap) {
						size_t new_cap= stack_cap * 2;
						BraceFrame *new_stack= realloc(stack, new_cap * sizeof(BraceFrame));
						assert(new_stack);
						stack= new_stack;
						stack_cap= new_cap;
					}
					stack[stack_len++]= frame;
				} else {
					if(has_top) {
						stack[++stack_len - 1]= top;
						top_requeued= true;
					}
				}
			} else if(matched && evkind == BRACE_EVT_HEADING_OPEN && allow_heading) {
				/* Track heading lines so newline can close and tokenize them. */
				BraceFrame heading_frame;
				if(brace_frame_init(&heading_frame, "=", 1, cur_index, cur_index + syntax_len, false)) {
					if(has_top) {
						stack[++stack_len - 1]= top;
						top_requeued= true;
					}
					if(stack_len + 1 > stack_cap) {
						size_t new_cap= stack_cap * 2;
						BraceFrame *new_stack= realloc(stack, new_cap * sizeof(BraceFrame));
						assert(new_stack);
						stack= new_stack;
						stack_cap= new_cap;
					}
					stack[stack_len++]= heading_frame;
				} else {
					if(has_top) {
						stack[stack_len++]= top;
						top_requeued= true;
					}
				}
			} else if(matched && evkind == BRACE_EVT_WIKILINK_OPEN) {
				/* Push a link frame so | inside [[...]] is not treated as template separator */
				BraceFrame link_frame;
				if(brace_frame_init(&link_frame, "[", 1, cur_index, cur_index + syntax_len, false)) {
					if(has_top) {
						stack[++stack_len - 1]= top;
						top_requeued= true;
					}
					if(stack_len + 1 > stack_cap) {
						size_t new_cap= stack_cap * 2;
						BraceFrame *new_stack= realloc(stack, new_cap * sizeof(BraceFrame));
						assert(new_stack);
						stack= new_stack;
						stack_cap= new_cap;
					}
					stack[stack_len++]= link_frame;
				} else {
					if(has_top) {
						stack[stack_len++]= top;
						top_requeued= true;
					}
				}
			} else if(matched && evkind == BRACE_EVT_CONVERTER_OPEN) {
				/* Push a converter frame so | inside -{...}- is not treated as template separator */
				BraceFrame conv_frame;
				if(brace_frame_init(&conv_frame, "-", 1, cur_index, cur_index + syntax_len, false)) {
					if(has_top) {
						stack[++stack_len - 1]= top;
						top_requeued= true;
					}
					if(stack_len + 1 > stack_cap) {
						size_t new_cap= stack_cap * 2;
						BraceFrame *new_stack= realloc(stack, new_cap * sizeof(BraceFrame));
						assert(new_stack);
						stack= new_stack;
						stack_cap= new_cap;
					}
					stack[stack_len++]= conv_frame;
				} else {
					if(has_top) {
						stack[stack_len++]= top;
						top_requeued= true;
					}
				}
			} else {
				if(has_top && !top_requeued) {
					if(!top_consumed) {
						stack[stack_len++]= top;
						top_requeued= true;
					}
				}
			}
		}

		/* If we popped a frame into `top` but didn't requeue it, free it
		 * now to avoid leaking its heap allocations (open, parts). */
		if(has_top && !top_requeued) {
			brace_frame_free(&top);
		}

		search_at= syntax_end;
		if(matched && search_at == match_start) search_at= match_start + 1;
	}

	if(next_write < tb->len) {
		ENSURE_OUT_CAP(tb->len - next_write + 1);
		sz_copy(out + out_len, tb->buf + next_write, tb->len - next_write);
		out_len+= tb->len - next_write;
	}
	out[out_len]= '\0';
	wiki_thread_buf_set(tb, out, out_len);

	free(out);
	/* Free any remaining frames stored in the stack to avoid leaking their
	 * inner allocations (open, parts). */
	for(size_t si= 0; si < stack_len; si++) {
		brace_frame_free(&stack[si]);
	}
	free(stack);
	return true;
}

static char braces_arg_symbol(const char *inner, size_t inner_len, const ParserConfig *cfg) {
	if(!inner || inner_len == 0 || !cfg) return 'a';

	/* Extract the second arg part (between first and second '|'). */
	size_t p= 0;
	while(p < inner_len && inner[p] != '|') p++;
	if(p >= inner_len) return 'a';
	size_t vstart= p + 1;
	size_t vend= vstart;
	while(vend < inner_len && inner[vend] != '|') vend++;
	if(vend <= vstart) return 'a';

	size_t cleaned_len= 0;
	char *cleaned= str_remove_comment(inner + vstart, vend - vstart, &cleaned_len);
	if(!cleaned) return 'a';

	/* trim */
	size_t i= 0, j= cleaned_len;
	while(i < j && isspace((unsigned char)cleaned[i])) i++;
	while(j > i && isspace((unsigned char)cleaned[j - 1])) j--;
	if(j <= i) {
		free(cleaned);
		return 'a';
	}

	if(cleaned[j - 1] != ':') {
		free(cleaned);
		return 'a';
	}

	size_t base_len= j - i - 1;
	if(base_len == 0) {
		free(cleaned);
		return 'a';
	}

	char *base= malloc(base_len + 1);
	if(!base) {
		free(cleaned);
		return 'a';
	}
	sz_lookup(base, base_len, cleaned + i, (const char *)fast_tolower_table());
	base[base_len]= '\0';
	free(cleaned);

	char sym= 'a';
	for(size_t n= 0; n < cfg->parser_function_subst.count; n++) {
		sz_ptr_t s;
		sz_size_t s_len;
		sz_string_range(&cfg->parser_function_subst.items[n], &s, &s_len);
		if(!s) continue;
		if(s_len == base_len && sz_equal(s, base, base_len) == sz_true_k) {
			sym= 's';
			break;
		}
	}
	free(base);
	return sym;
}
/* Pre-pass for simple innermost {{{...}}} arguments.
 * Mirrors the JS reReplace behavior for non-nested triple-brace arguments. */
// TODO:  Can this be changed to a Single-Pass Deterministic Finite Automaton (DFA) style lexer?
// Or a State Machine Lexer

typedef struct {
	const ParserConfig *cfg;
	Accum *accum;
	ThreadBuf *out_tb;
} BracesContext;

static void braces_append(BracesContext *ctx, const char *data, size_t len) {
	wiki_thread_buf_append(ctx->out_tb, (sz_string_view_t){.start= data, .length= len});
}

static void braces_callback(const char *segment, size_t len,
									 ParserSegmentKind kind, void *user_data) {
	BracesContext *ctx= (BracesContext *)user_data;
	if(kind != PARSER_SEG_INNER) {
		braces_append(ctx, segment, len);
		return;
	}

	Token *tok= build_from_inner(segment, len, true, NULL, 0, NULL, ctx->cfg, ctx->accum);
	if(tok) {
		char sentinel[64];
		size_t slen;
		char sym= braces_arg_symbol(segment, len, ctx->cfg);
		work_str_sentinel(ctx->accum->count - 1, sym, sentinel, &slen);
		{
			char hexbuf[128];
			size_t hexpos= 0;
			for(size_t _i= 0; _i < slen && hexpos + 3 < sizeof(hexbuf); _i++) {
				int wn= snprintf(hexbuf + hexpos, sizeof(hexbuf) - hexpos, "%02X", (unsigned char)sentinel[_i]);
				if(wn > 0) hexpos+= (size_t)wn;
				if(_i + 1 < slen && hexpos + 1 < sizeof(hexbuf)) hexbuf[hexpos++]= ' ';
			}
			hexbuf[hexpos]= '\0';
			log_debug_env_token("WTC_DEBUG_STAGE_1", NULL,
									  "[C parseBraces] wrote sentinel idx=%zu type=%c slen=%zu hex=%s",
									  ctx->accum->count - 1, sym, slen, hexbuf);
		}
		braces_append(ctx, sentinel, slen);
	} else {
		braces_append(ctx, "{{{", 3);
		braces_append(ctx, segment, len);
		braces_append(ctx, "}}}", 3);
	}
}

typedef struct {
	ThreadBuf *tb;
	const ParserConfig *cfg;
	Accum *accum;
} BracesPassCtx;

static void braces_run_pass(void *user_data) {
	BracesPassCtx *p= (BracesPassCtx *)user_data;

	ThreadBuf *out_tb= wiki_thread_buf_acquire_scratch();
	assert(out_tb);
	out_tb->len= 0;

	BracesContext ctx= {.cfg= p->cfg, .accum= p->accum, .out_tb= out_tb};
	parser_scan(p->tb->buf, p->tb->len, &wiki_rule_triple_brace_arg, braces_callback, &ctx);

	if(out_tb->len != p->tb->len || sz_equal(p->tb->buf, out_tb->buf, p->tb->len) != sz_true_k) {
		wiki_thread_buf_set(p->tb, out_tb->buf, out_tb->len);
	}
	wiki_thread_buf_release_scratch(out_tb);
}

static const char *braces_get_buf(void *tb) { return ((ThreadBuf *)tb)->buf; }
static size_t braces_get_len(void *tb) { return ((ThreadBuf *)tb)->len; }

static void parse_simple_args(ThreadBuf *tb, const ParserConfig *cfg, Accum *accum) {
	BracesPassCtx p= {.tb= tb, .cfg= cfg, .accum= accum};
	parser_scan_until_stable(tb, braces_run_pass, &p, braces_get_buf, braces_get_len);
}

/* ── ParserRules for the outer fixpoint loop in parse_braces ─────────────── */

/* ── Callbacks for the outer fixpoint loop ─────────────────────────────────── */

typedef struct {
	const ParserConfig *cfg;
	Accum *accum;
	ThreadBuf *out;
	char ***link_stack;		  /* &(char **) – one extra level for realloc */
	size_t **link_stack_lens; /* &(size_t *) */
	size_t *link_count;
	size_t *link_cap;
	const ParserRules *active_rule;
} MainBracesCtx;

/* Restore any nested link-stack placeholders in text[0..len), push to
 * link_stack, and emit the numeric placeholder \0<N>\x7F into ctx->out. */
static void main_braces_push_link_stack(MainBracesCtx *ctx,
													 const char *text, size_t text_len) {
	size_t restored_len= 0;
	char *restored= str_restore(text, text_len,
										 (const char **)*ctx->link_stack,
										 *ctx->link_count,
										 *ctx->link_stack_lens,
										 &restored_len);
	if(*ctx->link_count >= *ctx->link_cap) {
		*ctx->link_cap*= 2;
		*ctx->link_stack= (char **)realloc(*ctx->link_stack, *ctx->link_cap * sizeof(char *));
		*ctx->link_stack_lens= (size_t *)realloc(*ctx->link_stack_lens, *ctx->link_cap * sizeof(size_t));
		assert(*ctx->link_stack && *ctx->link_stack_lens);
	}
	(*ctx->link_stack)[*ctx->link_count]= restored;
	(*ctx->link_stack_lens)[*ctx->link_count]= restored_len;
	size_t link_idx= (*ctx->link_count)++;

	char mark[64];
	int n= snprintf(mark + 1, sizeof(mark) - 2, "%zu", link_idx);
	mark[0]= '\0';
	mark[1 + n]= '\x7F';
	size_t mlen= (size_t)(n + 2);
	wiki_thread_buf_append(ctx->out, (sz_string_view_t){.start= mark, .length= mlen});
}

/* Callback for {{...}} template matches: process inner via build_from_inner,
 * or park in link_stack if build_from_inner rejects it. */
static void main_braces_template_cb(const char *segment, size_t len,
												ParserSegmentKind kind, void *user_data) {
	MainBracesCtx *ctx= (MainBracesCtx *)user_data;
	if(kind == PARSER_SEG_TEXT) {
		wiki_thread_buf_append(ctx->out, (sz_string_view_t){.start= segment, .length= len});
		return;
	}
	/* PARSER_SEG_INNER: segment is the inner content of {{ ... }} */
	const char *inner= segment;
	size_t inner_len= len;

	/* JS parity (reReplace): newline is allowed in {{...}} only when not
	 * immediately followed by '=' or a placeholder-NUL. If disallowed,
	 * leave this match untouched for the stage-1 state machine. */
	for(size_t i= 0; i + 1 < inner_len; i++) {
		if(inner[i] == '\n' && (inner[i + 1] == '=' || inner[i + 1] == '\0')) {
			wiki_thread_buf_append(ctx->out, (sz_string_view_t){.start= "{{", .length= 2});
			wiki_thread_buf_append(ctx->out, (sz_string_view_t){.start= inner, .length= inner_len});
			wiki_thread_buf_append(ctx->out, (sz_string_view_t){.start= "}}", .length= 2});
			return;
		}
	}

	Token *tok= build_from_inner(inner, inner_len,
										  false,
										  *ctx->link_stack, *ctx->link_count,
										  *ctx->link_stack_lens,
										  ctx->cfg, ctx->accum);
	if(tok) {
		size_t tok_idx= ctx->accum->count - 1;
		char sym= 't';
		if(inner_len > 0) {
			size_t p0_end= 0;
			while(p0_end < inner_len && inner[p0_end] != '|') p0_end++;
			sym= braces_get_symbol(inner, p0_end, ctx->cfg, NULL);
		}
		char sent[64];
		size_t slen;
		work_str_sentinel(tok_idx, sym, sent, &slen);
		log_debug_env_token("WTC_DEBUG_STAGE_1", NULL,
								  "[C parseBraces] wrote sentinel idx=%zu type=%c slen=%zu",
								  tok_idx, sym, slen);
		wiki_thread_buf_append(ctx->out, (sz_string_view_t){.start= sent, .length= slen});
	} else {
		/* JS parity: on invalid template names, keep raw {{...}} so later
		 * sub-passes can still tokenize nested [[...]]/ -{...}- content. */
		wiki_thread_buf_append(ctx->out, (sz_string_view_t){.start= "{{", .length= 2});
		wiki_thread_buf_append(ctx->out, (sz_string_view_t){.start= inner, .length= inner_len});
		wiki_thread_buf_append(ctx->out, (sz_string_view_t){.start= "}}", .length= 2});
	}
}

/* Callback for [[...]] and -{...}- matches: park the full match in link_stack. */
static void main_braces_park_cb(const char *segment, size_t len,
										  ParserSegmentKind kind, void *user_data) {
	MainBracesCtx *ctx= (MainBracesCtx *)user_data;
	if(kind == PARSER_SEG_TEXT) {
		wiki_thread_buf_append(ctx->out, (sz_string_view_t){.start= segment, .length= len});
		return;
	}
	/* PARSER_SEG_INNER: reconstruct open+inner+close, park, emit placeholder. */
	const ParserRules *r= ctx->active_rule;
	size_t full_len= r->open_len + len + r->close_len;
	char *tmp= malloc(full_len + 1);
	assert(tmp);
	sz_copy(tmp, r->open_delim, r->open_len);
	sz_copy(tmp + r->open_len, segment, len);
	sz_copy(tmp + r->open_len + len, r->close_delim, r->close_len);
	tmp[full_len]= '\0';
	main_braces_push_link_stack(ctx, tmp, full_len);
	free(tmp);
}

typedef struct {
	ThreadBuf *tb;
	const ParserConfig *cfg;
	Accum *accum;
	char ***link_stack;
	size_t **link_stack_lens;
	size_t *link_count;
	size_t *link_cap;
} MainBracesPassArgs;

/*
 * One pass of the outer fixpoint loop.
 * Runs four sequential sub-scans (two template alternations, then wikilink,
 * then converter parking). Called by parser_scan_until_stable until stable.
 */
static void main_braces_run_pass(void *user_data) {
	MainBracesPassArgs *args= (MainBracesPassArgs *)user_data;
	ThreadBuf *out= wiki_thread_buf_acquire_scratch();
	assert(out);

	MainBracesCtx ctx= {
	.cfg= args->cfg,
	.accum= args->accum,
	.out= out,
	.link_stack= args->link_stack,
	.link_stack_lens= args->link_stack_lens,
	.link_count= args->link_count,
	.link_cap= args->link_cap,
	};

	/* Sub-pass 1a: {{...}} not preceded by { (alternation 1). */
	out->len= 0;
	ctx.active_rule= &wiki_rule_main_template_1;
	parser_scan(args->tb->buf, args->tb->len, &wiki_rule_main_template_1,
				main_braces_template_cb, &ctx);
	wiki_thread_buf_set(args->tb, out->buf, out->len);

	/* Sub-pass 1b: {{...}} not followed by } (alternation 2). */
	out->len= 0;
	ctx.active_rule= &wiki_rule_main_template_2;
	parser_scan(args->tb->buf, args->tb->len, &wiki_rule_main_template_2,
				main_braces_template_cb, &ctx);
	wiki_thread_buf_set(args->tb, out->buf, out->len);

	/* Sub-pass 2: park [[...]] wikilinks. */
	out->len= 0;
	ctx.active_rule= &wiki_rule_main_wikilink;
	parser_scan(args->tb->buf, args->tb->len, &wiki_rule_main_wikilink,
				main_braces_park_cb, &ctx);
	wiki_thread_buf_set(args->tb, out->buf, out->len);

	/* Sub-pass 3: park -{...}- converters. */
	out->len= 0;
	ctx.active_rule= &wiki_rule_main_converter;
	parser_scan(args->tb->buf, args->tb->len, &wiki_rule_main_converter,
				main_braces_park_cb, &ctx);
	wiki_thread_buf_set(args->tb, out->buf, out->len);

	wiki_thread_buf_release_scratch(out);
}

/* Main parse function */
void parse_braces_with_heading(ThreadBuf *tb, const ParserConfig *cfg, Accum *accum,
									 bool allow_heading) {
	if(!tb || !tb->buf) return;

	/* First, replace simple innermost triple-brace args. */
	parse_simple_args(tb, cfg, accum);

	/* linkStack: temporarily holds [[...]] and -{...}- text so brace matching
     * can proceed without those patterns interfering.  Stores the FULL matched
     * text (including delimiters) so it can be restored verbatim.
     * link_stack_lens[i] stores the binary-safe byte length of link_stack[i],
     * required because entries may contain embedded NUL bytes from token sentinels. */
	size_t link_cap= 16, link_count= 0;
	char **link_stack= malloc(link_cap * sizeof(char *));
	size_t *link_stack_lens= malloc(link_cap * sizeof(size_t));
	assert(link_stack && link_stack_lens);

	{
		MainBracesPassArgs args= {
		.tb= tb,
		.cfg= cfg,
		.accum= accum,
		.link_stack= &link_stack,
		.link_stack_lens= &link_stack_lens,
		.link_count= &link_count,
		.link_cap= &link_cap,
		};
		parser_scan_until_stable(tb, main_braces_run_pass, &args,
										 braces_get_buf, braces_get_len);
	}

	/* Second-pass state machine: handle nested templates/links and heading
     * closures that the simple regex replacement loop cannot resolve. */
	log_debug_env_token("WTC_DEBUG_STAGE_1", NULL,
							  "[C parseBraces] entering state machine: len=%zu, buf=%.200s, link_count=%zu",
							  tb->len, tb->buf, link_count);
	braces_state_machine(tb, cfg, accum, link_stack, link_count, link_stack_lens,
							allow_heading);

	/* Final restoration of parked [[...]] / -{...}- placeholders. */
	{
		size_t restored_len= 0;
		char *restored_all= str_restore(tb->buf, tb->len,
												  (const char **)link_stack, link_count,
												  link_stack_lens,
												  &restored_len);
		wiki_thread_buf_set(tb, restored_all, restored_len);
		free(restored_all);
	}

	/* JS parity: after innermost {{...}} replacements, outer {{{...}}} may become
     * simple enough to match (e.g. {{{a|{{T}}}}}). Re-run the simple-arg pass. */
	parse_simple_args(tb, cfg, accum);

	/* Cleanup link_stack */
	for(size_t i= 0; i < link_count; i++) free(link_stack[i]);
	free(link_stack);
	free(link_stack_lens);
}

void parse_braces(ThreadBuf *tb, const ParserConfig *cfg, Accum *accum) {
	parse_braces_with_heading(tb, cfg, accum, true);
}
