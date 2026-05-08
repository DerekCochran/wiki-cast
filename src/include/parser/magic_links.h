/*
 * magic_links.h — Stage 8: free URLs, RFC/PMID/ISBN (parseMagicLinks)
 */
#pragma once
#include "token.h"
#include "accum.h"
#include "config.h"
#include "util/string_util.h"

#include <stdbool.h>

/**
 * Parse free external URLs and magic-word links (RFC/PMID/ISBN).
 * Replaces matched substrings in `ws` with sentinel markers and pushes
 * corresponding MagicLink tokens onto `accum`.
 */
void parse_magic_links(ThreadBuf *tb, const ParserConfig *cfg, Accum *accum);
