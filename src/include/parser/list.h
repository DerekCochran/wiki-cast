/*
 * list.h — Stage 9: list parsing (parseList)
 *
 * Mirrors dist/parser/list.js: parses list prefixes at the start of a
 * line (`;:*#`) and emits `list` / `dd` tokens into the accumulator.
 */
#pragma once
#include "token.h"
#include "accum.h"
#include "config.h"
#include "../string_util.h"

/**
 * Parse list markers in the working string `ws` and push created tokens
 * to `accum`.  Modifies `ws` in-place.
 */
void parse_list(ThreadBuf *tb, const ParserConfig *cfg, Accum *accum);
