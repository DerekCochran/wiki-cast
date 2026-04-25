#include "table_token.h"
#include "token.h"
#include "accum.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Helper: create a simple text child token */
static Token *make_text_token(TokenType type, const char *type_name, const char *text, Accum *accum)
{
    Token *t = token_new(type, type_name);
    if (!t) return NULL;
    if (text) token_append_text_n(t, text, strlen(text));
    accum_push(accum, t);
    return t;
}

Token *table_token_create(const char *syntax, const char *attr, const char *inner, Accum *accum)
{
    if (!accum) return NULL;

    /* Create top-level table token and push it first (matches JS constructor order) */
    Token *table = token_new(TOKEN_TABLE, "table");
    if (!table) return NULL;
    accum_push(accum, table);

    /* Syntax token: "table-syntax" */
    Token *syntax_tok = make_text_token(TOKEN_SYNTAX, "table-syntax", syntax ? syntax : "", accum);
    if (!syntax_tok) return table;
    token_append_child(table, syntax_tok);

    /* Attributes token: "attributes" */
    Token *attr_tok = make_text_token(TOKEN_ATTRIBUTES, "attributes", attr ? attr : "", accum);
    if (!attr_tok) return table;
    token_append_child(table, attr_tok);

    /* Inner token: plain token holding remaining table text */
    Token *inner_tok = token_new(TOKEN_PLAIN, "table-inner");
    if (!inner_tok) return table;
    if (inner) token_append_text_n(inner_tok, inner, strlen(inner));
    accum_push(accum, inner_tok);
    token_append_child(table, inner_tok);

    return table;
}
