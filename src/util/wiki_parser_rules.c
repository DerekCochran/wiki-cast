/*
 * wiki_parser_rules.c — Centralised ParserRules registry
 *
 * Each entry is preceded by the original JavaScript regex it replaces, so
 * grepping for the pattern leads straight to the C struct.
 *
 * Static rules are registered with stable names ("rule-xxx").
 * Dynamic config-derived rules are registered with names ("config-xxx").
 */
#include "util/wiki_parser_rules.h"

#include <stddef.h>
#include <string.h>
#include <stdlib.h>

/* ── shared prohibited-pattern arrays (file-scope, never freed) ───────────── */

static const char s_pat_double_lbrack[]    = { '[', '[' };
static const char s_pat_newline_then_nul[] = { '\n', '\0' };

static const char *const s_tpl_prohibited_patterns[] = {
    s_pat_double_lbrack,
    s_pat_newline_then_nul,
};
static const size_t s_tpl_prohibited_pattern_lens[] = { 2, 2 };

/* ── nowiki ───────────────────────────────────────────────────────────────── */

/* <nowiki>[\s\S]*?<\/nowiki> */
const ParserRules wiki_rule_nowiki_paired = {
    .open_delim       = "<nowiki>",
    .open_len         = 8,
    .close_delim      = "</nowiki>",
    .close_len        = 9,
    .match_mode       = PARSER_MATCH_FIRST_CLOSE,
    .case_insensitive = true,
};

/* <nowiki\s*\/> (self-closing variant) */
const ParserRules wiki_rule_nowiki_sc = {
    .open_delim              = "<nowiki",
    .open_len                = 7,
    .open_terminator         = '>',
    .open_attr_forbidden     = "<",
    .open_attr_forbidden_len = 1,
    .self_closing_marker     = "/",
    .self_closing_marker_len = 1,
    .case_insensitive        = true,
};

/* ── translate ────────────────────────────────────────────────────────────── */

/* <translate( nowrap)?>[\s\S]*?<\/translate> */
const ParserRules wiki_rule_translate = {
    .open_delim               = "<translate",
    .open_len                 = 10,
    .open_terminator          = '>',
    .open_attr_forbidden      = "<",
    .open_attr_forbidden_len  = 1,
    .self_closing_marker      = "/",
    .self_closing_marker_len  = 1,
    .close_delim              = "</translate",
    .close_len                = 11,
    .close_terminator         = '>',
    .close_attr_forbidden     = "<",
    .close_attr_forbidden_len = 1,
    .match_mode               = PARSER_MATCH_FIRST_CLOSE,
    /* case_insensitive left false: MediaWiki requires exact case */
};

/* ── braces / args ────────────────────────────────────────────────────────── */

/* (?<!\{)\{\{\{(inner)\}\}\}(?!\})  —  triple-brace arg, simple-args pass
 *
 * inner = [^\n{}\[]|\[(?!\[)|\n(?!\x00)
 * prohibited_chars covers {}, [; the two-byte pattern re-allows \n only
 * when NOT followed by \0, and forbids [[.
 * no_preceding_byte='{'  implements (?<!\{).
 * no_following_byte='}'  implements (?!\}).
 */
const ParserRules wiki_rule_triple_brace_arg = {
    .open_delim                = "{{{",
    .open_len                  = 3,
    .close_delim               = "}}}",
    .close_len                 = 3,
    .match_mode                = PARSER_MATCH_FIRST_CLOSE,
    .prohibited_chars          = "{}",
    .prohibited_chars_len      = 2,
    .prohibited_patterns       = s_tpl_prohibited_patterns,
    .prohibited_pattern_lens   = s_tpl_prohibited_pattern_lens,
    .prohibited_patterns_count = 2,
    .no_preceding_byte         = '{',
    .no_following_byte         = '}',
};
/* (?<!\{)\{\{(inner)\}\}  — outer fixpoint pass template alternation 1 */
const ParserRules wiki_rule_main_template_1 = {
    .open_delim                = "{{",
    .open_len                  = 2,
    .close_delim               = "}}",
    .close_len                 = 2,
    .match_mode                = PARSER_MATCH_FIRST_CLOSE,
    .prohibited_chars          = "{}",
    .prohibited_chars_len      = 2,
    .prohibited_patterns       = s_tpl_prohibited_patterns,
    .prohibited_pattern_lens   = s_tpl_prohibited_pattern_lens,
    .prohibited_patterns_count = 1,
    .no_preceding_byte         = '{',
    .no_following_byte         = 0,
};

/* \{\{(inner)\}\}(?!\})  — outer fixpoint pass template alternation 2 */
const ParserRules wiki_rule_main_template_2 = {
    .open_delim                = "{{",
    .open_len                  = 2,
    .close_delim               = "}}",
    .close_len                 = 2,
    .match_mode                = PARSER_MATCH_FIRST_CLOSE,
    .prohibited_chars          = "{}",
    .prohibited_chars_len      = 2,
    .prohibited_patterns       = s_tpl_prohibited_patterns,
    .prohibited_pattern_lens   = s_tpl_prohibited_pattern_lens,
    .prohibited_patterns_count = 1,
    .no_preceding_byte         = 0,
    .no_following_byte         = '}',
};

/* \[\[(?:inner_link)*\]\]  — outer fixpoint pass wikilink parking */
const ParserRules wiki_rule_main_wikilink = {
    .open_delim                = "[[",
    .open_len                  = 2,
    .close_delim               = "]]",
    .close_len                 = 2,
    .match_mode                = PARSER_MATCH_FIRST_CLOSE,
    .prohibited_chars          = "[]{}",
    .prohibited_chars_len      = 4,
    .prohibited_patterns_count = 0,
};

/* -\{(?:inner)*\}-  — outer fixpoint pass converter parking */
const ParserRules wiki_rule_main_converter = {
    .open_delim                = "-{",
    .open_len                  = 2,
    .close_delim               = "}-",
    .close_len                 = 2,
    .match_mode                = PARSER_MATCH_FIRST_CLOSE,
    .prohibited_chars          = "{}",
    .prohibited_chars_len      = 2,
    .prohibited_patterns       = s_tpl_prohibited_patterns,
    .prohibited_pattern_lens   = s_tpl_prohibited_pattern_lens,
    .prohibited_patterns_count = 2,
};

/* ── C.0 Shared ParserRules additions ────────────────────────────────────── */

/* __([\s\S]*?)__ (validated by callback against cfg->double_underscore) */
const ParserRules wiki_rule_dunder_ascii = {
    .open_delim          = "__",
    .open_len            = 2,
    .close_delim         = "__",
    .close_len           = 2,
    .match_mode          = PARSER_MATCH_FIRST_CLOSE,
};

/* FULLWIDTH LOW LINE pair: ＿＿([\s\S]*?)＿＿ */
const ParserRules wiki_rule_dunder_fullwidth = {
    .open_delim          = "\xEF\xBC\xBF\xEF\xBC\xBF",
    .open_len            = 6,
    .close_delim         = "\xEF\xBC\xBF\xEF\xBC\xBF",
    .close_len           = 6,
    .match_mode          = PARSER_MATCH_FIRST_CLOSE,
};

/* \[([\s\S]*?)\] — used by parse_external_links callback scanner */
const ParserRules wiki_rule_extlink_bracket = {
    .open_delim   = "[",
    .open_len     = 1,
    .close_delim  = "]",
    .close_len    = 1,
    .match_mode   = PARSER_MATCH_FIRST_CLOSE,
};

/* <!--([\s\S]*?)--> (closed comments only; unclosed handled in parser fallback) */
const ParserRules wiki_rule_html_comment_closed = {
    .open_delim       = "<!--",
    .open_len         = 4,
    .close_delim      = "-->",
    .close_len        = 3,
    .match_mode       = PARSER_MATCH_FIRST_CLOSE,
    .case_insensitive = false,
};

/* ── Static registry (keyed by stable name "rule-xxx") ───────────────────── */

static const struct {
    const char      *name;
    const ParserRules *rule;
} s_static_registry[] = {
    /* nowiki */
    { "rule-nowiki-paired",     &wiki_rule_nowiki_paired     },
    { "rule-nowiki-sc",         &wiki_rule_nowiki_sc         },
    /* translate */
    { "rule-translate",         &wiki_rule_translate         },
    /* braces */
    { "rule-triple-brace-arg",  &wiki_rule_triple_brace_arg  },
    { "rule-main-template-1",   &wiki_rule_main_template_1   },
    { "rule-main-template-2",   &wiki_rule_main_template_2   },
    { "rule-main-wikilink",     &wiki_rule_main_wikilink     },
    { "rule-main-converter",    &wiki_rule_main_converter    },
    /* C.0 shared rules */
    { "rule-dunder-ascii",      &wiki_rule_dunder_ascii      },
    { "rule-dunder-fullwidth",  &wiki_rule_dunder_fullwidth  },
    { "rule-extlink-bracket",   &wiki_rule_extlink_bracket   },
    { "rule-html-comment-closed", &wiki_rule_html_comment_closed },
};

/* ── Dynamic registry (keyed by "config-xxx") ───────────────────────────── */
/* Uses a simple dynamic array; lookup is linear (small set, infrequent access) */

typedef struct {
    char        *name;    /* dynamically allocated */
    ParserRules  rule;    /* copied value */
} DynamicEntry;

static DynamicEntry *s_dynamic_entries = NULL;
static size_t s_dynamic_count = 0;
static size_t s_dynamic_capacity = 0;

static void dynamic_registry_init(void) {
    /* No-op if already initialized; called implicitly by set/add functions */
    if (s_dynamic_entries == NULL) {
        s_dynamic_capacity = 8;
        s_dynamic_entries = (DynamicEntry *)calloc(s_dynamic_capacity, sizeof(DynamicEntry));
        s_dynamic_count = 0;
    }
}

static void dynamic_registry_ensure_capacity(void) {
    if (s_dynamic_count >= s_dynamic_capacity) {
        size_t new_cap = s_dynamic_capacity * 2;
        DynamicEntry *grown = (DynamicEntry *)realloc(s_dynamic_entries, new_cap * sizeof(DynamicEntry));
        if (!grown) return; /* allocation failure */
        s_dynamic_entries = grown;
        /* Zero new entries */
        memset(&s_dynamic_entries[s_dynamic_capacity], 0, (new_cap - s_dynamic_capacity) * sizeof(DynamicEntry));
        s_dynamic_capacity = new_cap;
    }
}

bool wiki_parser_rules_set_dynamic(const char *name, const ParserRules *rule) {
    if (!name || !rule) return false;

    dynamic_registry_init();

    /* Check for duplicate */
    for (size_t i = 0; i < s_dynamic_count; i++) {
        if (strcmp(s_dynamic_entries[i].name, name) == 0) {
            return false; /* duplicate key */
        }
    }

    dynamic_registry_ensure_capacity();

    /* Allocate and copy */
    s_dynamic_entries[s_dynamic_count].name = strdup(name);
    if (!s_dynamic_entries[s_dynamic_count].name) return false;

    /* Copy the rule struct */
    s_dynamic_entries[s_dynamic_count].rule = *rule;

    s_dynamic_count++;
    return true;
}

void wiki_parser_rules_remove_dynamic(const char *name) {
    if (!name) return;

    for (size_t i = 0; i < s_dynamic_count; i++) {
        if (strcmp(s_dynamic_entries[i].name, name) == 0) {
            /* Free the name */
            free(s_dynamic_entries[i].name);

            /* Shift remaining entries down */
            for (size_t j = i; j < s_dynamic_count - 1; j++) {
                s_dynamic_entries[j] = s_dynamic_entries[j + 1];
            }
            s_dynamic_count--;
            return;
        }
    }
}

void wiki_parser_rules_clear_dynamic(void) {
    for (size_t i = 0; i < s_dynamic_count; i++) {
        free(s_dynamic_entries[i].name);
    }
    s_dynamic_count = 0;
    /* Note: we keep the allocated capacity for reuse */
}

/* ── Lookup (dynamic first, then static) ─────────────────────────────────── */

const ParserRules *wiki_parser_rules_get(const char *name) {
    if (!name) return NULL;

    /* Check dynamic registry first */
    for (size_t i = 0; i < s_dynamic_count; i++) {
        if (strcmp(s_dynamic_entries[i].name, name) == 0) {
            return &s_dynamic_entries[i].rule;
        }
    }

    /* Check static registry */
    for (size_t i = 0; i < sizeof(s_static_registry) / sizeof(s_static_registry[0]); i++) {
        if (strcmp(name, s_static_registry[i].name) == 0) {
            return s_static_registry[i].rule;
        }
    }

    return NULL;
}
