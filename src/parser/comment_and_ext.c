#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

#include "log.h"
#include "build.h"
#include "parser/braces.h"
#include "parser/comment_and_ext.h"
#include "parser/link.h"
#include "parser/links.h"
#include "string_util.h"
#include <stringzilla/stringzilla.h>
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
	char **items;
	size_t *lens;
	size_t count;
	size_t cap;
} TextStack;

static void text_stack_init(TextStack *st) {
	st->items= NULL;
	st->lens= NULL;
	st->count= 0;
	st->cap= 0;
}

static void text_stack_push(TextStack *st, const char *s, size_t len) {
	if(st->count >= st->cap) {
		size_t new_cap= st->cap ? st->cap * 2 : 8;
		st->items= realloc(st->items, new_cap * sizeof(char *));
		st->lens= realloc(st->lens, new_cap * sizeof(size_t));
		assert(st->items && st->lens);
		st->cap= new_cap;
	}
	char *copy= malloc(len + 1);
	assert(copy);
	sz_copy(copy, s, len);
	copy[len]= '\0';
	st->items[st->count]= copy;
	st->lens[st->count]= len;
	st->count++;
}

static void text_stack_free(TextStack *st) {
	for(size_t i= 0; i < st->count; i++) free(st->items[i]);
	free(st->items);
	free(st->lens);
	st->items= NULL;
	st->lens= NULL;
	st->count= 0;
	st->cap= 0;
}

static void append_numeric_placeholder(char *dst, size_t *len, size_t idx) {
	dst[(*len)++]= '\0';
	if(idx == 0) {
		dst[(*len)++] = '0';
	} else {
		char buf[32];
		size_t n = 0;
		size_t value = idx;
		while(value > 0) {
			buf[n++] = (char)('0' + (value % 10));
			value /= 10;
		}
		for(size_t i = 0; i < n; i++) {
			dst[*len + i] = buf[n - 1 - i];
		}
		*len += n;
	}
	dst[(*len)++]= '\x7F';
}

static const char *find_substr_cs(const char *hay, size_t hlen,
																	const char *needle, size_t nlen) {
	if(!hay || !needle || nlen == 0 || nlen > hlen) return NULL;
	return sz_find(hay, hlen, needle, nlen);
}

static char *token_string_dup(const Token *tok, size_t *out_len) {
	if(!tok) {
		if(out_len) *out_len= 0;
		return strdup("");
	}
	ThreadBuf *tb= wiki_thread_buf_acquire_scratch();
	char *s= token_to_string(tok, tb);
	size_t n= tb->len;
	char *dup= malloc(n + 1);
	assert(dup);
	memcpy(dup, s, n);
	dup[n]= '\0';
	wiki_thread_buf_release_scratch(tb);
	if(out_len) *out_len= n;
	return dup;
}

/* JS restore parity used by parseCommentAndExt:
 * restore(s, accum, 1): expand \0Ng\x7F recursively through mode 2.
 * restore(s, accum, 2): expand \0Nn\x7F.
 */
static char *restore_accum_mode(const char *s, size_t len,
																const Accum *accum, int mode,
																size_t *out_len) {
	size_t cap= len * 2 + 32;
	char *out= malloc(cap);
	assert(out);
	size_t j= 0;

#define ENSURE_RESTORE_CAP(need)   \
	do {                             \
		while(j + (need) + 1 >= cap) { \
			cap*= 2;                     \
			out= realloc(out, cap);      \
			assert(out);                 \
		}                              \
	} while(0)

	for(size_t i= 0; i < len;) {
		if((unsigned char)s[i] == '\0') {
			size_t k= i + 1;
			while(k < len && s[k] >= '0' && s[k] <= '9') k++;
			if(k > i + 1 && k + 1 < len && (unsigned char)s[k + 1] == '\x7F') {
				char ch= s[k];
				bool should_expand= (mode == 1 && ch == 'g') || (mode == 2 && ch == 'n');
				if(should_expand) {
					size_t idx= 0;
					for(size_t d= i + 1; d < k; d++) idx= idx * 10 + (size_t)(s[d] - '0');
					Token *ref= accum_get(accum, idx);
					if(ref) {
						size_t rep_len= 0;
						char *rep= token_string_dup(ref, &rep_len);
						if(mode == 1 && ch == 'g') {
							size_t nested_len= 0;
							char *nested= restore_accum_mode(rep, rep_len, accum, 2, &nested_len);
							free(rep);
							rep= nested;
							rep_len= nested_len;
						}
						ENSURE_RESTORE_CAP(rep_len);
						memcpy(out + j, rep, rep_len);
						j+= rep_len;
						free(rep);
						i= k + 2;
						continue;
					}
				}
			}
		}

		ENSURE_RESTORE_CAP(1);
		out[j++]= s[i++];
	}

#undef ENSURE_RESTORE_CAP
	out[j]= '\0';
	if(out_len) *out_len= j;
	return out;
}

/* ── Attribute parsing helpers ───────────────────────────────────────────── */

/**
 * Build an attr-key token.
 */
static Token *make_attr_key(const char *key, size_t key_len, Accum *accum) {
	Token *t= token_new(TOKEN_ATTR_KEY, "attr-key");
	if(!t) return NULL;
	if(key && key_len > 0) {
		const char *key_view = wiki_thread_buf_append_to_tokens(key, key_len);
		token_append_text_n(t, key_view, key_len);
	} else {
		token_append_text_n(t, "", 0);
	}
	accum_push(accum, t);
	return t;
}

/**
 * Build an attr-value token.
 */
static Token *make_attr_value(const char *val, size_t val_len, Accum *accum) {
	Token *t= token_new(TOKEN_ATTR_VALUE, "attr-value");
	if(!t) return NULL;
	if(val && val_len > 0) {
		const char *val_view = wiki_thread_buf_append_to_tokens(val, val_len);
		token_append_text_n(t, val_view, val_len);
	} else {
		token_append_text_n(t, "", 0);
	}
	accum_push(accum, t);
	return t;
}

/**
 * Build an ext-attr-dirty AtomToken.
 */
static Token *make_attr_dirty(const char *text, size_t text_len, Accum *accum) {
	Token *t= token_new(TOKEN_EXT_ATTR_DIRTY, "ext-attr-dirty");
	if(!t) return NULL;
	if(text && text_len > 0) {
		const char *text_view = wiki_thread_buf_append_to_tokens(text, text_len);
		token_append_text_n(t, text_view, text_len);
	} else {
		token_append_text_n(t, "", 0);
	}
	accum_push(accum, t);
	return t;
}

/**
 * Build an ext-attr AttributeToken with a key and optional value.
 * equal is the full equal-sign text (e.g. "=", " = "; empty/NULL for boolean attrs).
 * quote_open/close are the surrounding quote chars, or '\0' if unquoted.
 * tag_name is the parent tag (for setting the .name on the ext-attr).
 */
static Token *make_ext_attr(const char *tag_name,
														const char *key, size_t key_len,
														const char *val, size_t val_len, /* val may be NULL */
														const char *equal, size_t equal_len,
														char quote_open, char quote_close,
														Accum *accum) {
	Token *t= token_new(TOKEN_EXT_ATTR, "ext-attr");
	if(!t) return NULL;
	/* Lowercase the key for .name */
	char *lkey= str_trim_lc(key, key_len);
	t->name= lkey; /* ownership transferred */
	(void)tag_name;

	/* Store equal and quote chars (JS AttributeToken #equal / #quotes) */
	if(equal && equal_len > 0) {
		t->data.ext_attr.equal= malloc(equal_len + 1);
	sz_copy(t->data.ext_attr.equal, equal, equal_len);
		t->data.ext_attr.equal[equal_len]= '\0';
	}
	t->data.ext_attr.quote_open= quote_open;
	t->data.ext_attr.quote_close= quote_close;

	Token *attr_key= make_attr_key(key, key_len, accum);
	if(!attr_key) {
		token_free(t);
		return NULL;
	}
	token_append_child(t, attr_key);

	Token *attr_val= NULL;
	if(val) {
		attr_val= make_attr_value(val, val_len, accum);
	} else {
		/* JS parity: boolean attrs still have attr-value token, but with no text child. */
		attr_val= token_new(TOKEN_ATTR_VALUE, "attr-value");
		if(attr_val) accum_push(accum, attr_val);
	}
	if(!attr_val) {
		token_free(t);
		return NULL;
	}
	token_append_child(t, attr_val);

	accum_push(accum, t);
	return t;
}

/**
 * Parse an attribute string (e.g. ` name="foo" class="bar"`) and build ext-attrs
 * child tokens on `attrs_tok`.
 *
 * Simplified version of JS AttributesToken constructor.
 * Handles:
 *   - Leading/trailing whitespace → ext-attr-dirty
 *   - key          → ext-attr (boolean attribute)
 *   - key=value    → ext-attr
 *   - key="value"  → ext-attr
 *   - key='value'  → ext-attr
 */
static void parse_ext_attrs(Token *attrs_tok, const char *attr_str, size_t attr_len,
														const char *tag_name, Accum *accum) {
	if(!attr_str || attr_len == 0) return;

	size_t i= 0;
	char dirty_buf[4096]; /* Accumulates "dirty" text */
	size_t dirty_len= 0;

#define FLUSH_DIRTY()                                          \
	do {                                                         \
		if(dirty_len > 0) {                                        \
			Token *dt= make_attr_dirty(dirty_buf, dirty_len, accum); \
			if(dt) token_append_child(attrs_tok, dt);                \
			dirty_len= 0;                                            \
		}                                                          \
	} while(0)

	while(i < attr_len) {
		/* Skip whitespace by adding to dirty */
		if(isspace((unsigned char)attr_str[i])) {
			while(i < attr_len && isspace((unsigned char)attr_str[i])) {
				dirty_buf[dirty_len++]= attr_str[i++];
			}
			continue;
		}

		/* Skip '/' character (self-closing slash before >) */
		if(attr_str[i] == '/') {
			dirty_buf[dirty_len++]= attr_str[i++];
			continue;
		}

		/* Try to parse a key: [^\s/=]+ (simplified) */
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

		/* Validate key: must match [a-zA-Z_:][a-zA-Z0-9:._-]* (simplified) */
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
			/* Not a valid key — add to dirty */
			for(size_t k= 0; k < key_len; k++) dirty_buf[dirty_len++]= key[k];
			continue;
		}

		/* Skip optional whitespace before '=' */
		size_t eq_start= i;
		while(i < attr_len && isspace((unsigned char)attr_str[i])) i++;

		if(i >= attr_len || attr_str[i] != '=') {
			/* Boolean attribute (no value) */
			/* Flush dirty */
			FLUSH_DIRTY();
			Token *at= make_ext_attr(tag_name, key, key_len, NULL, 0,
															 "", 0, '\0', '\0', accum);
			if(at) token_append_child(attrs_tok, at);
			i= eq_start; /* reset to before optional whitespace */
			/* Whitespace before a missing '=' goes back to dirty in JS */
			continue;
		}

		/* Equal sign found */
		i++; /* skip '=' */

		/* Skip optional whitespace after '=' */
		while(i < attr_len && isspace((unsigned char)attr_str[i])) i++;

		/* Capture the full equal string (whitespace + '=' + whitespace) for JS parity */
		const char *equal_start= attr_str + eq_start;
		size_t equal_slen= i - eq_start;

		/* Parse value */
		const char *val= NULL;
		size_t val_len= 0;
		char quote_open= '\0', quote_close= '\0';

		if(i < attr_len && (attr_str[i] == '"' || attr_str[i] == '\'')) {
			quote_open= attr_str[i++];
			size_t val_start= i;
			while(i < attr_len && attr_str[i] != quote_open) i++;
			val= attr_str + val_start;
			val_len= i - val_start;
			bool found_close= (i < attr_len);
			quote_close= found_close ? quote_open : '\0';
			if(found_close) i++; /* skip closing quote */
		} else {
			/* Unquoted value: \S+ */
			size_t val_start= i;
			while(i < attr_len && !isspace((unsigned char)attr_str[i])) i++;
			val= attr_str + val_start;
			val_len= i - val_start;
		}

		FLUSH_DIRTY();
		Token *at= make_ext_attr(tag_name, key, key_len, val, val_len,
														 equal_start, equal_slen, quote_open, quote_close,
														 accum);
		if(at) token_append_child(attrs_tok, at);
	}

	FLUSH_DIRTY();
#undef FLUSH_DIRTY
}

/* ── ext-attrs token builder ─────────────────────────────────────────────── */

static Token *build_ext_attrs(const char *tag_name,
															const char *attr_str, size_t attr_len,
															Accum *accum) {
	Token *t= token_new(TOKEN_EXT_ATTRS, "ext-attrs");
	if(!t) return NULL;
	t->name= strdup(tag_name);
	accum_push(accum, t);

	/* Ensure attr starts with whitespace (JS always ensures this by prepending ' '
     * if it doesn't: `!attr || /^\s/u.test(attr) ? attr : ` ${attr}`` */
	if(attr_str && attr_len > 0 && !isspace((unsigned char)attr_str[0])) {
		/* Prepend a space */
		char *padded= malloc(attr_len + 2);
		padded[0]= ' ';
		memcpy(padded + 1, attr_str, attr_len);
		parse_ext_attrs(t, padded, attr_len + 1, tag_name, accum);
		free(padded);
	} else {
		parse_ext_attrs(t, attr_str, attr_len, tag_name, accum);
	}

	return t;
}

/* ── ext-inner token builder ─────────────────────────────────────────────── */

static bool is_multiline_tag(const char *tag_name) {
	static const char *const ML_TAGS[]= {
	"gallery",
	"imagemap",
	"dynamicpagelist",
	"inputbox",
	NULL,
	};
	for(int i= 0; ML_TAGS[i]; i++) {
		if(strcmp(tag_name, ML_TAGS[i]) == 0) return true;
	}
	return false;
}

static Token *build_ext_inner(const char *tag_name,
															const char *inner_str, size_t inner_len,
															bool self_closing,
															Accum *accum) {
	Token *t= token_new(TOKEN_EXT_INNER, "ext-inner");
	if(!t) return NULL;
	t->name= strdup(tag_name);
	if(is_multiline_tag(tag_name)) {
		t->sep= '\n';
	}
	if(self_closing && strcmp(tag_name, "nowiki") != 0) {
		accum_push(accum, t);
		return t;
	}
	if(inner_str && inner_len > 0) {
		const char *inner_view = wiki_thread_buf_append_to_tokens(inner_str, inner_len);
		token_append_text_n(t, inner_view, inner_len);
	} else {
		token_append_text_n(t, "", 0); /* empty inner */
	}
	accum_push(accum, t);
	return t;
}

/* JS parity for ExtToken(name='references') using NestedToken(inner, include, ['ref']):
 * 1) parseCommentAndExt(inner, includeOnly=false)
 * 2) parseBraces(inner)
 * 3) wrap plain text runs between sentinels into NoincludeToken sentinels
 */
static Token *build_references_inner_token(const char *inner_str, size_t inner_len,
																				 const ParserConfig *cfg,
																				 Accum *accum) {
	Token *t= token_new(TOKEN_EXT_INNER, "ext-inner");
	if(!t) return NULL;
	t->name= strdup("references");
	accum_push(accum, t);

	if(!inner_str || inner_len == 0) {
		return t;
	}

	ThreadBuf tmp;
	tmp.buf= malloc(inner_len + 1);
	if(!tmp.buf) return t;
	memcpy(tmp.buf, inner_str, inner_len);
	tmp.buf[inner_len]= '\0';
	tmp.len= inner_len;
	tmp.cap= inner_len + 1;
	tmp.shrink_size= (size_t)-1;
	tmp.target_size= tmp.cap;

	parse_comment_and_ext(&tmp, cfg, accum, false);
	parse_braces(&tmp, cfg, accum);

	size_t out_cap= tmp.len * 2 + 64;
	char *out= malloc(out_cap);
	assert(out);
	size_t out_len= 0;

#define ENSURE_REF_CAP(need)             \
	do {                                     \
		while(out_len + (need) >= out_cap) {    \
			out_cap*= 2;                            \
			out= realloc(out, out_cap);             \
			assert(out);                            \
		}                                        \
	} while(0)

	for(size_t i= 0; i < tmp.len;) {
		if((unsigned char)tmp.buf[i] == '\0') {
			size_t k= i + 1;
			while(k < tmp.len && tmp.buf[k] >= '0' && tmp.buf[k] <= '9') k++;
			if(k > i + 1 && k + 1 < tmp.len && (unsigned char)tmp.buf[k + 1] == '\x7F') {
				size_t mlen= (k + 2) - i;
				ENSURE_REF_CAP(mlen + 1);
				memcpy(out + out_len, tmp.buf + i, mlen);
				out_len+= mlen;
				i= k + 2;
				continue;
			}
		}

		size_t j= i;
		while(j < tmp.len && (unsigned char)tmp.buf[j] != '\0') j++;
		size_t run_len= j - i;
		if(run_len > 0) {
			Token *ni= token_new(TOKEN_NOINCLUDE, "noinclude");
			if(ni) {
				const char *ni_view = wiki_thread_buf_append_to_tokens(tmp.buf + i, run_len);
				token_append_text_n(ni, ni_view, run_len);
				accum_push(accum, ni);
				size_t idx= accum->count - 1;
				char sent[64];
				size_t slen;
				work_str_sentinel(idx, 'n', sent, &slen);
				ENSURE_REF_CAP(slen + 1);
				memcpy(out + out_len, sent, slen);
				out_len+= slen;
			}
		}
		i= j;
	}

#undef ENSURE_REF_CAP

	build_from_str(t, out, out_len, accum);

	free(out);
	free(tmp.buf);
	return t;
}

static Token *parse_gallery_image_line_local(const char *line, size_t line_len,
																			const ParserConfig *cfg,
																			Accum *accum) {
	if(!line || line_len == 0) return NULL;

	char *wrapped= malloc(line_len + 10);
	if(!wrapped) return NULL;
	wrapped[0]= '[';
	wrapped[1]= '[';
	memcpy(wrapped + 2, "File:", 5);
	memcpy(wrapped + 7, line, line_len);
	wrapped[7 + line_len]= ']';
	wrapped[8 + line_len]= ']';
	wrapped[9 + line_len]= '\0';

	ThreadBuf tmp_tb;
	tmp_tb.buf= wrapped;
	tmp_tb.len= line_len + 9;
	tmp_tb.cap= line_len + 10;
	tmp_tb.shrink_size= (size_t)-1;
	tmp_tb.target_size= tmp_tb.cap;

	parse_braces(&tmp_tb, cfg, accum);
	parse_links(&tmp_tb, cfg, accum, NULL, false);

	Token *tmp= token_new(TOKEN_PLAIN, "gallery-line");
	if(!tmp) {
		free(tmp_tb.buf);
		return NULL;
	}
	build_from_str(tmp, tmp_tb.buf, tmp_tb.len, accum);
	build_token_recursive(tmp, accum, cfg);

	Token *out= NULL;
	if(tmp->child_count == 1 && !tmp->children[0].is_text && tmp->children[0].token && tmp->children[0].token->type == TOKEN_FILE) {
		out= tmp->children[0].token;
		tmp->children[0].token= NULL;
		if(out->type_name) free(out->type_name);
		out->type_name= strdup("gallery-image");
		if(out->name) {
			free(out->name);
			out->name= NULL;
		}
		/* JS parity: GalleryImageToken stores raw line file text (without "File:"). */
		if(out->child_count > 0 && !out->children[0].is_text && out->children[0].token) {
			Token *target= out->children[0].token;
			if(target->child_count > 0 && target->children[0].is_text && target->children[0].text && target->children[0].text_len >= 5 && strncasecmp(target->children[0].text, "File:", 5) == 0) {
				const char *old_ptr = target->children[0].text;
				size_t old_len= target->children[0].text_len;
				size_t new_len= old_len - 5;
				char *nw= malloc(new_len + 1);
				if(nw) {
					memcpy(nw, old_ptr + 5, new_len);
					nw[new_len]= '\0';
					/* Free previous text only if it was heap-allocated */
					if(target->children[0].text_owned && target->children[0].text) {
						free((void*)target->children[0].text);
					}
					target->children[0].text= nw;
					target->children[0].text_len= new_len;
					target->children[0].text_owned = true;
				}
			}
		}
		/* JS stage-log parity: link/file names are assigned later in afterBuild(). */
		for(size_t ci= 0; ci < out->child_count; ci++) {
			if(out->children[ci].is_text || !out->children[ci].token) continue;
			Token *child= out->children[ci].token;
			if((child->type == TOKEN_LINK || child->type == TOKEN_FILE || child->type == TOKEN_CATEGORY) && child->name) {
				free(child->name);
				child->name= NULL;
			}
			for(size_t cj= 0; cj < child->child_count; cj++) {
				if(child->children[cj].is_text || !child->children[cj].token) continue;
				Token *g= child->children[cj].token;
				if((g->type == TOKEN_LINK || g->type == TOKEN_FILE || g->type == TOKEN_CATEGORY) && g->name) {
					free(g->name);
					g->name= NULL;
				}
			}
		}
	}

	token_free_shallow(tmp);
	free(tmp_tb.buf);
	return out;
}

static Token *make_empty_noinclude_local(Accum *accum) {
	Token *n= token_new(TOKEN_NOINCLUDE, "noinclude");
	if(!n) return NULL;
	token_append_text_n(n, "", 0);
	accum_push(accum, n);
	return n;
}

static Token *create_raw_link_token_local(const char *s, size_t len, Accum *accum) {
	if(!s) return NULL;

	/* Find first '|' using StringZilla for faster scanning */
	size_t pipe= SIZE_MAX;
	{
		const char needle = '|';
		const char *p = sz_find_byte(s, len, &needle);
		if(p) pipe = (size_t)(p - s);
	}

	const char *target_ptr= s;
	size_t target_len= (pipe == SIZE_MAX) ? len : pipe;
	Token *link= create_link_token(TOKEN_LINK, "link",
													 target_ptr, target_len,
													 NULL, 0, NULL,
													 NULL, accum, false);
	if(!link) return NULL;

	if(pipe != SIZE_MAX) {
		Token *text_tok= token_new(TOKEN_PLAIN, "link-text");
		if(text_tok) {
			const char *text_ptr= s + pipe + 1;
			size_t text_len= len - (pipe + 1);
			if(text_len > 0) {
				const char *txt_view = wiki_thread_buf_append_to_tokens(text_ptr, text_len);
				token_append_text_n(text_tok, txt_view, text_len);
			} else {
				token_append_text_n(text_tok, "", 0);
			}
			accum_push(accum, text_tok);
			token_append_child(link, text_tok);
		}
	}

	return link;
}

static Token *parse_imagemap_image_line_local(const char *line, size_t line_len,
														 const ParserConfig *cfg,
														 Accum *accum) {
	if(!line || line_len == 0) return NULL;

	char *wrapped= malloc(line_len + 5);
	if(!wrapped) return NULL;
	wrapped[0]= '[';
	wrapped[1]= '[';
	memcpy(wrapped + 2, line, line_len);
	wrapped[2 + line_len]= ']';
	wrapped[3 + line_len]= ']';
	wrapped[4 + line_len]= '\0';

	ThreadBuf tmp_tb;
	tmp_tb.buf= wrapped;
	tmp_tb.len= line_len + 4;
	tmp_tb.cap= line_len + 5;
	tmp_tb.shrink_size= (size_t)-1;
	tmp_tb.target_size= tmp_tb.cap;

	parse_braces(&tmp_tb, cfg, accum);
	parse_links(&tmp_tb, cfg, accum, NULL, false);

	Token *tmp= token_new(TOKEN_PLAIN, "imagemap-image-line");
	if(!tmp) {
		free(tmp_tb.buf);
		return NULL;
	}
	build_from_str(tmp, tmp_tb.buf, tmp_tb.len, accum);
	build_token_recursive(tmp, accum, cfg);

	Token *out= NULL;
	if(tmp->child_count == 1 && !tmp->children[0].is_text && tmp->children[0].token && tmp->children[0].token->type == TOKEN_FILE) {
		out= tmp->children[0].token;
		tmp->children[0].token= NULL;
		if(out->type_name) free(out->type_name);
		out->type_name= strdup("imagemap-image");
		if(out->name) {
			free(out->name);
			out->name= NULL;
		}
		/* JS stage-log parity: link/file names are assigned later in afterBuild(). */
		for(size_t ci= 0; ci < out->child_count; ci++) {
			if(out->children[ci].is_text || !out->children[ci].token) continue;
			Token *child= out->children[ci].token;
			if((child->type == TOKEN_LINK || child->type == TOKEN_FILE || child->type == TOKEN_CATEGORY) && child->name) {
				free(child->name);
				child->name= NULL;
			}
			for(size_t cj= 0; cj < child->child_count; cj++) {
				if(child->children[cj].is_text || !child->children[cj].token) continue;
				Token *g= child->children[cj].token;
				if((g->type == TOKEN_LINK || g->type == TOKEN_FILE || g->type == TOKEN_CATEGORY) && g->name) {
					free(g->name);
					g->name= NULL;
				}
			}
		}

		/* JS parity: preserve every empty image parameter slot (||) as an
		 * explicit empty caption parameter at the corresponding position. */
		size_t first_pipe= SIZE_MAX;
		{
			const char needle = '|';
			const char *p = sz_find_byte(line, line_len, &needle);
			if(p) first_pipe = (size_t)(p - line);
		}
		if(first_pipe != SIZE_MAX) {
			size_t *empty_pos= NULL;
			size_t empty_count= 0;
			size_t empty_cap= 0;
			size_t param_idx= 0;
			size_t seg_start= first_pipe + 1;

			for(size_t pi= seg_start; pi <= line_len; pi++) {
				if(pi != line_len && line[pi] != '|') continue;
				if(pi == seg_start) {
					if(empty_count >= empty_cap) {
						size_t new_cap= empty_cap ? empty_cap * 2 : 4;
						size_t *grown= realloc(empty_pos, new_cap * sizeof(size_t));
						if(!grown) {
							free(empty_pos);
							empty_pos= NULL;
							empty_count= 0;
							empty_cap= 0;
							break;
						}
						empty_pos= grown;
						empty_cap= new_cap;
					}
					empty_pos[empty_count++]= param_idx;
				}
				param_idx++;
				seg_start= pi + 1;
			}

			for(size_t ei= 0; ei < empty_count; ei++) {
				Token *cap= token_new(TOKEN_PLAIN, "image-parameter");
				if(!cap) continue;
				cap->name= strdup("caption");
				token_append_text_n(cap, "", 0);
				accum_push(accum, cap);

				if(out->child_count >= out->child_cap) {
					size_t new_cap= out->child_cap ? out->child_cap * 2 : 4;
					Child *grown= realloc(out->children, new_cap * sizeof(Child));
					if(grown) {
						out->children= grown;
						out->child_cap= new_cap;
					}
				}

				if(out->child_count < out->child_cap) {
					size_t ins= 1 + empty_pos[ei];
					if(ins > out->child_count) ins= out->child_count;
					if(ins < out->child_count) {
						memmove(&out->children[ins + 1], &out->children[ins], (out->child_count - ins) * sizeof(Child));
					}
					out->children[ins].is_text= false;
					out->children[ins].token= cap;
					out->children[ins].text_len= 0;
					out->child_count++;
				} else {
					token_free(cap);
				}
			}

			free(empty_pos);
		}
	}

	token_free_shallow(tmp);
	free(tmp_tb.buf);
	return out;
}

static Token *parse_imagemap_link_line_local(const char *line, size_t line_len,
														const ParserConfig *cfg,
														Accum *accum) {
	if(!line || line_len == 0) return NULL;
	/* Find opening '[[' and closing ']]' using sz_find */
	const char *p_open = sz_find(line, line_len, "[[", 2);
	if(!p_open) return NULL;
	size_t open = (size_t)(p_open - line);

	const char *p_close = sz_find(line + open + 2, line_len - (open + 2), "]]", 2);
	if(!p_close) return NULL;
	size_t close = (size_t)(p_close - line);
	if(close <= open + 1) return NULL;

	Token *t= token_new(TOKEN_PLAIN, "imagemap-link");
	if(!t) return NULL;
	accum_push(accum, t);

	if(open > 0) {
		const char *pre_view = wiki_thread_buf_append_to_tokens(line, open);
		token_append_text_n(t, pre_view, open);
	} else {
		token_append_text_n(t, "", 0);
	}

	const char *inner= line + open + 2;
	size_t inner_len= close - (open + 2);
	Token *link= create_raw_link_token_local(inner, inner_len, accum);
	if(link) {
		token_append_child(t, link);
	} else {
		size_t seg_len = (close + 2) - open;
		if(seg_len > 0) {
			const char *seg_view = wiki_thread_buf_append_to_tokens(line + open, seg_len);
			token_append_text_n(t, seg_view, seg_len);
		} else {
			token_append_text_n(t, "", 0);
		}
	}

	if(close + 2 < line_len) {
		size_t suffix_len = line_len - (close + 2);
		const char *suf_view = wiki_thread_buf_append_to_tokens(line + close + 2, suffix_len);
		token_append_text_n(t, suf_view, suffix_len);
	}

	Token *tail= make_empty_noinclude_local(accum);
	if(tail) token_append_child(t, tail);

	return t;
}

static Token *build_imagemap_inner_token(const char *inner_str, size_t inner_len,
													 const ParserConfig *cfg,
													 Accum *accum) {
	Token *t= token_new(TOKEN_EXT_INNER, "ext-inner");
	if(!t) return NULL;
	t->name= strdup("imagemap");
	t->sep= '\n';
	accum_push(accum, t);

	if(!inner_str || inner_len == 0) return t;

	bool image_seen= false;
	size_t line_start= 0;
	while(line_start <= inner_len) {
		const char *nl = sz_find_byte(inner_str + line_start, inner_len - line_start, "\n");
		size_t i = nl ? (size_t)(nl - inner_str) : inner_len;
		size_t line_len= i - line_start;
		const char *line_ptr= inner_str + line_start;

		if(line_len == 0) {
			Token *n= make_empty_noinclude_local(accum);
			if(n) token_append_child(t, n);
		} else {
			Token *tok= NULL;
			if(!image_seen) {
				tok= parse_imagemap_image_line_local(line_ptr, line_len, cfg, accum);
				if(tok) image_seen= true;
			}
			if(!tok) {
				tok= parse_imagemap_link_line_local(line_ptr, line_len, cfg, accum);
			}
			if(tok) {
				token_append_child(t, tok);
			} else {
				Token *n= token_new(TOKEN_NOINCLUDE, "noinclude");
				if(n) {
					if(line_len > 0) {
						const char *ln_view = wiki_thread_buf_append_to_tokens(line_ptr, line_len);
						token_append_text_n(n, ln_view, line_len);
					} else {
						token_append_text_n(n, "", 0);
					}
					accum_push(accum, n);
					token_append_child(t, n);
				} else {
					if(line_len > 0) {
						const char *ln_view = wiki_thread_buf_append_to_tokens(line_ptr, line_len);
						token_append_text_n(t, ln_view, line_len);
					} else {
						token_append_text_n(t, "", 0);
					}
				}
			}
		}

		if(!nl) break;
		line_start= i + 1;
	}

	return t;
}

static Token *build_gallery_inner_token(const char *inner_str, size_t inner_len,
																			const ParserConfig *cfg,
																			Accum *accum) {
	Token *t= token_new(TOKEN_EXT_INNER, "ext-inner");
	if(!t) return NULL;
	t->name= strdup("gallery");
	t->sep= '\n';
	accum_push(accum, t);

	if(!inner_str || inner_len == 0) return t;

	size_t line_start= 0;
	while(line_start <= inner_len) {
		const char *nl = sz_find_byte(inner_str + line_start, inner_len - line_start, "\n");
		size_t i = nl ? (size_t)(nl - inner_str) : inner_len;
		size_t line_len= i - line_start;
		const char *line_ptr= inner_str + line_start;

		if(line_len == 0) {
			token_append_text_n(t, "", 0);
		} else {
			Token *img= parse_gallery_image_line_local(line_ptr, line_len, cfg, accum);
			if(img) {
				token_append_child(t, img);
			} else {
				Token *ni= token_new(TOKEN_NOINCLUDE, "noinclude");
				if(ni) {
					if(line_len > 0) {
						const char *ln_view = wiki_thread_buf_append_to_tokens(line_ptr, line_len);
						token_append_text_n(ni, ln_view, line_len);
					} else {
						token_append_text_n(ni, "", 0);
					}
					accum_push(accum, ni);
					token_append_child(t, ni);
				}
			}
		}

		if(!nl) break;
		line_start= i + 1;
	}

	return t;
}

static Token *build_categorytree_inner_token(const char *inner_str, size_t inner_len,
																												Accum *accum) {
	Token *t= token_new(TOKEN_EXT_INNER, "ext-inner");
	if(!t) return NULL;
	t->name= strdup("categorytree");
	accum_push(accum, t);

	Token *target= token_new(TOKEN_ATOM, "link-target");
	if(!target) return t;
	if(inner_str && inner_len > 0) {
		const char *tview = wiki_thread_buf_append_to_tokens(inner_str, inner_len);
		token_append_text_n(target, tview, inner_len);
	} else {
		token_append_text_n(target, "", 0);
	}
	accum_push(accum, target);
	token_append_child(t, target);
	return t;
}

/* ── Main token builders ─────────────────────────────────────────────────── */

/**
 * Build a CommentToken and push to accum.
 * The substr is the full comment text "<!--...-->" or "<!--..." (unclosed).
 */
static Token *build_comment_token(const char *substr, size_t sub_len, Accum *accum) {
	/* Mirrors JS:
     * const closed = substr.endsWith('-->');
     * new CommentToken(restore(substr, accum, 1).slice(4, closed ? -3 : undefined), closed, config, accum);
     *
     * The data = inner text between <!-- and -->
     */
	bool closed= (sub_len >= 3 &&
								substr[sub_len - 3] == '-' &&
								substr[sub_len - 2] == '-' &&
								substr[sub_len - 1] == '>');

	/* Inner text: substr.slice(4, closed ? -3 : undefined) */
	const char *inner_start= substr + 4; /* skip "<!--" */
	size_t inner_len;
	if(closed && sub_len >= 7) {
		inner_len= sub_len - 7; /* strip "<!--" (4) and "-->" (3) */
	} else {
		inner_len= sub_len > 4 ? sub_len - 4 : 0;
	}

	Token *t= token_new(TOKEN_COMMENT, "comment");
	if(!t) return NULL;
	t->data.comment.closed= closed;
	if(inner_len > 0) {
		const char *com_view = wiki_thread_buf_append_to_tokens(inner_start, inner_len);
		token_append_text_n(t, com_view, inner_len);
	} else {
		token_append_text_n(t, "", 0);
	}
	accum_push(accum, t);
	return t;
}

/**
 * Build an ExtToken and push to accum (and its sub-tokens).
 */
static Token *build_ext_token(const char *name, size_t name_len,
															const char *attr, size_t attr_len,
															const char *inner, size_t inner_len,
															bool self_closing,
																			const ParserConfig *cfg,
															Accum *accum) {
	/* Lower-case the tag name */
	char *lcname= str_trim_lc(name, name_len);

	Token *t= token_new(TOKEN_EXT, "ext");
	if(!t) {
		free(lcname);
		return NULL;
	}
	t->name= strdup(lcname);
	/* Store original-cased tag name for toString() parity with JS */
	t->data.ext.name= strndup(name, name_len);

	/* Build sub-tokens */
	Token *attrs_tok= build_ext_attrs(lcname, attr, attr_len, accum);
	Token *inner_tok= NULL;
	if(strcmp(lcname, "references") == 0 && !self_closing) {
		inner_tok= build_references_inner_token(inner, inner_len, cfg, accum);
	} else if(strcmp(lcname, "gallery") == 0 && !self_closing) {
		inner_tok= build_gallery_inner_token(inner, inner_len, cfg, accum);
	} else if(strcmp(lcname, "imagemap") == 0 && !self_closing) {
		inner_tok= build_imagemap_inner_token(inner, inner_len, cfg, accum);
	} else if(strcmp(lcname, "categorytree") == 0) {
		inner_tok= build_categorytree_inner_token(inner, inner_len, accum);
	} else {
		inner_tok= build_ext_inner(lcname, inner, inner_len, self_closing, accum);
	}

	if(!attrs_tok || !inner_tok) {
		free(lcname);
		token_free(t);
		return NULL;
	}

	token_append_child(t, attrs_tok);
	token_append_child(t, inner_tok);
	t->data.ext.self_closing= self_closing;

	free(lcname);
	accum_push(accum, t);
	return t;
}

/**
 * Build a NoincludeToken for a single open/close noinclude/onlyinclude tag.
 *
 * Mirrors JS:
 *   new NoincludeToken(substr, config, accum, true)
 * → type "noinclude", one text child containing the raw tag text.
 */
static Token *build_noinclude_token(const char *substr, size_t sub_len, Accum *accum) {
	Token *t= token_new(TOKEN_NOINCLUDE, "noinclude");
	if(!t) return NULL;
	if(sub_len > 0) {
		const char *sub_view = wiki_thread_buf_append_to_tokens(substr, sub_len);
		token_append_text_n(t, sub_view, sub_len);
	} else {
		token_append_text_n(t, "", 0);
	}
	accum_push(accum, t);
	return t;
}

/**
 * Build an IncludeToken for a paired includeonly/noinclude tag.
 *
 * Mirrors JS:
 *   new IncludeToken(include, includeAttr, includeInner, includeClosing, config, accum)
 *
 * IncludeToken (as TagPairToken) has two text children:
 *   [0] = attr text (empty string for simple tags)
 *   [1] = inner text
 * data.include.closing stores the closing tag name (NULL if unclosed).
 */
static Token *build_include_token(const char *tag_name, size_t tag_name_len,
																	const char *attr, size_t attr_len,
																	const char *inner, size_t inner_len,
																	const char *closing, size_t closing_len,
																	Accum *accum) {
	Token *t= token_new(TOKEN_INCLUDE, "include");
	if(!t) return NULL;
	char *lcname= str_trim_lc(tag_name, tag_name_len);
	t->name= lcname;

	/* attr text (may be NULL for self-closing or no attributes) */
	const char *attr_src= attr ? attr : "";
	if(attr && attr_len > 0) {
		const char *attr_view = wiki_thread_buf_append_to_tokens(attr_src, attr_len);
		token_append_text_n(t, attr_view, attr_len);
	} else {
		token_append_text_n(t, "", 0);
	}

	/* inner text */
	const char *inner_src= inner ? inner : "";
	if(inner && inner_len > 0) {
		const char *inner_view = wiki_thread_buf_append_to_tokens(inner_src, inner_len);
		token_append_text_n(t, inner_view, inner_len);
	} else {
		token_append_text_n(t, "", 0);
	}

	/* closing tag name — NULL means unclosed (JS TagPairToken.closed = false) */
	if(closing && closing_len > 0) {
		t->data.include.closing= str_trim_lc(closing, closing_len);
	}

	accum_push(accum, t);
	return t;
}

static Token *build_translate_token(const char *attr, size_t attr_len,
																		const char *inner, size_t inner_len,
																		Accum *accum) {
	Token *t= token_new(TOKEN_TRANSLATE, "translate");
	if(!t) return NULL;
	t->name= strdup("translate");
	if(attr && attr_len > 0) {
		const char *attr_view = wiki_thread_buf_append_to_tokens(attr, attr_len);
		token_append_text_n(t, attr_view, attr_len);
	} else {
		token_append_text_n(t, "", 0);
	}
	if(inner && inner_len > 0) {
		const char *inner_view = wiki_thread_buf_append_to_tokens(inner, inner_len);
		token_append_text_n(t, inner_view, inner_len);
	} else {
		token_append_text_n(t, "", 0);
	}
	accum_push(accum, t);
	return t;
}

/* ── Regex compilation ───────────────────────────────────────────────────── */

static pcre2_code *compile_ext_regex(const ParserConfig *cfg, bool include_only) {
	const char *noinclude_re= include_only ? "includeonly" : "(?:no|only)include";
	const char *include_re= include_only ? "noinclude" : "includeonly";

	bool has_translate= false;
	for(size_t i= 0; i < cfg->ext.count; i++) {
		if(strcmp(cfg->ext.items[i], "translate") == 0) {
			has_translate= true;
			break;
		}
	}

	/* Build ext alternation mirroring JS newExt logic. */
	size_t exts_cap= 64;
	for(size_t i= 0; i < cfg->ext.count; i++) {
		const char *e= cfg->ext.items[i];
		if(has_translate && (strcmp(e, "translate") == 0 || strcmp(e, "tvar") == 0)) continue;
		exts_cap+= strlen(e) + 2;
	}
	char *exts= malloc(exts_cap);
	assert(exts);
	size_t ep= 0;
	bool first= true;
	for(size_t i= 0; i < cfg->ext.count; i++) {
		const char *e= cfg->ext.items[i];
		if(has_translate && (strcmp(e, "translate") == 0 || strcmp(e, "tvar") == 0)) continue;
		if(!first) exts[ep++]= '|';
		size_t elen= strlen(e);
		memcpy(exts + ep, e, elen);
		ep+= elen;
		first= false;
	}
	exts[ep]= '\0';

	size_t pat_cap= 256 + exts_cap + strlen(noinclude_re) * 4 + strlen(include_re) * 4;
	char *pattern= malloc(pat_cap);
	assert(pattern);
	size_t pos= 0;

	pos+= (size_t)snprintf(pattern + pos, pat_cap - pos,
												 "<!--[\\s\\S]*?(?:-->|$)"
												 "|<%s(?:\\s[^>]*)?\\/?>|<\\/%s\\s*>"
												 "|<(%s)(\\s[^>]*?)?(?:\\/>|>([\\s\\S]*?)<\\/(\\1\\s*)>)"
												 "|<(%s)(\\s[^>]*?)?(?:\\/>|>([\\s\\S]*?)(?:<\\/(%s\\s*)>|$))",
												 noinclude_re, noinclude_re,
												 exts,
												 include_re, include_re);

	free(exts);

	PCRE2_SIZE err_offset;
	int err_code;
	pcre2_code *re= pcre2_compile(
	(PCRE2_SPTR)pattern, PCRE2_ZERO_TERMINATED,
	PCRE2_CASELESS | PCRE2_UTF | PCRE2_UCP,
	&err_code, &err_offset, NULL);

	if(!re) {
		PCRE2_UCHAR8 err_buf[256];
		pcre2_get_error_message(err_code, err_buf, sizeof(err_buf));
		log_error("ext regex compile error at %zu: %s Pattern (truncated): %.200s",
							err_offset, err_buf, pattern);
	}
	free(pattern);
	return re;
}

static pcre2_code *compile_nowiki_regex(void) {
	const char *pattern= "<nowiki>[\\s\\S]*?<\\/nowiki>";
	PCRE2_SIZE err_offset;
	int err_code;
	return pcre2_compile((PCRE2_SPTR)pattern, PCRE2_ZERO_TERMINATED,
											 PCRE2_CASELESS | PCRE2_UTF | PCRE2_UCP,
											 &err_code, &err_offset, NULL);
}

static pcre2_code *compile_translate_regex(void) {
	const char *pattern= "<translate( nowrap)?>([\\s\\S]*?)<\\/translate>";
	PCRE2_SIZE err_offset;
	int err_code;
	return pcre2_compile((PCRE2_SPTR)pattern, PCRE2_ZERO_TERMINATED,
											 PCRE2_UTF | PCRE2_UCP,
											 &err_code, &err_offset, NULL);
}

static void apply_translate_prepass(ThreadBuf *tb, const ParserConfig *cfg, Accum *accum) {
	ParserConfig *mutable_cfg= (ParserConfig *)cfg;
	if(!mutable_cfg->regex_ext_translate) {
		mutable_cfg->regex_ext_translate= (ParserConfigRegex *)compile_nowiki_regex();
		if(!mutable_cfg->regex_ext_translate) return;
	}
	if(!mutable_cfg->regex_translate) {
		mutable_cfg->regex_translate= (ParserConfigRegex *)compile_translate_regex();
		if(!mutable_cfg->regex_translate) return;
	}

	pcre2_code *re_nowiki= (pcre2_code *)cfg->regex_ext_translate;
	pcre2_code *re_translate= (pcre2_code *)cfg->regex_translate;
	pcre2_match_data *md= pcre2_match_data_create_from_pattern(re_nowiki, NULL);
	if(!md) return;

	TextStack st;
	text_stack_init(&st);

	size_t out_cap= tb->len * 2 + 64;
	char *out= malloc(out_cap);
	assert(out);
	size_t out_len= 0;
	size_t search_at= 0;

#define ENSURE_PRE_CAP(buf, buflen, bufcap, need) \
	do {                                            \
		while((buflen) + (need) >= (bufcap)) {        \
			(bufcap)*= 2;                               \
			(buf)= realloc((buf), (bufcap));            \
			assert((buf));                              \
		}                                             \
	} while(0)

	while(search_at <= tb->len) {
		int rc= pcre2_match(re_nowiki, (PCRE2_SPTR)tb->buf, tb->len, search_at, 0, md, NULL);
		if(rc <= 0) {
			size_t rest= tb->len - search_at;
			ENSURE_PRE_CAP(out, out_len, out_cap, rest + 1);
			memcpy(out + out_len, tb->buf + search_at, rest);
			out_len+= rest;
			break;
		}
		PCRE2_SIZE *ov= pcre2_get_ovector_pointer(md);
		size_t ms= ov[0], me= ov[1];
		size_t before= ms - search_at;
		ENSURE_PRE_CAP(out, out_len, out_cap, before + 32);
		memcpy(out + out_len, tb->buf + search_at, before);
		out_len+= before;

		text_stack_push(&st, tb->buf + ms, me - ms);
		append_numeric_placeholder(out, &out_len, st.count - 1);

		search_at= me;
		if(me == ms) search_at++;
	}
	out[out_len]= '\0';
	wiki_thread_buf_set(tb, out, out_len);
	free(out);

	pcre2_match_data_free(md);
	md= pcre2_match_data_create_from_pattern(re_translate, NULL);
	if(!md) {
		text_stack_free(&st);
		return;
	}

	out_cap= tb->len * 2 + 64;
	out= malloc(out_cap);
	assert(out);
	out_len= 0;
	search_at= 0;

	while(search_at <= tb->len) {
		int rc= pcre2_match(re_translate, (PCRE2_SPTR)tb->buf, tb->len, search_at, 0, md, NULL);
		if(rc <= 0) {
			size_t rest= tb->len - search_at;
			ENSURE_PRE_CAP(out, out_len, out_cap, rest + 1);
			memcpy(out + out_len, tb->buf + search_at, rest);
			out_len+= rest;
			break;
		}
		PCRE2_SIZE *ov= pcre2_get_ovector_pointer(md);
		size_t ms= ov[0], me= ov[1];
		size_t before= ms - search_at;
		ENSURE_PRE_CAP(out, out_len, out_cap, before + 32);
		memcpy(out + out_len, tb->buf + search_at, before);
		out_len+= before;

		const char *attr= NULL;
		size_t attr_len= 0;
		if(rc > 1 && ov[2] != PCRE2_UNSET) {
			attr= tb->buf + ov[2];
			attr_len= ov[3] - ov[2];
		}
		const char *inner= "";
		size_t inner_len= 0;
		if(rc > 2 && ov[4] != PCRE2_UNSET) {
			inner= tb->buf + ov[4];
			inner_len= ov[5] - ov[4];
		}

		size_t restored_len= 0;
		char *restored_inner= str_restore(inner, inner_len,
																			(const char **)st.items,
																			st.count,
																			st.lens,
																			&restored_len);

		size_t tok_idx= accum->count;
		Token *tok= build_translate_token(attr, attr_len, restored_inner, restored_len, accum);
		if(tok) {
			char sent[64];
			size_t slen;
			work_str_sentinel(tok_idx, 'g', sent, &slen);
			ENSURE_PRE_CAP(out, out_len, out_cap, slen + 1);
			memcpy(out + out_len, sent, slen);
			out_len+= slen;
		} else {
			ENSURE_PRE_CAP(out, out_len, out_cap, me - ms + 1);
			memcpy(out + out_len, tb->buf + ms, me - ms);
			out_len+= me - ms;
		}
		free(restored_inner);

		search_at= me;
		if(me == ms) search_at++;
	}
	out[out_len]= '\0';

	size_t restored_all_len= 0;
	char *restored_all= str_restore(out, out_len,
																	(const char **)st.items,
																	st.count,
																	st.lens,
																	&restored_all_len);
	wiki_thread_buf_set(tb, restored_all, restored_all_len);
	free(restored_all);
	free(out);

	pcre2_match_data_free(md);
	text_stack_free(&st);

#undef ENSURE_PRE_CAP
}

/* ── onlyinclude handling (includeOnly mode) ─────────────────────────────── */

static bool handle_onlyinclude(ThreadBuf *tb, const ParserConfig *cfg, Accum *accum) {
	(void)cfg;
	const char *onlyinclude_open= "<onlyinclude>";
	const char *onlyinclude_close= "</onlyinclude>";
	size_t open_len= strlen(onlyinclude_open);
	size_t close_len= strlen(onlyinclude_close);

	const char *pos_open= find_substr_cs(tb->buf, tb->len, onlyinclude_open, open_len);
	if(!pos_open) return false;
	size_t after_open_len= tb->len - (size_t)(pos_open - tb->buf + open_len);
	const char *pos_close= find_substr_cs(pos_open + open_len, after_open_len, onlyinclude_close, close_len);
	if(!pos_close) return false;

	char *new_buf= malloc(tb->len * 3 + 256);
	assert(new_buf);
	size_t new_len= 0;

	const char *remaining= tb->buf;
	size_t remaining_len= tb->len;

	char sent_buf[64];

	while(1) {
		const char *next_open= find_substr_cs(remaining, remaining_len, onlyinclude_open, open_len);
		if(!next_open) break;

		size_t rel_open= (size_t)(next_open - remaining);
		size_t tail_after_open= remaining_len - rel_open - open_len;
		const char *next_close= find_substr_cs(next_open + open_len, tail_after_open, onlyinclude_close, close_len);
		if(!next_close) break;

		if(rel_open > 0) {
			size_t noincl_idx= accum->count;
			Token *ni= token_new(TOKEN_NOINCLUDE, "noinclude");
			if(rel_open > 0) {
				const char *rem_view = wiki_thread_buf_append_to_tokens(remaining, rel_open);
				token_append_text_n(ni, rem_view, rel_open);
			} else {
				token_append_text_n(ni, "", 0);
			}
			accum_push(accum, ni);
			size_t sent_len;
			work_str_sentinel(noincl_idx, 'n', sent_buf, &sent_len);
			memcpy(new_buf + new_len, sent_buf, sent_len);
			new_len+= sent_len;
		}

		const char *content_start= next_open + open_len;
		size_t content_len= (size_t)(next_close - content_start);
		size_t onlyi_idx= accum->count;
		Token *oi= token_new(TOKEN_ONLYINCLUDE, "onlyinclude");
		if(content_len > 0) {
			const char *cont_view = wiki_thread_buf_append_to_tokens(content_start, content_len);
			token_append_text_n(oi, cont_view, content_len);
		} else {
			token_append_text_n(oi, "", 0);
		}
		accum_push(accum, oi);
		size_t sent_len;
		work_str_sentinel(onlyi_idx, 'g', sent_buf, &sent_len);
		memcpy(new_buf + new_len, sent_buf, sent_len);
		new_len+= sent_len;

		remaining= next_close + close_len;
		remaining_len= tb->len - (size_t)(remaining - tb->buf);
	}

		if(remaining_len > 0) {
			size_t noincl_idx= accum->count;
			Token *ni= token_new(TOKEN_NOINCLUDE, "noinclude");
			const char *rem_view = wiki_thread_buf_append_to_tokens(remaining, remaining_len);
			token_append_text_n(ni, rem_view, remaining_len);
			accum_push(accum, ni);
			size_t sent_len;
			work_str_sentinel(noincl_idx, 'n', sent_buf, &sent_len);
			memcpy(new_buf + new_len, sent_buf, sent_len);
			new_len+= sent_len;
		}

	new_buf[new_len]= '\0';
	wiki_thread_buf_set(tb, new_buf, new_len);
	free(new_buf);
	return true;
}

/* ── Main parse function ─────────────────────────────────────────────────── */

void parse_comment_and_ext(ThreadBuf *tb, const ParserConfig *cfg,
													 Accum *accum, bool include_only) {
	bool has_translate= false;
	for(size_t i= 0; i < cfg->ext.count; i++) {
		if(strcmp(cfg->ext.items[i], "translate") == 0) {
			has_translate= true;
			break;
		}
	}

	if(include_only) {
		const char *oi_open= "<onlyinclude>";
		if(find_substr_cs(tb->buf, tb->len, oi_open, strlen(oi_open))) {
			if(handle_onlyinclude(tb, cfg, accum)) {
				return;
			}
		}
	}

	if(has_translate) {
		apply_translate_prepass(tb, cfg, accum);
	}

	int regex_idx= include_only ? 1 : 0;
	if(!cfg->regex_ext[regex_idx]) {
		ParserConfig *mutable_cfg= (ParserConfig *)cfg;
		mutable_cfg->regex_ext[regex_idx]=
		(ParserConfigRegex *)compile_ext_regex(cfg, include_only);
		if(!mutable_cfg->regex_ext[regex_idx]) return;
	}
	pcre2_code *re= (pcre2_code *)cfg->regex_ext[regex_idx];

	pcre2_match_data *md= pcre2_match_data_create_from_pattern(re, NULL);
	if(!md) return;

	size_t out_cap= tb->len * 2 + 64;
	char *out_buf= malloc(out_cap);
	assert(out_buf);
	size_t out_len= 0;
	size_t search_at= 0;

#define ENSURE_CAP(need)                  \
	do {                                    \
		while(out_len + (need) >= out_cap) {  \
			out_cap*= 2;                        \
			out_buf= realloc(out_buf, out_cap); \
			assert(out_buf);                    \
		}                                     \
	} while(0)

	while(search_at <= tb->len) {
		int rc= pcre2_match(re, (PCRE2_SPTR)tb->buf, tb->len,
												search_at, 0, md, NULL);
		if(rc <= 0) {
			size_t rest= tb->len - search_at;
			ENSURE_CAP(rest + 1);
			memcpy(out_buf + out_len, tb->buf + search_at, rest);
			out_len+= rest;
			break;
		}

		PCRE2_SIZE *ov= pcre2_get_ovector_pointer(md);
		size_t match_start= ov[0];
		size_t match_end= ov[1];

		size_t before= match_start - search_at;
		ENSURE_CAP(before + 64);
		memcpy(out_buf + out_len, tb->buf + search_at, before);
		out_len+= before;

		const char *substr= tb->buf + match_start;
		size_t sub_len= match_end - match_start;

		Token *tok= NULL;
		char ch= 'n';

		size_t ext_name_s= (rc > 1 && ov[2] != PCRE2_UNSET) ? ov[2] : 0;
		size_t ext_name_e= (rc > 1 && ov[3] != PCRE2_UNSET) ? ov[3] : 0;

		size_t inc_name_s= (rc > 5 && ov[10] != PCRE2_UNSET) ? ov[10] : 0;
		size_t inc_name_e= (rc > 5 && ov[11] != PCRE2_UNSET) ? ov[11] : 0;

		if(substr[0] == '<' && sub_len >= 4 &&
			 substr[1] == '!' && substr[2] == '-' && substr[3] == '-') {
			size_t restored_len= 0;
			char *restored= restore_accum_mode(substr, sub_len, accum, 1, &restored_len);
			tok= build_comment_token(restored, restored_len, accum);
			free(restored);
			ch= 'c';

		} else if(ext_name_s < ext_name_e) {
			const char *name= tb->buf + ext_name_s;
			size_t name_len= ext_name_e - ext_name_s;

			size_t attr_s= (rc > 2 && ov[4] != PCRE2_UNSET) ? ov[4] : 0;
			size_t attr_e= (rc > 2 && ov[5] != PCRE2_UNSET) ? ov[5] : 0;
			size_t inner_s= (rc > 3 && ov[6] != PCRE2_UNSET) ? ov[6] : 0;
			size_t inner_e= (rc > 3 && ov[7] != PCRE2_UNSET) ? ov[7] : 0;
			size_t close_s= (rc > 4 && ov[8] != PCRE2_UNSET) ? ov[8] : 0;
			size_t close_e= (rc > 4 && ov[9] != PCRE2_UNSET) ? ov[9] : 0;

			const char *attr= (attr_s < attr_e) ? tb->buf + attr_s : NULL;
			size_t alen= (attr_s < attr_e) ? attr_e - attr_s : 0;
			const char *inner= (inner_s < inner_e) ? tb->buf + inner_s : NULL;
			size_t ilen= (inner_s < inner_e) ? inner_e - inner_s : 0;
			bool self_closing= !(close_s < close_e);

			tok= build_ext_token(name, name_len, attr, alen, inner, ilen, self_closing, cfg, accum);
			ch= 'e';

		} else if(inc_name_s < inc_name_e) {
			const char *name= tb->buf + inc_name_s;
			size_t name_len= inc_name_e - inc_name_s;

			size_t attr_s= (rc > 6 && ov[12] != PCRE2_UNSET) ? ov[12] : 0;
			size_t attr_e= (rc > 6 && ov[13] != PCRE2_UNSET) ? ov[13] : 0;
			size_t inner_s= (rc > 7 && ov[14] != PCRE2_UNSET) ? ov[14] : 0;
			size_t inner_e= (rc > 7 && ov[15] != PCRE2_UNSET) ? ov[15] : 0;
			size_t close_s= (rc > 8 && ov[16] != PCRE2_UNSET) ? ov[16] : 0;
			size_t close_e= (rc > 8 && ov[17] != PCRE2_UNSET) ? ov[17] : 0;

			const char *attr= (attr_s < attr_e) ? tb->buf + attr_s : NULL;
			size_t alen= (attr_s < attr_e) ? attr_e - attr_s : 0;
			const char *inner= (inner_s < inner_e) ? tb->buf + inner_s : NULL;
			size_t ilen= (inner_s < inner_e) ? inner_e - inner_s : 0;
			const char *closing= (close_s < close_e) ? tb->buf + close_s : NULL;
			size_t clen= (close_s < close_e) ? close_e - close_s : 0;

			size_t rattr_len= 0, rinner_len= 0;
			char *rattr= NULL;
			char *rinner= NULL;
			if(attr && alen > 0) {
				rattr= restore_accum_mode(attr, alen, accum, 1, &rattr_len);
			}
			if(inner && ilen > 0) {
				rinner= restore_accum_mode(inner, ilen, accum, 1, &rinner_len);
			}

			tok= build_include_token(name, name_len,
															 rattr ? rattr : attr, rattr ? rattr_len : alen,
															 rinner ? rinner : inner, rinner ? rinner_len : ilen,
															 closing, clen, accum);
			free(rattr);
			free(rinner);
			ch= 'n';

		} else {
			tok= build_noinclude_token(substr, sub_len, accum);
			ch= 'n';
		}

		if(tok) {
			size_t tok_idx= accum->count - 1; /* token was last pushed */
			char sent_buf[64];
			size_t sent_len;
			work_str_sentinel(tok_idx, ch, sent_buf, &sent_len);
			ENSURE_CAP(sent_len);
			memcpy(out_buf + out_len, sent_buf, sent_len);
			out_len+= sent_len;
		} else {
			ENSURE_CAP(sub_len);
			memcpy(out_buf + out_len, substr, sub_len);
			out_len+= sub_len;
		}

		search_at= match_end;
		if(match_end == match_start) search_at++;
	}

	out_buf[out_len]= '\0';
	wiki_thread_buf_set(tb, out_buf, out_len);
	free(out_buf);

	pcre2_match_data_free(md);
}
