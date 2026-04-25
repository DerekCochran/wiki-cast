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

    /* ── Lazily-compiled PCRE2 patterns ─── */
    ParserConfigRegex *regex_redirect;       /* used in parse_redirect */
    ParserConfigRegex *regex_ext[2];         /* [0]=!includeOnly, [1]=includeOnly */
    ParserConfigRegex *regex_ext_translate;  /* nowiki inside translate pass */
    ParserConfigRegex *regex_translate;      /* <translate> tags */
    ParserConfigRegex *regex_magic_links;    /* stage 8: free URLs / magic links */
    /* Stage 4: HR and double-underscore cached regex */
    ParserConfigRegex *regex_hr_and_dunder;
    /* compiled regex used by parseLinks to detect protocol prefixes (lazy) */
    ParserConfigRegex *regex_links;
    /* Additional cached regexes for heavy/recurring patterns */
    ParserConfigRegex *regex_html;           /* parse_html */
    ParserConfigRegex *regex_braces;         /* parse_braces */
    ParserConfigRegex *regex_quotes;         /* parse_quotes */
    ParserConfigRegex *regex_list_prefix;    /* parse_list prefix */
    ParserConfigRegex *regex_list_full;      /* parse_list full */
    ParserConfigRegex *regex_list_brace;     /* parse_list brace */
    ParserConfigRegex *regex_external_links; /* parse_external_links */
    ParserConfigRegex *regex_converter;      /* parse_converter split regex */

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
