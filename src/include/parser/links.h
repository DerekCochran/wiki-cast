/*
 * links.h — Stage 5: internal links, files, categories (parseLinks)
 *
 * Simplified port of dist/parser/links.js.  This implementation detects
 * internal link bracket pairs `[[...]]`, validates targets with
 * `title_is_valid_half_parsed`, and constructs Link/File/Category tokens
 * which are pushed onto `accum` and replaced in the working string with
 * sentinel markers.
 */
#pragma once
#include "wiki_cast/token.h"
#include "accum.h"
#include "wiki_cast/config.h"
#include "util/string_util.h"
#include "../title.h"
#include <stdbool.h>

/**
 * Parse internal links in `ws` and push created tokens into `accum`.
 * The function updates the working string in-place (replacing accepted
 * `[[...]]` fragments with sentinel markers) and returns nothing.
 */
void parse_links(ThreadBuf *tb, const ParserConfig *cfg, Accum *accum,
                 const char *page, bool tidy);
