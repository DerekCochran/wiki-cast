/*
 * comment_and_ext.h — Stage 0b: HTML comments and extension tags.
 *
 * Mirrors dist/parser/commentAndExt.js.
 */
#pragma once
#include "wiki_cast/token.h"
#include "accum.h"
#include "wiki_cast/config.h"
#include "util/string_util.h"
#include <stdbool.h>

/**
 * Parse HTML comments (`<!-- -->`), extension tags (`<ref>`, `<nowiki>`, etc.),
 * and include/noinclude/onlyinclude tags.
 *
 * Modifies `ws` in place, replacing matched substrings with sentinel markers
 * and pushing the corresponding tokens to `accum`.
 *
 * @param ws         Working string (in/out).
 * @param cfg        Parser config.
 * @param accum      Accumulator.
 * @param include_only  Whether we are in includeOnly (transclusion) mode.
 */
void parse_comment_and_ext(ThreadBuf *tb, const ParserConfig *cfg,
                           Accum *accum, bool include_only);
