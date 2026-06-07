#include "accum.h"
#include "build.h"
#include "util/log.h"
#include "parser/html.h"
#include "util/string_util.h"
#include <stringzilla/stringzilla.h>
#include "wiki_cast/token.h"
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
	unsigned char lc = (unsigned char)(c | 0x20);
	return lc >= 'a' && lc <= 'z';
}

static const char HTML_WS_BYTES[] = " \t\r\n\v\f";

static inline bool html_has_any_sentinel(const char *s, size_t len) {
	if(!s || len == 0) return false;
	return sz_find_byte(s, len, "\0") != NULL;
}

static bool html_find_ci_lit(const char *s, size_t len, const char *lit, size_t lit_len) {
	if(!s || !lit || lit_len == 0 || len < lit_len) return false;
	const unsigned char first = (unsigned char)lit[0];
	char cand[2];
	cand[0] = (char)fast_tolower(first);
	cand[1] = (char)((first >= 'a' && first <= 'z') ? (first - ('a' - 'A')) : first);

	for(size_t i = 0; i + lit_len <= len;) {
		const char *found = sz_find_byte_from(s + i, len - i, cand, 2);
		if(!found) return false;
		size_t p = (size_t)(found - s);
		if(str_ci_eq_n(s + p, lit, lit_len)) return true;
		i = p + 1;
	}
	return false;
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
	const char name_stop[] = ">/ \t\r\n\v\f";
	const char *name_end = sz_find_byte_from(buf + pos, len - pos, name_stop, sizeof(name_stop) - 1);
	pos = name_end ? (size_t)(name_end - buf) : len;
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
			const char attr_stop[] = ">/";
			while(p < len) {
				const char *cand = sz_find_byte_from(buf + p, len - p, attr_stop, 2);
				if(!cand) {
					p = len;
					break;
				}
				p = (size_t)(cand - buf);
				if(buf[p] == '>') break;
				if(p + 1 < len && buf[p] == '/' && buf[p + 1] == '>') break;
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
static bool html_tag_allowed(const ParserConfig *cfg, const char *lcname, size_t lcname_len) {
	if(!cfg || !lcname || lcname_len == 0) return false;
	for(int grp= 0; grp < 3; grp++) {
		for(size_t i= 0; i < cfg->html[grp].count; i++) {
			sz_ptr_t html_name;
			sz_size_t html_len;
			sz_string_range(&cfg->html[grp].items[i], &html_name, &html_len);
			if(html_name && html_len == lcname_len && str_ci_eq_n(html_name, lcname, html_len)) return true;
		}
	}
	return false;
}

static Token *make_html_attr_key(const char *key, size_t key_len, Accum *accum) {
	Token *t= token_new(TOKEN_ATTR_KEY, "attr-key");
	if(!t) return NULL;
	token_append_text_owned(t, key, key_len);
	accum_push(accum, t);
	return t;
}

static char *html_normalize_equal(const char *equal, size_t equal_len, Accum *accum, size_t *out_len) {
	if(!equal || equal_len == 0) return NULL;
	if(out_len) *out_len = 0;

	if(sz_find_byte(equal, equal_len, "\0") == NULL) {
		char *out= malloc(equal_len + 1);
		if(!out) return NULL;
		sz_copy(out, equal, equal_len);
		out[equal_len]= '\0';
		if(out_len) *out_len = equal_len;
		return out;
	}

	Token *tmp= token_new(TOKEN_PLAIN, "attr-equal-tmp");
	if(!tmp) return NULL;
	build_from_str(tmp, equal, equal_len, accum);

	ThreadBuf *scratch= wiki_thread_buf_acquire_scratch();
	if(!scratch) {
		token_free_shallow(tmp);
		return NULL;
	}

	const char *s= token_to_string(tmp, scratch);
	size_t slen= scratch->len;
	char *out= malloc(slen + 1);
	if(out) {
		if(slen > 0 && s) sz_copy(out, s, slen);
		out[slen]= '\0';
		if(out_len) *out_len = slen;
	}

	wiki_thread_buf_release_scratch(scratch);
	token_free_shallow(tmp);
	return out;
}

static Token *make_html_attr_value(const char *val, size_t val_len, Accum *accum) {
	Token *t= token_new(TOKEN_ATTR_VALUE, "attr-value");
	if(!t) return NULL;
	if(val_len > 0) {
		token_append_text_owned(t, val, val_len);
	} else {
		/* Explicit empty value (`=` present) keeps an empty text child in JS. */
		token_append_text_owned(t, "", 0);
	}
	accum_push(accum, t);
	return t;
}

static Token *make_html_attr_dirty(const char *text, size_t text_len, Accum *accum) {
	Token *t= token_new(TOKEN_EXT_ATTR_DIRTY, "html-attr-dirty");
	if(!t) return NULL;
	token_append_text_owned(t, text, text_len);
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
		size_t equal_owned_len = 0;
		char *equal_owned= html_normalize_equal(equal, equal_len, accum, &equal_owned_len);
		assert(equal_owned);
		t->data.ext_attr.equal = (sz_string_view_t){ .start = equal_owned, .length = equal_owned_len };
	}
	t->data.ext_attr.quote_open= quote_open;
	t->data.ext_attr.quote_close= quote_close;

	Token *attr_key= make_html_attr_key(key, key_len, accum);
	if(!attr_key) {
		token_free(t);
		return NULL;
	}
	token_append_child(t, attr_key);

	/* JS parity: AttributeToken always has an attr-value child.
	 * For boolean attrs without '=', value node has no text child;
	 * for explicit empty `=`, it contains a text child "". */
	Token *attr_value= NULL;
	if(val) {
		attr_value= make_html_attr_value(val, val_len, accum);
	} else {
		attr_value= token_new(TOKEN_ATTR_VALUE, "attr-value");
		if(attr_value) accum_push(accum, attr_value);
	}
	if(!attr_value) {
		token_free(t);
		return NULL;
	}
	token_append_child(t, attr_value);

	accum_push(accum, t);
	return t;
}

/* JS parity for AttributesToken key validity check:
 * /^(?:[\w:]|\0\d+t\x7F)(?:[\w:.-]|\0\d+t\x7F)*$/u
 * Accept t/a/s sentinels because C stage-1 can emit arg-like sentinels in
 * places where JS carries transclusion-like ones. */
static bool html_attr_key_valid(const char *k, size_t klen) {
	size_t i = 0;
	if(i >= klen) return false;

	if((unsigned char)k[i] == 0x00) {
		i++;
		if(i >= klen) return false;
		if(k[i] < '0' || k[i] > '9') return false;
		while(i < klen && k[i] >= '0' && k[i] <= '9') i++;
		if(i >= klen || (k[i] != 't' && k[i] != 'a' && k[i] != 's')) return false;
		i++;
		if(i >= klen || (unsigned char)k[i] != 0x7F) return false;
		i++;
	} else {
		unsigned char c = (unsigned char)k[i];
		if(!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
			 (c >= '0' && c <= '9') || c == '_' || c == ':')) {
			return false;
		}
		i++;
	}

	while(i < klen) {
		if((unsigned char)k[i] == 0x00) {
			i++;
			if(i >= klen) return false;
			if(k[i] < '0' || k[i] > '9') return false;
			while(i < klen && k[i] >= '0' && k[i] <= '9') i++;
			if(i >= klen || (k[i] != 't' && k[i] != 'a' && k[i] != 's')) return false;
			i++;
			if(i >= klen || (unsigned char)k[i] != 0x7F) return false;
			i++;
		} else {
			unsigned char c = (unsigned char)k[i];
			if(!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
				 (c >= '0' && c <= '9') || c == '_' || c == ':' ||
				 c == '.' || c == '-')) {
				return false;
			}
			i++;
		}
	}

	return true;
}

/* Returns sentinel byte length at attr[i] when it matches type, else 0.
 * Sentinel form: \0<digits><type>\x7F */
static size_t html_sentinel_at(const char *attr, size_t attr_len, size_t i, char type) {
	if(!attr || i >= attr_len || (unsigned char)attr[i] != 0x00) return 0;
	size_t j= i + 1;
	if(j >= attr_len || attr[j] < '0' || attr[j] > '9') return 0;
	while(j < attr_len && attr[j] >= '0' && attr[j] <= '9') j++;
	if(j >= attr_len || attr[j] != type) return 0;
	if(j + 1 >= attr_len || (unsigned char)attr[j + 1] != 0x7F) return 0;
	return (j + 2) - i;
}

static size_t html_skip_ws_or_sentinels(const char *attr, size_t attr_len, size_t i,
														const char *sentinel_types, size_t sentinel_types_len) {
	while(i < attr_len) {
		if(isspace((unsigned char)attr[i])) {
			i++;
			continue;
		}
		bool consumed = false;
		for(size_t ti = 0; ti < sentinel_types_len; ti++) {
			size_t sl = html_sentinel_at(attr, attr_len, i, sentinel_types[ti]);
			if(sl > 0) {
				i += sl;
				consumed = true;
				break;
			}
		}
		if(!consumed) break;
	}
	return i;
}

/* JS parity for regex (?:\s|\0\d+[cn]\x7F)* used around equals. */
static size_t html_skip_eq_ws(const char *attr, size_t attr_len, size_t i) {
	return html_skip_ws_or_sentinels(attr, attr_len, i, "cn", 2);
}

static void parse_html_attrs(Token *attrs_tok, const char *attr_str, size_t attr_len, Accum *accum) {
	if(!attr_str || attr_len == 0) return;

	size_t i= 0;
	char dirty_buf[4096];
	size_t dirty_len= 0;

#define APPEND_HTML_DIRTY_RANGE(start_idx, end_idx)                              \
	do {                                                                            \
		size_t __start = (start_idx);                                                \
		size_t __end = (end_idx);                                                    \
		if(__end > __start) {                                                        \
			size_t __n = __end - __start;                                              \
			sz_copy(dirty_buf + dirty_len, attr_str + __start, __n);                   \
			dirty_len += __n;                                                          \
		}                                                                             \
	} while(0)

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
			if(html_sentinel_at(attr_str, attr_len, i, '~') > 0) break;
			i++;
		}
		size_t key_len= i - key_start;
		if(key_len == 0) {
			dirty_buf[dirty_len++]= attr_str[i++];
			continue;
		}

		const char *key= attr_str + key_start;
		bool valid_key= html_attr_key_valid(key, key_len);

		if(!valid_key) {
			APPEND_HTML_DIRTY_RANGE(key_start, key_start + key_len);

			/* JS parity: if an invalid key is followed by '=', treat the whole
			 * assignment as dirty instead of re-parsing the value as a new key. */
			size_t j = html_skip_eq_ws(attr_str, attr_len, i);
			size_t eq_bad_len = 0;
			if(j < attr_len && attr_str[j] == '=') {
				eq_bad_len = 1;
			} else {
				eq_bad_len = html_sentinel_at(attr_str, attr_len, j, '~');
			}

			if(eq_bad_len > 0) {
				APPEND_HTML_DIRTY_RANGE(i, j + eq_bad_len);
				i = j + eq_bad_len;

				size_t ws_after_eq = i;
				i = html_skip_eq_ws(attr_str, attr_len, i);
				APPEND_HTML_DIRTY_RANGE(ws_after_eq, i);

				if(i < attr_len && (attr_str[i] == '"' || attr_str[i] == '\'')) {
					char q = attr_str[i++];
					size_t vstart = i - 1;
					const char *qclose = sz_find_byte(attr_str + i, attr_len - i, &q);
					i = qclose ? (size_t)(qclose - attr_str) + 1 : attr_len;
					APPEND_HTML_DIRTY_RANGE(vstart, i);
				} else {
					size_t vstart = i;
					while(i < attr_len && !isspace((unsigned char)attr_str[i])) i++;
					APPEND_HTML_DIRTY_RANGE(vstart, i);
				}
			}
			continue;
		}

		size_t eq_start= i;
		i= html_skip_eq_ws(attr_str, attr_len, i);

		size_t eq_tok_len= 0;
		if(i < attr_len && attr_str[i] == '=') {
			eq_tok_len= 1;
		} else {
			eq_tok_len= html_sentinel_at(attr_str, attr_len, i, '~');
		}

		if(i >= attr_len || eq_tok_len == 0) {
			FLUSH_HTML_DIRTY();
			Token *at= make_html_attr(key, key_len, NULL, 0, "", 0, '\0', '\0', accum);
			if(at) token_append_child(attrs_tok, at);
			i= eq_start;
			continue;
		}

		i+= eq_tok_len;
		i= html_skip_eq_ws(attr_str, attr_len, i);

		const char *equal_start= attr_str + eq_start;
		size_t equal_len= i - eq_start;
		const char *val= NULL;
		size_t val_len= 0;
		char quote_open= '\0';
		char quote_close= '\0';

		if(i < attr_len && (attr_str[i] == '"' || attr_str[i] == '\'')) {
			quote_open= attr_str[i++];
			size_t val_start= i;
			const char *qclose = sz_find_byte(attr_str + i, attr_len - i, &quote_open);
			i = qclose ? (size_t)(qclose - attr_str) : attr_len;
			val= attr_str + val_start;
			val_len= i - val_start;
			if(i < attr_len) {
				quote_close= quote_open;
				i++;
			}
		} else {
			size_t val_start= i;
			const char *w = sz_find_byte_from(attr_str + i, attr_len - i, HTML_WS_BYTES, sizeof(HTML_WS_BYTES) - 1);
			i = w ? (size_t)(w - attr_str) : attr_len;
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
#undef APPEND_HTML_DIRTY_RANGE
}

static Token *build_html_attrs(const char *tag_name, const char *attr_str, size_t attr_len, Accum *accum) {
	Token *t= token_new(TOKEN_ATTRIBUTES, "html-attrs");
	if(!t) return NULL;
	 t->name= strdup(tag_name);
	accum_push(accum, t);

	if(attr_str && attr_len > 0 && !isspace((unsigned char)attr_str[0]) && attr_str[0] != '/') {
		ThreadBuf *tmp = wiki_thread_buf_acquire_scratch();
		if(!tmp) {
			log_fatal("build_html_attrs: failed to acquire scratch");
			abort();
		}
		wiki_thread_buf_reserve(tmp, attr_len + 2);
		tmp->buf[0]= ' ';
		sz_copy(tmp->buf + 1, attr_str, attr_len);
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
	size_t attr_len= strlen(attr_name);
	for(size_t i= 0; i < attrs->child_count; i++) {
		const Child *c= &attrs->children[i];
		if(c->is_text || !c->token) continue;
		const Token *a= c->token;
			if(a->type == TOKEN_EXT_ATTR && a->name) {
					if(a->name[attr_len] == '\0' && str_ci_eq_n(a->name, attr_name, attr_len)) return true;
		}
	}
	return false;
}

static bool html_attrs_has_valid_attr(const Token *attrs) {
	if(!attrs) return false;
	for(size_t i= 0; i < attrs->child_count; i++) {
		const Child *c= &attrs->children[i];
		if(c->is_text || !c->token) continue;
		if(c->token->type == TOKEN_EXT_ATTR) return true;
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
			sz_copy(out_tb->buf + out_tb->len, buf + pos, rest);
			out_tb->len+= rest;
			break;
		}

		/* copy text before '<' */
		size_t before= (size_t)(lt - (buf + pos));
		ENSURE_OUT_CAP(before + 1);
		sz_copy(out_tb->buf + out_tb->len, buf + pos, before);
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
				sz_copy(out_tb->buf + out_tb->len, seg_start, seg_len);
				out_tb->len+= seg_len;
			}
		} else {
			/* Have a candidate tag name */
			if(!htc.tag_name || htc.tag_name_len == 0) {
				ENSURE_OUT_CAP(1 + seg_len + 1);
				out_tb->buf[out_tb->len++]= '<';
				if(seg_len > 0) {
					sz_copy(out_tb->buf + out_tb->len, seg_start, seg_len);
					out_tb->len+= seg_len;
				}
			} else {
				char *lcname = str_trim_lc(htc.tag_name, htc.tag_name_len);
				if(!lcname) {
					ENSURE_OUT_CAP(1 + seg_len + 1);
					out_tb->buf[out_tb->len++]= '<';
					if(seg_len > 0) {
						sz_copy(out_tb->buf + out_tb->len, seg_start, seg_len);
						out_tb->len+= seg_len;
					}
				} else {
					if(!html_tag_allowed(cfg, lcname, htc.tag_name_len)) {
						/* unknown tag — emit raw */
						ENSURE_OUT_CAP(1 + seg_len + 1);
						out_tb->buf[out_tb->len++]= '<';
						if(seg_len > 0) {
							sz_copy(out_tb->buf + out_tb->len, seg_start, seg_len);
							out_tb->len+= seg_len;
						}
						free(lcname);
					} else {
						/* Allowed tag — build attrs token then html token and emit sentinel */
						size_t saved_accum = accum->count;

						const char *attr_ptr = htc.attrs;
						size_t attr_len = htc.attrs_len;

						/* Fast reject for plain attr text on meta/link before token creation. */
						bool early_reject = false;
						const bool is_meta = (htc.tag_name_len == 4 && str_ci_eq_n(htc.tag_name, "meta", 4));
						const bool is_link = (htc.tag_name_len == 4 && str_ci_eq_n(htc.tag_name, "link", 4));
						if((is_meta || is_link) && attr_ptr && attr_len > 0 && !html_has_any_sentinel(attr_ptr, attr_len)) {
							bool has_itemprop = html_find_ci_lit(attr_ptr, attr_len, "itemprop", 8);
							bool has_required = is_meta
								? html_find_ci_lit(attr_ptr, attr_len, "content", 7)
								: html_find_ci_lit(attr_ptr, attr_len, "href", 4);
							early_reject = !(has_itemprop && has_required);
						}

						if(early_reject) {
							ENSURE_OUT_CAP(1 + seg_len + 1);
							out_tb->buf[out_tb->len++]= '<';
							if(seg_len > 0) {
								sz_copy(out_tb->buf + out_tb->len, seg_start, seg_len);
								out_tb->len+= seg_len;
							}
							free(lcname);
							pos= (size_t)(seg_start - buf) + seg_len;
							continue;
						}

						Token *attrs = build_html_attrs(lcname, attr_ptr, attr_len, accum);

						/* Special-case: meta/link require itemprop+content/href. */
						bool reject = false;
						if(!htc.is_closing && attr_ptr && attr_len > 0) {
							size_t p = 0;
							while(p < attr_len && isspace((unsigned char)attr_ptr[p])) p++;
							if(p < attr_len && attr_ptr[p] == '[') {
								bool has_eq = sz_find(attr_ptr + p, attr_len - p, "=", 1) != NULL;
								bool has_close = sz_find(attr_ptr + p, attr_len - p, "]", 1) != NULL;
								if(has_eq && has_close && !html_attrs_has_valid_attr(attrs)) {
									reject = true;
								}
							}
						}
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
								sz_copy(out_tb->buf + out_tb->len, seg_start, seg_len);
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
							sz_copy(out_tb->buf + out_tb->len, sent_buf, sent_len);
							out_tb->len+= sent_len;
							if(rest_len > 0) {
								sz_copy(out_tb->buf + out_tb->len, rest_ptr, rest_len);
								out_tb->len+= rest_len;
							}

							/* Now create HtmlToken and push it (matching JS order) */
							Token *ht = token_new(TOKEN_HTML, "html");
							if(ht) {
													ht->name = strdup(lcname);
								/* orig_tag: original-case name for toString round-trip */
								char *orig_tag = malloc(htc.tag_name_len + 1);
								if(orig_tag) {
									sz_copy(orig_tag, htc.tag_name, htc.tag_name_len);
									orig_tag[htc.tag_name_len] = '\0';
								}
								ht->data.html.orig_tag = (sz_string_view_t){ .start = orig_tag, .length = orig_tag ? htc.tag_name_len : 0 };
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
