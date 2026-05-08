/*
 * table.h — Stage 3: tables
 */
#pragma once
#include <stddef.h>
#include "token.h"
#include "accum.h"
#include "config.h"
#include "../string_util.h"

/**
 * Parse tables `{| ... |}` and replace them with sentinel markers while
 * pushing corresponding `TOKEN_TABLE` tokens into the accumulator.
 */
void parse_table(ThreadBuf *tb, const ParserConfig *cfg, Accum *accum);
