/*
 * config.h — Parser configuration loaded from a JSON file.
 *
 * Mirrors the config object that wikiparser-node loads from config/<wildcard>.json.
 * The schema is:
 *   { ext, html, namespaces, nsid, functionHook, variable, parserFunction,
 *     doubleUnderscore, protocol, interwiki, img, redirection, variants,
 *     excludes }
 *
 * Compiled PCRE2 patterns are cached here on first use (lazy compilation).
 */
#pragma once
#include <stddef.h>
#include <stdbool.h>

/* Forward declaration for PCRE2 types (avoid pulling in pcre2.h in every TU) */
typedef void ParserConfigRegex; /* actual type: pcre2_code_8 * */

/* ── String list ─────────────────────────────────────────────────────────── */
typedef struct {
    char  **items;
    size_t  count;
} StrList;

/* ── String map (key->value pairs) ─────────────────────────────────────── */
typedef struct {
    char  **keys;
    char  **values;
    size_t  count;
} StrMap;

/* ── Namespace map entry ───────────────────────────────────────────────────── */
typedef struct {
    int   num;
    char *name;
} NsEntry;

/* ── Parser config ──────────────────────────────────────────────────────── */
typedef struct {
    /* extension tag names, lowercased e.g. ["ref","references","nowiki",...] */
    StrList ext;

    /* html[0]=normal inline/block, html[1]=li-like, html[2]=void */
    StrList html[3];

    /* namespace map */
    NsEntry *namespaces;
    size_t   ns_count;

    /* redirection keywords e.g. ["#redirect","#重定向",...] */
    StrList redirection;

    /* doubleUnderscore[0]=case-insensitive, [1]=case-sensitive,
       [2]=insensitive→canonical map (keys), [3]=sensitive→canonical map (keys) */
    StrList double_underscore[4];
     StrMap  double_underscore_alias[2]; /* [0]=insensitive alias map, [1]=sensitive alias map */

    /* protocol regex fragment e.g. "https?:|ftp:" */
    char *protocol;

    /* language variants for converter */
    StrList variants;
    
     /* magic variables list from config.variable (lowercased names) */
     StrList variable;

    /* parserFunction[3] in JS config: subst-like transclusion modifiers
     * (e.g. ["safesubst","subst"]) used by parseBraces for {{{...}}}. */
    StrList parser_function_subst;

    /* parserFunction maps used by JS getMagicWordInfo(name, parserFunction):
     * [0] case-insensitive map (lookup by lowercased key),
     * [1] case-sensitive map (lookup by exact key). */
    StrMap parser_function_insensitive;
    StrMap parser_function_sensitive;

    /* interwiki prefixes */
    StrList interwiki;

    /* image parameter syntax map from config.img (syntax -> canonical name) */
    StrMap img;

    /* token types to exclude from parsing (e.g. ["html","table"]) */
    StrList excludes;

    /* JS parseLinks toggles regex shape with config.inExt. */
    bool in_ext;

    /* Note: compiled PCRE2 patterns are now owned by the process-wide
     * `pcre_cache` utility. Parsers should call pcre_cache_get(pattern, flags)
     * to obtain a shared compiled `pcre2_code *` rather than storing them
     * in the ParserConfig. */
    /* Prebuilt dynamic regex pattern strings (cached here to avoid
     * per-parse snprintf/realloc work). Built lazily by parsers and
     * freed in `config_free`. */
    char *pattern_redirect;
    char *pattern_ext;                /* general ext-tags pattern */
    char *pattern_ext_includeonly;    /* include-only variant */
    char *pattern_hr_and_dunder;
    char *pattern_magic_links;
    char *pattern_external_links;
    char *pattern_converter;

} ParserConfig;

/* ── Config lifecycle ────────────────────────────────────────────────────── */

/**
 * Load config from a JSON file path (e.g. "config/default.json").
 * Returns NULL on error.  Caller must call config_free() when done.
 */
ParserConfig *config_load_file(const char *path);

/**
 * Load config from a JSON string.
 * Returns NULL on error.
 */
ParserConfig *config_load_string(const char *json_str, size_t len);

/**
 * Free all memory associated with a config.
 */
void config_free(ParserConfig *cfg);

/**
 * Returns true if the given string is in the excludes list.
 */
bool config_excluded(const ParserConfig *cfg, const char *name);

/**
 * Returns true if ext_name is in cfg->ext.
 */
bool config_has_ext(const ParserConfig *cfg, const char *name);
