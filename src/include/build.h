/*
 * build.h — Build phase: expand \0<n><ch>\x7F markers into child tokens.
 *
 * Mirrors the build() and buildFromStr() methods in dist/src/index.js.
 */
#pragma once
#include "token.h"
#include "accum.h"
#include "util/string_util.h"
#include <stddef.h>

/**
 * Expand sentinel markers in `t`'s text content and recursively build all
 * tokens in the accum.  After this call the token tree is fully resolved.
 *
 * The root token must be the first element of the accum (index 0), though the
 * JS implementation pops it and rebuilds the tree bottom-up via the accum.
 *
 * In the C implementation we follow the same pattern:
 *   - Each token in the accum with a text child containing \0N.\x7F markers
 *     has those markers replaced by the corresponding accum[N] subtree.
 */
/**
 * Drive the full build phase.  ws contains the sentinel-marked root text.
 * Calls build_from_str on root first, then recursively on every accum entry.
 */
void build(Token *root, const ThreadBuf *tb, Accum *accum,
           const ParserConfig *cfg);

/**
 * Parse the sentinel-marked string `str` and append the resulting nodes
 * (text leaves and token pointers from accum) as children of `parent`.
 *
 * After calling this the token's original text child is replaced by the
 * expanded children.
 */
void build_from_str(Token *parent, const char *str, size_t str_len,
                    Accum *accum);

/* Expand sentinels recursively for a token subtree created after build(). */
void build_token_recursive(Token *t, Accum *accum,
                           const ParserConfig *cfg);
void propagate_table_subtypes(Token *t);
