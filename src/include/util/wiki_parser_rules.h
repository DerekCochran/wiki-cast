#pragma once
/*
 * wiki_parser_rules.h — Centralised ParserRules registry
 *
 * `wiki_parser_rules` is split into two registries:
 * 1. Static rules: `const ParserRules` in `src/util/wiki_parser_rules.c`,
 *    declared here, and registered in static `s_registry[]` by stable rule name.
 * 2. Dynamic config rules: generated in `config_from_cjson()` from config-derived
 *    inputs (`cfg->ext`, `cfg->protocol`, `cfg->variants`, etc.) and inserted into
 *    a dynamic registry by names `config-xxx`.
 *
 * Call sites look up rules by stable names (not regex strings):
 *   const ParserRules *rule = wiki_parser_rules_get("rule-noinclude");
 *
 * Static rules use names like "rule-xxx".
 * Dynamic config rules use names like "config-xxx".
 *
 * Usage:
 *   #include "util/wiki_parser_rules.h"
 *
 *   // by stable name (preferred at call sites):
 *   const ParserRules *rule = wiki_parser_rules_get("rule-noinclude");
 *   if (rule) parser_scan(buf, len, rule, cb, ctx);
 *
 *   // by named constant (when available and convenient):
 *   parser_scan(buf, len, &wiki_rule_nowiki_paired, cb, ctx);
 */

#include "util/callback_parser.h"

/* ── nowiki ───────────────────────────────────────────────────────────────── */

/* <nowiki>[\s\S]*?<\/nowiki> */
extern const ParserRules wiki_rule_nowiki_paired;

/* <nowiki\s*\/> (self-closing variant) */
extern const ParserRules wiki_rule_nowiki_sc;

/* ── translate ────────────────────────────────────────────────────────────── */

/* <translate( nowrap)?>[\s\S]*?<\/translate> */
extern const ParserRules wiki_rule_translate;

/* ── braces / args ────────────────────────────────────────────────────────── */

/* (?<!\{)\{\{\{(inner)\}\}\}(?!\})  —  triple-brace arg, simple-args pass */
extern const ParserRules wiki_rule_triple_brace_arg;

/* (?<!\{)\{\{(inner)\}\}  — outer fixpoint pass template alternation 1 */
extern const ParserRules wiki_rule_main_template_1;

/* \{\{(inner)\}\}(?!\})  — outer fixpoint pass template alternation 2 */
extern const ParserRules wiki_rule_main_template_2;

/* \[\[(?:inner_link)*\]\]  — outer fixpoint pass wikilink parking */
extern const ParserRules wiki_rule_main_wikilink;

/* -\{(?:inner)*\}-  — outer fixpoint pass converter parking */
extern const ParserRules wiki_rule_main_converter;

/* ── C.0 Shared ParserRules additions ────────────────────────────────────── */

/* __([\s\S]*?)__ (validated by callback against cfg->double_underscore) */
extern const ParserRules wiki_rule_dunder_ascii;

/* FULLWIDTH LOW LINE pair: ＿＿([\s\S]*?)＿＿ */
extern const ParserRules wiki_rule_dunder_fullwidth;

/* \[([\s\S]*?)\] — used by parse_external_links callback scanner */
extern const ParserRules wiki_rule_extlink_bracket;

/* <!--([\s\S]*?)--> (closed comments only; unclosed handled in parser fallback) */
extern const ParserRules wiki_rule_html_comment_closed;

/* ── lookup ───────────────────────────────────────────────────────────────── */

/**
 * Return the ParserRules for the given stable name, or NULL.
 * Lookup order: dynamic registry first, then static registry.
 * Static rule names: "rule-xxx"
 * Dynamic config rule names: "config-xxx"
 */
const ParserRules *wiki_parser_rules_get(const char *name);

/**
 * Add or update a dynamic config rule.
 * Returns false on duplicate key (use wiki_parser_rules_remove_dynamic first)
 * or allocation failure.
 * The function copies the ParserRules value (does not store caller-owned pointer).
 */
bool wiki_parser_rules_set_dynamic(const char *name, const ParserRules *rule);

/**
 * Remove one dynamic key. Used when rebuilding a subset.
 */
void wiki_parser_rules_remove_dynamic(const char *name);

/**
 * Clear all config-derived keys during config reload.
 */
void wiki_parser_rules_clear_dynamic(void);
