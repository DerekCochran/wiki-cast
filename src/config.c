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
#include "util/log.h"
#include "util/thread_buffer.h"
#include "util/callback_parser.h"
#include "util/wiki_parser_rules.h"
#include "util/string_util.h"
#include <stringzilla/stringzilla.h>
#include <assert.h>
#include <ctype.h>
#include <cjson/cJSON.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Default allocator for sz_string_t operations */
sz_memory_allocator_t allocator_default;

/* Initialize the default allocator - can be called explicitly */
void config_init_allocator(void) {
    sz_memory_allocator_init_default(&allocator_default);
}

/* Auto-initialize at startup */
__attribute__((constructor))
static void init_allocator_default(void) {
    config_init_allocator();
}

/* ── StrList helpers ─────────────────────────────────────────────────────── */

static void str_list_init(StrList *sl) {
	sl->items= NULL;
	sl->count= 0;
}

static void str_list_free(StrList *sl) {
	for(size_t i= 0; i < sl->count; i++) {
		sz_string_free(&sl->items[i], &allocator_default);
	}
	free(sl->items);
	sl->items= NULL;
	sl->count= 0;
}

/** Populate a StrList from a cJSON array of strings. */
static void str_list_from_json_array(StrList *sl, const cJSON *arr) {
	str_list_init(sl);
	if(!arr || !cJSON_IsArray(arr)) return;
	int n= cJSON_GetArraySize(arr);
	sl->items= malloc((size_t)n * sizeof(sz_string_t));
	assert(sl->items);
	int k= 0;
	const cJSON *item;
	cJSON_ArrayForEach(item, arr) {
		if(cJSON_IsString(item) && item->valuestring) {
			size_t slen = strlen(item->valuestring);
			sz_ptr_t ptr = sz_string_init_length(&sl->items[k], slen, &allocator_default);
			sz_copy(ptr, (sz_ptr_t)item->valuestring, slen);
			k++;
		}
	}
	sl->count= (size_t)k;
}

/** Populate a StrList from the keys of a cJSON object. */
static void str_list_from_json_object_keys(StrList *sl, const cJSON *obj) {
	str_list_init(sl);
	if(!obj || !cJSON_IsObject(obj)) return;
	int n= cJSON_GetArraySize(obj);
	sl->items= malloc((size_t)n * sizeof(sz_string_t));
	assert(sl->items);
	int k= 0;
	const cJSON *item;

	cJSON_ArrayForEach(item, obj) {
		if(item->string) {
			size_t slen = strlen(item->string);
			sz_ptr_t ptr = sz_string_init_length(&sl->items[k], slen, &allocator_default);
			sz_copy(ptr, (sz_ptr_t)item->string, slen);
			k++;
		}
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
		sz_string_free(&m->keys[i], &allocator_default);
		sz_string_free(&m->values[i], &allocator_default);
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
	m->keys= malloc((size_t)n * sizeof(sz_string_t));
	m->values= malloc((size_t)n * sizeof(sz_string_t));
	assert(m->keys && m->values);

	int k= 0;
	const cJSON *item;

	cJSON_ArrayForEach(item, obj) {
		if(item->string && cJSON_IsString(item) && item->valuestring) {
			size_t klen = strlen(item->string);
			size_t vlen = strlen(item->valuestring);
			sz_ptr_t key_ptr = sz_string_init_length(&m->keys[k], klen, &allocator_default);
			sz_copy(key_ptr, (sz_ptr_t)item->string, klen);
			sz_ptr_t val_ptr = sz_string_init_length(&m->values[k], vlen, &allocator_default);
			sz_copy(val_ptr, (sz_ptr_t)item->valuestring, vlen);
			k++;
		}
	}
	m->count= (size_t)k;
}

static sz_string_t *realloc_sz_string_array_preserve_small(
		sz_string_t *arr, size_t old_count, size_t new_count);
static NsEntry *realloc_ns_entry_array_preserve_small(
		NsEntry *arr, size_t old_count, size_t new_count);

static bool str_list_contains_exact_n(const StrList *sl, const char *needle, size_t needle_len) {
	if(!sl || !needle) return false;
	for(size_t i= 0; i < sl->count; i++) {
		sz_ptr_t start;
		sz_size_t len;
		sz_string_range(&sl->items[i], &start, &len);
		if(start && len == needle_len && sz_equal(start, needle, needle_len) == sz_true_k) return true;
	}
	return false;
}

static void str_list_append_dup(StrList *sl, const char *s) {

	if(!sl || !s) return;
	sz_string_t *grown= realloc_sz_string_array_preserve_small(
		sl->items, sl->count, sl->count + 1);
	assert(grown);
	sl->items= grown;
	size_t slen = strlen(s);
	sz_ptr_t ptr = sz_string_init_length(&sl->items[sl->count], slen, &allocator_default);
	sz_copy(ptr, (sz_ptr_t)s, slen);
	sl->count++;
}

static bool str_map_contains_key_n(const StrMap *m, const char *key, size_t key_len) {
	if(!m || !key) return false;
	for(size_t i= 0; i < m->count; i++) {
		sz_ptr_t start;
		sz_size_t len;
		sz_string_range(&m->keys[i], &start, &len);
		if(start && len == key_len && sz_equal(start, key, key_len) == sz_true_k) return true;
	}
	return false;
}

static bool ns_entry_exists_ci(const NsEntry *arr, size_t count,
															 const char *name, int num) {
	if(!arr || !name) return false;
	size_t name_len = strlen(name);
	for(size_t i= 0; i < count; i++) {
		sz_ptr_t entry_name;
		sz_size_t entry_len;
		sz_string_range(&arr[i].name, &entry_name, &entry_len);
		if(arr[i].num == num && entry_name && entry_len == name_len && str_ci_eq_n((const char *)entry_name, name, entry_len)) {
			return true;
		}
	}
	return false;
}

/* Preserve small-string self-pointers when realloc moves backing arrays. */
static sz_string_t *realloc_sz_string_array_preserve_small(
		sz_string_t *arr, size_t old_count, size_t new_count) {
	bool *was_small= NULL;
	if(arr && old_count > 0) {
		was_small= malloc(old_count * sizeof(bool));
		assert(was_small);
		for(size_t i= 0; i < old_count; i++) {
			was_small[i]= sz_string_is_on_stack(&arr[i]);
		}
	}

	sz_string_t *grown= realloc(arr, new_count * sizeof(sz_string_t));
	assert(grown);

	if(grown != arr && was_small) {
		for(size_t i= 0; i < old_count; i++) {
			if(was_small[i]) {
				grown[i].internal.start= &grown[i].internal.chars[0];
			}
		}
	}

	free(was_small);
	return grown;
}

static NsEntry *realloc_ns_entry_array_preserve_small(
		NsEntry *arr, size_t old_count, size_t new_count) {
	bool *was_small= NULL;
	if(arr && old_count > 0) {
		was_small= malloc(old_count * sizeof(bool));
		assert(was_small);
		for(size_t i= 0; i < old_count; i++) {
			was_small[i]= sz_string_is_on_stack(&arr[i].name);
		}
	}

	NsEntry *grown= realloc(arr, new_count * sizeof(NsEntry));
	assert(grown);

	if(grown != arr && was_small) {
		for(size_t i= 0; i < old_count; i++) {
			if(was_small[i]) {
				grown[i].name.internal.start= &grown[i].name.internal.chars[0];
			}
		}
	}

	free(was_small);
	return grown;
}

static void str_map_append_dup(StrMap *m, const char *key, const char *value) {
	if(!m || !key || !value) return;
	sz_string_t *grown_keys= realloc_sz_string_array_preserve_small(
		m->keys, m->count, m->count + 1);
	sz_string_t *grown_vals= realloc_sz_string_array_preserve_small(
		m->values, m->count, m->count + 1);
	assert(grown_keys && grown_vals);
	m->keys= grown_keys;
	m->values= grown_vals;
	{
	size_t klen = strlen(key);
	sz_ptr_t key_ptr = sz_string_init_length(&m->keys[m->count], klen, &allocator_default);
	sz_copy(key_ptr, (sz_ptr_t)key, klen);
	}
	{
	size_t vlen = strlen(value);
	sz_ptr_t val_ptr = sz_string_init_length(&m->values[m->count], vlen, &allocator_default);
	sz_copy(val_ptr, (sz_ptr_t)value, vlen);
	}
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
	for(size_t i = 0; i < pl->count; i++) {
		sz_string_free(&pl->items[i].protocol_lower, &allocator_default);
	}
	free(pl->items);
	pl->items = NULL;
	pl->count = 0;
}

/* Free protocol buffer from ParserConfig */
static void protocol_buffer_free(ParserConfig *cfg) {
	if(cfg && cfg->protocol_buffer) {
		free(cfg->protocol_buffer);
		cfg->protocol_buffer = NULL;
		cfg->protocol_buffer_cap = 0;
		cfg->protocol_buffer_len = 0;
	}
}

/* Expand a raw token "foo?bar" into all literal alternatives by treating
 * each '?' as making the immediately preceding byte optional. The number of
 * alternatives is 2^k where k is the number of '?'. We cap k at 8 so a
 * pathological config cannot OOM the loader (256 alts per token). */
static bool expand_token_with_optional(const char *raw, size_t rlen,
		                               ParserConfig *cfg) {
	size_t qcount = 0;
	for(size_t i = 0; i < rlen; i++) if(raw[i] == '?') qcount++;
	if(qcount > 8) return false;

	size_t variants = (size_t)1 << qcount;
	for(size_t v = 0; v < variants; v++) {
		/* Build protocol string in a temporary buffer */
		char proto[64]; /* max protocol length is small */
		size_t out_len = 0;
		size_t qi = 0;
		for(size_t i = 0; i < rlen; i++) {
			if(i + 1 < rlen && raw[i + 1] == '?') {
				/* bit qi selects whether to keep raw[i] in this variant */
				if((v >> qi) & 1U) proto[out_len++] = raw[i];
				qi++;
				i++; /* skip the '?' */
				continue;
			}
			proto[out_len++] = raw[i];
		}
		if(out_len == 0) continue;

		/* Ensure protocol buffer has space */
		if(cfg->protocol_buffer_len + out_len + 1 > cfg->protocol_buffer_cap) {
			size_t new_cap = cfg->protocol_buffer_cap * 2 + out_len + 1;
			char *new_buf = realloc(cfg->protocol_buffer, new_cap);
			if(!new_buf) return false;
			cfg->protocol_buffer = new_buf;
			cfg->protocol_buffer_cap = new_cap;
		}

		/* Grow the items array */
		ProtocolItem *grown = realloc(cfg->protocol_items.items,
				(cfg->protocol_items.count + 1) * sizeof(ProtocolItem));
		if(!grown) return false;
		cfg->protocol_items.items = grown;

		/* Set up the string view to point into the buffer */
		char *dest = cfg->protocol_buffer + cfg->protocol_buffer_len;
		sz_copy((sz_ptr_t)dest, (sz_ptr_t)proto, out_len);
		dest[out_len] = '\0';

		cfg->protocol_items.items[cfg->protocol_items.count].protocol.start = (sz_cptr_t)dest;
		cfg->protocol_items.items[cfg->protocol_items.count].protocol.length = out_len;
		cfg->protocol_buffer_len += out_len + 1;

		cfg->protocol_items.count++;
	}

	return true;
}

/* Compare ProtocolItems by protocol length (descending) for greedy matching.
 * Since we use sz_string_view_t, we just compare the lengths. */
static int protocol_cmp_desc_len(const void *a, const void *b) {
	const ProtocolItem *pa = (const ProtocolItem *)a;
	const ProtocolItem *pb = (const ProtocolItem *)b;
	/* Sort by length descending (longest first) */
	if(pa->protocol.length != pb->protocol.length) {
		return (pa->protocol.length < pb->protocol.length) ? 1 : -1;
	}
	/* Equal length - compare content */
	return memcmp(pa->protocol.start, pb->protocol.start, pa->protocol.length);
}

static bool build_protocol_items(ParserConfig *cfg) {
	if(!cfg || !cfg->protocol || !cfg->protocol[0]) return false;
	/* Reset before (re)building so config reload is safe. */
	protocol_items_free(&cfg->protocol_items);
	protocol_buffer_free(cfg);
	memset(cfg->protocol_initials, 0, sizeof(cfg->protocol_initials));
	cfg->protocol_items_valid = false;

	/* Initialize protocol buffer */
	cfg->protocol_buffer_cap = 256;
	cfg->protocol_buffer = malloc(cfg->protocol_buffer_cap);
	if(!cfg->protocol_buffer) return false;
	cfg->protocol_buffer_len = 0;

	const char *p = cfg->protocol;
	while(*p) {
		const char *bar = strchr(p, '|');
		size_t n = bar ? (size_t)(bar - p) : strlen(p);
		if(!protocol_token_supported(p, n)) {
			protocol_items_free(&cfg->protocol_items);
			protocol_buffer_free(cfg);
			return false;
		}
		if(!expand_token_with_optional(p, n, cfg)) {
			protocol_items_free(&cfg->protocol_items);
			protocol_buffer_free(cfg);
			return false;
		}
		if(!bar) break;
		p = bar + 1;
	}
	if(cfg->protocol_items.count == 0) {
		protocol_items_free(&cfg->protocol_items);
		protocol_buffer_free(cfg);
		return false;
	}

	/* Sort by length descending (longest first) using a simple manual sort.
	 * We can't use qsort because it corrupts SSO strings by byte-swapping. */
	for(size_t i = 0; i < cfg->protocol_items.count - 1; i++) {
		for(size_t j = i + 1; j < cfg->protocol_items.count; j++) {
			if(protocol_cmp_desc_len(&cfg->protocol_items.items[i],
								  &cfg->protocol_items.items[j]) > 0) {
				/* Swap items properly (no byte-copy that breaks SSO) */
				ProtocolItem tmp = cfg->protocol_items.items[i];
				cfg->protocol_items.items[i] = cfg->protocol_items.items[j];
				cfg->protocol_items.items[j] = tmp;
			}
		}
	}

	/* Now build the protocol_lower strings for each item */
	for(size_t i = 0; i < cfg->protocol_items.count; i++) {
		size_t len = cfg->protocol_items.items[i].protocol.length;
		sz_ptr_t lower_ptr = sz_string_init_length(
			&cfg->protocol_items.items[i].protocol_lower, len, &allocator_default);
		if(!lower_ptr) {
			protocol_items_free(&cfg->protocol_items);
			protocol_buffer_free(cfg);
			return false;
		}
		const char *proto_start = (const char *)cfg->protocol_items.items[i].protocol.start;
		sz_lookup((sz_ptr_t)lower_ptr, len, proto_start, (const char *)fast_tolower_table());
		if(len > 0) {
			cfg->protocol_initials[(unsigned char)fast_tolower((unsigned char)proto_start[0])] = 1;
		}
	}
	cfg->protocol_items_valid = true;
	return true;
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
				size_t nlen = strlen(item->string);
				sz_ptr_t name_ptr = sz_string_init_length(&cfg->namespaces[k].name, nlen, &allocator_default);
				sz_copy(name_ptr, (sz_ptr_t)item->string, nlen);
					cfg->namespaces[k].num= (int)item->valuedouble;
					k++;
				} else if(item->string && cJSON_IsString(item) && item->valuestring) {
					char *endp= NULL;
					long nsnum= strtol(item->string, &endp, 10);
					if(endp && *endp == '\0') {
					size_t vlen = strlen(item->valuestring);
					sz_ptr_t name_ptr = sz_string_init_length(&cfg->namespaces[k].name, vlen, &allocator_default);
					sz_copy(name_ptr, (sz_ptr_t)item->valuestring, vlen);
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
				NsEntry *grown= realloc_ns_entry_array_preserve_small(
					cfg->namespaces, cfg->ns_count, cfg->ns_count + 1);
				assert(grown);
				cfg->namespaces= grown;
				size_t nslen= strlen(item->string);
			sz_ptr_t ptr = sz_string_init_length(&cfg->namespaces[cfg->ns_count].name, nslen, &allocator_default);
			sz_copy(ptr, (sz_ptr_t)item->string, nslen);
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

	/* parserFunction[2]/[3] modifiers, mirrors JS destructuring:
	 *   [, , raw, subst] = config.parserFunction */
	{
		const cJSON *pf= cJSON_GetObjectItemCaseSensitive(root, "parserFunction");
		if(pf && cJSON_IsArray(pf)) {
			const cJSON *ins= cJSON_GetArrayItem(pf, 0);
			const cJSON *sen= cJSON_GetArrayItem(pf, 1);
			const cJSON *raw= cJSON_GetArrayItem(pf, 2);
			const cJSON *subst= cJSON_GetArrayItem(pf, 3);
			str_map_from_json_object(&cfg->parser_function_insensitive, ins);
			str_map_from_json_object(&cfg->parser_function_sensitive, sen);
			str_list_from_json_array(&cfg->parser_function_raw, raw);
			str_list_from_json_array(&cfg->parser_function_subst, subst);
		} else {
			str_map_init(&cfg->parser_function_insensitive);
			str_map_init(&cfg->parser_function_sensitive);
			str_list_init(&cfg->parser_function_raw);
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
	if(str_list_contains_exact_n(&cfg->ext, "translate", sizeof("translate") - 1) &&
		 !str_list_contains_exact_n(&cfg->variable, "translationlanguage", sizeof("translationlanguage") - 1)) {
		str_list_append_dup(&cfg->variable, "translationlanguage");
		if(!str_map_contains_key_n(&cfg->parser_function_sensitive,
				"TRANSLATIONLANGUAGE", sizeof("TRANSLATIONLANGUAGE") - 1)) {
			str_map_append_dup(&cfg->parser_function_sensitive,
												 "TRANSLATIONLANGUAGE",
												 "translationlanguage");
		}
	}


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

	/* Clear dynamic rules before freeing config */
	wiki_parser_rules_clear_dynamic();

	/* Free expanded protocol items */
	protocol_items_free(&cfg->protocol_items);
	protocol_buffer_free(cfg);

	str_list_free(&cfg->ext);
	for(int i= 0; i < 3; i++) str_list_free(&cfg->html[i]);

	for(size_t i= 0; i < cfg->ns_count; i++) sz_string_free(&cfg->namespaces[i].name, &allocator_default);
	free(cfg->namespaces);

	str_list_free(&cfg->redirection);
	for(int i= 0; i < 4; i++) str_list_free(&cfg->double_underscore[i]);
	str_map_free(&cfg->double_underscore_alias[0]);
	str_map_free(&cfg->double_underscore_alias[1]);

	free(cfg->protocol);
	str_list_free(&cfg->variants);
	str_list_free(&cfg->variable);
	str_list_free(&cfg->parser_function_raw);
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
	size_t name_len= strlen(name);
	for(size_t i= 0; i < cfg->excludes.count; i++) {
		sz_ptr_t start;
		sz_size_t len;
		sz_string_range(&cfg->excludes.items[i], &start, &len);
		if(start && len == name_len && sz_equal(start, name, len) == sz_true_k) return true;
	}
	return false;
}

bool config_has_ext(const ParserConfig *cfg, const char *name) {
	if(!cfg || !name) return false;
	size_t name_len= strlen(name);

	for(size_t i= 0; i < cfg->ext.count; i++) {
		sz_ptr_t start;
		sz_size_t len;
		sz_string_range(&cfg->ext.items[i], &start, &len);
		if(start && len == name_len && str_ci_eq_n((const char *)start, name, len)) return true;
	}
	return false;
}
