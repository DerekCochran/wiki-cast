/*
 * html.h — Stage 2: HTML tags (parseHtml)
 *
 * Simplified C port of dist/parser/html.js.  This implementation scans for
 * allowed HTML tags from the config and replaces them with sentinel markers
 * while pushing a corresponding `Html` token and a small `html-attrs`
 * attributes token to the accumulator.
 */
#pragma once
#include "token.h"
#include "accum.h"
#include "config.h"
#include "../string_util.h"

/**
 * Parse HTML tags and replace allowed tags with sentinel markers.
 *
 * Modifies `ws` in-place and pushes created tokens into `accum`.
 */
void parse_html(ThreadBuf *tb, const ParserConfig *cfg, Accum *accum);
