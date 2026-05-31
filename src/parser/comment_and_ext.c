#include "util/log.h"
#include "build.h"
#include "parser/braces.h"
#include "parser/comment_and_ext.h"
#include "parser/external_links.h"
#include "parser/html.h"
#include "parser/link.h"
#include "parser/links.h"
#include "parser/magic_links.h"
#include "parser/quotes.h"
#include "title.h"
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
#include <strings.h>

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

static bool gallery_caption_may_need_inline_parse(const char *s, size_t len) {
	if(!s || len == 0) return false;

	/* Fast delimiter probes for stages that can change caption text. */
	if(sz_find_byte_from(s, len, "<[{':/", 6)) return true;
	if(sz_find(s, len, "RFC ", 4) || sz_find(s, len, "PMID ", 5) || sz_find(s, len, "ISBN ", 5)) {
		return true;
	}
	return false;
}

static size_t gallery_find_last_open_link_before(const char *txt, size_t upto) {
	if(!txt || upto < 2) return SIZE_MAX;
	const char *open = sz_rfind(txt, upto, "[[", 2);
	return open ? (size_t)(open - txt) : SIZE_MAX;
}

static inline bool cae_tag_name_boundary(unsigned char c) {
    return c == '>' || c == '/' || isspace(c);
}

static bool cae_match_open_named(const char *s, size_t len, size_t i,
                                 const char *name, size_t name_len,
                                 CaeOpenTag *out) {
    if(!s || !name || !out || i + 1 >= len) return false;
    if(s[i] != '<' || s[i + 1] == '/') return false;

    size_t p = i + 1;
    if(p + name_len > len) return false;
    if(!str_ci_eq_n(s + p, name, name_len)) return false;
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
		const char *close = sz_find_byte(s + p + 1, len - (p + 1), ">");
		if(!close) return false;
		size_t q = (size_t)(close - s);
		out->self_closing = (q > p && s[q - 1] == '/');
		out->attr_e = out->self_closing ? q - 1 : q;
		out->open_end = q + 1;
		return true;
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
    if(!str_ci_eq_n(s + p, name, name_len)) return false;
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
	if(!s || !name || !close_tag_s || !close_name_s || !close_name_e || !close_tag_e) {
		return false;
	}

	const char lt = '<';
	size_t q = from;
	while(q + 2 + name_len <= len) {
		const char *cand = sz_find_byte(s + q, len - q, &lt);
		if(!cand) return false;
		q = (size_t)(cand - s);
		if(q + 2 + name_len > len || s[q + 1] != '/') {
			q++;
			continue;
		}
		size_t n0 = q + 2;
		if(!str_ci_eq_n(s + n0, name, name_len)) {
			q++;
			continue;
		}

        size_t r = n0 + name_len;
        while(r < len && isspace((unsigned char)s[r])) r++;
        if(r < len && s[r] == '>') {
            *close_tag_s = q;
            *close_name_s = n0;
            *close_name_e = r;
            *close_tag_e = r + 1;
            return true;
        }
		q++;
    }
    return false;
}

static bool cae_match_comment(const char *s, size_t len, size_t i, CaeScanMatch *m) {
	if(!s || !m || i + 4 > len) return false;

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
	size_t n1_len = include_only ? 11 : 9;
	size_t n2_len = n2 ? 11 : 0;

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

	const unsigned char after_lt = (unsigned char)s[i + 1];
	const unsigned char after_lt_lc = (unsigned char)fast_tolower(after_lt);

    for(size_t ei = 0; ei < cfg->ext.count; ei++) {
        sz_ptr_t ename;
        sz_size_t ename_len;
        sz_string_range(&cfg->ext.items[ei], &ename, &ename_len);
        if(!ename) continue;

		if(ename_len == 0) continue;
		if(after_lt_lc != (unsigned char)fast_tolower((unsigned char)ename[0])) {
			continue;
		}

		if(has_translate && ((ename_len == 9 && sz_equal(ename, "translate", 9) == sz_true_k) ||
				   (ename_len == 4 && sz_equal(ename, "tvar", 4) == sz_true_k)))
            continue;

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
	size_t name_len = include_only ? 9 : 11;

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

	size_t i = at;
	const char lt = '<';
	while(i < len) {
		const char *cand = sz_find_byte(s + i, len - i, &lt);
		if(!cand) return false;
		i = (size_t)(cand - s);
		const unsigned char next = (i + 1 < len) ? (unsigned char)s[i + 1] : 0;
		const unsigned char next_lc = (unsigned char)fast_tolower(next);
		const bool maybe_include_family = (next == '/' || next_lc == 'n' || next_lc == 'i' || next_lc == 'o');

        /* Keep JS alternation order exactly:
         * 1) comment
         * 2) noincludeRegex single-tag
         * 3) dynamic ext
         * 4) includeRegex
         */
		if(i + 4 <= len && s[i + 1] == '!' && s[i + 2] == '-' && s[i + 3] == '-') {
			if(cae_match_comment(s, len, i, m)) return true;
		}
		if(maybe_include_family && cae_match_noinclude_single(s, len, i, include_only, m)) return true;
        if(cae_match_ext(s, len, i, cfg, has_translate, m)) return true;
		if(maybe_include_family && cae_match_include(s, len, i, include_only, m)) return true;

		i++;
    }
    return false;
}

typedef struct {
	sz_string_view_t *items;
	size_t count;
	size_t cap;
} TextStack;

static void text_stack_init(TextStack *st) {
	st->items= NULL;
	st->count= 0;
	st->cap= 0;
}

static void text_stack_push(TextStack *st, const char *s, size_t len) {
	if(st->count >= st->cap) {
		size_t new_cap= st->cap ? st->cap * 2 : 8;
		st->items= realloc(st->items, new_cap * sizeof(sz_string_view_t));
		assert(st->items);
		st->cap= new_cap;
	}
	const char *view = wiki_thread_buf_append_to_tokens(s, len);
	st->items[st->count]= (sz_string_view_t){ .start = view, .length = len };
	st->count++;
}

static void text_stack_free(TextStack *st) {
	/* Items are views into the tokens arena (append-only); do not free them. */
	free(st->items);
	st->items= NULL;
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
								const sz_string_view_t *stack, size_t stack_count,
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
				sz_copy(tb->buf + tb->len, p, rem);
				tb->len += rem;
			}
			break;
		}

		size_t seg = (size_t)(found - p);
		if(seg) {
			wiki_thread_buf_reserve(tb, tb->len + seg);
			sz_copy(tb->buf + tb->len, p, seg);
			tb->len += seg;
		}

		const char *k = found + 1;
		const char *not_digit = sz_find_byte_not_from(k, (size_t)(end - k), "0123456789", 10);
		k = not_digit ? not_digit : end;
		if(k < end && k > found + 1 && (unsigned char)*k == '\x7F') {
			size_t idx = 0;
			for(const char *d = found + 1; d < k; ++d) idx = idx * 10 + (size_t)(*d - '0');
			if(idx < stack_count && stack[idx].start) {
				const char *rep = stack[idx].start;
				size_t replen = stack[idx].length;
				if(replen) {
					wiki_thread_buf_reserve(tb, tb->len + replen);
					sz_copy(tb->buf + tb->len, rep, replen);
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
			const char *not_digit = sz_find_byte_not_from(s + k, len - k, "0123456789", 10);
			k = not_digit ? (size_t)(not_digit - s) : len;
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
								sz_copy(tb->buf + tb->len, rep_s, rep_len);
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
		token_append_text_n(t, key, key_len);
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
		token_append_text_n(t, val, val_len);
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
		token_append_text_n(t, text, text_len);
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
		char *equal_owned = build_normalize_attr_equal(equal, equal_len, accum);
		assert(equal_owned);
		t->data.ext_attr.equal = (sz_string_view_t){ .start = equal_owned, .length = strlen(equal_owned) };
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

#define APPEND_DIRTY_RANGE(start_idx, end_idx)                                   \
	do {                                                                      \
		size_t __start = (start_idx);                                    \
		size_t __end = (end_idx);                                        \
		if(__end > __start) {                                            \
			size_t __n = __end - __start;                            \
			sz_copy(dirty_buf + dirty_len, attr_str + __start, __n); \
			dirty_len += __n;                                        \
		}                                                                 \
	} while(0)

#define EXT_ATTR_WS_LEN(s, n, p) \
	((((p) < (n) && isspace((unsigned char)(s)[(p)])) ? 1 : \
	 (((p) + 1 < (n) && (unsigned char)(s)[(p)] == 0xC2 && (unsigned char)(s)[(p) + 1] == 0xA0) ? 2 : 0)))

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
			/* JS parity: malformed spans beginning with '=' (for example
			 * " =EJ440-444") remain dirty text and must not synthesize
			 * a boolean attribute from the following token. */
			if(attr_str[i] == '=') {
				size_t full_end= i + 1;
				size_t probe= full_end;
				while(probe < attr_len && isspace((unsigned char)attr_str[probe])) probe++;
				size_t ws_len= probe - full_end;

				if(probe < attr_len && (attr_str[probe] == '"' || attr_str[probe] == '\'')) {
					char q= attr_str[probe++];
					while(probe < attr_len && attr_str[probe] != q) probe++;
					if(probe < attr_len && attr_str[probe] == q) probe++;
					full_end= probe;
				} else if(probe < attr_len && ws_len == 0) {
					/* Keep contiguous "=value" fragments dirty, but allow
					 * spaced forms like "= name = ..." to parse `name` as attr. */
					while(probe < attr_len && !isspace((unsigned char)attr_str[probe])) probe++;
					full_end= probe;
				} else {
					full_end= i + 1 + ws_len;
				}

								APPEND_DIRTY_RANGE(i, full_end);
				i= full_end;
			} else {
				dirty_buf[dirty_len++]= attr_str[i++];
			}
			continue;
		}

		/* Validate key: JS allows leading \w or ':' (so digits are valid). */
		const char *key= attr_str + key_start;
		bool valid_key= isalnum((unsigned char)key[0]) || key[0] == '_' || key[0] == ':';
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
			/* JS parity: invalid key contributes the full attr-like span (key + optional '=value') to dirty text. */
			size_t full_end= i;
			size_t probe= i;
			while(probe < attr_len) {
				size_t ws= EXT_ATTR_WS_LEN(attr_str, attr_len, probe);
				if(ws == 0) break;
				probe += ws;
			}
			if(probe < attr_len && attr_str[probe] == '=') {
				probe++; /* skip '=' */
				while(probe < attr_len) {
					size_t ws= EXT_ATTR_WS_LEN(attr_str, attr_len, probe);
					if(ws == 0) break;
					probe += ws;
				}

				if(probe < attr_len && (attr_str[probe] == '"' || attr_str[probe] == '\'')) {
					char q= attr_str[probe++];
					while(probe < attr_len && attr_str[probe] != q) probe++;
					if(probe < attr_len && attr_str[probe] == q) probe++;
				} else {
					while(probe < attr_len && EXT_ATTR_WS_LEN(attr_str, attr_len, probe) == 0) probe++;
				}
				full_end= probe;
			}

						APPEND_DIRTY_RANGE(key_start, full_end);
			i= full_end;
			continue;
		}

		/* Skip optional whitespace before '=' */
		size_t eq_start= i;
		while(i < attr_len) {
			size_t ws= EXT_ATTR_WS_LEN(attr_str, attr_len, i);
			if(ws == 0) break;
			i += ws;
		}

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
		while(i < attr_len) {
			size_t ws= EXT_ATTR_WS_LEN(attr_str, attr_len, i);
			if(ws == 0) break;
			i += ws;
		}

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
			while(i < attr_len && EXT_ATTR_WS_LEN(attr_str, attr_len, i) == 0) i++;
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
#undef EXT_ATTR_WS_LEN
#undef FLUSH_DIRTY
#undef APPEND_DIRTY_RANGE
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
		sz_copy(scratch->buf + 1, attr_str, attr_len);
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
	if(!tag_name) return false;
	size_t nlen = strlen(tag_name);
	return (nlen == 7  && sz_equal(tag_name, "gallery", 7) == sz_true_k)
		|| (nlen == 8  && sz_equal(tag_name, "imagemap", 8) == sz_true_k)
		|| (nlen == 8  && sz_equal(tag_name, "inputbox", 8) == sz_true_k)
		|| (nlen == 15 && sz_equal(tag_name, "dynamicpagelist", 15) == sz_true_k);
}

static bool ext_attr_is_format_wikitext(const char *attr, size_t attr_len) {
	if(!attr || attr_len == 0) return false;

	size_t i= 0;
	const char ws[] = " \t\r\n\v\f";
	while(i < attr_len) {
		const char *nw = sz_find_byte_not_from(attr + i, attr_len - i, ws, sizeof(ws) - 1);
		if(!nw) break;
		i = (size_t)(nw - attr);
		if(i >= attr_len) break;

		size_t key_s= i;
		while(i < attr_len && (isalnum((unsigned char)attr[i]) || attr[i] == '_' || attr[i] == '-')) i++;
		size_t key_e= i;
		if(key_e == key_s) {
			i++;
			continue;
		}

		bool is_format= (key_e - key_s == 6 && str_ci_eq_n(attr + key_s, "format", 6));
		nw = sz_find_byte_not_from(attr + i, attr_len - i, ws, sizeof(ws) - 1);
		if(!nw) break;
		i = (size_t)(nw - attr);
		if(i >= attr_len || attr[i] != '=') {
			continue;
		}
		i++;
		nw = sz_find_byte_not_from(attr + i, attr_len - i, ws, sizeof(ws) - 1);
		if(!nw) break;
		i = (size_t)(nw - attr);

		char q= '\0';
		if(i < attr_len && (attr[i] == '\'' || attr[i] == '"')) {
			q= attr[i++];
		}

		size_t v_s= i;
		if(q) {
			const char *qclose = sz_find_byte(attr + i, attr_len - i, &q);
			i = qclose ? (size_t)(qclose - attr) : attr_len;
		} else {
			const char unquoted_stop[] = " \t\r\n\v\f>/";
			const char *vstop = sz_find_byte_from(attr + i, attr_len - i, unquoted_stop, sizeof(unquoted_stop) - 1);
			i = vstop ? (size_t)(vstop - attr) : attr_len;
		}
		size_t v_e= i;

		if(is_format) {
			while(v_s < v_e && isspace((unsigned char)attr[v_s])) v_s++;
			while(v_e > v_s && isspace((unsigned char)attr[v_e - 1])) v_e--;
			if(v_e - v_s == 8 && str_ci_eq_n(attr + v_s, "wikitext", 8)) {
				return true;
			}
		}

		if(q && i < attr_len && attr[i] == q) i++;
	}

	return false;
}

static Token *build_pre_noinclude_token(const char *substr, size_t sub_len, Accum *accum) {
	Token *t= token_new(TOKEN_NOINCLUDE, "noinclude");
	if(!t) return NULL;
	if(sub_len > 0) {
		const char *view= wiki_thread_buf_append_to_tokens(substr, sub_len);
		token_append_text_n(t, view, sub_len);
	} else {
		token_append_text_n(t, "", 0);
	}
	accum_push(accum, t);
	return t;
}

static bool find_ci_lit(const char *s, size_t len, size_t from,
							 const char *lit, size_t lit_len, size_t *out_pos) {
	if(!s || !lit || lit_len == 0 || from >= len) return false;
	unsigned char first= (unsigned char)lit[0];
	char cand[2];
	cand[0]= (char)fast_tolower(first);
	cand[1]= (char)((first >= 'a' && first <= 'z') ? (first - ('a' - 'A')) : first);

	for(size_t i= from; i + lit_len <= len;) {
		const char *found= sz_find_byte_from(s + i, len - i, cand, 2);
		if(!found) return false;
		size_t p= (size_t)(found - s);
		if(p + lit_len <= len && str_ci_eq_n(s + p, lit, lit_len)) {
			if(out_pos) *out_pos= p;
			return true;
		}
		i= p + 1;
	}
	return false;
}

static Token *build_pre_inner_token(const char *inner_str, size_t inner_len, Accum *accum) {
	Token *t= token_new(TOKEN_EXT_INNER, "ext-inner");
	if(!t) return NULL;
	 t->name= strdup("pre");
	accum_push(accum, t);

	if(!inner_str || inner_len == 0) {
		token_append_text_n(t, "", 0);
		return t;
	}

	ThreadBuf *out_tb= wiki_thread_buf_acquire_scratch();
	if(!out_tb) {
		log_fatal("thread_buffer: failed to acquire scratch in build_pre_inner_token");
		abort();
	}
	wiki_thread_buf_reserve(out_tb, inner_len * 2 + 64);

	const char *open_pat= "<nowiki>";
	const char *close_pat= "</nowiki>";
	const size_t open_len= 8;
	const size_t close_len= 9;

	size_t last_index= 0;
	size_t search_from= 0;
	size_t open_pos= 0;
	while(find_ci_lit(inner_str, inner_len, search_from, open_pat, open_len, &open_pos)) {
		size_t close_pos= 0;
		if(!find_ci_lit(inner_str, inner_len, open_pos + open_len, close_pat, close_len, &close_pos)) {
			break;
		}

		if(open_pos > last_index) {
			wiki_thread_buf_append(out_tb, (sz_string_view_t){.start= inner_str + last_index, .length= open_pos - last_index});
		}

		Token *open_tok= build_pre_noinclude_token(inner_str + open_pos, open_len, accum);
		Token *close_tok= build_pre_noinclude_token(inner_str + close_pos, close_len, accum);
		if(open_tok && close_tok) {
			size_t open_idx= accum->count - 2;
			size_t close_idx= accum->count - 1;
			char open_sent[64], close_sent[64];
			size_t open_slen= 0, close_slen= 0;
			work_str_sentinel(open_idx, 'n', open_sent, &open_slen);
			work_str_sentinel(close_idx, 'n', close_sent, &close_slen);
			wiki_thread_buf_append(out_tb, (sz_string_view_t){.start= open_sent, .length= open_slen});
			if(close_pos > open_pos + open_len) {
				wiki_thread_buf_append(out_tb, (sz_string_view_t){.start= inner_str + open_pos + open_len, .length= close_pos - (open_pos + open_len)});
			}
			wiki_thread_buf_append(out_tb, (sz_string_view_t){.start= close_sent, .length= close_slen});
		} else {
			/* Fallback: preserve raw text if token creation fails. */
			wiki_thread_buf_append(out_tb, (sz_string_view_t){.start= inner_str + open_pos, .length= (close_pos + close_len) - open_pos});
		}

		last_index= close_pos + close_len;
		search_from= last_index;
	}

	if(last_index < inner_len) {
		wiki_thread_buf_append(out_tb, (sz_string_view_t){.start= inner_str + last_index, .length= inner_len - last_index});
	}

	if(out_tb->len > 0) {
		const char *view= wiki_thread_buf_append_to_tokens(out_tb->buf, out_tb->len);
		token_append_text_n(t, view, out_tb->len);
	} else {
		token_append_text_n(t, "", 0);
	}

	wiki_thread_buf_release_scratch(out_tb);
	return t;
}

static bool ext_self_closing_inner_has_no_children(const char *tag_name) {
	if(!tag_name) return false;
	size_t nlen = strlen(tag_name);

	/* JS ExtToken parity: these tags use Token/Nested/Param/Pre-like constructors,
	 * which receive `inner=undefined` for self-closing tags and therefore keep an
	 * empty ext-inner with no text child.
	 */
	return (nlen == 3  && sz_equal(tag_name, "pre", 3) == sz_true_k)
		|| (nlen == 9  && sz_equal(tag_name, "indicator", 9) == sz_true_k)
		|| (nlen == 4  && sz_equal(tag_name, "poem", 4) == sz_true_k)
		|| (nlen == 3  && sz_equal(tag_name, "ref", 3) == sz_true_k)
		|| (nlen == 6  && sz_equal(tag_name, "option", 6) == sz_true_k)
		|| (nlen == 11 && sz_equal(tag_name, "combooption", 11) == sz_true_k)
		|| (nlen == 3  && sz_equal(tag_name, "tab", 3) == sz_true_k)
		|| (nlen == 4  && sz_equal(tag_name, "tabs", 4) == sz_true_k)
		|| (nlen == 4  && sz_equal(tag_name, "poll", 4) == sz_true_k)
		|| (nlen == 3  && sz_equal(tag_name, "seo", 3) == sz_true_k)
		|| (nlen == 11 && sz_equal(tag_name, "langconvert", 11) == sz_true_k)
		|| (nlen == 6  && sz_equal(tag_name, "phonos", 6) == sz_true_k)
		|| (nlen == 10 && sz_equal(tag_name, "references", 10) == sz_true_k)
		|| (nlen == 6  && sz_equal(tag_name, "choose", 6) == sz_true_k)
		|| (nlen == 8  && sz_equal(tag_name, "combobox", 8) == sz_true_k)
		|| (nlen == 15 && sz_equal(tag_name, "dynamicpagelist", 15) == sz_true_k)
		|| (nlen == 8  && sz_equal(tag_name, "inputbox", 8) == sz_true_k)
		|| (nlen == 7  && sz_equal(tag_name, "gallery", 7) == sz_true_k)
		|| (nlen == 8  && sz_equal(tag_name, "imagemap", 8) == sz_true_k);
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
	if(self_closing && ext_self_closing_inner_has_no_children(tag_name)) {
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

/* JS ParamTagToken/InputboxToken parity:
 * - inputbox: parseCommentAndExt + parseBraces on full inner text, then split
 *   by '\n' into ParamLineToken children.
 * - dynamicpagelist: split by '\n', parseCommentAndExt per line.
 */
static Token *build_param_tag_inner_token(const char *tag_name,
														const char *inner_str, size_t inner_len,
														const ParserConfig *cfg,
														Accum *accum,
														bool inputbox_mode) {
	Token *t= token_new(TOKEN_EXT_INNER, "ext-inner");
	if(!t) return NULL;
	 t->name= strdup(tag_name);
	t->sep= '\n';
	accum_push(accum, t);

	if(!inner_str || inner_len == 0) return t;

	const char *src= inner_str;
	size_t src_len= inner_len;
	ThreadBuf *pre_tb= NULL;
	if(inputbox_mode) {
		pre_tb= wiki_thread_buf_acquire_scratch_from_data(inner_str, inner_len);
		if(!pre_tb) {
			log_fatal("thread_buffer: failed to acquire scratch in build_param_tag_inner_token");
			abort();
		}
		parse_comment_and_ext(pre_tb, cfg, accum, false);
		parse_braces(pre_tb, cfg, accum);
		src= pre_tb->buf;
		src_len= pre_tb->len;
	}

	if(src_len == 0) {
		if(pre_tb) wiki_thread_buf_release_scratch(pre_tb);
		return t;
	}

	size_t line_start= 0;
	while(line_start <= src_len) {
		const char *nl= sz_find_byte(src + line_start, src_len - line_start, "\n");
		size_t end= nl ? (size_t)(nl - src) : src_len;
		size_t line_len= end - line_start;
		const char *line_ptr= src + line_start;

		const char *line_emit= line_ptr;
		size_t line_emit_len= line_len;
		ThreadBuf *line_tb= NULL;
		if(!inputbox_mode) {
			line_tb= wiki_thread_buf_acquire_scratch_from_data(line_ptr, line_len);
			if(line_tb) {
				parse_comment_and_ext(line_tb, cfg, accum, false);
				line_emit= line_tb->buf;
				line_emit_len= line_tb->len;
			}
		}

		Token *pl= token_new(TOKEN_PLAIN, "param-line");
		if(pl) {
				   pl->name= strdup(tag_name);
			if(line_emit_len > 0) {
				const char *line_view= wiki_thread_buf_append_to_tokens(line_emit, line_emit_len);
				token_append_text_n(pl, line_view, line_emit_len);
			} else {
				token_append_text_n(pl, "", 0);
			}
			accum_push(accum, pl);
			token_append_child(t, pl);
		}

		if(line_tb) wiki_thread_buf_release_scratch(line_tb);
		if(!nl) break;
		line_start= end + 1;
	}

	if(pre_tb) wiki_thread_buf_release_scratch(pre_tb);
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
		token_append_text_n(t, "", 0);
		return t;
	}

	ThreadBuf *tmp = wiki_thread_buf_acquire_scratch_from_data(inner_str, inner_len);
	if(!tmp) { log_fatal("thread_buffer: failed to acquire scratch in build_references_inner_token"); abort(); }

	parse_comment_and_ext(tmp, cfg, accum, false);
	parse_braces_with_heading(tmp, cfg, accum, false);

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
				sz_copy(out_tb->buf + out_tb->len, tmp->buf + i, mlen);
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
				sz_copy(out_tb->buf + out_tb->len, sent, slen);
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

/* JS GalleryImageToken/FileToken parity for fallback/direct gallery-image params:
 * - parse caption text through in-file inline stages so links/quotes build.
 * - for malformed outer links like [[A|[[B|C]] tail, split at first pipe and
 *   parse right side as a second caption parameter. */
static Token *make_gallery_caption_param_local(const char *txt, size_t tlen,
															 const ParserConfig *cfg,
															 const ParserConfig *links_cfg,
															 Accum *accum) {
	Token *cap= token_new(TOKEN_PLAIN, "image-parameter");
	if(!cap) return NULL;
	cap->name= strdup("caption");
	if(!cap->name) {
		token_free(cap);
		return NULL;
	}

	if(!gallery_caption_may_need_inline_parse(txt, tlen)) {
		token_append_text_n(cap, txt ? txt : "", tlen);
		accum_push(accum, cap);
		return cap;
	}

	ThreadBuf *tb= wiki_thread_buf_acquire_scratch_from_data(txt ? txt : "", tlen);
	if(!tb) {
		token_free(cap);
		return NULL;
	}

	parse_comment_and_ext(tb, cfg, accum, false);
	parse_braces(tb, cfg, accum);
	parse_html(tb, cfg, accum);
	parse_links(tb, links_cfg ? links_cfg : cfg, accum, NULL, false);
	parse_quotes(tb, cfg, accum, false);
	parse_external_links(tb, cfg, accum, false);
	parse_magic_links(tb, cfg, accum);

	build_from_str(cap, tb->buf, tb->len, accum);
	if(cap->child_count == 1 && !cap->children[0].is_text && cap->children[0].token
		&& cap->children[0].token->type == TOKEN_QUOTE) {
		Token *qt= cap->children[0].token;
		if(qt->child_count == 1 && qt->children[0].is_text && qt->children[0].text
			&& qt->children[0].text_len == 2
			&& qt->children[0].text[0] == '\'' && qt->children[0].text[1] == '\'') {
			char *owned= malloc(3);
			if(owned) {
				owned[0]= '\'';
				owned[1]= '\'';
				owned[2]= '\0';
				cap->children[0].is_text= true;
				cap->children[0].text= owned;
				cap->children[0].text_len= 2;
				cap->children[0].text_owned= true;
			}
		}
	}
	if(cap->child_count == 0) {
		token_append_text_n(cap, "", 0);
	}
	accum_push(accum, cap);

	wiki_thread_buf_release_scratch(tb);
	return cap;
}

static Token *make_gallery_caption_param_raw_local(const char *txt, size_t tlen, Accum *accum) {
	Token *cap= token_new(TOKEN_PLAIN, "image-parameter");
	if(!cap) return NULL;
	cap->name= strdup("caption");
	if(!cap->name) {
		token_free(cap);
		return NULL;
	}
	token_append_text_n(cap, txt ? txt : "", tlen);
	if(cap->child_count == 0) {
		token_append_text_n(cap, "", 0);
	}
	accum_push(accum, cap);
	return cap;
}

static void normalize_gallery_caption_lone_quote_local(Token *cap) {
	if(!cap || cap->child_count < 2) return;
	if(cap->children[0].is_text || !cap->children[0].token) return;
	if(cap->children[1].is_text || !cap->children[1].token) return;

	Token *q0= cap->children[0].token;
	Token *q1= cap->children[1].token;
	bool q0_is_two= q0->type == TOKEN_QUOTE && q0->child_count == 1
		&& q0->children[0].is_text && q0->children[0].text
		&& q0->children[0].text_len == 2
		&& q0->children[0].text[0] == '\'' && q0->children[0].text[1] == '\'';
	bool q1_is_two= q1->type == TOKEN_QUOTE && q1->child_count == 1
		&& q1->children[0].is_text && q1->children[0].text
		&& q1->children[0].text_len == 2
		&& q1->children[0].text[0] == '\'' && q1->children[0].text[1] == '\'';
	if(!q0_is_two || !q1_is_two) return;

	char *owned= malloc(3);
	if(!owned) return;
	owned[0]= '\'';
	owned[1]= '\'';
	owned[2]= '\0';

	cap->children[0].is_text= true;
	cap->children[0].text= owned;
	cap->children[0].text_len= 2;
	cap->children[0].text_owned= true;
}

static void normalize_gallery_image_caption_quotes_local(Token *img) {
	if(!img || img->type != TOKEN_FILE) return;
	for(size_t ci= 1; ci < img->child_count; ci++) {
		if(img->children[ci].is_text || !img->children[ci].token) continue;
		Token *param= img->children[ci].token;
		if(param->type != TOKEN_PLAIN || param->subtype != TOKEN_SUBTYPE_IMAGE_PARAMETER) continue;
		if(!param->name || strcmp(param->name, "caption") != 0) continue;
		normalize_gallery_caption_lone_quote_local(param);
	}
}

static bool has_sentinel_type_local(const char *s, size_t len, char want) {
	if(!s || len == 0) return false;
	char nul_cand[1];
	nul_cand[0]= '\0';
	for(size_t i= 0; i < len;) {
		const char *nul= sz_find_byte_from(s + i, len - i, nul_cand, 1);
		if(!nul) break;
		size_t p= (size_t)(nul - s);
		size_t j= p + 1;
		if(j >= len || !(s[j] >= '0' && s[j] <= '9')) {
			i= p + 1;
			continue;
		}
		const char *not_digit= sz_find_byte_not_from(s + j, len - j, "0123456789", 10);
		j= not_digit ? (size_t)(not_digit - s) : len;
		if(j + 1 < len && (unsigned char)s[j + 1] == 0x7F) {
			if(s[j] == want) return true;
			i= j + 2;
			continue;
		}
		i= p + 1;
	}
	return false;
}

static bool has_unclosed_link_from_local(const char *txt, size_t len, size_t open_pos) {
	if(!txt || open_pos >= len) return false;

	int depth= 0;
	char cand[2];
	cand[0]= '[';
	cand[1]= ']';
	for(size_t i= open_pos; i + 1 < len;) {
		const char *found= sz_find_byte_from(txt + i, len - i, cand, 2);
		if(!found) break;
		size_t p= (size_t)(found - txt);
		if(p + 1 >= len) break;
		if(txt[p] == '[' && txt[p + 1] == '[') {
			depth++;
			i= p + 2;
			continue;
		}
		if(txt[p] == ']' && txt[p + 1] == ']') {
			if(depth > 0) depth--;
			i= p + 2;
			continue;
		}
		i= p + 1;
	}

	return depth > 0;
}

static bool gallery_pipe_should_split_local(const char *txt, size_t tlen, size_t pipe_pos) {
	if(!txt || pipe_pos >= tlen) return false;

	/* Track open [[...]] frames up to the pipe. */
	size_t *stack= malloc((pipe_pos + 1) * sizeof(size_t));
	if(!stack) return false;
	size_t sp= 0;
	for(size_t i= 0; i + 1 < pipe_pos; ) {
		if(txt[i] == '[' && txt[i + 1] == '[') {
			stack[sp++]= i;
			i += 2;
			continue;
		}
		if(txt[i] == ']' && txt[i + 1] == ']' && sp > 0) {
			sp--;
			i += 2;
			continue;
		}
		i++;
	}

	/* Not inside a link: normal parameter delimiter. */
	if(sp == 0) {
		free(stack);
		return true;
	}

	size_t open_idx= stack[sp - 1];

	/* Inside a link: split if that innermost link is unclosed. */
	if(has_unclosed_link_from_local(txt, tlen, open_idx)) {
		free(stack);
		return true;
	}

	/* Closed link case: only split for file/image links that contain nested
	 * file/image links before their own closing ]]. */
	size_t head= open_idx + 2;
	while(head < pipe_pos && (txt[head] == ' ' || txt[head] == '\t' || txt[head] == ':')) head++;
	bool is_file_like= false;
	if(head < pipe_pos) {
		size_t avail= pipe_pos - head;
		if((avail >= 5 && str_ci_eq_n(txt + head, "file:", 5)) ||
		   (avail >= 6 && str_ci_eq_n(txt + head, "image:", 6))) {
			is_file_like= true;
		}
	}
	if(!is_file_like) {
		free(stack);
		return false;
	}

	bool nested_file_like= false;
	int depth= 0;
	for(size_t i= open_idx; i + 1 < tlen; ) {
		if(txt[i] == '[' && txt[i + 1] == '[') {
			depth++;
			if(depth >= 2) {
				size_t p= i + 2;
				while(p < tlen && (txt[p] == ' ' || txt[p] == '\t' || txt[p] == ':')) p++;
				size_t rem= tlen - p;
				if((rem >= 5 && str_ci_eq_n(txt + p, "file:", 5)) ||
				   (rem >= 6 && str_ci_eq_n(txt + p, "image:", 6))) {
					nested_file_like= true;
					break;
				}
			}
			i += 2;
			continue;
		}
		if(txt[i] == ']' && txt[i + 1] == ']' && depth > 0) {
			depth--;
			i += 2;
			if(depth == 0) break;
			continue;
		}
		i++;
	}

	bool split= nested_file_like;
	free(stack);
	return split;
}

static const char *gallery_find_split_pipe_local(const char *txt, size_t tlen) {
	if(!txt || tlen == 0) return NULL;

	/* JS parity: if caption starts an unclosed template (e.g. "{{center|...")
	 * GalleryImageToken parameter splitting still treats the first pipe as a
	 * parameter delimiter. */
	if(tlen >= 3 && txt[0] == '{' && txt[1] == '{') {
		const char pipe_ch= '|';
		const char *first_pipe= sz_find_byte(txt + 2, tlen - 2, &pipe_ch);
		if(first_pipe) {
			int depth= 0;
			for(size_t p= 0; p + 1 < tlen; p++) {
				if(txt[p] == '{' && txt[p + 1] == '{') {
					depth++;
					p++;
					continue;
				}
				if(txt[p] == '}' && txt[p + 1] == '}' && depth > 0) {
					depth--;
					p++;
				}
			}
			if(depth > 0) {
				return first_pipe;
			}
		}
	}

	int conv_depth= 0;
	int tpl_depth= 0;
	int arg_depth= 0;
	for(size_t i= 0; i < tlen; i++) {
		if(i + 2 < tlen && txt[i] == '{' && txt[i + 1] == '{' && txt[i + 2] == '{') {
			arg_depth++;
			i += 2;
			continue;
		}
		if(i + 1 < tlen && txt[i] == '{' && txt[i + 1] == '{') {
			tpl_depth++;
			i++;
			continue;
		}
		if(i + 2 < tlen && txt[i] == '}' && txt[i + 1] == '}' && txt[i + 2] == '}' && arg_depth > 0) {
			arg_depth--;
			i += 2;
			continue;
		}
		if(i + 1 < tlen && txt[i] == '}' && txt[i + 1] == '}' && tpl_depth > 0) {
			tpl_depth--;
			i++;
			continue;
		}
		if(i + 1 < tlen && txt[i] == '-' && txt[i + 1] == '{') {
			conv_depth++;
			i++;
			continue;
		}
		if(i + 1 < tlen && txt[i] == '}' && txt[i + 1] == '-' && conv_depth > 0) {
			conv_depth--;
			i++;
			continue;
		}
		if(txt[i] != '|') continue;
		if(conv_depth > 0) continue;
		if(tpl_depth > 0 || arg_depth > 0) continue;
		if(gallery_pipe_should_split_local(txt, tlen, i)) {
			return txt + i;
		}
	}
	return NULL;
}

static void split_gallery_unclosed_caption_local(Token *img,
												const ParserConfig *cfg,
												Accum *accum) {
	if(!img || img->type != TOKEN_FILE || img->child_count < 2) return;
	ParserConfig cfg_local;
	const ParserConfig *links_cfg= cfg;
	if(cfg) {
		cfg_local= *cfg;
		cfg_local.in_ext= true;
		links_cfg= &cfg_local;
	}

	for(size_t ci= 1; ci < img->child_count; ci++) {
		if(img->children[ci].is_text || !img->children[ci].token) continue;
		Token *cap= img->children[ci].token;
		if(cap->type != TOKEN_PLAIN || cap->subtype != TOKEN_SUBTYPE_IMAGE_PARAMETER) continue;
			if(!cap->name || strcmp(cap->name, "caption") != 0) continue;

		bool split_mixed= false;
		if(cap->child_count > 1) {
			for(size_t cj= 0; cj < cap->child_count; cj++) {
				if(!cap->children[cj].is_text || !cap->children[cj].text) continue;
				const char *txt= cap->children[cj].text;
				size_t tlen= cap->children[cj].text_len;
				const char *pipe_ptr= gallery_find_split_pipe_local(txt, tlen);
				if(!pipe_ptr) continue;

				size_t pipe_pos= (size_t)(pipe_ptr - txt);

				size_t left_len= pipe_pos;
				size_t right_len= tlen - (pipe_pos + 1);
				Token *cap2= make_gallery_caption_param_raw_local(
					txt + pipe_pos + 1, right_len, accum);
				if(!cap2) continue;
				char *left_owned= malloc(left_len + 1);
				if(!left_owned) continue;

				if(left_len > 0) sz_copy(left_owned, txt, left_len);
				left_owned[left_len]= '\0';
				if(cap->children[cj].text_owned && cap->children[cj].text) {
					free((void *)cap->children[cj].text);
				}
				cap->children[cj].text= left_owned;
				cap->children[cj].text_len= left_len;
				cap->children[cj].text_owned= true;

				for(size_t k= cj + 1; k < cap->child_count; k++) {
					if(cap->children[k].is_text) {
						token_append_text_n(cap2, cap->children[k].text, cap->children[k].text_len);
						if(cap->children[k].text_owned && cap->children[k].text) {
							free((void *)cap->children[k].text);
						}
					} else if(cap->children[k].token) {
						token_append_child(cap2, cap->children[k].token);
						cap->children[k].token= NULL;
					}
				}
				cap->child_count= cj + 1;

				token_append_child(img, cap2);

				split_mixed= true;
				break;
			}
		}
		if(split_mixed) continue;

		if(cap->child_count != 1 || !cap->children[0].is_text || !cap->children[0].text) continue;

		const char *txt= cap->children[0].text;
		size_t tlen= cap->children[0].text_len;
		bool has_link_sent= has_sentinel_type_local(txt, tlen, 'l');
		const char *pipe_ptr= gallery_find_split_pipe_local(txt, tlen);
		log_debug_env_token("WTC_DEBUG_STAGE_5", NULL,
			"[C split_gallery_caption] cap_single_text len=%zu has_link_sentinel=%d",
			tlen, has_link_sent ? 1 : 0);

		if(has_link_sent && !pipe_ptr) {
			/* JS GalleryImageToken parity: this caption text has already been
			 * through parseLinks before FileToken parameter splitting. Re-running
			 * parse_links here can incorrectly wrap it as an outer link.
			 */
			log_debug_env_token("WTC_DEBUG_STAGE_5", NULL,
				"[C split_gallery_caption] preserve pre-expanded caption via build_from_str");
			build_from_str(cap, txt, tlen, accum);
			if(cap->child_count == 0) {
				token_append_text_n(cap, "", 0);
			}
			continue;
		}

		bool needs_split= false;
		size_t split_at= 0;
		if(pipe_ptr) {
			split_at= (size_t)(pipe_ptr - txt);
			needs_split= true;
		}

		if(needs_split) {
			size_t left_len= split_at;
			size_t right_len= tlen - (split_at + 1);
			Token *cap2= make_gallery_caption_param_raw_local(
				pipe_ptr + 1, right_len, accum);
			char *left_owned= malloc(left_len + 1);
			if(!left_owned) return;
			if(left_len > 0) sz_copy(left_owned, txt, left_len);
			left_owned[left_len]= '\0';
			if(cap->children[0].text_owned && cap->children[0].text) {
				free((void *)cap->children[0].text);
			}
			cap->children[0].text= left_owned;
			cap->children[0].text_len= left_len;
			cap->children[0].text_owned= true;

			if(gallery_caption_may_need_inline_parse(cap->children[0].text, cap->children[0].text_len)) {
				/* Re-parse the left side only when it can still carry inline syntax. */
				ThreadBuf *left_tb= wiki_thread_buf_acquire_scratch_from_data(cap->children[0].text, cap->children[0].text_len);
				if(left_tb) {
					parse_comment_and_ext(left_tb, cfg, accum, false);
					parse_braces(left_tb, cfg, accum);
					parse_html(left_tb, cfg, accum);
					parse_links(left_tb, links_cfg, accum, NULL, false);
					parse_quotes(left_tb, cfg, accum, false);
					parse_external_links(left_tb, cfg, accum, false);
					parse_magic_links(left_tb, cfg, accum);

					if(cap->children[0].text_owned && cap->children[0].text) {
						free((void *)cap->children[0].text);
					}
					cap->child_count= 0;
					build_from_str(cap, left_tb->buf, left_tb->len, accum);
					normalize_gallery_caption_lone_quote_local(cap);
					if(cap->child_count == 0) {
						token_append_text_n(cap, "", 0);
					}
					wiki_thread_buf_release_scratch(left_tb);
				}
			}

			if(cap2) token_append_child(img, cap2);
			continue;
		}

		if(!gallery_caption_may_need_inline_parse(txt, tlen)) {
			continue;
		}

		ThreadBuf *tb= wiki_thread_buf_acquire_scratch_from_data(txt, tlen);
		if(!tb) return;
		parse_comment_and_ext(tb, cfg, accum, false);
		parse_braces(tb, cfg, accum);
		parse_html(tb, cfg, accum);
		parse_links(tb, links_cfg, accum, NULL, false);
		parse_quotes(tb, cfg, accum, false);
		parse_external_links(tb, cfg, accum, false);
		parse_magic_links(tb, cfg, accum);

		if(cap->children[0].text_owned && cap->children[0].text) {
			free((void *)cap->children[0].text);
		}
		cap->child_count= 0;
		build_from_str(cap, tb->buf, tb->len, accum);
		normalize_gallery_caption_lone_quote_local(cap);
		if(cap->child_count == 0) {
			token_append_text_n(cap, "", 0);
		}
		wiki_thread_buf_release_scratch(tb);
	}
}

static void split_gallery_param_pipe_tail_local(Token *img, Accum *accum) {
	if(!img || !accum || img->type != TOKEN_FILE) return;

	for(size_t ci= 1; ci < img->child_count; ci++) {
		if(img->children[ci].is_text || !img->children[ci].token) continue;
		Token *param= img->children[ci].token;
		if(param->type != TOKEN_PLAIN || param->subtype != TOKEN_SUBTYPE_IMAGE_PARAMETER) continue;

		for(size_t cj= 0; cj < param->child_count; cj++) {
			if(!param->children[cj].is_text || !param->children[cj].text) continue;
			const char *txt= param->children[cj].text;
			size_t tlen= param->children[cj].text_len;
			const char *pipe_ptr= gallery_find_split_pipe_local(txt, tlen);
			if(!pipe_ptr) continue;

			size_t split_at= (size_t)(pipe_ptr - txt);
			size_t right_len= tlen - (split_at + 1);
			Token *cap2= make_gallery_caption_param_raw_local(pipe_ptr + 1, right_len, accum);
			if(!cap2) continue;

			char *left_owned= malloc(split_at + 1);
			if(!left_owned) continue;
			if(split_at > 0) sz_copy(left_owned, txt, split_at);
			left_owned[split_at]= '\0';

			if(param->children[cj].text_owned && param->children[cj].text) {
				free((void *)param->children[cj].text);
			}
			param->children[cj].text= left_owned;
			param->children[cj].text_len= split_at;
			param->children[cj].text_owned= true;

			token_append_child(img, cap2);
			break;
		}

		if(param->name && strcmp(param->name, "link") == 0) {
			/* JS parity: keep free-ext-link text pipes inside malformed link=... values. */
			continue;
		}
	}
}

static void normalize_gallery_thumb_caption_local(Token *img, Accum *accum) {
	if(!img || !accum || img->type != TOKEN_FILE || img->subtype != TOKEN_SUBTYPE_GALLERY_IMAGE) return;

	for(size_t ci= 1; ci < img->child_count; ci++) {
		if(img->children[ci].is_text || !img->children[ci].token) continue;
		Token *param= img->children[ci].token;
		if(param->type != TOKEN_PLAIN || param->subtype != TOKEN_SUBTYPE_IMAGE_PARAMETER) continue;
			if(!param->name || strcmp(param->name, "caption") != 0) continue;
		if(param->child_count == 0 || !param->children[0].is_text || !param->children[0].text) continue;

		const char *txt= param->children[0].text;
		size_t txt_len= param->children[0].text_len;

		if((txt_len == 5 && str_ci_eq_n(txt, "thumb", 5)) ||
		   (txt_len == 9 && str_ci_eq_n(txt, "thumbnail", 9))) {
			char *new_name= strdup("thumbnail");
			if(new_name) {
				token_set_name_owned(param, new_name);
				free((void *)param->data.image_param.raw_syntax.start);
				char *owned_syntax= malloc(txt_len + 1);
				if(owned_syntax) {
					sz_copy(owned_syntax, txt, txt_len);
					owned_syntax[txt_len]= '\0';
					param->data.image_param.raw_syntax = (sz_string_view_t){ .start = owned_syntax, .length = txt_len };
				}
				if(param->children[0].is_text && param->children[0].text_owned && param->children[0].text) {
					free((void *)param->children[0].text);
				}
				param->child_count= 0;
			}
			continue;
		}

		size_t cut= 0;
		if(txt_len >= 6 && str_ci_eq_n(txt, "thumb|", 6)) {
			cut= 6;
		} else if(txt_len >= 10 && str_ci_eq_n(txt, "thumbnail|", 10)) {
			cut= 10;
		} else {
			continue;
		}

		Token *thumb= token_new(TOKEN_PLAIN, "image-parameter");
		if(!thumb) return;
			thumb->name= strdup("thumbnail");
			if(!thumb->name) {
			token_free(thumb);
			return;
		}
		size_t syntax_len= cut - 1; /* drop trailing '|' */
		char *owned_syntax= malloc(syntax_len + 1);
		if(!owned_syntax) {
			token_free(thumb);
			return;
		}
		sz_copy(owned_syntax, txt, syntax_len);
		owned_syntax[syntax_len]= '\0';
		thumb->data.image_param.raw_syntax = (sz_string_view_t){ .start = owned_syntax, .length = syntax_len };
		accum_push(accum, thumb);

		size_t remain_len= txt_len - cut;
		char *owned= malloc(remain_len + 1);
		if(!owned) {
			token_free(thumb);
			return;
		}
		if(remain_len > 0) sz_copy(owned, txt + cut, remain_len);
		owned[remain_len]= '\0';
		if(param->children[0].text_owned && param->children[0].text) free((void *)param->children[0].text);
		param->children[0].text= owned;
		param->children[0].text_len= remain_len;
		param->children[0].text_owned= true;

		if(img->child_count + 1 > img->child_cap) {
			size_t new_cap= img->child_cap ? img->child_cap * 2 : 4;
			Child *grown= realloc(img->children, new_cap * sizeof(Child));
			if(!grown) {
				return;
			}
			img->children= grown;
			img->child_cap= new_cap;
		}

		memmove(&img->children[ci + 1], &img->children[ci], (img->child_count - ci) * sizeof(Child));
		img->children[ci].is_text= false;
		img->children[ci].token= thumb;
		img->child_count++;
		return;
	}
}

static void normalize_gallery_named_param_prefixes_local(Token *img) {
	if(!img || img->type != TOKEN_FILE || img->subtype != TOKEN_SUBTYPE_GALLERY_IMAGE) return;

	for(size_t ci= 1; ci < img->child_count; ci++) {
		if(img->children[ci].is_text || !img->children[ci].token) continue;
		Token *param= img->children[ci].token;
		if(param->type != TOKEN_PLAIN || param->subtype != TOKEN_SUBTYPE_IMAGE_PARAMETER) continue;
		if(!param->name || strcmp(param->name, "caption") != 0 || param->child_count == 0) continue;

		Child *first= &param->children[0];
		if(!first->is_text || !first->text || first->text_len < 5) continue;

		size_t p= 0;
		while(p < first->text_len && (first->text[p] == ' ' || first->text[p] == '\t')) p++;

		const char *new_name_lit= NULL;
		const char *prefix_lit= NULL;
		size_t prefix_len= 0;
		size_t syntax_core_len= 0;
		if(p + 4 <= first->text_len && strncmp(first->text + p, "alt=", 4) == 0) {
			new_name_lit= "alt";
			prefix_lit= "alt=$1";
			prefix_len= 4;
			syntax_core_len= 6;
		} else if(p + 5 <= first->text_len && strncmp(first->text + p, "link=", 5) == 0) {
			new_name_lit= "link";
			prefix_lit= "link=$1";
			prefix_len= 5;
			syntax_core_len= 7;
		} else {
			continue;
		}

		if(new_name_lit[0] == 'l' && new_name_lit[1] == 'i' && new_name_lit[2] == 'n' && new_name_lit[3] == 'k' && new_name_lit[4] == '\0') {
			size_t value_off= p + prefix_len;
			size_t value_len= first->text_len - value_off;
			if(value_len > 0 && sz_find_byte(first->text + value_off, value_len, "|") != NULL) {
				/* JS parity: malformed link=...|... remains caption text. */
				continue;
			}
		}

		char *new_name= strdup(new_name_lit);
		if(!new_name) continue;
		token_set_name_owned(param, new_name);

		free((void *)param->data.image_param.raw_syntax.start);
		char *owned_syntax= malloc(p + syntax_core_len + 1);
		if(owned_syntax) {
			if(p > 0) sz_copy(owned_syntax, first->text, p);
			sz_copy(owned_syntax + p, prefix_lit, syntax_core_len);
			owned_syntax[p + syntax_core_len]= '\0';
			param->data.image_param.raw_syntax = (sz_string_view_t){ .start = owned_syntax, .length = p + syntax_core_len };
		}

		size_t strip_len= p + prefix_len;
		size_t new_len= first->text_len - strip_len;
		char *owned= malloc(new_len + 1);
		if(!owned) continue;
		if(new_len > 0) sz_copy(owned, first->text + strip_len, new_len);
		owned[new_len]= '\0';
		if(first->text_owned && first->text) free((void *)first->text);
		first->text= owned;
		first->text_len= new_len;
		first->text_owned= true;
	}
}

static void append_tail_to_last_image_param_local(Token *img, const char *tail, size_t tail_len) {
	if(!img || !tail || tail_len == 0) return;

	Token *last_param= NULL;
	for(size_t ci= img->child_count; ci > 0; ci--) {
		Child *ch= &img->children[ci - 1];
		if(ch->is_text || !ch->token) continue;
		Token *tok= ch->token;
		if(tok->type == TOKEN_PLAIN && tok->subtype == TOKEN_SUBTYPE_IMAGE_PARAMETER) {
			last_param= tok;
			break;
		}
	}
	if(!last_param) return;

	if(last_param->child_count > 0) {
		Child *last_child= &last_param->children[last_param->child_count - 1];
		if(last_child->is_text) {
			size_t old_len= last_child->text_len;
			char *merged= malloc(old_len + tail_len + 1);
			if(!merged) return;
			if(old_len > 0 && last_child->text) sz_copy(merged, last_child->text, old_len);
			sz_copy(merged + old_len, tail, tail_len);
			merged[old_len + tail_len]= '\0';
			if(last_child->text_owned && last_child->text) free((void *)last_child->text);
			last_child->text= merged;
			last_child->text_len= old_len + tail_len;
			last_child->text_owned= true;
			return;
		}
	}

	token_append_text_n(last_param, tail, tail_len);
}

static void normalize_gallery_caption_bracket_split_local(Token *img) {
	if(!img || img->type != TOKEN_FILE || img->subtype != TOKEN_SUBTYPE_GALLERY_IMAGE) return;

	for(size_t ci= 1; ci + 1 < img->child_count; ci++) {
		if(img->children[ci].is_text || img->children[ci + 1].is_text) continue;
		Token *left= img->children[ci].token;
		Token *right= img->children[ci + 1].token;
		if(!left || !right) continue;
		if(left->type != TOKEN_PLAIN || right->type != TOKEN_PLAIN) continue;
		if(left->subtype != TOKEN_SUBTYPE_IMAGE_PARAMETER || right->subtype != TOKEN_SUBTYPE_IMAGE_PARAMETER) continue;
		if(!left->name || !right->name) continue;
		if(strcmp(left->name, "caption") != 0 || strcmp(right->name, "caption") != 0) continue;
		if(left->child_count == 0 || right->child_count == 0) continue;

		Child *l_last= &left->children[left->child_count - 1];
		Child *r_last= &right->children[right->child_count - 1];
		if(!l_last->is_text || !r_last->is_text || !l_last->text || !r_last->text) continue;
		if(r_last->text_len < 2) continue;
		if(!(r_last->text[r_last->text_len - 2] == ']' && r_last->text[r_last->text_len - 1] == ']')) continue;
		if(l_last->text_len >= 2 && l_last->text[l_last->text_len - 2] == ']' && l_last->text[l_last->text_len - 1] == ']') continue;

		char *l_new= malloc(l_last->text_len + 2 + 1);
		if(!l_new) continue;
		if(l_last->text_len > 0) sz_copy(l_new, l_last->text, l_last->text_len);
		l_new[l_last->text_len]= ']';
		l_new[l_last->text_len + 1]= ']';
		l_new[l_last->text_len + 2]= '\0';
		if(l_last->text_owned && l_last->text) free((void *)l_last->text);
		l_last->text= l_new;
		l_last->text_len+= 2;
		l_last->text_owned= true;

		size_t r_new_len= r_last->text_len - 2;
		char *r_new= malloc(r_new_len + 1);
		if(!r_new) continue;
		if(r_new_len > 0) sz_copy(r_new, r_last->text, r_new_len);
		r_new[r_new_len]= '\0';
		if(r_last->text_owned && r_last->text) free((void *)r_last->text);
		r_last->text= r_new;
		r_last->text_len= r_new_len;
		r_last->text_owned= true;
	}
}

/* Fallback helper: parse gallery alt text into FileToken image parameters by
 * feeding a synthetic [[File:...|...]] through stage-1/5 parsing and moving
 * parameter children (index >= 1) onto the destination gallery-image token. */
static void append_gallery_params_via_wrapper_local(Token *dst,
															 const char *file_ptr, size_t file_len,
															 const char *alt_ptr, size_t alt_len,
															 const ParserConfig *cfg,
															 Accum *accum) {
	if(!dst || !file_ptr || file_len == 0 || !alt_ptr) return;
	ParserConfig cfg_local;
	const ParserConfig *links_cfg= cfg;
	if(cfg) {
		cfg_local= *cfg;
		cfg_local.in_ext= true;
		links_cfg= &cfg_local;
	}

	const char *fptr= file_ptr;
	size_t flen= file_len;
	while(flen > 0 && isspace((unsigned char)fptr[0])) {
		fptr++;
		flen--;
	}
	while(flen > 0 && isspace((unsigned char)fptr[flen - 1])) {
		flen--;
	}
	if(flen == 0) return;

	ThreadBuf *tb= wiki_thread_buf_acquire_scratch();
	if(!tb) return;

	/* [[File:<trimmed file>|<alt>]] */
	size_t wrapped_len= 2 + 5 + flen + 1 + alt_len + 2;
	wiki_thread_buf_reserve(tb, wrapped_len + 1);
	tb->buf[0]= '[';
	tb->buf[1]= '[';
	sz_copy(tb->buf + 2, "File:", 5);
	sz_copy(tb->buf + 7, fptr, flen);
	tb->buf[7 + flen]= '|';
	if(alt_len > 0) {
		sz_copy(tb->buf + 8 + flen, alt_ptr, alt_len);
	}
	tb->buf[8 + flen + alt_len]= ']';
	tb->buf[9 + flen + alt_len]= ']';
	tb->buf[10 + flen + alt_len]= '\0';
	tb->len= 10 + flen + alt_len;

	parse_braces(tb, cfg, accum);
	parse_links(tb, links_cfg, accum, NULL, false);

	Token *tmp= token_new(TOKEN_PLAIN, "gallery-param-wrapper");
	if(!tmp) {
		wiki_thread_buf_release_scratch(tb);
		return;
	}
	build_from_str(tmp, tb->buf, tb->len, accum);
	build_token_recursive(tmp, accum, cfg);

	if(tmp->child_count >= 1 && !tmp->children[0].is_text && tmp->children[0].token && tmp->children[0].token->type == TOKEN_FILE) {
		Token *ft= tmp->children[0].token;
		for(size_t pi= 1; pi < ft->child_count; pi++) {
			if(ft->children[pi].is_text || !ft->children[pi].token) continue;
			token_append_child(dst, ft->children[pi].token);
			ft->children[pi].token= NULL;
		}
		if(tmp->child_count > 1) {
			for(size_t ti= 1; ti < tmp->child_count; ti++) {
				if(tmp->children[ti].is_text && tmp->children[ti].text_len > 0) {
					append_tail_to_last_image_param_local(dst, tmp->children[ti].text, tmp->children[ti].text_len);
				}
			}
		}
	}

	token_free_shallow(tmp);
	wiki_thread_buf_release_scratch(tb);
}

static Token *parse_gallery_image_line_local(const char *line, size_t line_len,
																				const ParserConfig *cfg,
																				Accum *accum,
																				bool preparse_extlinks) {
	if(!line || line_len == 0) return NULL;
	ParserConfig cfg_local;
	const ParserConfig *links_cfg= cfg;
	if(cfg) {
		cfg_local= *cfg;
		cfg_local.in_ext= true;
		links_cfg= &cfg_local;
	}

	const char pipe_ch = '|';
	const char *pipe_ptr = sz_find_byte(line, line_len, &pipe_ch);
	size_t file_len= pipe_ptr ? (size_t)(pipe_ptr - line) : line_len;

	const char *trim_file_ptr= line;
	size_t trim_file_len= file_len;
	while(trim_file_len > 0 && isspace((unsigned char)trim_file_ptr[0])) {
		trim_file_ptr++;
		trim_file_len--;
	}
	while(trim_file_len > 0 && isspace((unsigned char)trim_file_ptr[trim_file_len - 1])) {
		trim_file_len--;
	}

	Title *file_title= title_parse_half_parsed(trim_file_ptr, trim_file_len, 6, cfg, true, "");
	bool file_valid= (file_title && file_title->valid);
	title_free(file_title);
	bool fallback_file_valid_known = (trim_file_ptr == line && trim_file_len == file_len);
	bool fallback_file_valid = file_valid;

	/* JS GalleryToken parity: invalid file titles produce CommentLineToken. */
	if(!file_valid) {
		Token *comment_line = token_new(TOKEN_NOINCLUDE, "noinclude");
		if(comment_line) {
			const char *line_view = wiki_thread_buf_append_to_tokens(line, line_len);
			token_append_text_n(comment_line, line_view, line_len);
			accum_push(accum, comment_line);
		}
		return comment_line;
	}

	ThreadBuf *pre_text_tb= NULL;
	char *pre_text_owned= NULL;
	const char *pre_text_ptr= NULL;
	size_t pre_text_len= 0;
	size_t pipe_idx= SIZE_MAX;
	if(pipe_ptr) {
		pipe_idx= (size_t)(pipe_ptr - line);
		if(pipe_idx + 1 < line_len) {
			pre_text_tb= wiki_thread_buf_acquire_scratch_from_data(line + pipe_idx + 1, line_len - (pipe_idx + 1));
			if(!pre_text_tb) { log_fatal("thread_buffer: failed to acquire scratch in parse_gallery_image_line_local (pre_text)"); abort(); }
			/* JS parity: GalleryImageToken pre-parses the text segment through
			 * later inline-link stages before FileToken parameter splitting. */
			parse_comment_and_ext(pre_text_tb, cfg, accum, false);
			parse_braces(pre_text_tb, cfg, accum);
			parse_html(pre_text_tb, cfg, accum);
			parse_quotes(pre_text_tb, cfg, accum, false);
			if(preparse_extlinks) {
				parse_external_links(pre_text_tb, cfg, accum, false);
				parse_magic_links(pre_text_tb, cfg, accum);
			}
			if(pre_text_tb->len > 0) {
				pre_text_owned= malloc(pre_text_tb->len);
				if(pre_text_owned) {
					sz_copy(pre_text_owned, pre_text_tb->buf, pre_text_tb->len);
					pre_text_ptr= pre_text_owned;
					pre_text_len= pre_text_tb->len;
					wiki_thread_buf_release_scratch(pre_text_tb);
					pre_text_tb= NULL;
				} else {
					/* OOM fallback: keep scratch alive until function end. */
					pre_text_ptr= pre_text_tb->buf;
					pre_text_len= pre_text_tb->len;
				}
			} else {
				wiki_thread_buf_release_scratch(pre_text_tb);
				pre_text_tb= NULL;
			}
		}
	}

	ThreadBuf *tmp_tb = wiki_thread_buf_acquire_scratch();
	if(!tmp_tb) { log_fatal("thread_buffer: failed to acquire scratch in parse_gallery_image_line_local"); abort(); }
	if(pre_text_ptr && pipe_idx != SIZE_MAX) {
		size_t lhs_len= pipe_idx + 1; /* include the first '|' */
		size_t wrapped_len= 2 + lhs_len + pre_text_len + 2; /* [[ + lhs + pre + ]] */
		wiki_thread_buf_reserve(tmp_tb, wrapped_len + 1);
		tmp_tb->buf[0]= '[';
		tmp_tb->buf[1]= '[';
		sz_copy(tmp_tb->buf + 2, line, lhs_len);
		if(pre_text_len > 0) {
			sz_copy(tmp_tb->buf + 2 + lhs_len, pre_text_ptr, pre_text_len);
		}
		size_t tail= 2 + lhs_len + pre_text_len;
		tmp_tb->buf[tail]= ']';
		tmp_tb->buf[tail + 1]= ']';
		tmp_tb->buf[tail + 2]= '\0';
		tmp_tb->len= tail + 2;
	} else {
		wiki_thread_buf_reserve(tmp_tb, line_len + 4);
		tmp_tb->buf[0]= '[';
		tmp_tb->buf[1]= '[';
		sz_copy(tmp_tb->buf + 2, line, line_len);
		tmp_tb->buf[2 + line_len]= ']';
		tmp_tb->buf[3 + line_len]= ']';
		tmp_tb->buf[4 + line_len]= '\0';
		tmp_tb->len= line_len + 4;
	}

	parse_braces(tmp_tb, cfg, accum);
	parse_links(tmp_tb, links_cfg, accum, NULL, false);

	Token *tmp= token_new(TOKEN_PLAIN, "gallery-line");
	if(!tmp) {
		if(pre_text_tb) wiki_thread_buf_release_scratch(pre_text_tb);
		wiki_thread_buf_release_scratch(tmp_tb);
		return NULL;
	}
	build_from_str(tmp, tmp_tb->buf, tmp_tb->len, accum);
	build_token_recursive(tmp, accum, cfg);

	Token *out= NULL;
	if(tmp->child_count >= 1 && !tmp->children[0].is_text && tmp->children[0].token && tmp->children[0].token->type == TOKEN_FILE) {
		out= tmp->children[0].token;
		tmp->children[0].token= NULL;
		out->subtype= TOKEN_SUBTYPE_GALLERY_IMAGE;
		if(out->name) {
			token_clear_name(out);
		}
		if(tmp->child_count > 1) {
			for(size_t ti= 1; ti < tmp->child_count; ti++) {
				if(tmp->children[ti].is_text && tmp->children[ti].text_len > 0) {
					append_tail_to_last_image_param_local(out, tmp->children[ti].text, tmp->children[ti].text_len);
				}
			}
		}
		normalize_gallery_named_param_prefixes_local(out);
		split_gallery_unclosed_caption_local(out, cfg, accum);
		split_gallery_param_pipe_tail_local(out, accum);
		normalize_gallery_thumb_caption_local(out, accum);
		normalize_gallery_image_caption_quotes_local(out);
		normalize_gallery_caption_bracket_split_local(out);
		/* Preserve the original gallery line target text exactly as parsed. */
		/* JS stage-log parity: link/file names are assigned later in afterBuild(). */
		for(size_t ci= 0; ci < out->child_count; ci++) {
			if(out->children[ci].is_text || !out->children[ci].token) continue;
			Token *child= out->children[ci].token;
			if((child->type == TOKEN_LINK || child->type == TOKEN_FILE || child->type == TOKEN_CATEGORY) && child->name) {
				token_clear_name(child);
			}
			for(size_t cj= 0; cj < child->child_count; cj++) {
				if(child->children[cj].is_text || !child->children[cj].token) continue;
				Token *g= child->children[cj].token;
				if((g->type == TOKEN_LINK || g->type == TOKEN_FILE || g->type == TOKEN_CATEGORY) && g->name) {
					token_clear_name(g);
				}
			}
		}
	}
	if(!out) {
		size_t non_ws= 0;
		while(non_ws < line_len && isspace((unsigned char)line[non_ws])) non_ws++;
		if(non_ws < line_len) {
			const char pipe_ch2 = '|';
			const char *pipe_ptr2 = sz_find_byte(line, line_len, &pipe_ch2);
			size_t fallback_file_len= pipe_ptr2 ? (size_t)(pipe_ptr2 - line) : line_len;
			/* JS GalleryToken parity: only construct gallery-image when
			 * normalizeTitle(file, 6, {halfParsed:true, decode:true, page:''}).valid. */
			bool file_valid_local = fallback_file_valid;
			if(!fallback_file_valid_known || fallback_file_len != file_len) {
				Title *file_title= title_parse_half_parsed(line, fallback_file_len, 6, cfg, true, "");
				file_valid_local= (file_title && file_title->valid);
				title_free(file_title);
			}

			if(!file_valid_local) {
				Token *comment_line = token_new(TOKEN_NOINCLUDE, "noinclude");
				if(comment_line) {
					const char *line_view = wiki_thread_buf_append_to_tokens(line, line_len);
					token_append_text_n(comment_line, line_view, line_len);
					accum_push(accum, comment_line);
					out = comment_line;
				}
			}

			if(!out) {
			/* JS GalleryToken parity: caption text may legitimately include ext tags
			 * such as <ref>...</ref>, so do not reject on angle brackets here. */
			}

			if(!out) {
			bool built= false;
			if(pipe_ptr2) {
				size_t lhs_len= (size_t)(pipe_ptr2 - line);
				size_t rhs_len= line_len - lhs_len - 1;
				if(lhs_len > 0 && sz_find(line, lhs_len, "[[", 2) == NULL) {
					Token *fallback= token_new(TOKEN_FILE, "gallery-image");
					if(fallback) {
						Token *target= token_new(TOKEN_ATOM, "link-target");
						if(target) {
							const char *lhs_view= wiki_thread_buf_append_to_tokens(line, lhs_len);
							token_append_text_n(target, lhs_view, lhs_len);
							accum_push(accum, target);
							token_append_child(fallback, target);
						}

						const char *alt_src= pipe_ptr2 + 1;
						size_t alt_len= rhs_len;
						if(pre_text_ptr) {
							alt_src= pre_text_ptr;
							alt_len= pre_text_len;
						}
						append_gallery_params_via_wrapper_local(fallback, line, lhs_len, alt_src, alt_len, cfg, accum);
						if(fallback->child_count == 1) {
							Token *cap= token_new(TOKEN_PLAIN, "image-parameter");
							if(cap) {
													cap->name= strdup("caption");
								const char *rhs_view= wiki_thread_buf_append_to_tokens(pipe_ptr2 + 1, rhs_len);
								token_append_text_n(cap, rhs_view, rhs_len);
								accum_push(accum, cap);
								token_append_child(fallback, cap);
							}
						}

						normalize_gallery_named_param_prefixes_local(fallback);
						split_gallery_unclosed_caption_local(fallback, cfg, accum);
						split_gallery_param_pipe_tail_local(fallback, accum);
						normalize_gallery_thumb_caption_local(fallback, accum);
						normalize_gallery_image_caption_quotes_local(fallback);
						normalize_gallery_caption_bracket_split_local(fallback);

						accum_push(accum, fallback);
						out= fallback;
						built= true;
					}
				}
			}
			if(!built && sz_find(line, line_len, "[[", 2) == NULL) {
				Token *fallback= token_new(TOKEN_FILE, "gallery-image");
				if(fallback) {
					Token *target= token_new(TOKEN_ATOM, "link-target");
					if(target) {
						const char *view= wiki_thread_buf_append_to_tokens(line, line_len);
						token_append_text_n(target, view, line_len);
						accum_push(accum, target);
						token_append_child(fallback, target);
					}
					accum_push(accum, fallback);
					out= fallback;
				}
			}
			}
		}
	}

	token_free_shallow(tmp);
	if(pre_text_owned) free(pre_text_owned);
	if(pre_text_tb) wiki_thread_buf_release_scratch(pre_text_tb);
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

static bool imagemap_tail_ok_local(const char *s, size_t len) {
	for(size_t i= 0; i < len; i++) {
		unsigned char c= (unsigned char)s[i];
		if(isalnum(c) || c == '_' || isspace(c)) continue;
		return false;
	}
	return true;
}

static Token *create_raw_ext_link_token_local(const char *url, size_t url_len,
																					 const char *space, size_t space_len,
																					 const char *text, size_t text_len,
																					 Accum *accum) {
	if(!url || url_len == 0) return NULL;

	Token *url_tok= token_new(TOKEN_MAGIC_LINK, "ext-link-url");
	if(!url_tok) return NULL;
	const char *url_view= wiki_thread_buf_append_to_tokens(url, url_len);
	token_append_text_n(url_tok, url_view, url_len);
	accum_push(accum, url_tok);

	Token *ext= token_new(TOKEN_EXT_LINK, "ext-link");
	if(!ext) {
		token_free(url_tok);
		return NULL;
	}

	if(space_len > 0) {
		char *owned_space= malloc(space_len + 1);
		if(!owned_space) {
			token_free(ext);
			return NULL;
		}
		sz_copy(owned_space, space, space_len);
		owned_space[space_len]= '\0';
		ext->data.ext_link.space = (sz_string_view_t){ .start = owned_space, .length = space_len };
	}

	token_append_child(ext, url_tok);

	if(text_len > 0) {
		Token *txt= token_new(TOKEN_PLAIN, "ext-link-text");
		if(!txt) {
			token_free(ext);
			return NULL;
		}
		const char *txt_view= wiki_thread_buf_append_to_tokens(text, text_len);
		token_append_text_n(txt, txt_view, text_len);
		accum_push(accum, txt);
		token_append_child(ext, txt);
	}

	accum_push(accum, ext);
	return ext;
}

static Token *create_imagemap_link_wrapper_local(const char *pre, size_t pre_len,
																						  Token *link,
																						  const char *post, size_t post_len,
																						  Accum *accum) {
	if(!link) return NULL;

	Token *t= token_new(TOKEN_PLAIN, "imagemap-link");
	if(!t) return NULL;
	accum_push(accum, t);

	if(pre_len > 0) {
		const char *pre_view= wiki_thread_buf_append_to_tokens(pre, pre_len);
		token_append_text_n(t, pre_view, pre_len);
	} else {
		token_append_text_n(t, "", 0);
	}

	token_append_child(t, link);

	Token *tail= token_new(TOKEN_NOINCLUDE, "noinclude");
	if(tail) {
		if(post_len > 0) {
			const char *post_view= wiki_thread_buf_append_to_tokens(post, post_len);
			token_append_text_n(tail, post_view, post_len);
		} else {
			token_append_text_n(tail, "", 0);
		}
		accum_push(accum, tail);
		token_append_child(t, tail);
	}

	return t;
}

static Token *parse_imagemap_image_line_local(const char *line, size_t line_len,
														 const ParserConfig *cfg,
														 Accum *accum) {
	if(!line || line_len == 0) return NULL;

	/* Imagemap image lines may be prefixed with ':' indentation. JS treats this
	 * as line syntax, not part of the file target text. */
	const char *parse_ptr= line;
	size_t parse_len= line_len;
	size_t off= 0;
	bool had_indent_colon= false;
	while(off < line_len && (line[off] == ' ' || line[off] == '\t')) off++;
	if(off < line_len && line[off] == ':') {
		had_indent_colon= true;
		off++;
		while(off < line_len && (line[off] == ' ' || line[off] == '\t')) off++;
		parse_ptr= line + off;
		parse_len= line_len - off;
	}

	/* JS parity: ImagemapToken first-line image uses GalleryImageToken logic.
	 * Reuse gallery-image parsing and retag to imagemap-image. */
	Token *out= parse_gallery_image_line_local(parse_ptr, parse_len, cfg, accum, false);
	if(!out || out->type != TOKEN_FILE) return NULL;

	out->subtype= TOKEN_SUBTYPE_IMAGEMAP_IMAGE;
	if(had_indent_colon && out->child_count > 0 && !out->children[0].is_text && out->children[0].token) {
		Token *target= out->children[0].token;
		if(target->child_count > 0 && target->children[0].is_text && target->children[0].text) {
			size_t old_len= target->children[0].text_len;
			char *owned= malloc(old_len + 2);
			if(owned) {
				owned[0]= ':';
				if(old_len > 0) sz_copy(owned + 1, target->children[0].text, old_len);
				owned[old_len + 1]= '\0';
				if(target->children[0].text_owned && target->children[0].text) {
					free((void *)target->children[0].text);
				}
				target->children[0].text= owned;
				target->children[0].text_len= old_len + 1;
				target->children[0].text_owned= true;
			}
		}
	}
	if(out->name) {
		token_clear_name(out);
	}

	/* JS stage-log parity: link/file names are assigned later in afterBuild(). */
	for(size_t ci= 0; ci < out->child_count; ci++) {
		if(out->children[ci].is_text || !out->children[ci].token) continue;
		Token *child= out->children[ci].token;
		if((child->type == TOKEN_LINK || child->type == TOKEN_FILE || child->type == TOKEN_CATEGORY) && child->name) {
			token_clear_name(child);
		}
		for(size_t cj= 0; cj < child->child_count; cj++) {
			if(child->children[cj].is_text || !child->children[cj].token) continue;
			Token *g= child->children[cj].token;
			if((g->type == TOKEN_LINK || g->type == TOKEN_FILE || g->type == TOKEN_CATEGORY) && g->name) {
				token_clear_name(g);
			}
		}
	}

	return out;
}

static Token *parse_imagemap_link_line_from_open_local(const char *line, size_t line_len,
																		  size_t open,
																		  const ParserConfig *cfg,
																		  Accum *accum) {
	if(!line || line_len == 0 || open >= line_len) return NULL;

	const char *substr= line + open;
	size_t substr_len= line_len - open;

	/* Internal link branch: [[target|text]]post */
	if(substr_len >= 4 && substr[0] == '[' && substr[1] == '[') {
		const char *p_close= sz_find(substr + 2, substr_len - 2, "]]", 2);
		if(!p_close) return NULL;

		size_t close_rel= (size_t)(p_close - substr);
		const char *after= p_close + 2;
		size_t after_len= substr_len - (close_rel + 2);
		if(!imagemap_tail_ok_local(after, after_len)) return NULL;

		const char *inner= substr + 2;
		size_t inner_len= close_rel - 2;
		const char *pipe= sz_find_byte(inner, inner_len, "|");
		const char *target= inner;
		size_t target_len= pipe ? (size_t)(pipe - inner) : inner_len;
		if(target_len == 0) return NULL;

		Title *title= title_parse_half_parsed(target, target_len, 0, cfg, true, "");
		bool valid= title && title->valid;
		title_free(title);
		if(!valid) return NULL;

		Token *link= create_raw_link_token_local(inner, inner_len, accum);
		if(!link) return NULL;
		return create_imagemap_link_wrapper_local(line, open, link, after, after_len, accum);
	}

	/* External link branch: [url text]post */
	if(substr_len >= 3 && substr[0] == '[' && substr[1] != '[') {
		const char *p_close= sz_find(substr + 1, substr_len - 1, "]", 1);
		if(!p_close) return NULL;

		size_t close_rel= (size_t)(p_close - substr);
		const char *after= p_close + 1;
		size_t after_len= substr_len - (close_rel + 1);
		if(!imagemap_tail_ok_local(after, after_len)) return NULL;

		const char *inner= substr + 1;
		size_t inner_len= close_rel - 1;
		if(inner_len == 0) return NULL;

		size_t url_end= 0;
		while(url_end < inner_len && !isspace((unsigned char)inner[url_end])) url_end++;
		if(url_end == 0) return NULL;

		const char *url= inner;
		size_t url_len= url_end;
		bool proto_ok= (url_len >= 2 && url[0] == '/' && url[1] == '/');
		if(!proto_ok) {
			proto_ok= match_proto_prefix(url, url_len, cfg) > 0;
		}
		if(!proto_ok) return NULL;

		const char *space= NULL;
		size_t space_len= 0;
		const char *text= NULL;
		size_t text_len= 0;

		if(url_end < inner_len) {
			size_t p= url_end;
			while(p < inner_len && isspace((unsigned char)inner[p])) p++;
			if(p == url_end) return NULL;
			space= inner + url_end;
			space_len= p - url_end;
			text= inner + p;
			text_len= inner_len - p;
		}

		Token *ext= create_raw_ext_link_token_local(url, url_len,
			space ? space : "", space_len,
			text ? text : "", text_len,
			accum);
		if(!ext) return NULL;
		return create_imagemap_link_wrapper_local(line, open, ext, after, after_len, accum);
	}

	return NULL;
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

	bool first= true;
	bool error= false;
	size_t line_start= 0;
	while(line_start <= inner_len) {
		const char *nl = sz_find_byte(inner_str + line_start, inner_len - line_start, "\n");
		size_t i = nl ? (size_t)(nl - inner_str) : inner_len;
		size_t line_len= i - line_start;
		const char *line_ptr= inner_str + line_start;

		size_t lead= 0;
		while(lead < line_len && isspace((unsigned char)line_ptr[lead])) lead++;
		bool trimmed_empty= (lead >= line_len);
		bool comment_line= (!trimmed_empty && line_ptr[lead] == '#');

		if(error || trimmed_empty || comment_line) {
			Token *n= token_new(TOKEN_NOINCLUDE, "noinclude");
			if(n) {
				if(line_len > 0) {
					const char *ln_view= wiki_thread_buf_append_to_tokens(line_ptr, line_len);
					token_append_text_n(n, ln_view, line_len);
				} else {
					token_append_text_n(n, "", 0);
				}
				accum_push(accum, n);
				token_append_child(t, n);
			}
		} else {
			Token *tok= NULL;
			if(first) {
				tok= parse_imagemap_image_line_local(line_ptr, line_len, cfg, accum);
				if(tok) {
					token_append_child(t, tok);
					first= false;
					if(!nl) break;
					line_start= i + 1;
					continue;
				}
				error= true;
			}

			/* desc lines are preserved as plain text (JS ImagemapToken parity). */
			size_t word_end= lead;
			while(word_end < line_len && line_ptr[word_end] != ' ' && line_ptr[word_end] != '\t') word_end++;
			if(word_end > lead && (word_end - lead) == 4 && strncmp(line_ptr + lead, "desc", 4) == 0) {
				const char *ln_view= wiki_thread_buf_append_to_tokens(line_ptr, line_len);
				token_append_text_n(t, ln_view, line_len);
				if(!nl) break;
				line_start= i + 1;
				continue;
			}

			const char *p_open = sz_find_byte(line_ptr, line_len, "[");
			if(p_open != NULL) {
				tok= parse_imagemap_link_line_from_open_local(line_ptr, line_len,
					(size_t)(p_open - line_ptr), cfg, accum);
			}

			if(tok) {
				token_append_child(t, tok);
			} else {
				Token *n= token_new(TOKEN_NOINCLUDE, "noinclude");
				if(n) {
					const char *ln_view= wiki_thread_buf_append_to_tokens(line_ptr, line_len);
					token_append_text_n(n, ln_view, line_len);
					accum_push(accum, n);
					token_append_child(t, n);
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
			Token *img= parse_gallery_image_line_local(line_ptr, line_len, cfg, accum, true);
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
																			const char *closing, size_t closing_len,
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
	char *ext_name = strndup(name, name_len);
	t->data.ext.name = (sz_string_view_t){ .start = ext_name, .length = ext_name ? name_len : 0 };
	if(closing && closing_len > 0) {
		char *ext_closing = strndup(closing, closing_len);
		t->data.ext.closing = (sz_string_view_t){ .start = ext_closing, .length = ext_closing ? closing_len : 0 };
	}

	/* Build sub-tokens */
	Token *attrs_tok= build_ext_attrs(lcname, attr, attr_len, accum);
	Token *inner_tok= NULL;
	size_t lc_len = strlen(lcname);
	if(lc_len == 10 && sz_equal(lcname, "references", 10) == sz_true_k && !self_closing) {
		inner_tok= build_references_inner_token(inner, inner_len, cfg, accum);
	} else if(lc_len == 3 && sz_equal(lcname, "pre", 3) == sz_true_k
					&& !self_closing && !ext_attr_is_format_wikitext(attr, attr_len)) {
		inner_tok= build_pre_inner_token(inner, inner_len, accum);
	} else if(lc_len == 15 && sz_equal(lcname, "dynamicpagelist", 15) == sz_true_k && !self_closing) {
		inner_tok= build_param_tag_inner_token(lcname, inner, inner_len, cfg, accum, false);
	} else if(lc_len == 8 && sz_equal(lcname, "inputbox", 8) == sz_true_k && !self_closing) {
		inner_tok= build_param_tag_inner_token(lcname, inner, inner_len, cfg, accum, true);
	} else if(lc_len == 7 && sz_equal(lcname, "gallery", 7) == sz_true_k && !self_closing) {
		inner_tok= build_gallery_inner_token(inner, inner_len, cfg, accum);
	} else if(lc_len == 8 && sz_equal(lcname, "imagemap", 8) == sz_true_k && !self_closing) {
		inner_tok= build_imagemap_inner_token(inner, inner_len, cfg, accum);
	} else if(lc_len == 12 && sz_equal(lcname, "categorytree", 12) == sz_true_k) {
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
		token_append_text_n(t, substr, sub_len);
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
		char *include_closing= str_trim_lc(closing, closing_len);
		t->data.include.closing = (sz_string_view_t){ .start = include_closing,
			.length = include_closing ? strlen(include_closing) : 0 };
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
		token_append_text_n(t, attr, attr_len);
	} else {
		token_append_text_n(t, "", 0);
	}
	if(inner && inner_len > 0) {
		token_append_text_n(t, inner, inner_len);
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
		str_restore_to_tb(segment, len, ctx->st->items, ctx->st->count, tmp_restore);
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
		sz_copy(ctx->out->buf + ctx->out->len, sent, slen);
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
	if(!tb || tb->len == 0) return;

	/* Fast reject: no translate opener means nothing to do in this prepass. */
	if(!find_ci_lit(tb->buf, tb->len, 0, "<translate", 10, NULL)) {
		return;
	}

	TextStack st;
	text_stack_init(&st);
	bool has_nowiki = find_ci_lit(tb->buf, tb->len, 0, "<nowiki", 7, NULL);

	ThreadBuf *out_tb = NULL;
	NowikiScanCtx ctx = { .st = &st, .out = NULL };

	if(has_nowiki) {
		/* Pass 1: handle paired <nowiki>...</nowiki> */
		out_tb = wiki_thread_buf_acquire_scratch();
		if(!out_tb) { log_fatal("thread_buffer: failed to acquire scratch in apply_translate_prepass (pass1)"); abort(); }
		wiki_thread_buf_reserve(out_tb, tb->len * 2 + 64);
		out_tb->len = 0;

		ctx.out = out_tb;
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
	}

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
	str_restore_to_tb(out_tb->buf, out_tb->len, st.items, st.count, tmp_all);
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
	const size_t open_len= 13;
	const size_t close_len= 14;

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
	const char *next_open= pos_open;
	const char *next_close= pos_close;

	char sent_buf[64];

	while(1) {
		if(!next_open) break;

		size_t rel_open= (size_t)(next_open - remaining);
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
			sz_copy(new_tb->buf + new_len, sent_buf, sent_len);
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
			sz_copy(new_tb->buf + new_len, sent_buf, sent_len);
			new_len+= sent_len;

		remaining= next_close + close_len;
		remaining_len= tb->len - (size_t)(remaining - tb->buf);

		next_open= find_substr_cs(remaining, remaining_len, onlyinclude_open, open_len);
		if(!next_open) break;
		size_t tail_after_open= remaining_len - (size_t)(next_open - remaining) - open_len;
		next_close= find_substr_cs(next_open + open_len, tail_after_open, onlyinclude_close, close_len);
	}

		if(remaining_len > 0) {
			size_t noincl_idx= accum->count;
			Token *ni= token_new(TOKEN_NOINCLUDE, "noinclude");
			const char *rem_view = wiki_thread_buf_append_to_tokens(remaining, remaining_len);
			token_append_text_n(ni, rem_view, remaining_len);
			accum_push(accum, ni);
			size_t sent_len;
			work_str_sentinel(noincl_idx, 'n', sent_buf, &sent_len);
			sz_copy(new_tb->buf + new_len, sent_buf, sent_len);
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
        sz_ptr_t ext_name;
        sz_size_t ext_len;
        sz_string_range(&cfg->ext.items[i], &ext_name, &ext_len);
		if(ext_len == 9 && sz_equal(ext_name, "translate", 9) == sz_true_k) {
            has_translate = true;
            break;
        }
    }

    if(include_only) {
		if(handle_onlyinclude(tb, cfg, accum)) {
			return;
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
        sz_copy(out_tb->buf + out_tb->len, tb->buf + search_at, before);
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
			const char *closing = mm.has_close ? (tb->buf + mm.close_s) : NULL;
			size_t clen = mm.has_close ? (mm.close_e - mm.close_s) : 0;
            bool self_closing = !mm.has_close;

			tok = build_ext_token(name, name_len, attr, alen, inner, ilen, closing, clen,
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
            sz_copy(out_tb->buf + out_tb->len, sent_buf, sent_len);
            out_tb->len += sent_len;
        } else {
            ENSURE_CAP(sub_len);
            sz_copy(out_tb->buf + out_tb->len, substr, sub_len);
            out_tb->len += sub_len;
        }

        search_at = mm.mend;
    }

    if(search_at < tb->len) {
        size_t rest = tb->len - search_at;
        ENSURE_CAP(rest + 1);
        sz_copy(out_tb->buf + out_tb->len, tb->buf + search_at, rest);
        out_tb->len += rest;
    }

    out_tb->buf[out_tb->len] = '\0';
    wiki_thread_buf_set(tb, out_tb->buf, out_tb->len);
    wiki_thread_buf_release_scratch(out_tb);

#undef ENSURE_CAP
}
