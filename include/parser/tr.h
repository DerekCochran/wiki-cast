/* tr.h — TrToken constructor helper */
#pragma once
#include "../token.h"
#include "../accum.h"

/** Create a TrToken with given syntax and attr, push into accum, and return it.
 * syntax_len / attr_len are byte lengths and may include embedded NUL bytes.
 */
Token *create_tr_token(const char *syntax, size_t syntax_len,
					   const char *attr, size_t attr_len,
					   Accum *accum);
