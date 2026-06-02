/*
 * parse.h — Main entry point for the C wikitext parser.
 *
 * The 11-stage pipeline is driven by wiki_parse().  Each stage
 * transforms the working string by replacing matched substrings with
 * opaque \0<index><char>\x7F sentinel markers and pushing the
 * corresponding Token into the accumulator.  After all 11 stages,
 * build() expands the markers back into the child token tree.
 */
#pragma once
#include "wiki_cast/token.h"
#include "wiki_cast/config.h"
#include <stdbool.h>

/**
 * Parse wikitext and return the root Token.
 *
 * @param wikitext   UTF-8 input; must be null-terminated.
 * @param input_len  Length of the wikitext input in bytes.
 * @param cfg        Parser configuration (from config_load_file / config_load_string).
 * @param include    Whether to parse in includeOnly (transclusion) mode.
 * @param max_stage  0–10; run stages 0..max_stage then build().  Use 10 for full parse.
 * @return           Root Token * (caller must call token_free).
 *                   NULL on allocation failure.
 */
Token *wiki_parse_with_page(const char *wikitext, size_t input_len, const ParserConfig *cfg,
                            bool include, int max_stage,
                            const char *page);

Token *wiki_parse(const char *wikitext, size_t input_len, const ParserConfig *cfg,
                  bool include, int max_stage);
