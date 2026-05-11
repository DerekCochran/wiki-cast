/*
 * wiki_parser_rules.c — Centralised ParserRules registry
 *
 * Each entry is preceded by the original JavaScript regex it replaces, so
 * grepping for the pattern leads straight to the C struct.
 */
#include "util/wiki_parser_rules.h"

#include <stddef.h>
#include <string.h>

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

/* ── lookup table ─────────────────────────────────────────────────────────── */

static const struct {
    const char      *regex;
    const ParserRules *rule;
} s_registry[] = {
    /* nowiki */
    { "<nowiki>[\\s\\S]*?<\\/nowiki>",                  &wiki_rule_nowiki_paired     },
    { "<nowiki\\s*\\/>",                                &wiki_rule_nowiki_sc         },
    /* translate */
    { "<translate( nowrap)?>[\\s\\S]*?<\\/translate>",  &wiki_rule_translate         },
    /* braces */
    { "(?<!\\{)\\{\\{\\{(inner)\\}\\}\\}(?!\\})",       &wiki_rule_triple_brace_arg  },
};

const ParserRules *wiki_parser_rules_get(const char *regex) {
    if (!regex) return NULL;
    for (size_t i = 0; i < sizeof(s_registry) / sizeof(s_registry[0]); i++) {
        if (strcmp(regex, s_registry[i].regex) == 0)
            return s_registry[i].rule;
    }
    return NULL;
}
