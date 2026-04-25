/*
 * accum.h — Accumulator for tokens created during parsing.
 *
 * Mirrors the #accum array in the JS Token class.  Each time a sub-token is
 * created during a parse stage it is pushed here; at the end build() resolves
 * \0<n><ch>\x7F markers in text nodes by looking up accum[n].
 */
#pragma once
#include "token.h"
#include <stddef.h>

typedef struct {
    Token **tokens;   /* pointer array — non-owning during parse, owned after build */
    size_t  count;
    size_t  cap;
} Accum;

/** Initialise an empty accumulator. */
void accum_init(Accum *a);

/** Push a token onto the accumulator.  Does NOT take ownership. */
void accum_push(Accum *a, Token *t);

/** Return accum[i] or NULL if out of range. */
Token *accum_get(const Accum *a, size_t i);

/** Free the accumulator itself (does NOT free the contained tokens). */
void accum_free(Accum *a);

/** Number of tokens in the accumulator. */
static inline size_t accum_len(const Accum *a) { return a->count; }
