#pragma once
/*
 * wiki_parser_rules.h — Centralised ParserRules registry
 *
 * Every ParserRules entry is defined in wiki_parser_rules.c, keyed by the
 * original JavaScript regular-expression string that it replaces.  To find
 * the C equivalent of a regex, search this file for the pattern; the struct
 * definition follows immediately.
 *
 * Usage:
 *   #include "util/wiki_parser_rules.h"
 *
 *   // by named constant (preferred at call sites):
 *   parser_scan(buf, len, &wiki_rule_nowiki_paired, cb, ctx);
 *
 *   // by original regex string (useful for documentation / asserting intent):
 *   const ParserRules *r = wiki_parser_rules_get("<nowiki>[\\s\\S]*?<\\/nowiki>");
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

/* ── lookup ───────────────────────────────────────────────────────────────── */

/**
 * Return the ParserRules for the original JavaScript regex string, or NULL.
 * Linear scan; only used at init / debugging paths so performance is not
 * critical.
 */
const ParserRules *wiki_parser_rules_get(const char *regex);
