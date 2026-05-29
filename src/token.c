/*
 * token.c — Token node lifecycle implementation.
 */
#include "token.h"
#include "util/log.h"
#include "util/thread_buffer.h"
#include "stringzilla/stringzilla.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Initial capacity for children array */
#define CHILD_INIT_CAP 4

static inline size_t token_name_len(const Token *t) {
	if(!t || !t->name) return 0;
	return strlen(t->name);
}

typedef struct {
	const char *name;
	TokenSubType subtype;
} TokenSubTypeMapEntry;

static const TokenSubTypeMapEntry TOKEN_SUBTYPE_MAP[]= {
	{ "root", TOKEN_SUBTYPE_ROOT },
	{ "redirect", TOKEN_SUBTYPE_REDIRECT },
	{ "redirect-syntax", TOKEN_SUBTYPE_REDIRECT_SYNTAX },
	{ "redirect-target", TOKEN_SUBTYPE_REDIRECT_TARGET },
	{ "comment", TOKEN_SUBTYPE_COMMENT },
	{ "ext", TOKEN_SUBTYPE_EXT },
	{ "noinclude", TOKEN_SUBTYPE_NOINCLUDE },
	{ "include", TOKEN_SUBTYPE_INCLUDE },
	{ "includeonly", TOKEN_SUBTYPE_INCLUDEONLY },
	{ "onlyinclude", TOKEN_SUBTYPE_ONLYINCLUDE },
	{ "translate", TOKEN_SUBTYPE_TRANSLATE },
	{ "arg", TOKEN_SUBTYPE_ARG },
	{ "arg-name", TOKEN_SUBTYPE_ARG_NAME },
	{ "arg-default", TOKEN_SUBTYPE_ARG_DEFAULT },
	{ "template", TOKEN_SUBTYPE_TEMPLATE },
	{ "template-name", TOKEN_SUBTYPE_TEMPLATE_NAME },
	{ "magic-word", TOKEN_SUBTYPE_MAGIC_WORD },
	{ "magic-word-name", TOKEN_SUBTYPE_MAGIC_WORD_NAME },
	{ "parameter", TOKEN_SUBTYPE_PARAMETER },
	{ "parameter-key", TOKEN_SUBTYPE_PARAMETER_KEY },
	{ "parameter-value", TOKEN_SUBTYPE_PARAMETER_VALUE },
	{ "heading", TOKEN_SUBTYPE_HEADING },
	{ "heading-title", TOKEN_SUBTYPE_HEADING_TITLE },
	{ "heading-trail", TOKEN_SUBTYPE_HEADING_TRAIL },
	{ "html", TOKEN_SUBTYPE_HTML },
	{ "html-attrs", TOKEN_SUBTYPE_HTML_ATTRS },
	{ "html-attr", TOKEN_SUBTYPE_HTML_ATTR },
	{ "html-attr-dirty", TOKEN_SUBTYPE_HTML_ATTR_DIRTY },
	{ "table", TOKEN_SUBTYPE_TABLE },
	{ "tr", TOKEN_SUBTYPE_TR },
	{ "td", TOKEN_SUBTYPE_TD },
	{ "table-syntax", TOKEN_SUBTYPE_TABLE_SYNTAX },
	{ "table-attrs", TOKEN_SUBTYPE_TABLE_ATTRS },
	{ "table-attr", TOKEN_SUBTYPE_TABLE_ATTR },
	{ "table-attr-dirty", TOKEN_SUBTYPE_TABLE_ATTR_DIRTY },
	{ "table-inter", TOKEN_SUBTYPE_TABLE_INTER },
	{ "table-inner", TOKEN_SUBTYPE_TABLE_INNER },
	{ "td-inner", TOKEN_SUBTYPE_TD_INNER },
	{ "hr", TOKEN_SUBTYPE_HR },
	{ "double-underscore", TOKEN_SUBTYPE_DOUBLE_UNDERSCORE },
	{ "link", TOKEN_SUBTYPE_LINK },
	{ "file", TOKEN_SUBTYPE_FILE },
	{ "category", TOKEN_SUBTYPE_CATEGORY },
	{ "text", TOKEN_SUBTYPE_TEXT },
	{ "link-target", TOKEN_SUBTYPE_LINK_TARGET },
	{ "link-text", TOKEN_SUBTYPE_LINK_TEXT },
	{ "quote", TOKEN_SUBTYPE_QUOTE },
	{ "ext-link", TOKEN_SUBTYPE_EXT_LINK },
	{ "ext-link-url", TOKEN_SUBTYPE_EXT_LINK_URL },
	{ "ext-link-text", TOKEN_SUBTYPE_EXT_LINK_TEXT },
	{ "magic-link", TOKEN_SUBTYPE_MAGIC_LINK },
	{ "free-ext-link", TOKEN_SUBTYPE_FREE_EXT_LINK },
	{ "list", TOKEN_SUBTYPE_LIST },
	{ "dd", TOKEN_SUBTYPE_DD },
	{ "converter", TOKEN_SUBTYPE_CONVERTER },
	{ "converter-rule", TOKEN_SUBTYPE_CONVERTER_RULE },
	{ "converter-rule-from", TOKEN_SUBTYPE_CONVERTER_RULE_FROM },
	{ "converter-rule-variant", TOKEN_SUBTYPE_CONVERTER_RULE_VARIANT },
	{ "converter-rule-to", TOKEN_SUBTYPE_CONVERTER_RULE_TO },
	{ "converter-flags", TOKEN_SUBTYPE_CONVERTER_FLAGS },
	{ "converter-flag", TOKEN_SUBTYPE_CONVERTER_FLAG },
	{ "attributes", TOKEN_SUBTYPE_ATTRIBUTES },
	{ "attr-equal-tmp", TOKEN_SUBTYPE_ATTR_EQUAL_TMP },
	{ "attr-key", TOKEN_SUBTYPE_ATTR_KEY },
	{ "attr-value", TOKEN_SUBTYPE_ATTR_VALUE },
	{ "atom", TOKEN_SUBTYPE_ATOM },
	{ "hidden", TOKEN_SUBTYPE_HIDDEN },
	{ "ext-attrs", TOKEN_SUBTYPE_EXT_ATTRS },
	{ "ext-inner", TOKEN_SUBTYPE_EXT_INNER },
	{ "ext-attr-dirty", TOKEN_SUBTYPE_EXT_ATTR_DIRTY },
	{ "ext-attr", TOKEN_SUBTYPE_EXT_ATTR },
	{ "image-parameter", TOKEN_SUBTYPE_IMAGE_PARAMETER },
	{ "gallery-image", TOKEN_SUBTYPE_GALLERY_IMAGE },
	{ "imagemap-image", TOKEN_SUBTYPE_IMAGEMAP_IMAGE },
	{ "gallery-line", TOKEN_SUBTYPE_GALLERY_LINE },
	{ "gallery-param-wrapper", TOKEN_SUBTYPE_GALLERY_PARAM_WRAPPER },
	{ "imagemap-link-inner", TOKEN_SUBTYPE_IMAGEMAP_LINK_INNER },
	{ "imagemap-image-line", TOKEN_SUBTYPE_IMAGEMAP_IMAGE_LINE },
	{ "imagemap-link", TOKEN_SUBTYPE_IMAGEMAP_LINK },
	{ "invoke-module", TOKEN_SUBTYPE_INVOKE_MODULE },
	{ "invoke-function", TOKEN_SUBTYPE_INVOKE_FUNCTION },
	{ "param-line", TOKEN_SUBTYPE_PARAM_LINE },
};

sz_string_view_t token_subtype_name(TokenSubType subtype) {
	for(size_t i= 0; i < sizeof(TOKEN_SUBTYPE_MAP) / sizeof(TOKEN_SUBTYPE_MAP[0]); i++) {
		if(TOKEN_SUBTYPE_MAP[i].subtype == subtype) {
			return (sz_string_view_t){ TOKEN_SUBTYPE_MAP[i].name, strlen(TOKEN_SUBTYPE_MAP[i].name) };
		}
	}
	return (sz_string_view_t){ NULL, 0 };
}

TokenSubType token_subtype_from_name(const char *type_name) {
	if(!type_name || !type_name[0]) return TOKEN_SUBTYPE_NONE;
	for(size_t i= 0; i < sizeof(TOKEN_SUBTYPE_MAP) / sizeof(TOKEN_SUBTYPE_MAP[0]); i++) {
		if(strcmp(TOKEN_SUBTYPE_MAP[i].name, type_name) == 0) {
			return TOKEN_SUBTYPE_MAP[i].subtype;
		}
	}
	return TOKEN_SUBTYPE_NONE;
}

Token *token_new(TokenType type, const char *type_name) {
	return token_new_with_subtype(type, token_subtype_from_name(type_name));
}

Token *token_new_with_subtype(TokenType type, TokenSubType subtype) {
	Token *t= calloc(1, sizeof(Token));
	if(!t) return NULL;
	t->type= type;
	t->subtype= subtype;
	t->child_cap= CHILD_INIT_CAP;
	t->children= malloc(CHILD_INIT_CAP * sizeof(Child));
	if(!t->children) {
		free(t);
		return NULL;
	}
	return t;
}

void token_append_text_n(Token *t, const char *text, size_t len) {
	assert(t);
	if(len > 0) assert(text);
	if(t->child_count >= t->child_cap) {
		t->child_cap*= 2;
		t->children= realloc(t->children, t->child_cap * sizeof(Child));
		assert(t->children);
	}
	char *owned= malloc(len + 1);
	assert(owned);
	if(len > 0) {
		sz_copy(owned, text, len);
	}
	owned[len]= '\0';
	Child *c= &t->children[t->child_count++];
	c->is_text= true;
	c->text_len= len;
	c->text= owned;
	c->text_owned = true;
}

void token_append_child(Token *t, Token *child) {
	assert(t && child);
	if(t->child_count >= t->child_cap) {
		t->child_cap*= 2;
		t->children= realloc(t->children, t->child_cap * sizeof(Child));
		assert(t->children);
	}
	Child *c= &t->children[t->child_count++];
	c->is_text= false;
	c->text_len= 0;
	c->token= child;
}

void token_set_name_owned(Token *t, char *name) {
	if(!t) {
		free(name);
		return;
	}
	free(t->name);
	t->name= name;
}

void token_clear_name(Token *t) {
	if(!t) return;
	free(t->name);
	t->name= NULL;
}

// TODO:  Should these be freed or can they use the thread buffer?
static void free_token_data(Token *t) {
	switch(t->type) {
	case TOKEN_EXT_ATTR:
		free((void *)t->data.ext_attr.equal.start);
		break;
	case TOKEN_PLAIN:
		if(t->subtype == TOKEN_SUBTYPE_IMAGE_PARAMETER) {
			free((void *)t->data.image_param.raw_syntax.start);
		}
		break;
	case TOKEN_TD:
		free((void *)t->data.td.inner_syntax.start);
		break;
	case TOKEN_HTML:
		free((void *)t->data.html.orig_tag.start);
		break;
	case TOKEN_REDIRECT:
		free((void *)t->data.redirect.pre.start);
		free((void *)t->data.redirect.post.start);
		free((void *)t->data.redirect.link.start);
		free((void *)t->data.redirect.display.start);
		break;
	case TOKEN_EXT_LINK:
		free((void *)t->data.ext_link.space.start);
		break;
	case TOKEN_EXT:
		free((void *)t->data.ext.name.start);
		free((void *)t->data.ext.attr.start);
		free((void *)t->data.ext.inner.start);
		free((void *)t->data.ext.closing.start);
		break;
	case TOKEN_TRANSCLUDE:
		free((void *)t->data.transclude.modifier.start);
		break;
	case TOKEN_INCLUDE:
	case TOKEN_NOINCLUDE:
		free((void *)t->data.include.tag.start);
		free((void *)t->data.include.attr.start);
		free((void *)t->data.include.inner.start);
		free((void *)t->data.include.closing.start);
		break;
	default:
		break;
	}
}

static void token_free_graph(Token *node, unsigned epoch) {
	if(!node) return;
	if(node->seen_epoch == epoch) return;

	node->seen_epoch = epoch;

	for(size_t i= 0; i < node->child_count; i++) {
		Child *c= &node->children[i];
		if(c->is_text) {
			if(c->text_owned && c->text) free((void*)c->text);
		} else {
			token_free_graph(c->token, epoch);
		}
	}

	free(node->children);
	free_token_data(node);
	free(node->name);
	free(node);
}

void token_free(Token *t) {
	if(!t) return;
	static unsigned token_free_epoch = 1;
	unsigned epoch = token_free_epoch++;
	if(token_free_epoch == 0) token_free_epoch = 1;
	token_free_graph(t, epoch);
}

void token_free_shallow(Token *t) {
	if(!t) return;
	/* Free text children only; do not recurse into token children. */
	for(size_t i= 0; i < t->child_count; i++) {
		Child *c= &t->children[i];
		if(c->is_text) {
			if(c->text_owned && c->text) free((void*)c->text);
		} else {
			/* Clear pointer but do not free c->token here. Caller will free */
			c->token= NULL;
		}
	}
	free(t->children);
	free_token_data(t);
	free(t->name);
	free(t);
}

static void thread_buf_append(ThreadBuf *tb, const char *data, size_t len) {
	if(len == 0) return;
	wiki_thread_buf_reserve(tb, tb->len + len);
	sz_copy(tb->buf + tb->len, data, len);
	tb->len+= len;
}

static void thread_buf_append_char(ThreadBuf *tb, char ch) {
	wiki_thread_buf_reserve(tb, tb->len + 1);
	tb->buf[tb->len++]= ch;
}

static void token_to_string_rec(const Token *t, ThreadBuf *tb) {
	if(!t || !tb) return;
	const size_t t_name_len = (t->name && t->name[0] != '\0') ? strlen(t->name) : 0;

	// TODO:  Look at the different types and see if functionality can be combined
	//        For example, can we have a preamble and postamble such as <!--, <, [[, etc
	//        These can be static to the file and a pointer to them added to the token struct
	switch(t->type) {
	case TOKEN_COMMENT:
		thread_buf_append(tb, "<!--", 4);
		for(size_t i= 0; i < t->child_count; i++) {
			const Child *c= &t->children[i];
			if(c->is_text) {
				thread_buf_append(tb, c->text, c->text_len);
			} else {
				token_to_string_rec(c->token, tb);
			}
		}
		if(t->data.comment.closed) {
			thread_buf_append(tb, "-->", 3);
		}
		return;

	case TOKEN_EXT: {
		/* Use original-cased tag name for serialization (JS parity) */
		const char *ext_tag= t->data.ext.name.start ? t->data.ext.name.start : t->name;
		size_t ext_tag_len= t->data.ext.name.start ? t->data.ext.name.length : t_name_len;
		const char *ext_closing= t->data.ext.closing.start ? t->data.ext.closing.start : ext_tag;
		size_t ext_closing_len= t->data.ext.closing.start ? t->data.ext.closing.length : ext_tag_len;
		thread_buf_append_char(tb, '<');
		if(ext_tag) thread_buf_append(tb, ext_tag, ext_tag_len);
		if(t->child_count > 0) {
			const Child *c= &t->children[0];
			if(c->is_text)
				thread_buf_append(tb, c->text, c->text_len);
			else
				token_to_string_rec(c->token, tb);
		}
		if(t->data.ext.self_closing) {
			thread_buf_append(tb, "/>", 2);
			return;
		}
		thread_buf_append_char(tb, '>');
		if(t->child_count > 1) {
			const Child *c= &t->children[1];
			if(c->is_text)
				thread_buf_append(tb, c->text, c->text_len);
			else
				token_to_string_rec(c->token, tb);
		}
		thread_buf_append(tb, "</", 2);
		if(ext_closing) thread_buf_append(tb, ext_closing, ext_closing_len);
		thread_buf_append_char(tb, '>');
		return;
	}

	case TOKEN_EXT_ATTR: {
		/* JS AttributeToken.toString(): key + equal + quoteOpen + value + quoteClose */
		if(t->child_count > 0) {
			const Child *c= &t->children[0];
			if(c->is_text)
				thread_buf_append(tb, c->text, c->text_len);
			else
				token_to_string_rec(c->token, tb);
		}
		if(t->data.ext_attr.equal.start) {
			thread_buf_append(tb, t->data.ext_attr.equal.start, t->data.ext_attr.equal.length);
			if(t->data.ext_attr.quote_open)
				thread_buf_append_char(tb, t->data.ext_attr.quote_open);
			if(t->child_count > 1) {
				const Child *c= &t->children[1];
				if(c->is_text)
					thread_buf_append(tb, c->text, c->text_len);
				else
					token_to_string_rec(c->token, tb);
			}
			if(t->data.ext_attr.quote_close)
				thread_buf_append_char(tb, t->data.ext_attr.quote_close);
		}
		return;
	}

	case TOKEN_PLAIN: {
		if(t->subtype == TOKEN_SUBTYPE_CONVERTER_RULE) {
			if(t->child_count == 3) {
				const Child *from= &t->children[0];
				const Child *variant= &t->children[1];
				const Child *to= &t->children[2];

				if(from->is_text)
					thread_buf_append(tb, from->text, from->text_len);
				else
					token_to_string_rec(from->token, tb);

				thread_buf_append(tb, "=>", 2);

				if(variant->is_text)
					thread_buf_append(tb, variant->text, variant->text_len);
				else
					token_to_string_rec(variant->token, tb);

				thread_buf_append_char(tb, ':');

				if(to->is_text)
					thread_buf_append(tb, to->text, to->text_len);
				else
					token_to_string_rec(to->token, tb);
				return;
			}

			for(size_t i= 0; i < t->child_count; i++) {
				if(i > 0) thread_buf_append_char(tb, ':');
				const Child *c= &t->children[i];
				if(c->is_text)
					thread_buf_append(tb, c->text, c->text_len);
				else
					token_to_string_rec(c->token, tb);
			}
			return;
		}

		for(size_t i= 0; i < t->child_count; i++) {
			if(i > 0 && t->sep != '\0') {
				thread_buf_append_char(tb, t->sep);
			}
			const Child *c= &t->children[i];
			if(c->is_text) {
				thread_buf_append(tb, c->text, c->text_len);
			} else {
				token_to_string_rec(c->token, tb);
			}
		}
		return;
	}

	case TOKEN_LINK:
	case TOKEN_FILE:
	case TOKEN_CATEGORY:
	case TOKEN_REDIRECT_TARGET: {
		bool is_file_line_image= (t->type == TOKEN_FILE &&
			(t->subtype == TOKEN_SUBTYPE_GALLERY_IMAGE || t->subtype == TOKEN_SUBTYPE_IMAGEMAP_IMAGE));

		/* Internal links: [[target|text]] — join children with '|' and wrap.
             * Gallery images serialize as plain lines without [[...]]. */
		if(!is_file_line_image) {
			thread_buf_append(tb, "[[", 2);
		}
		for(size_t i= 0; i < t->child_count; i++) {
			if(i > 0) {
				/* Special-case: leading ':' text child should not be separated from the following target by a delimiter. */
				if(!(i == 1 && t->children[0].is_text && t->children[0].text && t->children[0].text[0] == ':')) {
					if(t->type == TOKEN_FILE) {
						/* JS LinkBaseToken parity: FileToken uses one delimiter style for all separators. */
						if(t->data.link.magic_pipe) {
							thread_buf_append(tb, "{{!}}", 5);
						} else {
							thread_buf_append_char(tb, '|');
						}
					} else if(t->data.link.magic_pipe && i == 1) {
						/* For LinkToken, only use {{!}} for the first delimiter if magic_pipe is set. */
						thread_buf_append(tb, "{{!}}", 5);
					} else {
						thread_buf_append_char(tb, '|');
					}
				}
			}
			const Child *c= &t->children[i];
			if(c->is_text) {
				thread_buf_append(tb, c->text, c->text_len);
			} else if(t->type == TOKEN_FILE && c->token && c->token->type == TOKEN_PLAIN && c->token->subtype == TOKEN_SUBTYPE_IMAGE_PARAMETER) {
				const Token *p= c->token;
				if(p->data.image_param.raw_syntax.start) {
					const char *syntax= p->data.image_param.raw_syntax.start;
					size_t syntax_len= p->data.image_param.raw_syntax.length;
					const char *slot= strstr(syntax, "$1");
					if(!slot) {
						thread_buf_append(tb, syntax, syntax_len);
					} else {
						size_t pre_len= (size_t)(slot - syntax);
						size_t post_len= syntax_len - pre_len - 2;  /* Use cached length to compute post_len */
						thread_buf_append(tb, syntax, pre_len);
						for(size_t pi= 0; pi < p->child_count; pi++) {
							const Child *pc= &p->children[pi];
							if(pc->is_text)
								thread_buf_append(tb, pc->text, pc->text_len);
							else
								token_to_string_rec(pc->token, tb);
						}
						thread_buf_append(tb, slot + 2, post_len);
					}
							} else if(p->name && strcmp(p->name, "caption") == 0) {
					for(size_t pi= 0; pi < p->child_count; pi++) {
						const Child *pc= &p->children[pi];
						if(pc->is_text)
							thread_buf_append(tb, pc->text, pc->text_len);
						else
							token_to_string_rec(pc->token, tb);
					}
							} else if(p->name && p->name[0] != '\0') {
									size_t p_name_len = strlen(p->name);
									thread_buf_append(tb, p->name, p_name_len);
				} else {
					token_to_string_rec(c->token, tb);
				}
			} else {
				token_to_string_rec(c->token, tb);
			}
		}
		if(!is_file_line_image) {
			thread_buf_append(tb, "]]", 2);
		}
		return;
	}

	case TOKEN_EXT_LINK: {
		/* External link: [url label] or [url] */
		thread_buf_append_char(tb, '[');
		if(t->child_count > 0) {
			const char *space= t->data.ext_link.space.start;
			size_t space_len= t->data.ext_link.space.length;
			const Child *c= &t->children[0];
			if(c->is_text)
				thread_buf_append(tb, c->text, c->text_len);
			else
				token_to_string_rec(c->token, tb);

			if(t->child_count == 1) {
				if(space) {
					thread_buf_append(tb, space, space_len);
				}
			} else {
				if(space) {
					thread_buf_append(tb, space, space_len);
				} else {
					thread_buf_append_char(tb, ' ');
				}
				for(size_t i= 1; i < t->child_count; i++) {
					const Child *ci= &t->children[i];
					if(ci->is_text)
						thread_buf_append(tb, ci->text, ci->text_len);
					else
						token_to_string_rec(ci->token, tb);
				}
			}
		}
		thread_buf_append_char(tb, ']');
		return;
	}

	case TOKEN_ARG: {
		/* Triple-brace argument: {{{name|default}}} */
		thread_buf_append(tb, "{{{", 3);
		for(size_t i= 0; i < t->child_count; i++) {
			if(i > 0) thread_buf_append_char(tb, '|');
			const Child *c= &t->children[i];
			if(c->is_text)
				thread_buf_append(tb, c->text, c->text_len);
			else
				token_to_string_rec(c->token, tb);
		}
		thread_buf_append(tb, "}}}", 3);
		return;
	}

	case TOKEN_TRANSCLUDE: {
		/* Template/magic-word: {{name|params}} */
		thread_buf_append(tb, "{{", 2);
		if(t->data.transclude.modifier.start) {
			thread_buf_append(tb, t->data.transclude.modifier.start, t->data.transclude.modifier.length);
		}
		bool is_magic_word= (t->subtype == TOKEN_SUBTYPE_MAGIC_WORD);
		for(size_t i= 0; i < t->child_count; i++) {
			if(i > 0) {
				if(is_magic_word && i == 1)
					thread_buf_append_char(tb, ':');
				else
					thread_buf_append_char(tb, '|');
			}
			const Child *c= &t->children[i];
			if(c->is_text)
				thread_buf_append(tb, c->text, c->text_len);
			else
				token_to_string_rec(c->token, tb);
		}
		thread_buf_append(tb, "}}", 2);
		return;
	}

	case TOKEN_PARAMETER: {
		/* ParameterToken: positional => value; named => key=value */
		if(t->child_count == 0) return;

		if(t->child_count == 1) {
			const Child *only= &t->children[0];
			if(only->is_text)
				thread_buf_append(tb, only->text, only->text_len);
			else
				token_to_string_rec(only->token, tb);
			return;
		}

		const Child *k= &t->children[0];
		const Child *v= &t->children[1];
		bool key_empty= true;

		if(k->is_text) {
			key_empty= (k->text_len == 0);
		} else if(k->token) {
			key_empty= (k->token->child_count == 0);
		}

		if(!key_empty) {
			if(k->is_text)
				thread_buf_append(tb, k->text, k->text_len);
			else
				token_to_string_rec(k->token, tb);
			thread_buf_append_char(tb, '=');
		}

		if(v->is_text)
			thread_buf_append(tb, v->text, v->text_len);
		else
			token_to_string_rec(v->token, tb);
		return;
	}

	case TOKEN_HTML: {
		/* HTML tags: opening, attrs, self-closing, or closing */
		/* Use orig_tag (original case) for round-trip toString, like JS this.tag */
			const char *tag_str= t->data.html.orig_tag.start ? t->data.html.orig_tag.start : t->name;
			size_t tag_len= t->data.html.orig_tag.start ? t->data.html.orig_tag.length : t_name_len;
		if(t->data.html.closing) {
			thread_buf_append(tb, "</", 2);
			if(tag_str) thread_buf_append(tb, tag_str, tag_len);
			if(t->child_count > 0) {
				const Child *c= &t->children[0];
				if(c->is_text)
					thread_buf_append(tb, c->text, c->text_len);
				else
					token_to_string_rec(c->token, tb);
			}
			if(t->data.html.self_closing) {
				thread_buf_append(tb, "/>", 2);
			} else {
				thread_buf_append_char(tb, '>');
			}
			return;
		}
		thread_buf_append_char(tb, '<');
		if(tag_str) thread_buf_append(tb, tag_str, tag_len);
		if(t->child_count > 0) {
			const Child *c= &t->children[0];
			if(c->is_text)
				thread_buf_append(tb, c->text, c->text_len);
			else
				token_to_string_rec(c->token, tb);
		}
		if(t->data.html.self_closing) {
			thread_buf_append(tb, "/>", 2);
		} else {
			thread_buf_append_char(tb, '>');
		}
		return;
	}

	case TOKEN_TABLE: {
		/* Table: emit children in order */
		for(size_t i= 0; i < t->child_count; i++) {
			const Child *c= &t->children[i];
			if(c->is_text)
				thread_buf_append(tb, c->text, c->text_len);
			else
				token_to_string_rec(c->token, tb);
		}
		return;
	}

	case TOKEN_TR: {
		/* Table row: syntax + attrs + cells */
		for(size_t i= 0; i < t->child_count; i++) {
			const Child *c= &t->children[i];
			if(c->is_text)
				thread_buf_append(tb, c->text, c->text_len);
			else
				token_to_string_rec(c->token, tb);
		}
		return;
	}

	case TOKEN_TD: {
		/* Table cell: syntax + attrs + innerSyntax + inner. */
		if(t->child_count > 0) {
			const Child *c= &t->children[0];
			if(c->is_text)
				thread_buf_append(tb, c->text, c->text_len);
			else
				token_to_string_rec(c->token, tb);
		}
		if(t->child_count > 1) {
			const Child *ac= &t->children[1];
			if(!ac->is_text && ac->token) {
				token_to_string_rec(ac->token, tb);
			} else if(ac->is_text && ac->text && ac->text_len > 0) {
				thread_buf_append(tb, ac->text, ac->text_len);
			}
		}
		if(t->data.td.inner_syntax.start) {
			thread_buf_append(tb, t->data.td.inner_syntax.start, t->data.td.inner_syntax.length);
		}
		if(t->child_count > 2) {
			const Child *c= &t->children[2];
			if(c->is_text)
				thread_buf_append(tb, c->text, c->text_len);
			else
				token_to_string_rec(c->token, tb);
		}
		return;
	}

	case TOKEN_INCLUDE:
		thread_buf_append_char(tb, '<');
		if(t_name_len > 0) thread_buf_append(tb, t->name, t_name_len);
		if(t->child_count > 0) {
			const Child *c= &t->children[0];
			if(c->is_text)
				thread_buf_append(tb, c->text, c->text_len);
			else
				token_to_string_rec(c->token, tb);
		}
		thread_buf_append_char(tb, '>');
		if(t->child_count > 1) {
			const Child *c= &t->children[1];
			if(c->is_text)
				thread_buf_append(tb, c->text, c->text_len);
			else
				token_to_string_rec(c->token, tb);
		}
		if(t->data.include.closing.start) {
			thread_buf_append(tb, "</", 2);
			thread_buf_append(tb, t->data.include.closing.start, t->data.include.closing.length);
			thread_buf_append_char(tb, '>');
		}
		return;

	case TOKEN_TRANSLATE:
		thread_buf_append(tb, "<translate", 10);
		if(t->child_count > 0) {
			const Child *c= &t->children[0];
			if(c->is_text)
				thread_buf_append(tb, c->text, c->text_len);
			else
				token_to_string_rec(c->token, tb);
		}
		thread_buf_append_char(tb, '>');
		if(t->child_count > 1) {
			const Child *c= &t->children[1];
			if(c->is_text)
				thread_buf_append(tb, c->text, c->text_len);
			else
				token_to_string_rec(c->token, tb);
		}
		thread_buf_append(tb, "</translate>", 12);
		return;

	case TOKEN_REDIRECT: {
		/* Redirect: pre + children.join(sep) + post */
		if(t->data.redirect.pre.start) thread_buf_append(tb, t->data.redirect.pre.start, t->data.redirect.pre.length);
		for(size_t i= 0; i < t->child_count; i++) {
			if(i > 0 && t->sep != '\0') thread_buf_append_char(tb, t->sep);
			const Child *c= &t->children[i];
			if(c->is_text)
				thread_buf_append(tb, c->text, c->text_len);
			else
				token_to_string_rec(c->token, tb);
		}
		if(t->data.redirect.post.start) thread_buf_append(tb, t->data.redirect.post.start, t->data.redirect.post.length);
		return;
	}

	case TOKEN_HEADING: {
		/* Heading: ==<title>==<trail>
             * child[0] = heading-title, child[1] = heading-trail */
		int level= t->data.heading.level;
		for(int k= 0; k < level; k++) thread_buf_append_char(tb, '=');
		if(t->child_count > 0) {
			const Child *c= &t->children[0];
			if(c->is_text)
				thread_buf_append(tb, c->text, c->text_len);
			else
				token_to_string_rec(c->token, tb);
		}
		for(int k= 0; k < level; k++) thread_buf_append_char(tb, '=');
		if(t->child_count > 1) {
			const Child *c= &t->children[1];
			if(c->is_text)
				thread_buf_append(tb, c->text, c->text_len);
			else
				token_to_string_rec(c->token, tb);
		}
		return;
	}

	case TOKEN_HR: {
		/* HR: output the original matched dash sequence (stored as text child). */
		for(size_t i= 0; i < t->child_count; i++) {
			const Child *c= &t->children[i];
			if(c->is_text)
				thread_buf_append(tb, c->text, c->text_len);
			else
				token_to_string_rec(c->token, tb);
		}
		return;
	}

	case TOKEN_DOUBLE_UNDERSCORE: {
		/* __KEYWORD__: wrap the child text in double underscores. */
		if(t->data.dunder.fullwidth) {
			thread_buf_append(tb, "\xEF\xBC\xBF\xEF\xBC\xBF", 6);
		} else {
			thread_buf_append(tb, "__", 2);
		}
		for(size_t i= 0; i < t->child_count; i++) {
			const Child *c= &t->children[i];
			if(c->is_text)
				thread_buf_append(tb, c->text, c->text_len);
			else
				token_to_string_rec(c->token, tb);
		}
		if(t->data.dunder.fullwidth) {
			thread_buf_append(tb, "\xEF\xBC\xBF\xEF\xBC\xBF", 6);
		} else {
			thread_buf_append(tb, "__", 2);
		}
		return;
	}

	case TOKEN_CONVERTER: {
		/* Converter: -{flags|rules}- where child[0]=converter-flags, child[1..]=converter-rule */
		thread_buf_append(tb, "-{", 2);

		if(t->child_count > 0) {
			const Child *fc= &t->children[0];
			if(!fc->is_text && fc->token) {
				const Token *flags= fc->token;
				for(size_t i= 0; i < flags->child_count; i++) {
					if(i > 0) thread_buf_append_char(tb, ';');
					const Child *f= &flags->children[i];
					if(f->is_text)
						thread_buf_append(tb, f->text, f->text_len);
					else
						token_to_string_rec(f->token, tb);
				}
			}
		}

		if(t->child_count > 1) {
			const Child *fc= (t->child_count > 0) ? &t->children[0] : NULL;
			bool has_flags= false;
			if(fc && !fc->is_text && fc->token) {
				has_flags= (fc->token->child_count > 0);
			}
			if(has_flags) thread_buf_append_char(tb, '|');

			for(size_t i= 1; i < t->child_count; i++) {
				if(i > 1) thread_buf_append_char(tb, ';');
				const Child *r= &t->children[i];
				if(r->is_text)
					thread_buf_append(tb, r->text, r->text_len);
				else
					token_to_string_rec(r->token, tb);
			}
		}

		thread_buf_append(tb, "}-", 2);
		return;
	}

	case TOKEN_EXT_INNER: {
		for(size_t i= 0; i < t->child_count; i++) {
			if(i > 0 && t->sep != '\0') {
				thread_buf_append_char(tb, t->sep);
			}
			const Child *c= &t->children[i];
			if(c->is_text) {
				thread_buf_append(tb, c->text, c->text_len);
			} else {
				token_to_string_rec(c->token, tb);
			}
		}
		return;
	}

	default:
		for(size_t i= 0; i < t->child_count; i++) {
			if(i > 0 && t->sep != '\0') {
				thread_buf_append_char(tb, t->sep);
			}
			const Child *c= &t->children[i];
			if(c->is_text) {
				thread_buf_append(tb, c->text, c->text_len);
			} else {
				token_to_string_rec(c->token, tb);
			}
		}
		return;
	}
}

char *token_to_string(const Token *t, ThreadBuf *tb) {
	assert(tb);
	tb->len= 0;
	token_to_string_rec(t, tb);
	/* Ensure the returned C-string is NUL-terminated for callers that use
	 * the result as a NUL-terminated C string (tests use `strcmp`). Public
	 * thread-buffer helpers normally maintain a terminator, but the local
	 * append helpers used above do not — make sure we terminate here. */
	if (tb->len >= tb->cap) wiki_thread_buf_reserve(tb, tb->len + 1);
	tb->buf[tb->len] = '\0';
	return tb->buf;
}

/* ── JSON serialization ──────────────────────────────────────────────────── */

static void json_string(ThreadBuf *tb, const char *s) {
	thread_buf_append_char(tb, '"');
	if(!s) {
		thread_buf_append_char(tb, '"');
		return;
	}
	for(; *s; s++) {
		unsigned char c= (unsigned char)*s;
		if(c == '"')
			thread_buf_append(tb, "\\\"", 2);
		else if(c == '\\')
			thread_buf_append(tb, "\\\\", 2);
		else if(c == '\n')
			thread_buf_append(tb, "\\n", 2);
		else if(c == '\r')
			thread_buf_append(tb, "\\r", 2);
		else if(c == '\t')
			thread_buf_append(tb, "\\t", 2);
		else if(c < 0x20) {
			char buf[8];
			int len = snprintf(buf, sizeof(buf), "\\u%04x", c);
			thread_buf_append(tb, buf, len);
		}
		else
			thread_buf_append_char(tb, c);
	}
	thread_buf_append_char(tb, '"');
}

/* Write a JSON-escaped string of given length to tb (surrounded by quotes). */
static void json_write_escaped_len(ThreadBuf *tb, const char *s, size_t len) {
	thread_buf_append_char(tb, '"');
	if(!s || len == 0) {
		thread_buf_append_char(tb, '"');
		return;
	}
	const char *p = s;
	const char *end = s + len;
	const char *chunk = p;
	while(p < end) {
		unsigned char ch = (unsigned char)*p;
		if(ch == '"' || ch == '\\' || ch < 0x20) {
			if(chunk < p) thread_buf_append(tb, chunk, p - chunk);
			switch(ch) {
			case '"': thread_buf_append(tb, "\\\"", 2); break;
			case '\\': thread_buf_append(tb, "\\\\", 2); break;
			case '\b': thread_buf_append(tb, "\\b", 2); break;
			case '\f': thread_buf_append(tb, "\\f", 2); break;
			case '\n': thread_buf_append(tb, "\\n", 2); break;
			case '\r': thread_buf_append(tb, "\\r", 2); break;
			case '\t': thread_buf_append(tb, "\\t", 2); break;
			default: {
				char buf[8];
				int len_esc = snprintf(buf, sizeof(buf), "\\u%04x", ch);
				thread_buf_append(tb, buf, len_esc);
				break;
			}
			}
			p++;
			chunk = p;
		} else {
			p++;
		}
	}
	if(chunk < end) thread_buf_append(tb, chunk, end - chunk);
	thread_buf_append_char(tb, '"');
}

void json_stringify_wikiparser_node(const Token *t, ThreadBuf *tb) {
	if(!t) {
		thread_buf_append(tb, "null", 4);
		return;
	}

	if(t->type == TOKEN_TEXT) {
		/* Text node: {"data":"..."} */
		thread_buf_append(tb, "{\"data\":", 8);
		assert(t->child_count == 1 && t->children[0].is_text);
		json_write_escaped_len(tb, t->children[0].text, t->children[0].text_len);
		thread_buf_append_char(tb, '}');
		return;
	}

	thread_buf_append_char(tb, '{');

	if(t->child_count > 0) {
		thread_buf_append(tb, "\"childNodes\":[", 14);
		for(size_t i= 0; i < t->child_count; i++) {
			if(i > 0) thread_buf_append_char(tb, ',');
			const Child *c= &t->children[i];
			if(c->is_text) {
				thread_buf_append(tb, "{\"data\":", 8);
				json_write_escaped_len(tb, c->text, c->text_len);
				thread_buf_append_char(tb, '}');
			} else {
				json_stringify_wikiparser_node(c->token, tb);
			}
		}
		thread_buf_append_char(tb, ']');
	}
	if(t->name && t->name[0] != '\0') {
		thread_buf_append(tb, ",\"name\":", 8);
		json_string(tb, t->name);
	}

	thread_buf_append_char(tb, '}');
}

char token_sentinel_char(TokenType type) {
	switch(type) {
	case TOKEN_COMMENT: return 'c';
	case TOKEN_EXT: return 'e';
	case TOKEN_NOINCLUDE: return 'n';
	case TOKEN_INCLUDE: return 'n';
	case TOKEN_TRANSLATE: return 'g';
	case TOKEN_ONLYINCLUDE: return 'g';
	case TOKEN_ARG: return 'a';
	case TOKEN_TRANSCLUDE: return 't';
	case TOKEN_HEADING: return 'h';
	case TOKEN_HTML: return 'x';
	case TOKEN_TABLE: return 'b';
	case TOKEN_TR: return 'b';
	case TOKEN_TD: return 'b';
	case TOKEN_HR: return 'r';
	case TOKEN_DOUBLE_UNDERSCORE: return 'n';
	case TOKEN_LINK: return 'l';
	case TOKEN_FILE: return 'l';
	case TOKEN_CATEGORY: return 'l';
	case TOKEN_REDIRECT: return 'o';
	case TOKEN_QUOTE: return 'q';
	case TOKEN_EXT_LINK: return 'w';
	case TOKEN_MAGIC_LINK: return 'i';
	case TOKEN_LIST: return 'd';
	case TOKEN_DD: return 'd';
	case TOKEN_CONVERTER: return 'v';
	case TOKEN_ATTRIBUTES: return 'a';
	default: return '?';
	}
}
