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
#include "log.h"
#include "thread_buffer.h"
#include <assert.h>
#include <cjson/cJSON.h>
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

static void str_map_append_dup(StrMap *m, const char *key, const char *value) {
	if(!m || !key || !value) return;
	char **grown_keys= realloc(m->keys, (m->count + 1) * sizeof(char *));
	char **grown_vals= realloc(m->values, (m->count + 1) * sizeof(char *));
	assert(grown_keys && grown_vals);
	m->keys= grown_keys;
	m->values= grown_vals;
	m->keys[m->count]= strdup(key);
	m->values[m->count]= strdup(value);
	assert(m->keys[m->count] && m->values[m->count]);
	m->count++;
}

/* ── Internal parse of the cJSON root object ─────────────────────────────── */

static ParserConfig *config_from_cjson(const cJSON *root) {
	if(!root || !cJSON_IsObject(root)) return NULL;

	ParserConfig *cfg= calloc(1, sizeof(ParserConfig));
	if(!cfg) return NULL;

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

	return cfg;
}

/* ── Public API ─────────────────────────────────────────────────────────── */

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
