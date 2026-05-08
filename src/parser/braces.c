#define PCRE2_CODE_UNIT_WIDTH 8
#include "util/pcre_cache.h"
#include "util/log.h"
#include "parser/braces.h"
#include "util/string_util.h"
#include "title.h"
#include "token.h"
#include <stringzilla/stringzilla.h>
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool str_list_contains_ci(const StrList *sl, const char *needle) {
	if(!sl || !needle) return false;
	for(size_t i= 0; i < sl->count; i++) {
		if(sl->items[i] && strcasecmp(sl->items[i], needle) == 0) return true;
	}
	return false;
}

/* Build a regex subject buffer with identical length/offsets where embedded
 * NUL bytes are replaced by SOH so PCRE lookaheads like \n(?![=\x00]) do not
 * spuriously reject lines that only contain internal sentinels. */
static char *braces_make_match_subject(const char *buf, size_t len) {
	if(!buf || len == 0) return NULL;

	/* Quick check: if the input contains no NUL sentinels, avoid the copy
	 * and let the caller use the original buffer. This is the common case. */
	char needle = '\0';
	const char *found = sz_find_byte(buf, len, &needle);
	if(!found) return NULL;

	char *subject= malloc(len);
	if(!subject) return NULL;
	sz_copy(subject, buf, len);

	/* Replace embedded NUL bytes with SOH in the copied buffer. Use
	 * sz_find_byte to locate NULs efficiently. */
	const char *p = found;
	while(p && p < buf + len) {
		size_t idx = (size_t)(p - buf);
		subject[idx] = '\x01';
		if(idx + 1 >= len) break;
		p = sz_find_byte(p + 1, len - idx - 1, &needle);
	}
	return subject;
}

static const char *str_map_get_exact(const StrMap *m, const char *key) {
	if(!m || !key) return NULL;
	for(size_t i= 0; i < m->count; i++) {
		if(m->keys[i] && m->values[i] && strcmp(m->keys[i], key) == 0) {
			return m->values[i];
		}
	}
	return NULL;
}

/* Forward declaration: lower_copy is defined later but used above. */
static char *lower_copy(const char *s, size_t len);


/* JS parity: parser/braces.js getSymbol() for {{...}} replacements. */
static char braces_get_symbol(const char *name, size_t len,
															const ParserConfig *cfg,
															bool *is_magic_out) {
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
	char colon_ch = ':';
	const char *colon_orig= sz_find_byte(trimmed, n, &colon_ch);
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
		canonical= str_map_get_exact(&cfg->parser_function_sensitive, trimmed);
		if(!canonical) {
			canonical= str_map_get_exact(&cfg->parser_function_insensitive, lc);
		}
		if(!canonical && base_orig && base_orig[0]) {
			canonical= str_map_get_exact(&cfg->parser_function_sensitive, base_orig);
		}
		if(!canonical && base_lc && base_lc[0]) {
			canonical= str_map_get_exact(&cfg->parser_function_insensitive, base_lc);
		}
	}

	char out= 't';
	if(strcmp(lc, "!") == 0) {
		out= '!';
		if(is_magic_out) *is_magic_out= true;
	} else if(strcmp(lc, "!!") == 0) {
		out= '+';
		if(is_magic_out) *is_magic_out= true;
	} else if(strcmp(lc, "(!") == 0) {
		out= '{';
		if(is_magic_out) *is_magic_out= true;
	} else if(strcmp(lc, "!)") == 0) {
		out= '}';
		if(is_magic_out) *is_magic_out= true;
	} else if(strcmp(lc, "!-") == 0) {
		out= '-';
		if(is_magic_out) *is_magic_out= true;
	} else if(strcmp(lc, "=") == 0) {
		out= '~';
		if(is_magic_out) *is_magic_out= true;
	} else if(strcmp(lc, "server") == 0) {
		out= 'm';
		if(is_magic_out) *is_magic_out= true;
	} else if((strncmp(lc, "filepath:", 9) == 0 && n > 9) || (strncmp(lc, "fullurl:", 8) == 0 && n > 8) || (strncmp(lc, "fullurle:", 9) == 0 && n > 9) || (strncmp(lc, "canonicalurl:", 13) == 0 && n > 13) || (strncmp(lc, "canonicalurle:", 14) == 0 && n > 14)) {
		out= 'm';
		if(is_magic_out) *is_magic_out= true;
	} else if(strncmp(lc, "#vardefine:", 11) == 0 && n > 11) {
		out= 'n';
		if(is_magic_out) *is_magic_out= true;
	} else if(lc[0] == '#') {
		if(is_magic_out) *is_magic_out= true;
	} else if(cfg && canonical && canonical[0]) {
		if(is_magic_out) *is_magic_out= true;
	} else if(cfg && base_lc && base_lc[0]) {
		const char *base_canonical= str_map_get_exact(&cfg->parser_function_insensitive, base_lc);
		if(base_canonical) {
			if(is_magic_out) *is_magic_out= true;
		}
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
	sz_copy(out, s + i, n);
	out[n]= '\0';
	return out;
}

static char *lower_copy(const char *s, size_t len) {
	if(!s) return NULL;

	/* Use a precomputed lookup table + Stringzilla's sz_lookup for faster
	 * bulk lowercase transformation. This preserves the byte-wise tolower()
	 * semantics used previously (C locale/unsigned-char based). */
	static unsigned char lut[256];
	static int lut_inited = 0;
	if(!lut_inited) {
		for(int i = 0; i < 256; ++i) lut[i] = (unsigned char)tolower((unsigned char)i);
		lut_inited = 1;
	}

	char *out = malloc(len + 1);
	if(!out) return NULL;
	sz_lookup(out, len, s, (const char *)lut);
	out[len] = '\0';
	return out;
}

static const char *parser_function_canonical(const ParserConfig *cfg, const char *name, size_t len) {
	if(!cfg || !name || len == 0) return NULL;

	char *trimmed= trim_copy(name, len);
	if(!trimmed || trimmed[0] == '\0') {
		free(trimmed);
		return NULL;
	}

	const char *canonical= str_map_get_exact(&cfg->parser_function_sensitive, trimmed);
	if(!canonical) {
		char *lc= lower_copy(trimmed, strlen(trimmed));
		if(lc) {
			canonical= str_map_get_exact(&cfg->parser_function_insensitive, lc);
			free(lc);
		}
	}

	free(trimmed);
	return canonical;
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

			/* Persist the name into the tokens arena to avoid dangling views */
			const char *name_view = wiki_thread_buf_append_to_tokens(parts_restored[0], parts_lens[0]);
			token_append_text_n(name_tok, name_view, parts_lens[0]);
			token_append_child(t, name_tok);

			char *nm= trim_copy(parts_restored[0], parts_lens[0]);
			if(nm) t->name= nm;
		}

		if(parts_count > 1 && parts_restored[1]) {
			Token *def_tok= token_new(TOKEN_PLAIN, "arg-default");
			if(def_tok) {
				/* Persist default text into tokens arena */
				const char *def_view = wiki_thread_buf_append_to_tokens(parts_restored[1], parts_lens[1]);
				token_append_text_n(def_tok, def_view, parts_lens[1]);
				token_append_child(t, def_tok);
			}
		}

		for(size_t k= 2; k < parts_count; k++) {
			if(!parts_restored[k]) continue;
			Token *hidden= token_new(TOKEN_HIDDEN, "hidden");
			if(!hidden) continue;
			/* Persist hidden part into tokens arena */
			const char *hid_view = wiki_thread_buf_append_to_tokens(parts_restored[k], parts_lens[k]);
			token_append_text_n(hidden, hid_view, parts_lens[k]);
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
		char colon_ch = ':';
		const char *colon= sz_find_byte(title_part, title_part_len, &colon_ch);
		if(colon) {
			size_t prefix_len= (size_t)(colon - title_part);
			char *prefix= trim_copy(title_part, prefix_len);
			if(prefix && str_list_contains_ci(&cfg->parser_function_subst, prefix)) {
				size_t mod_len= prefix_len + 1;
				t->data.transclude.modifier= malloc(mod_len + 1);
				if(t->data.transclude.modifier) {
					sz_copy(t->data.transclude.modifier, title_part, mod_len);
					t->data.transclude.modifier[mod_len]= '\0';
				}
				title_part= colon + 1;
				title_part_len-= mod_len;
			}
			free(prefix);
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
		(void)braces_get_symbol(title_part, p0_len, cfg, &magic);
		if(magic) {
			transclude_is_magic= true;
			magic_title_len= p0_len;
				char colon_ch = ':';
				const char *colon= sz_find_byte(title_part, p0_len, &colon_ch);
				if(colon) {
					magic_title_len= (size_t)(colon - title_part);
					magic_first_arg= colon + 1;
					magic_first_arg_len= p0_len - magic_title_len - 1;
				}

			free(t->type_name);
			t->type_name= strdup("magic-word");
			const char *canonical= parser_function_canonical(cfg, title_part, magic_title_len);
			char *magic_raw_name= trim_copy(title_part, magic_title_len);
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
					for(char *p= nm; *p; p++) {
						*p= (char)tolower((unsigned char)*p);
					}
					t->name= nm;
				}
			}
			if(canonical && magic_raw_name) free(magic_raw_name);
			if(t->name && strcmp(t->name, "invoke") == 0) invoke_magic= true;

			Token *mw_name= token_new(TOKEN_SYNTAX, "magic-word-name");
			if(mw_name) {
				/* Persist magic-word name into tokens arena */
				const char *mw_view = wiki_thread_buf_append_to_tokens(title_part, magic_title_len);
				token_append_text_n(mw_name, mw_view, magic_title_len);
				token_append_child(t, mw_name);
			}
		} else {
			Token *tpl_name= token_new(TOKEN_ATOM, "template-name");
			if(tpl_name) {
				/* Persist template name into tokens arena */
				const char *tpl_view = wiki_thread_buf_append_to_tokens(title_part, p0_len);
				token_append_text_n(tpl_name, tpl_view, p0_len);
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
					/* Persist module name into tokens arena */
					const char *mod_view = wiki_thread_buf_append_to_tokens(magic_first_arg, magic_first_arg_len);
					token_append_text_n(mod_tok, mod_view, magic_first_arg_len);
				token_append_child(t, mod_tok);
			}
			if(parts_count > 1 && parts_restored[1]) {
				Token *fn_tok= token_new(TOKEN_ATOM, "invoke-function");
				if(fn_tok) {
						/* Persist invoke-function into tokens arena */
						const char *fn_view = wiki_thread_buf_append_to_tokens(parts_restored[1], parts_lens[1]);
						token_append_text_n(fn_tok, fn_view, parts_lens[1]);
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
						/* Persist positional magic argument into tokens arena */
						const char *val_view = wiki_thread_buf_append_to_tokens(part, part_len);
						token_append_text_n(val_tok, val_view, part_len);
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
                
                /* JS parity: for certain magic words, don't split early parameters on '='.
                 * For #tag, only parameters at k > params_start_idx (3rd+) allow key=value splitting.
                 * Earlier params remain positional. */
                bool force_positional= false;
                if(transclude_is_magic && t->name) {
                        if(strcmp(t->name, "tag") == 0 && k == params_start_idx) {
                                /* #tag: first param after ':' is positional even if it contains '=' */
                                force_positional= true;
                        }
                }
                
                /* JS parity: use pre-determined named/positional flag when available.
         * part_is_named[k]==false means the raw part had no '=', so even if
         * the restored text contains '=' (e.g. from [[=]]), it is positional. */
				char eq_ch = '=';
				const char *eq= (force_positional || (part_is_named && !part_is_named[k]))
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
					/* Persist key and value into tokens arena */
					const char *key_view = wiki_thread_buf_append_to_tokens(part, key_len);
					const char *val_view = wiki_thread_buf_append_to_tokens(eq + 1, val_len);
					token_append_text_n(key_tok, key_view, key_len);
					token_append_text_n(val_tok, val_view, val_len);
					token_append_child(param, key_tok);
					token_append_child(param, val_tok);

			char *pname= trim_copy(part, key_len);
			if(pname) param->name= pname;
				} else {
					/* Persist positional parameter value into tokens arena */
					const char *val_view = wiki_thread_buf_append_to_tokens(part, part_len);
					token_append_child(param, key_tok);
					token_append_text_n(val_tok, val_view, part_len);
					token_append_child(param, val_tok);

			char idx_buf[32];
			int n= snprintf(idx_buf, sizeof(idx_buf), "%zu", positional++);
			if(n > 0) {
				char *pname= malloc((size_t)n + 1);
				if(pname) {
					sz_copy(pname, idx_buf, (size_t)n + 1);
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
			char eq_ch = '=';
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
	size_t count;
	size_t cap;
} PartList;

static bool parts_init(PartList *parts) {
	if(!parts) return false;
	parts->items= NULL;
	parts->lens= NULL;
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
	parts->items= NULL;
	parts->lens= NULL;
	parts->count= 0;
	parts->cap= 0;
}

static bool parts_grow(PartList *parts) {
	if(parts->count + 1 > parts->cap) {
		size_t cap= parts->cap ? parts->cap * 2 : 4;
		char **items= realloc(parts->items, cap * sizeof(char *));
		size_t *lens= realloc(parts->lens, cap * sizeof(size_t));
		if(!items || !lens) return false;
		parts->items= items;
		parts->lens= lens;
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
} BraceFrame;

static bool brace_frame_init(BraceFrame *frame, const char *open, size_t open_len, size_t index, size_t pos, bool find_equal) {
	if(!frame) return false;
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
			frame->open = NULL;
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

static char braces_arg_symbol(const char *inner, size_t inner_len, const ParserConfig *cfg);

static bool braces_state_machine(ThreadBuf *tb, const ParserConfig *cfg,
																 Accum *accum, char **link_stack, size_t link_count,
																 const size_t *link_stack_lens) {
	if(!tb || !tb->buf) return false;
	const char *pattern= "^((?:\\0\\d+[cno]\\x7F)*)={1,6}|\\[\\[|-\\{(?!\\{)|\\{{2,}|\\n(?!(?:[^\\S\\n]|\\0\\d+[cn]\\x7F)*\\n)|[|=]|\\}{2,}|\\}-|\\]\\]";
	pcre2_code *re = pcre_cache_get(pattern, PCRE2_UTF | PCRE2_MULTILINE);
	pcre2_match_data *md = pcre2_match_data_create_from_pattern(re, NULL);
	if(!md) {
		log_error("braces: pcre2_match_data_create_from_pattern failed");
		return false;
	}

	char *match_subject= braces_make_match_subject(tb->buf, tb->len);
	const char *subject= match_subject ? match_subject : tb->buf;

	BraceFrame *stack= malloc(64 * sizeof(BraceFrame));
	if(!stack) {
		pcre2_match_data_free(md);
		free(match_subject);
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
	bool has_last_index= false;
	//size_t last_index= 0;

#define ENSURE_OUT_CAP(need)             \
	do {                                   \
		while(out_len + (need) >= out_cap) { \
			out_cap*= 2;                       \
			out= realloc(out, out_cap);        \
			assert(out);                       \
		}                                    \
	} while(0)

	while(1) {
		int rc= pcre2_match(re, (PCRE2_SPTR)subject, tb->len, search_at, 0, md, NULL);
		bool matched= rc > 0;
		PCRE2_SIZE *ov= matched ? pcre2_get_ovector_pointer(md) : NULL;
		size_t ms= 0, me= 0;
		size_t prefix_len= 0;
		bool has_heading_prefix= false;
		if(matched) {
			ms= ov[0];
			me= ov[1];
			if(ov[2] != PCRE2_UNSET && ov[3] != PCRE2_UNSET) {
				prefix_len= (size_t)(ov[3] - ov[2]);
				has_heading_prefix= true;
			}
		}
		if(!matched && (!has_last_index || stack_len == 0 || stack[stack_len - 1].open[0] != '=')) {
			break;
		}

		size_t cur_index;
		size_t syntax_start;
		size_t syntax_end;
		if(matched) {
			if(has_heading_prefix) {
				syntax_start= ms + prefix_len;
				syntax_end= me;
				cur_index= syntax_start;
			} else {
				syntax_start= ms;
				syntax_end= me;
				cur_index= ms;
			}
		} else {
			cur_index= tb->len;
			syntax_start= cur_index;
			syntax_end= cur_index;
		}

		size_t syntax_len= syntax_end > syntax_start ? syntax_end - syntax_start : 0;
		const char *syntax= tb->buf + syntax_start;
		BraceFrame top;
		bool has_top= false;
		bool top_requeued= false;
		if(stack_len > 0) {
			top= stack[stack_len - 1];
			has_top= true;
			stack_len--;
		}

		if(matched && syntax_len == 2 && syntax[0] == ']' && syntax[1] == ']') {
			//last_index= cur_index + 2;
			has_last_index= true;
			/* ]] closes a [[ link frame; preserve any non-link frame below it */
			if(has_top && !(top.open_len >= 1 && top.open[0] == '[')) {
				stack[stack_len++]= top;
				top_requeued= true;
			}
		} else if(matched && syntax_len == 2 && syntax[0] == '}' && syntax[1] == '-') {
			//last_index= cur_index + 2;
			has_last_index= true;
			/* }- closes a -{ converter frame; preserve any non-converter frame below it */
			if(has_top && !(top.open_len >= 1 && top.open[0] == '-')) {
				stack[stack_len++]= top;
				top_requeued= true;
			}
		} else if(matched && syntax_len == 1 && syntax[0] == '\n') {
			//last_index= cur_index + 1;
			if(has_top && top.open_len == 1 && top.open[0] == '=') {
				const char *slice= tb->buf + top.index;
				size_t slice_len= cur_index - top.index;
				pcre2_code *hd_re = pcre_cache_get("^(={1,6})(.+)\\1((?:\\s|\\0\\d+[cn]\\x7F)*)$", PCRE2_UTF);
				pcre2_match_data *hd_md = pcre2_match_data_create_from_pattern(hd_re, NULL);
				if(hd_md) {
					int hrc = pcre2_match(hd_re, (PCRE2_SPTR)slice, slice_len, 0, 0, hd_md, NULL);
					if(hrc > 0) {
						PCRE2_SIZE *hov = pcre2_get_ovector_pointer(hd_md);
						size_t title_start = hov[2];
						size_t title_end = hov[3];
						size_t trail_start = hov[4];
						size_t trail_end = hov[5];
						size_t title_len = title_end - title_start;
						size_t trail_len = trail_end - trail_start;
						char *title = str_restore(slice + title_start, title_len, (const char **)link_stack, link_count, link_stack_lens, &title_len);
						if(title) {
							Token *heading_tok = token_new(TOKEN_HEADING, "heading");
							if(heading_tok) {
								Token *title_tok = token_new(TOKEN_PLAIN, "heading-title");
								if(title_tok) {
									/* Persist heading title into tokens arena */
									const char *title_view = wiki_thread_buf_append_to_tokens(title, title_len);
									token_append_text_n(title_tok, title_view, title_len);
									token_append_child(heading_tok, title_tok);
									if(trail_len > 0) {
										Token *trail_tok = token_new(TOKEN_SYNTAX, "heading-trail");
										if(trail_tok) {
											/* Persist heading trail into tokens arena */
											const char *trail_view = wiki_thread_buf_append_to_tokens(slice + trail_start, trail_len);
											token_append_text_n(trail_tok, trail_view, trail_len);
											token_append_child(heading_tok, trail_tok);
										}
									}
									accum_push(accum, heading_tok);
									size_t idx = accum->count - 1;
									char sent[64];
									size_t slen;
									work_str_sentinel(idx, 'h', sent, &slen);
									ENSURE_OUT_CAP((top.index > next_write ? top.index - next_write : 0) + slen);
									if(top.index > next_write) {
										memcpy(out + out_len, tb->buf + next_write, top.index - next_write);
										out_len+= top.index - next_write;
									}
									memcpy(out + out_len, sent, slen);
									out_len+= slen;
									next_write= cur_index + 1;
								} else {
									token_free(heading_tok);
								}
							}
							free(title);
						}
					}
					pcre2_match_data_free(hd_md);
				}
			} else if(has_top) {
				/* \n only closes = heading frames; preserve other frames */
				stack[stack_len++]= top;
				top_requeued= true;
			}
		} else {
			/* Only treat | as parameter separator; don't split on = in the state machine.
			 * JS parity: = is preserved in parameter values, and splitting is handled
			 * by build_from_inner which checks the RAW part (with sentinels) before restore. */
			if(matched && syntax_len == 1 && syntax[0] == '|') {
				if(has_top && top.has_parts) {
					if(!brace_push_part(&top, tb->buf, top.pos, cur_index, (const char **)link_stack, link_count, link_stack_lens)) {
						;
					}
					brace_frame_append_part(&top);
					top.pos= cur_index + 1;
				}
			} else if(matched && syntax_len >= 2 && syntax[0] == '}' && syntax[1] == '}') {
				if(has_top && top.has_parts) {
					if(!brace_push_part(&top, tb->buf, top.pos, cur_index, (const char **)link_stack, link_count, link_stack_lens)) {
						;
					}
					size_t close_len= 0;
					while(close_len < syntax_len && syntax[close_len] == '}') close_len++;
					size_t rest= top.open_len > close_len ? top.open_len - close_len : 0;
					char *inner= NULL;
					size_t inner_len= 0;
					if(!brace_frame_join_inner(&top, &inner, &inner_len)) {
						inner= NULL;
						inner_len= 0;
					}
					Token *tok= build_template_token((const char **)top.parts.items, top.parts.lens, top.parts.count, top.open_len == 3, cfg, accum, NULL);
					if(tok) {
						size_t tok_idx= accum->count - 1;
						char sent[64];
						size_t slen;
						char sym= 't';
						if(top.open_len == 3) {
							if(inner) sym= braces_arg_symbol(inner, inner_len, cfg);
						} else if(top.parts.count > 0) {
							sym= braces_get_symbol(top.parts.items[0], top.parts.lens[0], cfg, NULL);
						}
						work_str_sentinel(tok_idx, sym, sent, &slen);
						size_t rep_start= top.index + rest;
						size_t rep_end= cur_index + close_len;
						ENSURE_OUT_CAP((rep_start > next_write ? rep_start - next_write : 0) + slen);
						if(rep_start > next_write) {
							memcpy(out + out_len, tb->buf + next_write, rep_start - next_write);
							out_len+= rep_start - next_write;
						}
						memcpy(out + out_len, sent, slen);
						out_len+= slen;
						next_write= rep_end;
						if(rest > 1) {
							BraceFrame child;
							if(brace_frame_init(&child, top.open, rest, top.index, top.index + rest, false)) {
								stack[++stack_len - 1]= child;
							}
						} else if(rest == 1 && top.index > 0 && tb->buf[top.index - 1] == '-') {
							BraceFrame child;
							if(brace_frame_init(&child, "-{", 2, top.index - 1, top.index + 1, false)) {
								stack[++stack_len - 1]= child;
							}
						}
					}
					free(inner);
				}
			}
			if(matched && syntax_len > 0 && syntax[0] == '{') {
				BraceFrame frame;
				if(brace_frame_init(&frame, syntax, syntax_len, cur_index, cur_index + syntax_len, false)) {
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
			} else if(matched && syntax_len == 2 && syntax[0] == '[' && syntax[1] == '[') {
				/* Push a link frame so | inside [[...]] is not treated as template separator */
				BraceFrame link_frame;
				if(brace_frame_init(&link_frame, "[", 1, cur_index, cur_index + 2, false)) {
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
			} else if(matched && syntax_len == 2 && syntax[0] == '-' && syntax[1] == '{') {
				/* Push a converter frame so | inside -{...}- is not treated as template separator */
				BraceFrame conv_frame;
				if(brace_frame_init(&conv_frame, "-", 1, cur_index, cur_index + 2, false)) {
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
					stack[stack_len++]= top;
					top_requeued= true;
				}
			}
		}

		if(!matched) {
			break;
		}

			/* If we popped a frame into `top` but didn't requeue it, free it
			 * now to avoid leaking its heap allocations (open, parts). */
			if(has_top && !top_requeued) {
				brace_frame_free(&top);
			}
		search_at= syntax_end;
		if(search_at == ms) search_at= ms + 1;
	}

	if(next_write < tb->len) {
		ENSURE_OUT_CAP(tb->len - next_write + 1);
		memcpy(out + out_len, tb->buf + next_write, tb->len - next_write);
		out_len+= tb->len - next_write;
	}
	out[out_len]= '\0';
	wiki_thread_buf_set(tb, out, out_len);

	free(out);
	/* Free any remaining frames stored in the stack to avoid leaking their
	 * inner allocations (open, parts). */
	for(size_t si = 0; si < stack_len; si++) {
		brace_frame_free(&stack[si]);
	}
	free(stack);
	free(match_subject);
	pcre2_match_data_free(md);
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
	for(size_t k= 0; k < base_len; k++) {
		base[k]= (char)tolower((unsigned char)cleaned[i + k]);
	}
	base[base_len]= '\0';
	free(cleaned);

	char sym= 'a';
	for(size_t n= 0; n < cfg->parser_function_subst.count; n++) {
		const char *s= cfg->parser_function_subst.items[n];
		if(!s) continue;
		if(strcmp(base, s) == 0) {
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
static void parse_simple_args(ThreadBuf *tb, const ParserConfig *cfg, Accum *accum) {
	const char *pattern_with_lb=
	"(?<!\\{)\\{\\{\\{((?:[^\\n{}\\[]|\\[(?!\\[)|\\n(?![\\x00]))*)\\}\\}\\}(?!\\})";

	pcre2_code *re = pcre_cache_get(pattern_with_lb, PCRE2_UTF);

	char *prev= NULL;
	size_t prev_len= 0;

	while(1) {
		pcre2_match_data *md= pcre2_match_data_create_from_pattern(re, NULL);
		if(!md) break;

		size_t out_cap= tb->len * 2 + 64;
		char *out= malloc(out_cap);
		assert(out);
		size_t out_len= 0;
		size_t search_at= 0;
		char *match_subject= braces_make_match_subject(tb->buf, tb->len);
		const char *subject= match_subject ? match_subject : tb->buf;

#define ENSURE_ARG_CAP(need)             \
	do {                                   \
		while(out_len + (need) >= out_cap) { \
			out_cap*= 2;                       \
			out= realloc(out, out_cap);        \
			assert(out);                       \
		}                                    \
	} while(0)

		while(search_at <= tb->len) {
			int rc= pcre2_match(re, (PCRE2_SPTR)subject, tb->len, search_at, 0, md, NULL);
			if(rc <= 0) {
				size_t rest= tb->len - search_at;
				ENSURE_ARG_CAP(rest + 1);
				memcpy(out + out_len, tb->buf + search_at, rest);
				out_len+= rest;
				break;
			}

			PCRE2_SIZE *ov= pcre2_get_ovector_pointer(md);
			size_t ms= ov[0], me= ov[1];
			size_t cs= (ov[2] != PCRE2_UNSET) ? ov[2] : 0;
			size_t ce= (ov[3] != PCRE2_UNSET) ? ov[3] : 0;

			size_t before= ms - search_at;
			ENSURE_ARG_CAP(before + 32);
			memcpy(out + out_len, tb->buf + search_at, before);
			out_len+= before;

			const char *inner= (cs < ce) ? tb->buf + cs : "";
			size_t inner_len= (cs < ce) ? (ce - cs) : 0;

			Token *tok= build_from_inner(inner, inner_len, true,
																	 NULL, 0, NULL, cfg, accum);
			if(tok) {
				size_t idx= accum->count - 1;
				char sent[64];
				size_t slen;
				char sym= braces_arg_symbol(inner, inner_len, cfg);
				work_str_sentinel(idx, sym, sent, &slen);
				ENSURE_ARG_CAP(slen);
				memcpy(out + out_len, sent, slen);
				out_len+= slen;
			} else {
				ENSURE_ARG_CAP(me - ms);
				memcpy(out + out_len, tb->buf + ms, me - ms);
				out_len+= me - ms;
			}

			search_at= me;
			if(me == ms) search_at++;
		}

#undef ENSURE_ARG_CAP

		out[out_len]= '\0';

		if(prev && prev_len == out_len && memcmp(prev, out, out_len) == 0) {
			free(out);
			free(match_subject);
			pcre2_match_data_free(md);
			break;
		}

		wiki_thread_buf_set(tb, out, out_len);
		free(out);

		free(prev);
		prev= malloc(out_len + 1);
		assert(prev);
		memcpy(prev, tb->buf, out_len);
		prev[out_len]= '\0';
		prev_len= out_len;
		free(match_subject);

		pcre2_match_data_free(md);
	}

	free(prev);
}

/* Main parse function */
void parse_braces(ThreadBuf *tb, const ParserConfig *cfg, Accum *accum) {
	if(!tb || !tb->buf) return;

	/* First, replace simple innermost triple-brace args. */
	parse_simple_args(tb, cfg, accum);

	/*
     * JS parity for reReplace in parser/braces.js:
     *   /(?<!\{)\{\{((?:[^\n{}[]|\[(?!\[)|\n(?![=\0]))*)\}\}
     *   |\{\{((?:[^\n{}[]|\[(?!\[)|\n(?![=\0]))*)\}\}(?!\})
     *   |\[\[(?:[^\n[\]{]|\n(?![=\0]))*\]\]
     *   |-\{(?:[^\n{}[]|\[(?!\[)|\n(?![=\0]))*\}-/gu
     */
	const char *pattern_with_lb=
	"(?<!\\{)\\{\\{((?:[^\\n{}\\[]|\\[(?!\\[)|\\n(?![\\x00]))*)\\}\\}"
	"|\\{\\{((?:[^\\n{}\\[]|\\[(?!\\[)|\\n(?![\\x00]))*)\\}\\}(?!\\})"
	"|\\[\\[(?:[^\\n\\[\\]\\{]|\\n(?![\\x00]))*\\]\\]"
	"|-\\{(?:[^\\n{}\\[]|\\[(?!\\[)|\\n(?![\\x00]))*\\}-";

	pcre2_code *re = pcre_cache_get(pattern_with_lb, PCRE2_UTF);

	/* linkStack: temporarily holds [[...]] and -{...}- text so brace matching
     * can proceed without those patterns interfering.  Stores the FULL matched
     * text (including delimiters) so it can be restored verbatim.
     * link_stack_lens[i] stores the binary-safe byte length of link_stack[i],
     * required because entries may contain embedded NUL bytes from token sentinels. */
	size_t link_cap= 16, link_count= 0;
	char **link_stack= malloc(link_cap * sizeof(char *));
	size_t *link_stack_lens= malloc(link_cap * sizeof(size_t));
	assert(link_stack && link_stack_lens);

	char *prev_buf= NULL;
	size_t prev_buf_len= 0;
	int _dbg_pass= 0;
	static int _braces_debug= -1;
	if(_braces_debug < 0) {
		_braces_debug= getenv("BRACES_DEBUG") ? 1 : 0;
	}

	while(1) {
		_dbg_pass++;
		if(_braces_debug) {
			fprintf(stderr, "[C parseBraces] pre-pass iter %d, buf_len=%zu, buf=%.200s\n",
							_dbg_pass, tb->len, tb->buf);
		}
		pcre2_match_data *md= pcre2_match_data_create_from_pattern(re, NULL);
		if(!md) {
			free(link_stack);
			free(link_stack_lens);
			return;
		}

		size_t out_cap= tb->len * 2 + 64;
		char *out_buf= malloc(out_cap);
		assert(out_buf);
		size_t out_len= 0;
		size_t search_at= 0;
		char *match_subject= braces_make_match_subject(tb->buf, tb->len);
		const char *subject= match_subject ? match_subject : tb->buf;

#define ENSURE_CAP(need)                  \
	do {                                    \
		while(out_len + (need) >= out_cap) {  \
			out_cap*= 2;                        \
			out_buf= realloc(out_buf, out_cap); \
			assert(out_buf);                    \
		}                                     \
	} while(0)

		while(search_at <= tb->len) {
			int rc= pcre2_match(re, (PCRE2_SPTR)subject, tb->len,
													search_at, 0, md, NULL);
			if(rc <= 0) {
				if(rc < 0 && rc != PCRE2_ERROR_NOMATCH) {
					PCRE2_UCHAR8 err_buf[256];
					pcre2_get_error_message(rc, err_buf, sizeof(err_buf));
				}
				size_t rest= tb->len - search_at;
				ENSURE_CAP(rest + 1);
				memcpy(out_buf + out_len, tb->buf + search_at, rest);
				out_len+= rest;
				break;
			}

			PCRE2_SIZE *ov= pcre2_get_ovector_pointer(md);
			size_t mstart= ov[0];
			size_t mend= ov[1];

			if(_braces_debug) {
				fprintf(stderr, "[C parseBraces] pass %d match at %zu..%zu: g1=%s g2=%s match=%.80s\n",
								_dbg_pass, mstart, mend,
								ov[2] != PCRE2_UNSET ? "set" : "unset",
								ov[4] != PCRE2_UNSET ? "set" : "unset",
								subject + mstart < subject + tb->len ? subject + mstart : "(end)");
			}

			/* Copy verbatim text before the match */
			size_t before= mstart - search_at;
			ENSURE_CAP(before + 8);
			memcpy(out_buf + out_len, tb->buf + search_at, before);
			out_len+= before;

			/* Dispatch on which alternative matched. */
			if(ov[2] != PCRE2_UNSET || ov[4] != PCRE2_UNSET) {
				/* {{...}} branch matched with captured inner in group 1 or 2. */
				PCRE2_SIZE cstart= (ov[2] != PCRE2_UNSET) ? ov[2] : ov[4];
				PCRE2_SIZE cend= (ov[2] != PCRE2_UNSET) ? ov[3] : ov[5];
				const char *inner= tb->buf + cstart;
				size_t inner_len= cend - cstart;

				/* Keep nested token markers intact; this parser stage does not
                 * resolve \0N<type>\x7F inside template parts. */
				/*
                 * NOTE: Do NOT skip if inner contains \0 bytes -- those are
                 * sentinel markers from prior iterations (e.g. {{Inner}} already
                 * tokenized). The build_from_inner/str_restore pipeline handles
                 * them correctly. Only skip if the regex pattern itself is unstable
                 * (i.e., inner would expand infinitely), which the convergence check
                 * handles via the prev_buf comparison.
                 */

				Token *tok= build_from_inner(inner, inner_len,
																		 false, link_stack, link_count,
																		 link_stack_lens,
																		 cfg,
																		 accum);
				if(tok) {
					size_t tok_idx= accum->count - 1;
					char sent[64];
					size_t slen;
					char sym= 't';
					if(inner_len > 0) {
						size_t p0_end= 0;
						while(p0_end < inner_len && inner[p0_end] != '|') p0_end++;
						sym= braces_get_symbol(inner, p0_end, cfg, NULL);
					}
					work_str_sentinel(tok_idx, sym, sent, &slen);
					ENSURE_CAP(slen);
					memcpy(out_buf + out_len, sent, slen);
					out_len+= slen;
				} else {
					/* JS parity: invalid {{...}} (e.g. {{}}) is parked in linkStack
					 * and restored at the end, rather than left inline. */
					size_t llen= mend - mstart;
					char *tmp= malloc(llen + 1);
					memcpy(tmp, tb->buf + mstart, llen);
					tmp[llen]= '\0';
					size_t restored_llen= 0;
					char *restored= str_restore(tmp, llen,
																				(const char **)link_stack, link_count,
																				link_stack_lens,
																				&restored_llen);
					free(tmp);

					if(link_count >= link_cap) {
						link_cap*= 2;
						link_stack= realloc(link_stack, link_cap * sizeof(char *));
						link_stack_lens= realloc(link_stack_lens, link_cap * sizeof(size_t));
						assert(link_stack && link_stack_lens);
					}
					link_stack[link_count]= restored;
					link_stack_lens[link_count]= restored_llen;
					size_t link_idx= link_count++;

					char mark[64];
					int n= snprintf(mark + 1, sizeof(mark) - 2, "%zu", link_idx);
					mark[0]= '\0';
					mark[1 + n]= '\x7F';
					size_t mlen= (size_t)(n + 2);
					ENSURE_CAP(mlen);
					memcpy(out_buf + out_len, mark, mlen);
					out_len+= mlen;
				}

			} else {
				/* [[...]] and -{...}- branches are parked and restored later. */
				size_t llen= mend - mstart;
				char *tmp= malloc(llen + 1);
				memcpy(tmp, tb->buf + mstart, llen);
				tmp[llen]= '\0';
				/* Restore any nested link-stack entries embedded in this match */
				size_t restored_llen= 0;
				char *restored= str_restore(tmp, llen,
																		(const char **)link_stack, link_count,
																		link_stack_lens,
																		&restored_llen);
				free(tmp);

				if(link_count >= link_cap) {
					link_cap*= 2;
					link_stack= realloc(link_stack, link_cap * sizeof(char *));
					link_stack_lens= realloc(link_stack_lens, link_cap * sizeof(size_t));
					assert(link_stack && link_stack_lens);
				}
				link_stack[link_count]= restored;
				link_stack_lens[link_count]= restored_llen;
				size_t link_idx= link_count++;

				/* Numeric-only sentinel \0<N>\x7F — matched by str_restore */
				char mark[64];
				int n= snprintf(mark + 1, sizeof(mark) - 2, "%zu", link_idx);
				mark[0]= '\0';
				mark[1 + n]= '\x7F';
				size_t mlen= (size_t)(n + 2);
				ENSURE_CAP(mlen);
				memcpy(out_buf + out_len, mark, mlen);
				out_len+= mlen;
			}

			search_at= mend;
			if(mend == mstart) search_at++;
		}

#undef ENSURE_CAP

		out_buf[out_len]= '\0'; /* NUL-terminate for safety */

		/* Binary-safe convergence check on the unresolved placeholder form. */
		if(prev_buf && prev_buf_len == out_len && memcmp(prev_buf, out_buf, out_len) == 0) {
			if(_braces_debug) {
				fprintf(stderr, "[C parseBraces] converged after pass %d, out=%.200s\n",
								_dbg_pass, out_buf);
			}
			free(out_buf);
			free(prev_buf);
			free(match_subject);
			pcre2_match_data_free(md);
			break;
		}

		if(_braces_debug) {
			fprintf(stderr, "[C parseBraces] pass %d output: %.200s\n", _dbg_pass, out_buf);
		}

		/* Keep parked placeholders for the next pass; restore only once at end. */
		wiki_thread_buf_set(tb, out_buf, out_len);

		free(prev_buf);
		prev_buf= malloc(out_len + 1);
		memcpy(prev_buf, out_buf, out_len);
		prev_buf[out_len]= '\0';
		prev_buf_len= out_len;
		free(out_buf);
		free(match_subject);

		pcre2_match_data_free(md);
	}

	/* Second-pass state machine: handle nested templates/links and heading
     * closures that the simple regex replacement loop cannot resolve. */
	if(_braces_debug) {
		fprintf(stderr, "[C parseBraces] entering state machine: len=%zu, buf=%.200s, link_count=%zu\n",
						tb->len, tb->buf, link_count);
	}
	braces_state_machine(tb, cfg, accum, link_stack, link_count, link_stack_lens);

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
