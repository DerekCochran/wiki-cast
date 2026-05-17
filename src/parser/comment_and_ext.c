#include "util/log.h"
#include "build.h"
#include "parser/braces.h"
#include "parser/comment_and_ext.h"
#include "parser/link.h"
#include "parser/links.h"
#include "util/string_util.h"
#include "util/thread_buffer.h"
#include "util/callback_parser.h"
#include "util/wiki_parser_rules.h"
#include <stringzilla/stringzilla.h>
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    CAE_MATCH_COMMENT = 1,
    CAE_MATCH_NOINCLUDE_SINGLE = 2,
    CAE_MATCH_EXT = 3,
    CAE_MATCH_INCLUDE = 4,
} CaeMatchKind;

typedef struct {
    CaeMatchKind kind;
    size_t mstart, mend;

    size_t name_s, name_e;
    size_t attr_s, attr_e;
    size_t inner_s, inner_e;
    size_t close_s, close_e;

    bool has_attr;
    bool has_inner;
    bool has_close;
} CaeScanMatch;

typedef struct {
    size_t name_s, name_e;
    size_t attr_s, attr_e;
    size_t open_end;
    bool has_attr;
    bool self_closing;
} CaeOpenTag;

static const char *find_substr_cs(const char *hay, size_t hlen,
								const char *needle, size_t nlen) {
	if(!hay || !needle || nlen == 0 || nlen > hlen) return NULL;
	return sz_find(hay, hlen, needle, nlen);
}

static inline bool cae_tag_name_boundary(unsigned char c) {
    return c == '>' || c == '/' || isspace(c);
}

static bool cae_ci_eq_n(const char *a, const char *b, size_t n) {
    for(size_t i = 0; i < n; i++) {
        if(tolower((unsigned char)a[i]) != tolower((unsigned char)b[i])) return false;
    }
    return true;
}

static bool cae_match_open_named(const char *s, size_t len, size_t i,
                                 const char *name, size_t name_len,
                                 CaeOpenTag *out) {
    if(!s || !name || !out || i + 1 >= len) return false;
    if(s[i] != '<' || s[i + 1] == '/') return false;

    size_t p = i + 1;
    if(p + name_len > len) return false;
    if(!cae_ci_eq_n(s + p, name, name_len)) return false;
    p += name_len;
    if(p >= len || !cae_tag_name_boundary((unsigned char)s[p])) return false;

    out->name_s = i + 1;
    out->name_e = i + 1 + name_len;
    out->has_attr = false;
    out->attr_s = out->attr_e = 0;
    out->self_closing = false;
    out->open_end = 0;

    if(isspace((unsigned char)s[p])) {
        out->has_attr = true;
        out->attr_s = p;
        for(size_t q = p + 1; q < len; q++) {
            if(s[q] == '/' && q + 1 < len && s[q + 1] == '>') {
                out->attr_e = q;
                out->self_closing = true;
                out->open_end = q + 2;
                return true;
            }
            if(s[q] == '>') {
                out->attr_e = q;
                out->self_closing = false;
                out->open_end = q + 1;
                return true;
            }
        }
        return false;
    }

    if(s[p] == '/' && p + 1 < len && s[p + 1] == '>') {
        out->self_closing = true;
        out->open_end = p + 2;
        return true;
    }
    if(s[p] == '>') {
        out->self_closing = false;
        out->open_end = p + 1;
        return true;
    }
    return false;
}

static bool cae_match_close_named(const char *s, size_t len, size_t i,
                                  const char *name, size_t name_len,
                                  size_t *mend) {
    if(!s || !name || !mend || i + 2 >= len) return false;
    if(s[i] != '<' || s[i + 1] != '/') return false;

    size_t p = i + 2;
    if(p + name_len > len) return false;
    if(!cae_ci_eq_n(s + p, name, name_len)) return false;
    p += name_len;

    while(p < len && isspace((unsigned char)s[p])) p++;
    if(p < len && s[p] == '>') {
        *mend = p + 1;
        return true;
    }
    return false;
}

static bool cae_find_close_named(const char *s, size_t len, size_t from,
                                 const char *name, size_t name_len,
                                 size_t *close_tag_s,
                                 size_t *close_name_s, size_t *close_name_e,
                                 size_t *close_tag_e) {
    if(!s || !name || !close_tag_s || !close_name_s || !close_name_e || !close_tag_e)
        return false;

    for(size_t q = from; q + 2 + name_len <= len; q++) {
        if(s[q] != '<' || s[q + 1] != '/') continue;
        size_t n0 = q + 2;
        if(!cae_ci_eq_n(s + n0, name, name_len)) continue;

        size_t r = n0 + name_len;
        while(r < len && isspace((unsigned char)s[r])) r++;
        if(r < len && s[r] == '>') {
            *close_tag_s = q;
            *close_name_s = n0;
            *close_name_e = r;
            *close_tag_e = r + 1;
            return true;
        }
    }
    return false;
}

static bool cae_match_comment(const char *s, size_t len, size_t i, CaeScanMatch *m) {
    if(!s || !m || i + 4 > len) return false;
    if(!(s[i] == '<' && s[i + 1] == '!' && s[i + 2] == '-' && s[i + 3] == '-')) return false;

    const char *tail = s + i + 4;
    size_t rem = len - (i + 4);
    const char *close = find_substr_cs(tail, rem, "-->", 3);

    memset(m, 0, sizeof(*m));
    m->kind = CAE_MATCH_COMMENT;
    m->mstart = i;
    m->mend = close ? (size_t)((close - s) + 3) : len;
    return true;
}

static bool cae_match_noinclude_single(const char *s, size_t len, size_t i,
                                       bool include_only, CaeScanMatch *m) {
    const char *n1 = include_only ? "includeonly" : "noinclude";
    const char *n2 = include_only ? NULL : "onlyinclude";
    size_t n1_len = strlen(n1);
    size_t n2_len = n2 ? strlen(n2) : 0;

    CaeOpenTag ot;
    size_t mend = 0;

    if(cae_match_open_named(s, len, i, n1, n1_len, &ot)
       || cae_match_close_named(s, len, i, n1, n1_len, &mend)
       || (n2 && (cae_match_open_named(s, len, i, n2, n2_len, &ot)
                  || cae_match_close_named(s, len, i, n2, n2_len, &mend)))) {
        memset(m, 0, sizeof(*m));
        m->kind = CAE_MATCH_NOINCLUDE_SINGLE;
        m->mstart = i;
        m->mend = (mend > 0) ? mend : ot.open_end;
        return true;
    }

    return false;
}

static bool cae_match_ext(const char *s, size_t len, size_t i,
                          const ParserConfig *cfg, bool has_translate,
                          CaeScanMatch *m) {
    if(!s || !cfg || !m || i + 1 >= len) return false;
    if(s[i] != '<' || s[i + 1] == '/') return false;

    for(size_t ei = 0; ei < cfg->ext.count; ei++) {
        const char *ename = cfg->ext.items[ei];
        if(!ename) continue;
        if(has_translate && (strcmp(ename, "translate") == 0 || strcmp(ename, "tvar") == 0))
            continue;

        size_t ename_len = strlen(ename);
        CaeOpenTag ot;
        if(!cae_match_open_named(s, len, i, ename, ename_len, &ot)) continue;

        memset(m, 0, sizeof(*m));
        m->kind = CAE_MATCH_EXT;
        m->mstart = i;
        m->name_s = ot.name_s;
        m->name_e = ot.name_e;
        m->has_attr = ot.has_attr;
        m->attr_s = ot.attr_s;
        m->attr_e = ot.attr_e;

        if(ot.self_closing) {
            m->mend = ot.open_end;
            return true;
        }

        size_t close_tag_s = 0, close_name_s = 0, close_name_e = 0, close_tag_e = 0;
        if(!cae_find_close_named(s, len, ot.open_end, ename, ename_len,
                                 &close_tag_s, &close_name_s, &close_name_e, &close_tag_e)) {
            continue;
        }

        m->has_inner = true;
        m->inner_s = ot.open_end;
        m->inner_e = close_tag_s;
        m->has_close = true;
        m->close_s = close_name_s;
        m->close_e = close_name_e;
        m->mend = close_tag_e;
        return true;
    }

    return false;
}

static bool cae_match_include(const char *s, size_t len, size_t i,
                              bool include_only, CaeScanMatch *m) {
    const char *name = include_only ? "noinclude" : "includeonly";
    size_t name_len = strlen(name);

    CaeOpenTag ot;
    if(!cae_match_open_named(s, len, i, name, name_len, &ot)) return false;

    memset(m, 0, sizeof(*m));
    m->kind = CAE_MATCH_INCLUDE;
    m->mstart = i;
    m->name_s = ot.name_s;
    m->name_e = ot.name_e;
    m->has_attr = ot.has_attr;
    m->attr_s = ot.attr_s;
    m->attr_e = ot.attr_e;

    if(ot.self_closing) {
        m->mend = ot.open_end;
        return true;
    }

    size_t close_tag_s = 0, close_name_s = 0, close_name_e = 0, close_tag_e = 0;
    if(cae_find_close_named(s, len, ot.open_end, name, name_len,
                            &close_tag_s, &close_name_s, &close_name_e, &close_tag_e)) {
        m->has_inner = true;
        m->inner_s = ot.open_end;
        m->inner_e = close_tag_s;
        m->has_close = true;
        m->close_s = close_name_s;
        m->close_e = close_name_e;
        m->mend = close_tag_e;
        return true;
    }

    /* JS includeRegex allows unclosed EOF via ...(?:</(name\s*)>|$). */
    m->has_inner = true;
    m->inner_s = ot.open_end;
    m->inner_e = len;
    m->has_close = false;
    m->mend = len;
    return true;
}

static bool cae_find_next_match(const char *s, size_t len, size_t at,
                                const ParserConfig *cfg, bool include_only,
                                bool has_translate, CaeScanMatch *m) {
    if(!s || !cfg || !m) return false;

    for(size_t i = at; i < len; i++) {
        /* Keep JS alternation order exactly:
         * 1) comment
         * 2) noincludeRegex single-tag
         * 3) dynamic ext
         * 4) includeRegex
         */
        if(cae_match_comment(s, len, i, m)) return true;
        if(cae_match_noinclude_single(s, len, i, include_only, m)) return true;
        if(cae_match_ext(s, len, i, cfg, has_translate, m)) return true;
        if(cae_match_include(s, len, i, include_only, m)) return true;
    }
    return false;
}

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
	const char *view = wiki_thread_buf_append_to_tokens(s, len);
	st->items[st->count]= (char *)view;
	st->lens[st->count]= len;
	st->count++;
}

static void text_stack_free(TextStack *st) {
	/* Items are views into the tokens arena (append-only); do not free them. */
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

/* Restore a sentinel-marked string using an external stack into a ThreadBuf.
 * Mirrors `str_restore` but writes into `tb` (scratch) instead of allocating.
 * Returns number of bytes appended to `tb` (new tb->len).
 */
static size_t str_restore_to_tb(const char *s, size_t len,
								const char **stack, size_t stack_count,
								const size_t *stack_lengths,
								ThreadBuf *tb) {
	if(!s || len == 0) return 0;
	const char *p = s;
	const char *end = s + len;
	char needle = '\0';

	while(p < end) {
		const char *found = sz_find_byte(p, (size_t)(end - p), &needle);
		if(!found) {
			size_t rem = (size_t)(end - p);
			if(rem) {
				wiki_thread_buf_reserve(tb, tb->len + rem);
				memcpy(tb->buf + tb->len, p, rem);
				tb->len += rem;
			}
			break;
		}

		size_t seg = (size_t)(found - p);
		if(seg) {
			wiki_thread_buf_reserve(tb, tb->len + seg);
			memcpy(tb->buf + tb->len, p, seg);
			tb->len += seg;
		}

		const char *k = found + 1;
		while(k < end && *k >= '0' && *k <= '9') k++;
		if(k < end && k > found + 1 && (unsigned char)*k == '\x7F') {
			size_t idx = 0;
			for(const char *d = found + 1; d < k; ++d) idx = idx * 10 + (size_t)(*d - '0');
			if(idx < stack_count && stack[idx]) {
				const char *rep = stack[idx];
				size_t replen = (stack_lengths && stack_lengths[idx]) ? stack_lengths[idx] : strlen(rep);
				if(replen) {
					wiki_thread_buf_reserve(tb, tb->len + replen);
					memcpy(tb->buf + tb->len, rep, replen);
					tb->len += replen;
				}
				p = k + 1;
				continue;
			}
		}

		/* Fallback: emit literal NUL byte */
		wiki_thread_buf_reserve(tb, tb->len + 1);
		tb->buf[tb->len++] = *found;
		p = found + 1;
	}
	tb->buf[tb->len] = '\0';
	return tb->len;
}

/* Restore accumulator-mode sentinels into a provided ThreadBuf (scratch).
 * Expands \0<index><ch>\x7F markers where `ch` determines behavior
 * (mode==1 -> expand 'g'; mode==2 -> expand 'n').
 * Returns number of bytes appended to `tb`.
 */
static size_t restore_accum_mode_to_tb(const char *s, size_t len,
									   const Accum *accum, int mode,
									   ThreadBuf *tb) {
	if(!s || len == 0) return 0;
	size_t before = tb->len;

	for(size_t i = 0; i < len;) {
		if((unsigned char)s[i] == '\0') {
			size_t k = i + 1;
			while(k < len && s[k] >= '0' && s[k] <= '9') k++;
			if(k > i + 1 && k + 1 < len && (unsigned char)s[k + 1] == '\x7F') {
				char ch = s[k];
				bool should_expand = (mode == 1 && ch == 'g') || (mode == 2 && ch == 'n');
				if(should_expand) {
					size_t idx = 0;
					for(size_t d = i + 1; d < k; d++) idx = idx * 10 + (size_t)(s[d] - '0');
					Token *ref = accum_get(accum, idx);
					if(ref) {
						ThreadBuf *tmp = wiki_thread_buf_acquire_scratch();
						if(!tmp) { log_fatal("thread_buffer: failed to acquire scratch in restore_accum_mode_to_tb"); abort(); }
						char *rep_s = token_to_string(ref, tmp);
						size_t rep_len = tmp->len;

						if(mode == 1 && ch == 'g') {
							/* Nested expansion into tb */
							restore_accum_mode_to_tb(rep_s, rep_len, accum, 2, tb);
						} else {
							if(rep_len) {
								wiki_thread_buf_reserve(tb, tb->len + rep_len);
								memcpy(tb->buf + tb->len, rep_s, rep_len);
								tb->len += rep_len;
							}
						}

						wiki_thread_buf_release_scratch(tmp);
						i = k + 2;
						continue;
					}
				}
			}
		}

		wiki_thread_buf_reserve(tb, tb->len + 1);
		tb->buf[tb->len++] = s[i++];
	}

	tb->buf[tb->len] = '\0';
	return tb->len - before;
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
		/* Prepend a space using a scratch buffer to avoid a transient heap alloc. */
		ThreadBuf *scratch = wiki_thread_buf_acquire_scratch();
		if(!scratch) { log_fatal("thread_buffer: failed to acquire scratch in build_ext_attrs"); abort(); }
		wiki_thread_buf_reserve(scratch, attr_len + 1);
		scratch->buf[0]= ' ';
		memcpy(scratch->buf + 1, attr_str, attr_len);
		scratch->len = attr_len + 1;
		scratch->buf[scratch->len] = '\0';
		parse_ext_attrs(t, scratch->buf, scratch->len, tag_name, accum);
		wiki_thread_buf_release_scratch(scratch);
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

	ThreadBuf *tmp = wiki_thread_buf_acquire_scratch_from_data(inner_str, inner_len);
	if(!tmp) { log_fatal("thread_buffer: failed to acquire scratch in build_references_inner_token"); abort(); }

	parse_comment_and_ext(tmp, cfg, accum, false);
	parse_braces(tmp, cfg, accum);

	ThreadBuf *out_tb = wiki_thread_buf_acquire_scratch();
	if(!out_tb) { log_fatal("thread_buffer: failed to acquire scratch in build_references_inner_token (out)"); abort(); }
	wiki_thread_buf_reserve(out_tb, tmp->len * 2 + 64);

#define ENSURE_REF_CAP(tb, need) do { wiki_thread_buf_reserve((tb), (tb)->len + (need)); } while(0)

	for(size_t i= 0; i < tmp->len;) {
		if((unsigned char)tmp->buf[i] == '\0') {
			size_t k= i + 1;
			while(k < tmp->len && tmp->buf[k] >= '0' && tmp->buf[k] <= '9') k++;
			if(k > i + 1 && k + 1 < tmp->len && (unsigned char)tmp->buf[k + 1] == '\x7F') {
				size_t mlen= (k + 2) - i;
				ENSURE_REF_CAP(out_tb, mlen + 1);
				memcpy(out_tb->buf + out_tb->len, tmp->buf + i, mlen);
				out_tb->len += mlen;
				i= k + 2;
				continue;
			}
		}

		size_t j= i;
		while(j < tmp->len && (unsigned char)tmp->buf[j] != '\0') j++;
		size_t run_len= j - i;
		if(run_len > 0) {
			Token *ni= token_new(TOKEN_NOINCLUDE, "noinclude");
			if(ni) {
				const char *ni_view = wiki_thread_buf_append_to_tokens(tmp->buf + i, run_len);
				token_append_text_n(ni, ni_view, run_len);
				accum_push(accum, ni);
				size_t idx= accum->count - 1;
				char sent[64];
				size_t slen;
				work_str_sentinel(idx, 'n', sent, &slen);
				ENSURE_REF_CAP(out_tb, slen + 1);
				memcpy(out_tb->buf + out_tb->len, sent, slen);
				out_tb->len += slen;
			}
		}
		i= j;
	}

#undef ENSURE_REF_CAP

	build_from_str(t, out_tb->buf, out_tb->len, accum);

	wiki_thread_buf_release_scratch(out_tb);
	wiki_thread_buf_release_scratch(tmp);
	return t;
}

static Token *parse_gallery_image_line_local(const char *line, size_t line_len,
																			const ParserConfig *cfg,
																			Accum *accum) {
	if(!line || line_len == 0) return NULL;

	ThreadBuf *tmp_tb = wiki_thread_buf_acquire_scratch();
	if(!tmp_tb) { log_fatal("thread_buffer: failed to acquire scratch in parse_gallery_image_line_local"); abort(); }
	wiki_thread_buf_reserve(tmp_tb, line_len + 9);
	tmp_tb->buf[0]= '[';
	tmp_tb->buf[1]= '[';
	memcpy(tmp_tb->buf + 2, "File:", 5);
	memcpy(tmp_tb->buf + 7, line, line_len);
	tmp_tb->buf[7 + line_len]= ']';
	tmp_tb->buf[8 + line_len]= ']';
	tmp_tb->buf[9 + line_len]= '\0';
	tmp_tb->len= line_len + 9;

	parse_braces(tmp_tb, cfg, accum);
	parse_links(tmp_tb, cfg, accum, NULL, false);

	Token *tmp= token_new(TOKEN_PLAIN, "gallery-line");
	if(!tmp) {
		wiki_thread_buf_release_scratch(tmp_tb);
		return NULL;
	}
	build_from_str(tmp, tmp_tb->buf, tmp_tb->len, accum);
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
	wiki_thread_buf_release_scratch(tmp_tb);
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

	ThreadBuf *tmp_tb = wiki_thread_buf_acquire_scratch();
	if(!tmp_tb) { log_fatal("thread_buffer: failed to acquire scratch in parse_imagemap_image_line_local"); abort(); }
	wiki_thread_buf_reserve(tmp_tb, line_len + 5);
	tmp_tb->buf[0]= '[';
	tmp_tb->buf[1]= '[';
	memcpy(tmp_tb->buf + 2, line, line_len);
	tmp_tb->buf[2 + line_len]= ']';
	tmp_tb->buf[3 + line_len]= ']';
	tmp_tb->buf[4 + line_len]= '\0';
	tmp_tb->len= line_len + 4;

	parse_braces(tmp_tb, cfg, accum);
	parse_links(tmp_tb, cfg, accum, NULL, false);

	Token *tmp= token_new(TOKEN_PLAIN, "imagemap-image-line");
	if(!tmp) {
		wiki_thread_buf_release_scratch(tmp_tb);
		return NULL;
	}
	build_from_str(tmp, tmp_tb->buf, tmp_tb->len, accum);
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
	wiki_thread_buf_release_scratch(tmp_tb);
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



typedef struct {
	TextStack *st;
	ThreadBuf *out;
} NowikiScanCtx;

static void nowiki_scan_cb(const char *segment, size_t len,
						   ParserSegmentKind kind, void *user_data) {
	NowikiScanCtx *ctx = (NowikiScanCtx *)user_data;
	if (kind == PARSER_SEG_TEXT) {
		wiki_thread_buf_append(ctx->out, (sz_string_view_t){ .start = segment, .length = len });
		return;
	}
	/* matched inner or self-closing nowiki: stash and emit numeric placeholder */
	text_stack_push(ctx->st, segment, len);
	wiki_thread_buf_reserve(ctx->out, ctx->out->len + 32);
	append_numeric_placeholder(ctx->out->buf, &ctx->out->len, ctx->st->count - 1);
}



typedef struct {
	Accum     *accum;
	ThreadBuf *out;
	TextStack *st;            /* nowiki stack to allow restoration */
	const char *buf;          /* base buffer pointer (for rfind) */
	const ParserRules *rules; /* pointer to rule_translate */
} TranslateScanCtx;

static void translate_scan_cb_wrap(const char *segment, size_t len,
								   ParserSegmentKind kind, void *user_data) {
	TranslateScanCtx *ctx = (TranslateScanCtx *)user_data;
	const ParserRules *r = ctx->rules;

	if (kind == PARSER_SEG_TEXT) {
		wiki_thread_buf_append(ctx->out, (sz_string_view_t){ .start = segment, .length = len });
		return;
	}

	/* Compute attribute span: bytes between end of open_delim and the '>' terminator (exclusive). */
	const char *attr_ptr = NULL;
	size_t attr_len = 0;

	if (kind == PARSER_SEG_SELF_CLOSING) {
		/* segment is the full opener span (open + attrs + terminator) */
		if (len > r->open_len + 1) {
			attr_ptr = segment + r->open_len;
			attr_len = (size_t)(len - r->open_len - 1);
		}
	} else {
		/* PARSER_SEG_INNER: segment points at first byte after '>' (inner_start).
		 * Find the last occurrence of open_delim before segment within the
		 * original buffer to locate the opener and its attributes. */
		if (segment > ctx->buf && (size_t)(segment - ctx->buf) >= r->open_len) {
			size_t hay_len = (size_t)(segment - ctx->buf);
			const char *open = (const char *)sz_rfind(ctx->buf, hay_len, r->open_delim, r->open_len);
			if (open) {
				attr_ptr = open + r->open_len;
				/* terminator is at (segment - 1) */
				if ((const char *)segment > attr_ptr) {
					attr_len = (size_t)((segment - 1) - attr_ptr);
				}
			}
		}
	}

	/* Restore inner content (expand nowiki placeholders) into a scratch buffer. */
	ThreadBuf *tmp_restore = wiki_thread_buf_acquire_scratch();
	if(!tmp_restore) { log_fatal("thread_buffer: failed to acquire scratch in translate_scan_cb_wrap"); abort(); }
	tmp_restore->len = 0;

	const char *inner_ptr = NULL;
	size_t inner_len = 0;
	if (kind == PARSER_SEG_INNER && len > 0) {
		str_restore_to_tb(segment, len, (const char **)ctx->st->items, ctx->st->count, ctx->st->lens, tmp_restore);
		inner_ptr = tmp_restore->buf;
		inner_len = tmp_restore->len;
	}

	/* Build Translate token (attr may be NULL/empty). */
	size_t tok_idx = ctx->accum->count;
	Token *tok = build_translate_token(attr_ptr, attr_len, inner_ptr, inner_len, ctx->accum);

	if (tmp_restore) wiki_thread_buf_release_scratch(tmp_restore);

	if (tok) {
		char sent[64]; size_t slen;
		work_str_sentinel(tok_idx, 'g', sent, &slen);
		wiki_thread_buf_reserve(ctx->out, ctx->out->len + slen);
		memcpy(ctx->out->buf + ctx->out->len, sent, slen);
		ctx->out->len += slen;
	} else {
		/* Fallback: emit original matched bytes as-is. For self-closing the
		 * original bytes are the segment span; for INNER the bytes include the
		 * full matched span (open..close) but parser gives only inner. Emit
		 * inner bytes in that case. */
		if (kind == PARSER_SEG_SELF_CLOSING) {
			wiki_thread_buf_append(ctx->out, (sz_string_view_t){ .start = segment, .length = len });
		} else {
			wiki_thread_buf_append(ctx->out, (sz_string_view_t){ .start = segment, .length = len });
		}
	}
}

static void apply_translate_prepass(ThreadBuf *tb, const ParserConfig *cfg, Accum *accum) {
	TextStack st;
	text_stack_init(&st);

	/* Pass 1: handle paired <nowiki>...</nowiki> */
	ThreadBuf *out_tb = wiki_thread_buf_acquire_scratch();
	if(!out_tb) { log_fatal("thread_buffer: failed to acquire scratch in apply_translate_prepass (pass1)"); abort(); }
	wiki_thread_buf_reserve(out_tb, tb->len * 2 + 64);
	out_tb->len = 0;

	NowikiScanCtx ctx = { .st = &st, .out = out_tb };
	parser_scan(tb->buf, tb->len, &wiki_rule_nowiki_paired, nowiki_scan_cb, &ctx);

	out_tb->buf[out_tb->len]= '\0';
	wiki_thread_buf_set(tb, out_tb->buf, out_tb->len);
	wiki_thread_buf_release_scratch(out_tb);

	/* Pass 2: handle self-closing <nowiki ... /> forms */
	out_tb = wiki_thread_buf_acquire_scratch();
	if(!out_tb) { log_fatal("thread_buffer: failed to acquire scratch in apply_translate_prepass (pass2)"); abort(); }
	wiki_thread_buf_reserve(out_tb, tb->len * 2 + 64);
	out_tb->len = 0;

	ctx.out = out_tb;
	parser_scan(tb->buf, tb->len, &wiki_rule_nowiki_sc, nowiki_scan_cb, &ctx);

	out_tb->buf[out_tb->len]= '\0';
	wiki_thread_buf_set(tb, out_tb->buf, out_tb->len);
	wiki_thread_buf_release_scratch(out_tb);

	/* Re-acquire an output scratch buffer for the translate pass. */
	out_tb = wiki_thread_buf_acquire_scratch();
	if(!out_tb) { log_fatal("thread_buffer: failed to acquire scratch in apply_translate_prepass (translate pass)"); abort(); }
	wiki_thread_buf_reserve(out_tb, tb->len * 2 + 64);
	out_tb->len = 0;

	TranslateScanCtx tctx = {
		.accum = accum,
		.out   = out_tb,
		.st    = &st,
		.buf   = tb->buf,
		.rules = &wiki_rule_translate,
	};

	parser_scan(tb->buf, tb->len, &wiki_rule_translate, translate_scan_cb_wrap, &tctx);

	out_tb->buf[out_tb->len]= '\0';

	/* Use scratch-based restore into tb directly */
	ThreadBuf *tmp_all = wiki_thread_buf_acquire_scratch();
	if(!tmp_all) { log_fatal("thread_buffer: failed to acquire scratch in apply_translate_prepass (final restore)\n"); abort(); }
	tmp_all->len = 0;
	str_restore_to_tb(out_tb->buf, out_tb->len, (const char **)st.items, st.count, st.lens, tmp_all);
	wiki_thread_buf_set(tb, tmp_all->buf, tmp_all->len);
	wiki_thread_buf_release_scratch(tmp_all);
	wiki_thread_buf_release_scratch(out_tb);

	text_stack_free(&st);
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

	ThreadBuf *new_tb = wiki_thread_buf_acquire_scratch();
	if(!new_tb) { log_fatal("thread_buffer: failed to acquire scratch in handle_onlyinclude"); abort(); }
	wiki_thread_buf_reserve(new_tb, tb->len * 3 + 256);
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
			memcpy(new_tb->buf + new_len, sent_buf, sent_len);
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
			memcpy(new_tb->buf + new_len, sent_buf, sent_len);
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
			memcpy(new_tb->buf + new_len, sent_buf, sent_len);
			new_len+= sent_len;
		}

	new_tb->buf[new_len]= '\0';
	wiki_thread_buf_set(tb, new_tb->buf, new_len);
	wiki_thread_buf_release_scratch(new_tb);
	return true;
}

/* ── Main parse function ─────────────────────────────────────────────────── */

void parse_comment_and_ext(ThreadBuf *tb, const ParserConfig *cfg,
                           Accum *accum, bool include_only) {
    bool has_translate = false;
    for(size_t i = 0; i < cfg->ext.count; i++) {
        if(strcmp(cfg->ext.items[i], "translate") == 0) {
            has_translate = true;
            break;
        }
    }

    if(include_only) {
        const char *oi_open = "<onlyinclude>";
        if(find_substr_cs(tb->buf, tb->len, oi_open, strlen(oi_open))) {
            if(handle_onlyinclude(tb, cfg, accum)) {
                return;
            }
        }
    }

    if(has_translate) {
        apply_translate_prepass(tb, cfg, accum);
    }

    ThreadBuf *out_tb = wiki_thread_buf_acquire_scratch();
    if(!out_tb) { log_fatal("thread_buffer: failed to acquire scratch in parse_comment_and_ext"); abort(); }
    wiki_thread_buf_reserve(out_tb, tb->len * 2 + 64);

#define ENSURE_CAP(need) do { wiki_thread_buf_reserve(out_tb, out_tb->len + (need)); } while(0)
    out_tb->len = 0;
    size_t search_at = 0;

    CaeScanMatch mm;
    while(cae_find_next_match(tb->buf, tb->len, search_at, cfg, include_only, has_translate, &mm)) {
        if(mm.mend <= mm.mstart) break;

        size_t before = mm.mstart - search_at;
        ENSURE_CAP(before + 64);
        memcpy(out_tb->buf + out_tb->len, tb->buf + search_at, before);
        out_tb->len += before;

        const char *substr = tb->buf + mm.mstart;
        size_t sub_len = mm.mend - mm.mstart;

        Token *tok = NULL;
        char ch = 'n';

        if(mm.kind == CAE_MATCH_COMMENT) {
            ThreadBuf *tmp_c = wiki_thread_buf_acquire_scratch();
            if(!tmp_c) { log_fatal("thread_buffer: failed to acquire scratch in parse_comment_and_ext (comment restore)"); abort(); }
            tmp_c->len = 0;
            restore_accum_mode_to_tb(substr, sub_len, accum, 1, tmp_c);
            tok = build_comment_token(tmp_c->buf, tmp_c->len, accum);
            wiki_thread_buf_release_scratch(tmp_c);
            ch = 'c';

        } else if(mm.kind == CAE_MATCH_EXT) {
            const char *name = tb->buf + mm.name_s;
            size_t name_len = mm.name_e - mm.name_s;

            const char *attr = mm.has_attr ? tb->buf + mm.attr_s : NULL;
            size_t alen = mm.has_attr ? (mm.attr_e - mm.attr_s) : 0;
            const char *inner = mm.has_inner ? tb->buf + mm.inner_s : NULL;
            size_t ilen = mm.has_inner ? (mm.inner_e - mm.inner_s) : 0;
            bool self_closing = !mm.has_close;

            tok = build_ext_token(name, name_len, attr, alen, inner, ilen,
                                  self_closing, cfg, accum);
            ch = 'e';

        } else if(mm.kind == CAE_MATCH_INCLUDE) {
            const char *name = tb->buf + mm.name_s;
            size_t name_len = mm.name_e - mm.name_s;

            const char *attr = mm.has_attr ? tb->buf + mm.attr_s : NULL;
            size_t alen = mm.has_attr ? (mm.attr_e - mm.attr_s) : 0;
            const char *inner = mm.has_inner ? tb->buf + mm.inner_s : NULL;
            size_t ilen = mm.has_inner ? (mm.inner_e - mm.inner_s) : 0;
            const char *closing = mm.has_close ? (tb->buf + mm.close_s) : NULL;
            size_t clen = mm.has_close ? (mm.close_e - mm.close_s) : 0;

            size_t rattr_len = 0, rinner_len = 0;
            ThreadBuf *tmp_attr = NULL;
            ThreadBuf *tmp_inner = NULL;
            const char *rattr = NULL;
            const char *rinner = NULL;

            if(attr && alen > 0) {
                tmp_attr = wiki_thread_buf_acquire_scratch();
                if(!tmp_attr) { log_fatal("thread_buffer: failed to acquire scratch in parse_comment_and_ext (include attr)"); abort(); }
                tmp_attr->len = 0;
                restore_accum_mode_to_tb(attr, alen, accum, 1, tmp_attr);
                rattr = tmp_attr->buf;
                rattr_len = tmp_attr->len;
            }
            if(inner && ilen > 0) {
                tmp_inner = wiki_thread_buf_acquire_scratch();
                if(!tmp_inner) { log_fatal("thread_buffer: failed to acquire scratch in parse_comment_and_ext (include inner)"); abort(); }
                tmp_inner->len = 0;
                restore_accum_mode_to_tb(inner, ilen, accum, 1, tmp_inner);
                rinner = tmp_inner->buf;
                rinner_len = tmp_inner->len;
            }

            tok = build_include_token(name, name_len,
                                      rattr ? rattr : attr, rattr ? rattr_len : alen,
                                      rinner ? rinner : inner, rinner ? rinner_len : ilen,
                                      closing, clen, accum);
            if(tmp_attr) wiki_thread_buf_release_scratch(tmp_attr);
            if(tmp_inner) wiki_thread_buf_release_scratch(tmp_inner);
            ch = 'n';

        } else {
            tok = build_noinclude_token(substr, sub_len, accum);
            ch = 'n';
        }

        if(tok) {
            size_t tok_idx = accum->count - 1;
            char sent_buf[64];
            size_t sent_len = 0;
            work_str_sentinel(tok_idx, ch, sent_buf, &sent_len);
            ENSURE_CAP(sent_len);
            memcpy(out_tb->buf + out_tb->len, sent_buf, sent_len);
            out_tb->len += sent_len;
        } else {
            ENSURE_CAP(sub_len);
            memcpy(out_tb->buf + out_tb->len, substr, sub_len);
            out_tb->len += sub_len;
        }

        search_at = mm.mend;
    }

    if(search_at < tb->len) {
        size_t rest = tb->len - search_at;
        ENSURE_CAP(rest + 1);
        memcpy(out_tb->buf + out_tb->len, tb->buf + search_at, rest);
        out_tb->len += rest;
    }

    out_tb->buf[out_tb->len] = '\0';
    wiki_thread_buf_set(tb, out_tb->buf, out_tb->len);
    wiki_thread_buf_release_scratch(out_tb);

#undef ENSURE_CAP
}
