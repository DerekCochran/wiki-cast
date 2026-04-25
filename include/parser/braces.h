/*
 * braces.h — Stage 1: templates, arguments, headings (parseBraces)
 *
 * Simplified C port of dist/parser/braces.js.  This implementation performs
 * a conservative set of transformations for `{{...}}`, `{{{...}}}`,
 * `[[...]]` and `-{...}-` fragments, pushing the corresponding tokens into
 * the accumulator and replacing the matched substrings in the working string
 * with sentinel markers.
 */
#pragma once
#include "../token.h"
#include "../accum.h"
#include "../config.h"
#include "../string_util.h"

/**
 * Parse templates, args, links and -{...}- fragments.
 *
 * Modifies `ws` in-place, pushing created tokens into `accum` and writing
 * sentinel markers into the working string.  This is a simplified port and
 * intentionally conservative; it is designed to match common cases and to be
 * safe for further stages.
 */
void parse_braces(ThreadBuf *tb, const ParserConfig *cfg, Accum *accum);
