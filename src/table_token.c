#include "table_token.h"
#include "accum.h"
#include "token.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

/* Helper: create a simple text child token */
static Token *make_text_token_n(TokenType type, const char *type_name,
												const char *text, size_t text_len,
												Accum *accum) {
	Token *t= token_new(type, type_name);
	if(!t) return NULL;
	token_append_text_n(t, text ? text : "", text ? text_len : 0);
	accum_push(accum, t);
	return t;
}

Token *table_token_create(const char *syntax, const char *attr, const char *inner, Accum *accum) {
	const char *syntax_text = syntax ? syntax : "";
	const char *attr_text = attr ? attr : "";
	const char *inner_text = inner ? inner : "";
	return table_token_create_n(syntax_text, strlen(syntax_text),
										 attr_text, strlen(attr_text),
										 inner_text, strlen(inner_text),
										 accum);
}

Token *table_token_create_n(const char *syntax, size_t syntax_len,
										 const char *attr, size_t attr_len,
										 const char *inner, size_t inner_len,
										 Accum *accum) {
	if(!accum) return NULL;

	/* Create top-level table token and push it first (matches JS constructor order) */
	Token *table= token_new(TOKEN_TABLE, "table");
	if(!table) return NULL;
	accum_push(accum, table);

	const char *syntax_text = syntax ? syntax : "";
	const char *attr_text = attr ? attr : "";
	const char *inner_text = inner ? inner : "";

	/* Syntax token: "table-syntax" */
	Token *syntax_tok= make_text_token_n(TOKEN_SYNTAX, "table-syntax", syntax_text, syntax_len, accum);
	if(!syntax_tok) return table;
	token_append_child(table, syntax_tok);

	/* Attributes token: "attributes" */
	Token *attr_tok= make_text_token_n(TOKEN_ATTRIBUTES, "attributes", attr_text, attr_len, accum);
	if(!attr_tok) return table;
	token_append_child(table, attr_tok);

	/* Inner token: plain token holding remaining table text */
	Token *inner_tok= token_new(TOKEN_PLAIN, "table-inner");
	if(!inner_tok) return table;
	token_append_text_n(inner_tok, inner_text, inner_len);
	accum_push(accum, inner_tok);
	token_append_child(table, inner_tok);

	return table;
}
