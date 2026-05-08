/*
 * external_links.h — Stage 7: bracketed external links parser
 */
#pragma once
#include "token.h"
#include "accum.h"
#include "config.h"
#include "../string_util.h"
#include <stdbool.h>

/**
 * Parse bracketed external links like `[http://example.com Label]`.
 * Replaces matches in `ws` with sentinel markers and pushes tokens
 * to `accum`.
 *
 * @param ws     Working string (in/out)
 * @param cfg    Parser config
 * @param accum  Accumulator
 * @param in_file Whether we are inside a file/image parameter
 */
void parse_external_links(ThreadBuf *tb, const ParserConfig *cfg,
                           Accum *accum, bool in_file);
