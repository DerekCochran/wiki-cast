/*
 * quotes.h — Stage 6: bold/italic quote balancing (parseQuotes)
 */
#pragma once
#include "token.h"
#include "accum.h"
#include "config.h"
#include "util/string_util.h"
#include <stdbool.h>

/**
 * Parse runs of apostrophes ('' / ''' / ''''') and replace them with
 * QuoteToken sentinels in the working string.
 *
 * Mirrors dist/parser/quotes.js behaviour for balancing and sentinel
 * creation. `tidy` mirrors the JS flag (but the current callers pass
 * false from wiki_parse).
 */
void parse_quotes(ThreadBuf *tb, const ParserConfig *cfg, Accum *accum, bool tidy);
