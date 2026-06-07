/*
 * wiki_cast/config.h — Parser configuration loaded from a JSON file.
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
#include "wiki_cast/types.h"

/* Global default allocator for sz_string_t operations */
extern sz_memory_allocator_t allocator_default;

/* Initialize the default allocator */
void config_init_allocator(void);

/* ── Namespace map entry ───────────────────────────────────────────────────── */
typedef struct {
    int         num;
    sz_string_t name;
} NsEntry;

/* ── Protocol item & list ─────────────────────────────────────────────────── */
typedef struct {
    sz_string_view_t protocol;   /* View into protocol buffer (no ownership) */
    sz_string_t protocol_lower; /* Precomputed lowercase version */
} ProtocolItem;

typedef struct {
    ProtocolItem *items;
    size_t         count;
} ProtocolList;

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

    /* expanded protocol items */
    ProtocolList protocol_items;
    bool protocol_items_valid;
    unsigned char protocol_initials[256]; /* lowercase first-byte filter for protocols */
    char *protocol_buffer;       /* Storage for expanded protocol strings */
    size_t protocol_buffer_cap;   /* Capacity of protocol buffer */
    size_t protocol_buffer_len;   /* Used length of protocol buffer */

    /* language variants for converter */
    StrList variants;
    
     /* magic variables list from config.variable (lowercased names) */
     StrList variable;

    /* parserFunction[2] in JS config: raw-like transclusion modifiers
     * (e.g. ["msg","raw"]) used by parseBraces TranscludeToken modifier handling. */
    StrList parser_function_raw;

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
