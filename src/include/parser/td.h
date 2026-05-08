/* td.h — TdToken constructor helper */
#pragma once
#include "token.h"
#include "accum.h"

/** Create a TdToken with given syntax, attr and inner text, push into accum, and return it.
 * syntax_len / attr_len / inner_len are byte lengths (binary-safe; may contain embedded NUL from sentinels). */
Token *create_td_token(const char *syntax,
                       size_t syntax_len,
                       const char *attr, size_t attr_len,
                       const char *inner_syntax, size_t inner_syntax_len,
                       const char *inner, size_t inner_len,
                       Accum *accum);
