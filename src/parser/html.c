#include "accum.h"
#include "util/log.h"
#include "parser/html.h"
#include "util/string_util.h"
#include <stringzilla/stringzilla.h>
#include "token.h"
#include "util/thread_buffer.h"
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Html tag parse result (allocation-free views into the provided buffer) */
typedef struct {
	bool        is_closing;        /* group 1: leading '/' present    */
	const char *tag_name;          /* group 2: [a-z][^\s/>]*          */
	size_t      tag_name_len;
	const char *attrs;             /* group 3: may be NULL            */
	size_t      attrs_len;
	bool        is_self_closing;   /* group 4: ends with '/>'         */
	const char *trailing_text;     /* group 5: [^<]* after '>'       */
	size_t      trailing_text_len;
} HtmlTagComponents;

static inline bool is_alpha_ci_char(unsigned char c) {
	return ((c | 0x20) - 'a') <= ('z' - 'a');
}

/**
 * Forward-scan implementation of the HTML tag pattern described in
 * proposals/callback_parser_regexes.md. `buf` points to the first byte
 * after the '<'. `len` covers everything up to and including trailing
 * text. On success, fills `out` with views into `buf` (no allocations)
 * and returns true. Returns false on mismatch.
 */
bool html_tag_parse(const char        *buf,
					size_t             len,
					HtmlTagComponents *out) {
	if(!buf || !out) return false;

	/* init */
	out->is_closing = false;
	out->tag_name = NULL; out->tag_name_len = 0;
	out->attrs = NULL; out->attrs_len = 0;
	out->is_self_closing = false;
	out->trailing_text = NULL; out->trailing_text_len = 0;

	size_t pos = 0;
	/* 1. optional leading '/' */
	if(pos < len && buf[pos] == '/') { out->is_closing = true; pos++; }

	/* 2. tag name: first byte must be ASCII letter */
	if(pos >= len) return false;
	unsigned char c0 = (unsigned char)buf[pos];
	if(!is_alpha_ci_char(c0)) return false;
	size_t name_start = pos;
	pos++;
	while(pos < len) {
		unsigned char cc = (unsigned char)buf[pos];
		if(cc == '>' || cc == '/' || cc == ' ' || cc == '\t' || cc == '\r' || cc == '\n' || cc == '\v' || cc == '\f') break;
		pos++;
	}
	size_t name_len = pos - name_start;
	if(name_len == 0) return false;
	out->tag_name = buf + name_start;
	out->tag_name_len = name_len;

	/* 3. attrs: start if whitespace OR '/' not followed by '>' */
	size_t attrs_start = pos;
	bool has_attrs = false;
	if(pos < len) {
		unsigned char cc = (unsigned char)buf[pos];
		bool start_attrs = false;
		if(cc == ' ' || cc == '\t' || cc == '\r' || cc == '\n' || cc == '\v' || cc == '\f') start_attrs = true;
		else if(cc == '/') {
			if(pos + 1 < len && buf[pos + 1] != '>') start_attrs = true;
		}
		if(start_attrs) {
			has_attrs = true;
			size_t p = pos;
			while(p < len) {
				if(buf[p] == '>') break;
				if(buf[p] == '/' && p + 1 < len && buf[p + 1] == '>') break;
				p++;
			}
			pos = p;
		}
	}
	if(has_attrs) {
		out->attrs = buf + attrs_start;
		out->attrs_len = pos - attrs_start;
	} else {
		out->attrs = NULL;
		out->attrs_len = 0;
	}

	/* 4. closing: '/>' or '>' */
	if(pos >= len) return false;
	if(buf[pos] == '/' && pos + 1 < len && buf[pos + 1] == '>') {
		out->is_self_closing = true;
		pos += 2;
	} else if(buf[pos] == '>') {
		out->is_self_closing = false;
		pos += 1;
	} else {
		return false;
	}

	/* 5. trailing text must not contain '<' */
	if(pos > len) return false;
	out->trailing_text = buf + pos;
	out->trailing_text_len = len - pos;
	if(out->trailing_text_len > 0) {
		const char needle = '<';
		const char *found = sz_find_byte(out->trailing_text, out->trailing_text_len, &needle);
		if(found) return false;
	}

	return true;
}


/* Helper: whether a tag name (lowercase) is in any of cfg->html lists */
static bool html_tag_allowed(const ParserConfig *cfg, const char *lcname) {
	if(!cfg || !lcname) return false;
	for(int grp= 0; grp < 3; grp++) {
		for(size_t i= 0; i < cfg->html[grp].count; i++) {
			if(strcasecmp(cfg->html[grp].items[i], lcname) == 0) return true;
		}
	}
	return false;
}

static Token *make_html_attr_key(const char *key, size_t key_len, Accum *accum) {
	Token *t= token_new(TOKEN_ATTR_KEY, "attr-key");
	if(!t) return NULL;
	/* Ensure key text is stored in the persistent tokens arena */
	const char *key_view = wiki_thread_buf_append_to_tokens(key, key_len);
	token_append_text_n(t, key_view, key_len);
	accum_push(accum, t);
	return t;
}

static Token *make_html_attr_value(const char *val, size_t val_len, Accum *accum) {
	Token *t= token_new(TOKEN_ATTR_VALUE, "attr-value");
	if(!t) return NULL;
	/* Ensure value text is stored in the persistent tokens arena */
	const char *val_view = wiki_thread_buf_append_to_tokens(val, val_len);
	token_append_text_n(t, val_view, val_len);
	accum_push(accum, t);
	return t;
}

static Token *make_html_attr_dirty(const char *text, size_t text_len, Accum *accum) {
	Token *t= token_new(TOKEN_EXT_ATTR_DIRTY, "html-attr-dirty");
	if(!t) return NULL;
	/* Dirty buffer is stack-local; copy into tokens arena to obtain stable view */
	const char *text_view = wiki_thread_buf_append_to_tokens(text, text_len);
	token_append_text_n(t, text_view, text_len);
	accum_push(accum, t);
	return t;
}

static Token *make_html_attr(const char *key, size_t key_len,
														 const char *val, size_t val_len,
														 const char *equal, size_t equal_len,
														 char quote_open, char quote_close,
														 Accum *accum) {
	Token *t= token_new(TOKEN_EXT_ATTR, "html-attr");
	if(!t) return NULL;

	t->name= str_trim_lc(key, key_len);
	if(equal && equal_len > 0) {
		t->data.ext_attr.equal= malloc(equal_len + 1);
		assert(t->data.ext_attr.equal);
		memcpy(t->data.ext_attr.equal, equal, equal_len);
		t->data.ext_attr.equal[equal_len]= '\0';
	}
	t->data.ext_attr.quote_open= quote_open;
	t->data.ext_attr.quote_close= quote_close;

	Token *attr_key= make_html_attr_key(key, key_len, accum);
	if(!attr_key) {
		token_free(t);
		return NULL;
	}
	token_append_child(t, attr_key);

	if(val) {
		Token *attr_value= make_html_attr_value(val, val_len, accum);
		if(!attr_value) {
			token_free(t);
			return NULL;
		}
		token_append_child(t, attr_value);
	}

	accum_push(accum, t);
	return t;
}

static void parse_html_attrs(Token *attrs_tok, const char *attr_str, size_t attr_len, Accum *accum) {
	if(!attr_str || attr_len == 0) return;

	size_t i= 0;
	char dirty_buf[4096];
	size_t dirty_len= 0;

#define FLUSH_HTML_DIRTY()                                          \
	do {                                                              \
		if(dirty_len > 0) {                                             \
			Token *dt= make_html_attr_dirty(dirty_buf, dirty_len, accum); \
			if(dt) token_append_child(attrs_tok, dt);                     \
			dirty_len= 0;                                                 \
		}                                                               \
	} while(0)

	while(i < attr_len) {
		if(isspace((unsigned char)attr_str[i])) {
			while(i < attr_len && isspace((unsigned char)attr_str[i])) {
				dirty_buf[dirty_len++]= attr_str[i++];
			}
			continue;
		}

		if(attr_str[i] == '/') {
			dirty_buf[dirty_len++]= attr_str[i++];
			continue;
		}

		size_t key_start= i;
		while(i < attr_len && !isspace((unsigned char)attr_str[i]) &&
					attr_str[i] != '=' && attr_str[i] != '/') {
			i++;
		}
		size_t key_len= i - key_start;
		if(key_len == 0) {
			dirty_buf[dirty_len++]= attr_str[i++];
			continue;
		}

		const char *key= attr_str + key_start;
		bool valid_key= isalpha((unsigned char)key[0]) || key[0] == '_' || key[0] == ':';
		if(valid_key) {
			for(size_t k= 1; k < key_len; k++) {
				unsigned char kc= (unsigned char)key[k];
				if(!isalnum(kc) && kc != ':' && kc != '.' && kc != '_' && kc != '-') {
					valid_key= false;
					break;
				}
			}
		}

		if(!valid_key) {
			for(size_t k= 0; k < key_len; k++) dirty_buf[dirty_len++]= key[k];
			continue;
		}

		size_t eq_start= i;
		while(i < attr_len && isspace((unsigned char)attr_str[i])) i++;

		if(i >= attr_len || attr_str[i] != '=') {
			FLUSH_HTML_DIRTY();
			Token *at= make_html_attr(key, key_len, NULL, 0, "", 0, '\0', '\0', accum);
			if(at) token_append_child(attrs_tok, at);
			i= eq_start;
			continue;
		}

		i++;
		while(i < attr_len && isspace((unsigned char)attr_str[i])) i++;

		const char *equal_start= attr_str + eq_start;
		size_t equal_len= i - eq_start;
		const char *val= NULL;
		size_t val_len= 0;
		char quote_open= '\0';
		char quote_close= '\0';

		if(i < attr_len && (attr_str[i] == '"' || attr_str[i] == '\'')) {
			quote_open= attr_str[i++];
			size_t val_start= i;
			while(i < attr_len && attr_str[i] != quote_open) i++;
			val= attr_str + val_start;
			val_len= i - val_start;
			if(i < attr_len) {
				quote_close= quote_open;
				i++;
			}
		} else {
			size_t val_start= i;
			while(i < attr_len && !isspace((unsigned char)attr_str[i])) i++;
			val= attr_str + val_start;
			val_len= i - val_start;
		}

		FLUSH_HTML_DIRTY();
		Token *at= make_html_attr(key, key_len, val, val_len,
															equal_start, equal_len,
															quote_open, quote_close,
															accum);
		if(at) token_append_child(attrs_tok, at);
	}

	FLUSH_HTML_DIRTY();
#undef FLUSH_HTML_DIRTY
}

static Token *build_html_attrs(const char *tag_name, const char *attr_str, size_t attr_len, Accum *accum) {
	Token *t= token_new(TOKEN_ATTRIBUTES, "html-attrs");
	if(!t) return NULL;
	t->name= strdup(tag_name);
	accum_push(accum, t);

	if(attr_str && attr_len > 0 && !isspace((unsigned char)attr_str[0])) {
		ThreadBuf *tmp = wiki_thread_buf_acquire_scratch();
		if(!tmp) {
			log_fatal("build_html_attrs: failed to acquire scratch");
			abort();
		}
		wiki_thread_buf_reserve(tmp, attr_len + 2);
		tmp->buf[0]= ' ';
		memcpy(tmp->buf + 1, attr_str, attr_len);
		tmp->len = attr_len + 1;
		parse_html_attrs(t, tmp->buf, tmp->len, accum);
		wiki_thread_buf_release_scratch(tmp);
	} else {
		parse_html_attrs(t, attr_str, attr_len, accum);
	}

	return t;
}

static bool html_attrs_has_attr(const Token *attrs, const char *attr_name) {
	if(!attrs || !attr_name) return false;
	for(size_t i= 0; i < attrs->child_count; i++) {
		const Child *c= &attrs->children[i];
		if(c->is_text || !c->token) continue;
		const Token *a= c->token;
		if(a->type == TOKEN_EXT_ATTR && a->name && strcasecmp(a->name, attr_name) == 0) {
			return true;
		}
	}
	return false;
}

static void accum_rollback_shallow(Accum *accum, size_t saved_count) {
	if(!accum) return;
	while(accum->count > saved_count) {
		Token *t= accum->tokens[--accum->count];
		if(t) token_free_shallow(t);
	}
}

void parse_html(ThreadBuf *tb, const ParserConfig *cfg, Accum *accum) {
	if(!tb || !tb->buf) return;

	/* HTML tag matching implemented via forward scan (html_tag_parse) */

	ThreadBuf *out_tb = wiki_thread_buf_acquire_scratch();
	if(!out_tb) {
		log_fatal("parse_html: failed to acquire scratch");
		abort();
	}
	wiki_thread_buf_reserve(out_tb, tb->len * 2 + 64);
	out_tb->len = 0;

	const char *buf= tb->buf;
	size_t len= tb->len;
	size_t pos= 0;

#define ENSURE_OUT_CAP(need) wiki_thread_buf_reserve(out_tb, out_tb->len + (need))

	while(pos < len) {
		/* find next '<' */
		const char needle_lt = '<';
		const char *lt= sz_find_byte(buf + pos, len - pos, &needle_lt);
		if(!lt) {
			size_t rest= len - pos;
			ENSURE_OUT_CAP(rest + 1);
			memcpy(out_tb->buf + out_tb->len, buf + pos, rest);
			out_tb->len+= rest;
			break;
		}

		/* copy text before '<' */
		size_t before= (size_t)(lt - (buf + pos));
		ENSURE_OUT_CAP(before + 1);
		memcpy(out_tb->buf + out_tb->len, buf + pos, before);
		out_tb->len+= before;

		/* define segment between this '<' and the next '<' (or end) */
		const char *seg_start= lt + 1;
		size_t seg_rem= len - (size_t)(seg_start - buf);
		const char needle_lt2 = '<';
		const char *next_lt= sz_find_byte(seg_start, seg_rem, &needle_lt2);
		size_t seg_len= next_lt ? (size_t)(next_lt - seg_start) : seg_rem;

		/* Try to match the HTML tag pattern against the segment using fast scanner */
		HtmlTagComponents htc;
		if(!html_tag_parse(seg_start, seg_len, &htc)) {
			/* No match — emit literally: '<' + segment */
			ENSURE_OUT_CAP(1 + seg_len + 1);
			out_tb->buf[out_tb->len++]= '<';
			if(seg_len > 0) {
				memcpy(out_tb->buf + out_tb->len, seg_start, seg_len);
				out_tb->len+= seg_len;
			}
		} else {
			/* Have a candidate tag name */
			if(!htc.tag_name || htc.tag_name_len == 0) {
				ENSURE_OUT_CAP(1 + seg_len + 1);
				out_tb->buf[out_tb->len++]= '<';
				if(seg_len > 0) {
					memcpy(out_tb->buf + out_tb->len, seg_start, seg_len);
					out_tb->len+= seg_len;
				}
			} else {
				char *lcname = str_trim_lc(htc.tag_name, htc.tag_name_len);
				if(!lcname) {
					ENSURE_OUT_CAP(1 + seg_len + 1);
					out_tb->buf[out_tb->len++]= '<';
					if(seg_len > 0) {
						memcpy(out_tb->buf + out_tb->len, seg_start, seg_len);
						out_tb->len+= seg_len;
					}
				} else {
					if(!html_tag_allowed(cfg, lcname)) {
						/* unknown tag — emit raw */
						ENSURE_OUT_CAP(1 + seg_len + 1);
						out_tb->buf[out_tb->len++]= '<';
						if(seg_len > 0) {
							memcpy(out_tb->buf + out_tb->len, seg_start, seg_len);
							out_tb->len+= seg_len;
						}
						free(lcname);
					} else {
						/* Allowed tag — build attrs token then html token and emit sentinel */
						size_t saved_accum = accum->count;

						const char *attr_ptr = htc.attrs;
						size_t attr_len = htc.attrs_len;

						Token *attrs = build_html_attrs(lcname, attr_ptr, attr_len, accum);

						/* Special-case: meta/link require itemprop+content/href. */
						bool reject = false;
						if(strcmp(lcname, "meta") == 0 || strcmp(lcname, "link") == 0) {
							bool has_itemprop = html_attrs_has_attr(attrs, "itemprop");
							bool has_required = strcmp(lcname, "meta") == 0
												? html_attrs_has_attr(attrs, "content")
												: html_attrs_has_attr(attrs, "href");
							reject = !(has_itemprop && has_required);
						}

						if(reject) {
							accum_rollback_shallow(accum, saved_accum);
							ENSURE_OUT_CAP(1 + seg_len + 1);
							out_tb->buf[out_tb->len++]= '<';
							if(seg_len > 0) {
								memcpy(out_tb->buf + out_tb->len, seg_start, seg_len);
								out_tb->len+= seg_len;
							}
							free(lcname);
						} else {
							const char *rest_ptr = htc.trailing_text;
							size_t rest_len = htc.trailing_text_len;

							size_t html_idx = accum->count; /* HtmlToken will be at this index */
							char sent_buf[64];
							size_t sent_len = 0;
							work_str_sentinel(html_idx, 'x', sent_buf, &sent_len);

							ENSURE_OUT_CAP(sent_len + rest_len + 1);
							memcpy(out_tb->buf + out_tb->len, sent_buf, sent_len);
							out_tb->len+= sent_len;
							if(rest_len > 0) {
								memcpy(out_tb->buf + out_tb->len, rest_ptr, rest_len);
								out_tb->len+= rest_len;
							}

							/* Now create HtmlToken and push it (matching JS order) */
							Token *ht = token_new(TOKEN_HTML, "html");
							if(ht) {
								ht->name = strdup(lcname);
								/* orig_tag: original-case name for toString round-trip */
								char *orig_tag = malloc(htc.tag_name_len + 1);
								if(orig_tag) {
									memcpy(orig_tag, htc.tag_name, htc.tag_name_len);
									orig_tag[htc.tag_name_len] = '\0';
								}
								ht->data.html.orig_tag = orig_tag;
								/* closing flag: leading slash present? */
								ht->data.html.closing = htc.is_closing;
								/* self-closing flag */
								ht->data.html.self_closing = htc.is_self_closing;

								if(attrs) token_append_child(ht, attrs);
								accum_push(accum, ht);
							}
							free(lcname);
						}
					}
				}
			}
		}

		/* Advance pos to after this segment (i.e. to the next '<' or end) */
		pos= (size_t)(seg_start - buf) + seg_len;
	}

	ENSURE_OUT_CAP(1);
	out_tb->buf[out_tb->len]= '\0';
	wiki_thread_buf_set(tb, out_tb->buf, out_tb->len);
	wiki_thread_buf_release_scratch(out_tb);

	(void)0;
}
