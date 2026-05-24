#pragma once
#include "token.h"
#include "accum.h"
#include "config.h"
#include "util/string_util.h"

/**
 * Stage 4: parse horizontal rules and double-underscore magic words.
 * Modifies `ws` in-place, pushing created tokens into `accum` and writing
 * sentinel markers into the working string.
 */
void parse_hr_and_double_underscore(ThreadBuf *tb, const ParserConfig *cfg, Accum *accum,
                                    TokenType root_type, const char *root_name);
