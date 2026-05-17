/*
 * config.c — Parser configuration: load from JSON using cJSON.
 *
 * The wikiparser-node-1.38.1 config JSON schema:
 *   ext            : string[]            // extension tag names
 *   html           : [string[], string[], string[]]  // normal / li-like / void
 *   namespaces     : {[name: string]: number}
 *   redirection    : string[]
 *   doubleUnderscore: [string[], string[], object, object]
 *   protocol       : string             // regex fragment for external link protocols
 *   variants       : string[]
 *   excludes       : string[]           // absent in raw JSON; added by getConfig()
 */
#include "config.h"
#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>
#include "util/log.h"
#include "util/thread_buffer.h"
#include "util/pcre_cache.h"
#include <assert.h>
#include <ctype.h>
#include <cjson/cJSON.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── StrList helpers ─────────────────────────────────────────────────────── */

static void str_list_init(StrList *sl) {
	sl->items= NULL;
	sl->count= 0;
}

static void str_list_free(StrList *sl) {
	for(size_t i= 0; i < sl->count; i++) free(sl->items[i]);
	free(sl->items);
	sl->items= NULL;
	sl->count= 0;
}

/** Populate a StrList from a cJSON array of strings. */
static void str_list_from_json_array(StrList *sl, const cJSON *arr) {
	str_list_init(sl);
	if(!arr || !cJSON_IsArray(arr)) return;
	int n= cJSON_GetArraySize(arr);
	sl->items= malloc((size_t)n * sizeof(char *));
	assert(sl->items);
	int k= 0;
	const cJSON *item;
	cJSON_ArrayForEach(item, arr) {
		if(cJSON_IsString(item) && item->valuestring) {
			sl->items[k++]= strdup(item->valuestring);
		}
	}
	sl->count= (size_t)k;
}

/** Populate a StrList from the keys of a cJSON object. */
static void str_list_from_json_object_keys(StrList *sl, const cJSON *obj) {
	str_list_init(sl);
	if(!obj || !cJSON_IsObject(obj)) return;
	int n= cJSON_GetArraySize(obj);
	sl->items= malloc((size_t)n * sizeof(char *));
	assert(sl->items);
	int k= 0;
	const cJSON *item;
	cJSON_ArrayForEach(item, obj) {
		if(item->string) sl->items[k++]= strdup(item->string);
	}
	sl->count= (size_t)k;
}

static void str_map_init(StrMap *m) {
	m->keys= NULL;
	m->values= NULL;
	m->count= 0;
}

static void str_map_free(StrMap *m) {
	for(size_t i= 0; i < m->count; i++) {
		free(m->keys[i]);
		free(m->values[i]);
	}
	free(m->keys);
	free(m->values);
	m->keys= NULL;
	m->values= NULL;
	m->count= 0;
}

/* Populate a key->value map from a cJSON object whose values are strings. */
static void str_map_from_json_object(StrMap *m, const cJSON *obj) {
	str_map_init(m);
	if(!obj || !cJSON_IsObject(obj)) return;

	int n= cJSON_GetArraySize(obj);
	m->keys= malloc((size_t)n * sizeof(char *));
	m->values= malloc((size_t)n * sizeof(char *));
	assert(m->keys && m->values);

	int k= 0;
	const cJSON *item;
	cJSON_ArrayForEach(item, obj) {
		if(item->string && cJSON_IsString(item) && item->valuestring) {
			m->keys[k]= strdup(item->string);
			m->values[k]= strdup(item->valuestring);
			k++;
		}
	}
	m->count= (size_t)k;
}

static bool str_list_contains_exact(const StrList *sl, const char *needle) {
	if(!sl || !needle) return false;
	for(size_t i= 0; i < sl->count; i++) {
		if(sl->items[i] && strcmp(sl->items[i], needle) == 0) return true;
	}
	return false;
}

static void str_list_append_dup(StrList *sl, const char *s) {
	if(!sl || !s) return;
	char **grown= realloc(sl->items, (sl->count + 1) * sizeof(char *));
	assert(grown);
	sl->items= grown;
	sl->items[sl->count]= strdup(s);
	assert(sl->items[sl->count]);
	sl->count++;
}

static bool str_map_contains_key(const StrMap *m, const char *key) {
	if(!m || !key) return false;
	for(size_t i= 0; i < m->count; i++) {
		if(m->keys[i] && strcmp(m->keys[i], key) == 0) return true;
	}
	return false;
}

static bool ns_entry_exists_ci(const NsEntry *arr, size_t count,
															 const char *name, int num) {
	if(!arr || !name) return false;
	for(size_t i= 0; i < count; i++) {
		if(arr[i].num == num && arr[i].name && strcasecmp(arr[i].name, name) == 0) {
			return true;
		}
	}
	return false;
}

static void cfg_append_regex_escaped(char **buf, size_t *cap, size_t *len,
						 const char *s) {
	for(const char *p= s; *p; p++) {
		unsigned char c= (unsigned char)*p;
		bool meta= (c < 0x80) && (c=='\\' || c=='.' || c=='^' || c=='$' ||
						 c=='|' || c=='?' || c=='*' || c=='+' || c=='(' || c==')' ||
						 c=='[' || c==']' || c=='{' || c=='}');
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

static void str_map_append_dup(StrMap *m, const char *key, const char *value) {
	if(!m || !key || !value) return;
	char **grown_keys= realloc(m->keys, (m->count + 1) * sizeof(char *));
	char **grown_vals= realloc(m->values, (m->count + 1) * sizeof(char *));
	assert(grown_keys && grown_vals);
	m->keys= grown_keys;
	m->values= grown_vals;
	m->keys[m->count]= strdup(key);
	m->values[m->count]= strdup(value);
	assert(m->keys[m->count]);
	assert(m->values[m->count]);
	m->count++;
}

static bool protocol_token_supported(const char *s, size_t len) {
	/* Lock grammar to literal prefixes plus the single supported meta '?'.
	 * MediaWiki's wgUrlProtocols ships entries like "https?://"; rejecting
	 * '?' would refuse standard configs. All other regex metacharacters are
	 * forbidden so the C scanner can treat each item as a literal string
	 * (with optional one-char preceding-character optionality, see expand
	 * step in build_protocol_items). */
	for(size_t i = 0; i < len; i++) {
		unsigned char c = (unsigned char)s[i];
		if(c == '\\' || c == '[' || c == ']' || c == '(' || c == ')' ||
		   c == '{' || c == '}' || c == '*' || c == '+' ||
		   c == '^' || c == '$' || c <= 0x20 || c == 0x7F) {
			return false;
		}
	}
	return len > 0;
}

/* Free helper used on partial-failure paths to avoid leaks across config
 * load attempts. */
static void protocol_items_free(ProtocolList *pl) {
	if(!pl) return;
	for(size_t i = 0; i < pl->count; i++) free(pl->items[i]);
	free(pl->items);
	pl->items = NULL;
	pl->count = 0;
}

/* Expand a raw token "foo?bar" into all literal alternatives by treating
 * each '?' as making the immediately preceding byte optional. The number of
 * alternatives is 2^k where k is the number of '?'. We cap k at 8 so a
 * pathological config cannot OOM the loader (256 alts per token). */
static bool expand_token_with_optional(const char *raw, size_t rlen,
		                               ProtocolList *out) {
	size_t qcount = 0;
	for(size_t i = 0; i < rlen; i++) if(raw[i] == '?') qcount++;
	if(qcount > 8) return false;

	size_t variants = (size_t)1 << qcount;
	for(size_t v = 0; v < variants; v++) {
		char *buf = malloc(rlen + 1);
		if(!buf) return false;
		size_t out_len = 0;
		size_t qi = 0;
		for(size_t i = 0; i < rlen; i++) {
			if(i + 1 < rlen && raw[i + 1] == '?') {
				/* bit qi selects whether to keep raw[i] in this variant */
				if((v >> qi) & 1U) buf[out_len++] = raw[i];
				qi++;
				i++; /* skip the '?' */
				continue;
			}
			buf[out_len++] = raw[i];
		}
		buf[out_len] = '\0';
		if(out_len == 0) { free(buf); continue; }

		char **grown = realloc(out->items, (out->count + 1) * sizeof(char *));
		if(!grown) { free(buf); return false; }
		out->items = grown;
		out->items[out->count++] = buf;
	}
	return true;
}

/* Order tokens longest-first so that scheme prefix matching is greedy
 * (e.g. "https://" matches before "http://"). qsort comparator below. */
static int protocol_cmp_desc_len(const void *a, const void *b) {
	const char *sa = *(const char * const *)a;
	const char *sb = *(const char * const *)b;
	size_t la = strlen(sa), lb = strlen(sb);
	if(la != lb) return (la < lb) ? 1 : -1;
	return strcmp(sa, sb);
}

static bool build_protocol_items(ParserConfig *cfg) {
	if(!cfg || !cfg->protocol || !cfg->protocol[0]) return false;
	/* Reset before (re)building so config reload is safe. */
	protocol_items_free(&cfg->protocol_items);
	cfg->protocol_items_valid = false;

	const char *p = cfg->protocol;
	while(*p) {
		const char *bar = strchr(p, '|');
		size_t n = bar ? (size_t)(bar - p) : strlen(p);
		if(!protocol_token_supported(p, n)) {
			protocol_items_free(&cfg->protocol_items);
			return false;
		}
		if(!expand_token_with_optional(p, n, &cfg->protocol_items)) {
			protocol_items_free(&cfg->protocol_items);
			return false;
		}
		if(!bar) break;
		p = bar + 1;
	}

	if(cfg->protocol_items.count == 0) {
		protocol_items_free(&cfg->protocol_items);
		return false;
	}
	/* Greedy / longest-first ordering is required by C.4 match_proto_prefix
	 * and C.2 starts_with_proto, which both return on first prefix hit. */
	qsort(cfg->protocol_items.items, cfg->protocol_items.count,
		  sizeof(char *), protocol_cmp_desc_len);

	cfg->protocol_items_valid = true;
	return true;
}

static void build_pattern_ext_one(ParserConfig *cfg, bool include_only) {
	const char *noinclude_re = include_only ? "includeonly" : "(?:no|only)include";
	const char *include_re   = include_only ? "noinclude"   : "includeonly";
	char **target_pat = include_only ? &cfg->pattern_ext_includeonly
						 : &cfg->pattern_ext;
	bool has_translate = false;
	for(size_t i = 0; i < cfg->ext.count; i++) {
		if(strcmp(cfg->ext.items[i], "translate") == 0) { has_translate = true; break; }
	}
	size_t exts_cap = 64;
	for(size_t i = 0; i < cfg->ext.count; i++) {
		const char *e = cfg->ext.items[i];
		if(has_translate && (strcmp(e,"translate")==0 || strcmp(e,"tvar")==0)) continue;
		exts_cap += strlen(e) + 2;
	}
	char *exts = malloc(exts_cap); assert(exts);
	size_t ep = 0; bool first = true;
	for(size_t i = 0; i < cfg->ext.count; i++) {
		const char *e = cfg->ext.items[i];
		if(has_translate && (strcmp(e,"translate")==0 || strcmp(e,"tvar")==0)) continue;
		if(!first) exts[ep++] = '|';
		size_t elen = strlen(e); memcpy(exts + ep, e, elen); ep += elen; first = false;
	}
	exts[ep] = '\0';
	size_t pat_cap = 256 + exts_cap + strlen(noinclude_re)*4 + strlen(include_re)*4;
	char *pattern = malloc(pat_cap); assert(pattern);
	size_t pos = 0;
	pos += (size_t)snprintf(pattern + pos, pat_cap - pos,
		"<!--[\\s\\S]*?(?:-->|$)"
		"|<%s(?:\\s[^>]*)?\\/>|<\\/%s\\s*>"
		"|<(%s)(\\s[^>]*?)?(?:\\/>|>([\\s\\S]*?)<\\/(\\1\\s*)>)"
		"|<(%s)(\\s[^>]*?)?(?:\\/>|>([\\s\\S]*?)(?:<\\/(%s\\s*)>|$))",
		noinclude_re, noinclude_re, exts, include_re, include_re);
	free(exts);
	*target_pat = pattern;
}

static void build_pattern_ext(ParserConfig *cfg) {
	build_pattern_ext_one(cfg, false);
	build_pattern_ext_one(cfg, true);
}

static int cfg_is_fullwidth_wrapped_dunder(const char *s) {
	static const char fw[] = "\xEF\xBC\xBF"; /* U+FF3F FULLWIDTH LOW LINE */
	size_t fwl = sizeof(fw) - 1U;
	size_t len = s ? strlen(s) : 0;
	if(len < 4U * fwl + 1U) return 0;
	return memcmp(s, fw, fwl) == 0 && memcmp(s + fwl, fw, fwl) == 0
		&& memcmp(s + len - fwl, fw, fwl) == 0
		&& memcmp(s + len - 2U * fwl, fw, fwl) == 0;
}

static void cfg_pattern_append(char **buf, size_t *cap, size_t *len,
					 const char *s) {
	size_t add = strlen(s);
	if(*len + add + 1 > *cap) {
		while(*len + add + 1 > *cap) *cap *= 2;
		*buf = realloc(*buf, *cap); assert(*buf);
	}
	memcpy(*buf + *len, s, add); *len += add; (*buf)[*len] = '\0';
}

static void cfg_pattern_append_n(char **buf, size_t *cap, size_t *len,
					   const char *s, size_t n) {
	if(*len + n + 1 > *cap) {
		while(*len + n + 1 > *cap) *cap *= 2;
		*buf = realloc(*buf, *cap); assert(*buf);
	}
	memcpy(*buf + *len, s, n); *len += n; (*buf)[*len] = '\0';
}

static void build_pattern_hr_and_dunder(ParserConfig *cfg) {
	/* Mirrors build_hr_and_dunder_pattern() from hr_and_double_underscore.c */
	static const char fw[] = "\xEF\xBC\xBF";
	size_t cap = 256; size_t len = 0;
	char *pattern = malloc(cap); assert(pattern); pattern[0] = '\0';

	cfg_pattern_append(&pattern, &cap, &len,
		"^((?:\\x00\\d+[cno]\\x7F)*)(-{4,})|__(");

	int first = 1;
	for(int list = 0; list < 2; list++) {
		const StrList *sl = &cfg->double_underscore[list];
		for(size_t i = 0; i < sl->count; i++) {
			const char *it = sl->items[i];
			if(!it || cfg_is_fullwidth_wrapped_dunder(it)) continue;
			if(!first) cfg_pattern_append(&pattern, &cap, &len, "|");
			cfg_pattern_append(&pattern, &cap, &len, it);
			first = 0;
		}
	}
	cfg_pattern_append(&pattern, &cap, &len, ")__|");
	cfg_pattern_append(&pattern, &cap, &len, fw);
	cfg_pattern_append(&pattern, &cap, &len, "{2}(");

	first = 1;
	for(int list = 0; list < 2; list++) {
		const StrList *sl = &cfg->double_underscore[list];
		for(size_t i = 0; i < sl->count; i++) {
			const char *it = sl->items[i];
			if(!it || !cfg_is_fullwidth_wrapped_dunder(it)) continue;
			size_t it_len = strlen(it);
			if(!first) cfg_pattern_append(&pattern, &cap, &len, "|");
			cfg_pattern_append_n(&pattern, &cap, &len,
				it + 2U * (sizeof(fw) - 1U),
				it_len - 4U * (sizeof(fw) - 1U));
			first = 0;
		}
	}
	cfg_pattern_append(&pattern, &cap, &len, ")");
	cfg_pattern_append(&pattern, &cap, &len, fw);
	cfg_pattern_append(&pattern, &cap, &len, "{2}");

	cfg->pattern_hr_and_dunder = pattern;
}

/* ── Internal parse of the cJSON root object ─────────────────────────────── */

static ParserConfig *config_from_cjson(const cJSON *root) {
	if(!root || !cJSON_IsObject(root)) return NULL;

	ParserConfig *cfg= calloc(1, sizeof(ParserConfig));
	if(!cfg) return NULL;

	/* Clear any stale dynamic rules from previous config load */
	wiki_parser_rules_clear_dynamic();

	/* ext */
	str_list_from_json_array(&cfg->ext, cJSON_GetObjectItemCaseSensitive(root, "ext"));

	/* html: array of 3 arrays */
	{
		const cJSON *html= cJSON_GetObjectItemCaseSensitive(root, "html");
		if(html && cJSON_IsArray(html)) {
			for(int i= 0; i < 3; i++) {
				str_list_from_json_array(&cfg->html[i], cJSON_GetArrayItem(html, i));
			}
		}
	}

	/*
     * namespaces:
     * - legacy shape: {"File": 6}
     * - wikiparser config shape: {"6": "File"}
     * Store as name->num entries for lookup by parser stages.
     */
	{
		const cJSON *ns= cJSON_GetObjectItemCaseSensitive(root, "namespaces");
		if(ns && cJSON_IsObject(ns)) {
			int n= cJSON_GetArraySize(ns);
			cfg->namespaces= malloc((size_t)n * sizeof(NsEntry));
			assert(cfg->namespaces);
			int k= 0;
			const cJSON *item;
			cJSON_ArrayForEach(item, ns) {
				if(item->string && cJSON_IsNumber(item)) {
					cfg->namespaces[k].name= strdup(item->string);
					cfg->namespaces[k].num= (int)item->valuedouble;
					k++;
				} else if(item->string && cJSON_IsString(item) && item->valuestring) {
					char *endp= NULL;
					long nsnum= strtol(item->string, &endp, 10);
					if(endp && *endp == '\0') {
						cfg->namespaces[k].name= strdup(item->valuestring);
						cfg->namespaces[k].num= (int)nsnum;
						k++;
					}
				}
			}
			cfg->ns_count= (size_t)k;
		}
	}

	/* nsid aliases: {"image": 6, "wp": 4, ...}
     * Merge into namespace lookup table so normalizeTitle/title parsing
     * recognizes aliases exactly like JS config.nsid. */
	{
		const cJSON *nsid= cJSON_GetObjectItemCaseSensitive(root, "nsid");
		if(nsid && cJSON_IsObject(nsid)) {
			const cJSON *item;
			cJSON_ArrayForEach(item, nsid) {
				if(!item->string || !cJSON_IsNumber(item)) continue;
				int nsnum= (int)item->valuedouble;
				if(ns_entry_exists_ci(cfg->namespaces, cfg->ns_count, item->string, nsnum)) {
					continue;
				}
				NsEntry *grown= realloc(cfg->namespaces, (cfg->ns_count + 1) * sizeof(NsEntry));
				assert(grown);
				cfg->namespaces= grown;
				cfg->namespaces[cfg->ns_count].name= strdup(item->string);
				cfg->namespaces[cfg->ns_count].num= nsnum;
				cfg->ns_count++;
			}
		}
	}

	/* redirection */
	str_list_from_json_array(&cfg->redirection,
													 cJSON_GetObjectItemCaseSensitive(root, "redirection"));

	/* doubleUnderscore: [insensitive_list, sensitive_list, ins_map, sen_map]
     * Indices 0 and 1 are arrays; indices 2 and 3 are objects (key→canonical).
     * After getConfig() in JS the arrays at [0] and [1] are populated from the
     * map keys if they were empty.  We replicate that logic here. */
	{
		const cJSON *du= cJSON_GetObjectItemCaseSensitive(root, "doubleUnderscore");
		if(du && cJSON_IsArray(du)) {
			/* [0] insensitive list (or empty → populated from [2] keys) */
			const cJSON *du0= cJSON_GetArrayItem(du, 0);
			const cJSON *du1= cJSON_GetArrayItem(du, 1);
			const cJSON *du2= cJSON_GetArrayItem(du, 2);
			const cJSON *du3= cJSON_GetArrayItem(du, 3);

			if(cJSON_IsArray(du0) && cJSON_GetArraySize(du0) > 0) {
				str_list_from_json_array(&cfg->double_underscore[0], du0);
			} else {
				str_list_from_json_object_keys(&cfg->double_underscore[0], du2);
			}

			if(cJSON_IsArray(du1) && cJSON_GetArraySize(du1) > 0) {
				str_list_from_json_array(&cfg->double_underscore[1], du1);
			} else {
				str_list_from_json_object_keys(&cfg->double_underscore[1], du3);
			}

			str_list_from_json_object_keys(&cfg->double_underscore[2], du2);
			str_list_from_json_object_keys(&cfg->double_underscore[3], du3);
			str_map_from_json_object(&cfg->double_underscore_alias[0], du2);
			str_map_from_json_object(&cfg->double_underscore_alias[1], du3);
		}
	}

	/* protocol (string) */
	{
		const cJSON *proto= cJSON_GetObjectItemCaseSensitive(root, "protocol");
		if(proto && cJSON_IsString(proto) && proto->valuestring) {
			cfg->protocol= strdup(proto->valuestring);
		}
	}

	/* Build expanded protocol items (required by C.2/C.4 scanners).
	 * Abort config load if protocol grammar is invalid. */
	if(!build_protocol_items(cfg)) {
		log_error("config_from_cjson: invalid protocol grammar, aborting config load");
		config_free(cfg);
		return NULL;
	}

	/* variants */
	str_list_from_json_array(&cfg->variants,
													 cJSON_GetObjectItemCaseSensitive(root, "variants"));

	/* variable: magic variables (e.g. pagename, currentyear, ...) */
	str_list_from_json_array(&cfg->variable,
													 cJSON_GetObjectItemCaseSensitive(root, "variable"));

	/* parserFunction[3] (subst-like modifiers), mirrors JS destructuring:
     *   [, , , subst] = config.parserFunction */
	{
		const cJSON *pf= cJSON_GetObjectItemCaseSensitive(root, "parserFunction");
		if(pf && cJSON_IsArray(pf)) {
			const cJSON *ins= cJSON_GetArrayItem(pf, 0);
			const cJSON *sen= cJSON_GetArrayItem(pf, 1);
			const cJSON *subst= cJSON_GetArrayItem(pf, 3);
			str_map_from_json_object(&cfg->parser_function_insensitive, ins);
			str_map_from_json_object(&cfg->parser_function_sensitive, sen);
			str_list_from_json_array(&cfg->parser_function_subst, subst);
		} else {
			str_map_init(&cfg->parser_function_insensitive);
			str_map_init(&cfg->parser_function_sensitive);
			str_list_init(&cfg->parser_function_subst);
		}
	}

	/* interwiki prefixes */
	str_list_from_json_array(&cfg->interwiki,
													 cJSON_GetObjectItemCaseSensitive(root, "interwiki"));

	/* image parameter map */
	{
		const cJSON *img= cJSON_GetObjectItemCaseSensitive(root, "img");
		str_map_from_json_object(&cfg->img, img);
	}

	/* excludes — always starts empty (added by Parser.getConfig in JS) */
	str_list_init(&cfg->excludes);

	/* JS getConfig parity:
     * if (ext includes "translate" && !variable includes "translationlanguage") {
     *   variable.push("translationlanguage");
     *   parserFunction[1]["TRANSLATIONLANGUAGE"] = "translationlanguage";
     * }
     */
	if(str_list_contains_exact(&cfg->ext, "translate") && !str_list_contains_exact(&cfg->variable, "translationlanguage")) {
		str_list_append_dup(&cfg->variable, "translationlanguage");
		if(!str_map_contains_key(&cfg->parser_function_sensitive, "TRANSLATIONLANGUAGE")) {
			str_map_append_dup(&cfg->parser_function_sensitive,
												 "TRANSLATIONLANGUAGE",
												 "translationlanguage");
		}
	}

	/* pattern_redirect removed - redirect now uses callback parsing */

	build_pattern_ext(cfg);
	if(cfg->pattern_ext)
		pcre_cache_get(cfg->pattern_ext, PCRE2_CASELESS | PCRE2_UTF | PCRE2_UCP);
	if(cfg->pattern_ext_includeonly)
		pcre_cache_get(cfg->pattern_ext_includeonly, PCRE2_CASELESS | PCRE2_UTF | PCRE2_UCP);

	/* Register config-external-links dynamic rule.
     * The external_links parser now uses a callback scanner with wiki_rule_extlink_bracket,
     * so we don't need pattern_external_links anymore. */
	/* config-external-links is now handled by the callback scanner in external_links.c */

	build_pattern_hr_and_dunder(cfg);
	if(cfg->pattern_hr_and_dunder)
		pcre_cache_get(cfg->pattern_hr_and_dunder,
					   PCRE2_UTF | PCRE2_MULTILINE | PCRE2_CASELESS);

	/* Register config-magic-links dynamic rule.
     * The magic_links parser now uses a callback scanner with protocol_items,
     * so we don't need pattern_magic_links anymore. */
	/* config-magic-links is now handled by the callback scanner in magic_links.c */

	return cfg;
}

ParserConfig *config_load_file(const char *path) {
	FILE *f= fopen(path, "rb");
	if(!f) {
		log_error("config_load_file: cannot open %s", path);
		return NULL;
	}

	if(fseek(f, 0, SEEK_END) != 0) {
		fclose(f);
		log_error("config_load_file: fseek failed for %s", path);
		return NULL;
	}
	long sz= ftell(f);
	if(sz < 0) {
		fclose(f);
		log_error("config_load_file: ftell failed for %s", path);
		return NULL;
	}
	rewind(f);

	/* Use a scratch ThreadBuf instead of heap malloc for the temporary file buffer. */
	ThreadBuf *tb = wiki_thread_buf_acquire_scratch();
	wiki_thread_buf_reserve(tb, (size_t)sz);
	size_t got = fread(tb->buf, 1, (size_t)sz, f);
	if(got == 0 && sz > 0 && ferror(f)) {
		/* Read error */
		fclose(f);
		wiki_thread_buf_release_scratch(tb);
		log_error("config_load_file: fread failed for %s", path);
		return NULL;
	}
	tb->buf[got] = '\0';
	tb->len = got;
	fclose(f);

	ParserConfig *cfg = config_load_string(tb->buf, got);
	wiki_thread_buf_release_scratch(tb);
	return cfg;
}

ParserConfig *config_load_string(const char *json_str, size_t len) {
	(void)len; /* cJSON_Parse uses null-terminated strings */
	cJSON *root= cJSON_Parse(json_str);
	if(!root) {
		log_error("config_load_string: JSON parse error near: %s",
							cJSON_GetErrorPtr() ? cJSON_GetErrorPtr() : "(unknown)");
		return NULL;
	}
	ParserConfig *cfg= config_from_cjson(root);
	cJSON_Delete(root);
	return cfg;
}

void config_free(ParserConfig *cfg) {
	if(!cfg) return;

	/* Free expanded protocol items */
	protocol_items_free(&cfg->protocol_items);

	str_list_free(&cfg->ext);
	for(int i= 0; i < 3; i++) str_list_free(&cfg->html[i]);

	for(size_t i= 0; i < cfg->ns_count; i++) free(cfg->namespaces[i].name);
	free(cfg->namespaces);

	str_list_free(&cfg->redirection);
	for(int i= 0; i < 4; i++) str_list_free(&cfg->double_underscore[i]);
	str_map_free(&cfg->double_underscore_alias[0]);
	str_map_free(&cfg->double_underscore_alias[1]);

	free(cfg->protocol);
	str_list_free(&cfg->variants);
	str_list_free(&cfg->variable);
	str_list_free(&cfg->parser_function_subst);
	str_map_free(&cfg->parser_function_insensitive);
	str_map_free(&cfg->parser_function_sensitive);
	str_list_free(&cfg->interwiki);
	str_map_free(&cfg->img);
	str_list_free(&cfg->excludes);

	/* Free lazily-built pattern strings cached in the config */
	/* pattern_redirect removed - redirect now uses callback parsing */
	if(cfg->pattern_ext) free(cfg->pattern_ext);
	if(cfg->pattern_ext_includeonly) free(cfg->pattern_ext_includeonly);
	if(cfg->pattern_hr_and_dunder) free(cfg->pattern_hr_and_dunder);
	/* pattern_magic_links removed - magic_links now uses callback scanner */
	/* pattern_external_links removed - external_links now uses callback scanner */

	free(cfg);
}

bool config_excluded(const ParserConfig *cfg, const char *name) {
	if(!cfg || !name) return false;
	for(size_t i= 0; i < cfg->excludes.count; i++) {
		if(strcmp(cfg->excludes.items[i], name) == 0) return true;
	}
	return false;
}

bool config_has_ext(const ParserConfig *cfg, const char *name) {
	if(!cfg || !name) return false;
	for(size_t i= 0; i < cfg->ext.count; i++) {
		if(strcasecmp(cfg->ext.items[i], name) == 0) return true;
	}
	return false;
}
