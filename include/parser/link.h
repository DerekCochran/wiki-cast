/* link.h — Link token constructors (LinkToken, FileToken, CategoryToken)
 *
 * Mirrors the JS structure under dist/src/link/:
 * LinkBaseToken -> LinkToken, FileToken, CategoryToken
 */
#pragma once

#include "../token.h"
#include "../accum.h"
#include "../config.h"

/**
 * Create a LinkToken, FileToken, or CategoryToken with just the link-target child.
 *
 * Mirrors JS LinkBaseToken constructor — basic token factory:
 * - Creates token with given type and type_name
 * - Creates link-target atom child containing raw link string
 * - Pushes token to accum
 *
 * The caller is responsible for parsing and appending the text child via:
 *   Token *text_tok = parse_inner_fragment(...);
 *   token_append_child(link_tok, text_tok);
 *
 * Parameters:
 *   type: one of TOKEN_LINK, TOKEN_FILE, TOKEN_CATEGORY
 *   type_name: "link", "file", or "category"
 *   link: raw link target string (may contain sentinels from earlier stages)
 *   link_len: byte length of link
 *   text: NULL or raw text (not used by this function; for informational only)
 *   text_len: unused
 *   delimiter: NULL or delimiter marker (not used; for informational only)
 *   cfg: required (may be used by extended implementations)
 *   accum: accumulator for pushing tokens
 *   tidy: unused
 *
 * Returns the created token (already pushed to accum with link-target child).
 */
Token *create_link_token(TokenType type, const char *type_name,
                         const char *link, size_t link_len,
                         const char *text, size_t text_len,
                         const char *delimiter,
                         const ParserConfig *cfg,
                         Accum *accum,
                         bool tidy);
