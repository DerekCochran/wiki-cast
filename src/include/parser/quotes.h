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

/* Quote run scanner ----------------------------------------------------- */
typedef void (*QuoteRunCb)(size_t pos, size_t run_len, void *user_data);

/*
 * Scan `line` for every maximal run of '\'' with length >= 2. Fires cb for
 * each such run in left-to-right order. Single isolated apostrophes are skipped.
 */
void quote_scan(const char   *line,
				size_t        len,
				QuoteRunCb    cb,
				void         *user_data);
