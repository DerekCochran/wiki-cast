/* table_wiki_cast/token.h — helpers to construct table-related tokens */
#pragma once
#include <stddef.h>
#include "wiki_cast/token.h"
#include "accum.h"

/* Create a TableToken with a syntax token, attributes token, and inner plain token.
 * The function pushes the created top-level TableToken into the accumulator first,
 * then creates and appends child tokens (which are also pushed into the accumulator).
 * Returns the created TableToken (owned by caller/accumulator).
 */
Token *table_token_create(const char *syntax, const char *attr, const char *inner, Accum *accum);
Token *table_token_create_n(const char *syntax, size_t syntax_len,
									 const char *attr, size_t attr_len,
									 const char *inner, size_t inner_len,
									 Accum *accum);
