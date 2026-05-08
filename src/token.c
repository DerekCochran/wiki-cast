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

Token *token_new(TokenType type, const char *type_name) {
	Token *t= calloc(1, sizeof(Token));
	if(!t) return NULL;
	t->type= type;
	t->type_name= type_name ? strdup(type_name) : NULL;
	t->child_cap= CHILD_INIT_CAP;
	t->children= malloc(CHILD_INIT_CAP * sizeof(Child));
	if(!t->children) {
		free(t->type_name);
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
	Child *c= &t->children[t->child_count++];
	c->is_text= true;
	c->text_len= len;
	c->text= text;
	c->text_owned = false;
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

// TODO:  Should these be freed or can they use the thread buffer?
static void free_token_data(Token *t) {
	switch(t->type) {
	case TOKEN_EXT_ATTR:
		free(t->data.ext_attr.equal);
		break;
	case TOKEN_PLAIN:
		if(t->type_name && strcmp(t->type_name, "image-parameter") == 0) {
			free(t->data.image_param.raw_syntax);
		}
		break;
	case TOKEN_TD:
		free(t->data.td.inner_syntax);
		break;
	case TOKEN_HTML:
		free(t->data.html.orig_tag);
		break;
	case TOKEN_REDIRECT:
		free(t->data.redirect.pre);
		free(t->data.redirect.post);
		free(t->data.redirect.link);
		free(t->data.redirect.display);
		break;
	case TOKEN_EXT_LINK:
		free(t->data.ext_link.space);
		break;
	case TOKEN_EXT:
		free(t->data.ext.name);
		free(t->data.ext.attr);
		free(t->data.ext.inner);
		free(t->data.ext.closing);
		break;
	case TOKEN_TRANSCLUDE:
		free(t->data.transclude.modifier);
		break;
	case TOKEN_INCLUDE:
	case TOKEN_NOINCLUDE:
		free(t->data.include.tag);
		free(t->data.include.attr);
		free(t->data.include.inner);
		free(t->data.include.closing);
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
	free(node->type_name);
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
	free(t->type_name);
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

void token_log_json(const Token *t) {
	if(!t) return;

	char *json= NULL;
	size_t json_len= 0;
	FILE *fp= open_memstream(&json, &json_len);
	if(!fp) return;

	token_to_json(t, fp);
	fclose(fp);

	if(json) {
		log_debug("token_to_json=%s", json);
		free(json);
	}
}

static void token_to_string_rec(const Token *t, ThreadBuf *tb) {
	if(!t || !tb) return;

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
		const char *ext_tag= t->data.ext.name ? t->data.ext.name : t->name;
		thread_buf_append_char(tb, '<');
		if(ext_tag) thread_buf_append(tb, ext_tag, strlen(ext_tag));
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
		if(ext_tag) thread_buf_append(tb, ext_tag, strlen(ext_tag));
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
		if(t->data.ext_attr.equal) {
			thread_buf_append(tb, t->data.ext_attr.equal, strlen(t->data.ext_attr.equal));
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
		if(t->type_name && strcmp(t->type_name, "converter-rule") == 0) {
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
		bool is_file_line_image= (t->type == TOKEN_FILE && t->type_name && (strcmp(t->type_name, "gallery-image") == 0 || strcmp(t->type_name, "imagemap-image") == 0));

		/* Internal links: [[target|text]] — join children with '|' and wrap.
             * Gallery images serialize as plain lines without [[...]]. */
		if(!is_file_line_image) {
			thread_buf_append(tb, "[[", 2);
		}
		for(size_t i= 0; i < t->child_count; i++) {
			if(i > 0) {
				/* Special-case: leading ':' text child should not be
                     * separated from the following target by a '|'. */
				if(!(i == 1 && t->children[0].is_text && t->children[0].text && t->children[0].text[0] == ':')) {
					thread_buf_append_char(tb, '|');
				}
			}
			const Child *c= &t->children[i];
			if(c->is_text) {
				thread_buf_append(tb, c->text, c->text_len);
			} else if(t->type == TOKEN_FILE && c->token && c->token->type == TOKEN_PLAIN && c->token->type_name && strcmp(c->token->type_name, "image-parameter") == 0) {
				const Token *p= c->token;
				if(p->data.image_param.raw_syntax) {
					const char *syntax= p->data.image_param.raw_syntax;
					const char *slot= strstr(syntax, "$1");
					if(!slot) {
						thread_buf_append(tb, syntax, strlen(syntax));
					} else {
						size_t pre_len= (size_t)(slot - syntax);
						size_t post_len= strlen(slot + 2);
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
				} else if(p->name) {
					thread_buf_append(tb, p->name, strlen(p->name));
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
			const Child *c= &t->children[0];
			if(c->is_text)
				thread_buf_append(tb, c->text, c->text_len);
			else
				token_to_string_rec(c->token, tb);

			if(t->child_count == 1) {
				if(t->data.ext_link.space) {
					thread_buf_append(tb, t->data.ext_link.space, strlen(t->data.ext_link.space));
				}
			} else {
				if(t->data.ext_link.space) {
					thread_buf_append(tb, t->data.ext_link.space, strlen(t->data.ext_link.space));
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
		if(t->data.transclude.modifier) {
			thread_buf_append(tb, t->data.transclude.modifier, strlen(t->data.transclude.modifier));
		}
		bool is_magic_word= (t->type_name && strcmp(t->type_name, "magic-word") == 0);
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
		const char *tag_str= t->data.html.orig_tag ? t->data.html.orig_tag : t->name;
		if(t->data.html.closing) {
			thread_buf_append(tb, "</", 2);
			if(tag_str) thread_buf_append(tb, tag_str, strlen(tag_str));
			thread_buf_append_char(tb, '>');
			return;
		}
		thread_buf_append_char(tb, '<');
		if(tag_str) thread_buf_append(tb, tag_str, strlen(tag_str));
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
		if(t->data.td.inner_syntax) {
			thread_buf_append(tb, t->data.td.inner_syntax, strlen(t->data.td.inner_syntax));
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
		if(t->name) thread_buf_append(tb, t->name, strlen(t->name));
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
		if(t->data.include.closing) {
			thread_buf_append(tb, "</", 2);
			thread_buf_append(tb, t->data.include.closing, strlen(t->data.include.closing));
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
		if(t->data.redirect.pre) thread_buf_append(tb, t->data.redirect.pre, strlen(t->data.redirect.pre));
		for(size_t i= 0; i < t->child_count; i++) {
			if(i > 0 && t->sep != '\0') thread_buf_append_char(tb, t->sep);
			const Child *c= &t->children[i];
			if(c->is_text)
				thread_buf_append(tb, c->text, c->text_len);
			else
				token_to_string_rec(c->token, tb);
		}
		if(t->data.redirect.post) thread_buf_append(tb, t->data.redirect.post, strlen(t->data.redirect.post));
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
	wiki_thread_buf_assert_no_leased_scratch("token_to_string(entry)", tb);
	tb->len= 0;
	token_to_string_rec(t, tb);
	wiki_thread_buf_reserve(tb, tb->len);
	tb->buf[tb->len]= '\0';
	wiki_thread_buf_assert_no_leased_scratch("token_to_string(exit)", tb);
	return tb->buf;
}

/* ── JSON serialization ──────────────────────────────────────────────────── */

static void json_string(const char *s, FILE *fp) {
	fputc('"', fp);
	if(!s) {
		fputc('"', fp);
		return;
	}
	for(; *s; s++) {
		unsigned char c= (unsigned char)*s;
		if(c == '"')
			fputs("\\\"", fp);
		else if(c == '\\')
			fputs("\\\\", fp);
		else if(c == '\n')
			fputs("\\n", fp);
		else if(c == '\r')
			fputs("\\r", fp);
		else if(c == '\t')
			fputs("\\t", fp);
		else if(c < 0x20)
			fprintf(fp, "\\u%04x", c);
		else
			fputc(c, fp);
	}
	fputc('"', fp);
}

/* Write a JSON-escaped string of given length to fp (surrounded by quotes). */
static void json_write_escaped_len(const char *s, size_t len, FILE *fp) {
	fputc('"', fp);
	if(!s || len == 0) {
		fputc('"', fp);
		return;
	}
	const char *p = s;
	const char *end = s + len;
	const char *chunk = p;
	while(p < end) {
		unsigned char ch = (unsigned char)*p;
		if(ch == '"' || ch == '\\' || ch < 0x20) {
			if(chunk < p) fwrite(chunk, 1, p - chunk, fp);
			switch(ch) {
			case '"': fputs("\\\"", fp); break;
			case '\\': fputs("\\\\", fp); break;
			case '\b': fputs("\\b", fp); break;
			case '\f': fputs("\\f", fp); break;
			case '\n': fputs("\\n", fp); break;
			case '\r': fputs("\\r", fp); break;
			case '\t': fputs("\\t", fp); break;
			default: fprintf(fp, "\\u%04x", ch); break;
			}
			p++;
			chunk = p;
		} else {
			p++;
		}
	}
	if(chunk < end) fwrite(chunk, 1, end - chunk, fp);
	fputc('"', fp);
}

void token_to_json(const Token *t, FILE *fp) {
	if(!t) {
		fputs("null", fp);
		return;
	}

	if(t->type == TOKEN_TEXT) {
		/* Text node: {"type":"text","data":"..."} */
		fprintf(fp, "{\"type\":\"text\",\"data\":");
		assert(t->child_count == 1 && t->children[0].is_text);
		json_write_escaped_len(t->children[0].text, t->children[0].text_len, fp);
		fputc('}', fp);
		return;
	}

	fputs("{\"type\":", fp);
	json_string(t->type_name, fp);

	if(t->name) {
		fputs(",\"name\":", fp);
		json_string(t->name, fp);
	}

	if(t->child_count > 0) {
		fputs(",\"childNodes\":[", fp);
		for(size_t i= 0; i < t->child_count; i++) {
			if(i > 0) fputc(',', fp);
			const Child *c= &t->children[i];
			if(c->is_text) {
				fprintf(fp, "{\"type\":\"text\",\"data\":");
				json_write_escaped_len(c->text, c->text_len, fp);
				fputc('}', fp);
			} else {
				token_to_json(c->token, fp);
			}
		}
		fputs("]", fp);
	}
	fputc('}', fp);
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
