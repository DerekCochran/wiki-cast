#include "util/log.h"
#include "parser/external_links.h"
#include "util/string_util.h"
#include <stringzilla/stringzilla.h>
#include "token.h"
#include "util/wiki_parser_rules.h"
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* JS parity helpers for external-links URL grammar */
static size_t skip_typed_sentinel(const char *s, size_t len, size_t i, char type) {
    if(i + 3 >= len || (unsigned char)s[i] != 0) return 0;
    size_t j = i + 1;
    if((unsigned char)s[j] < '0' || (unsigned char)s[j] > '9') return 0;
    while(j < len && (unsigned char)s[j] >= '0' && (unsigned char)s[j] <= '9') j++;
    if(j + 1 >= len) return 0;
    if(s[j] != type || (unsigned char)s[j + 1] != 0x7F) return 0;
    return j + 2 - i;
}

static size_t skip_cn_sentinel(const char *s, size_t len, size_t i) {
    size_t sc = skip_typed_sentinel(s, len, i, 'c');
    if(sc > 0) return sc;
    return skip_typed_sentinel(s, len, i, 'n');
}

/* extUrlChar allows only \x00\d+[cn!~]\x7F */
static size_t skip_exturl_sentinel(const char *s, size_t len, size_t i) {
    size_t sc = skip_typed_sentinel(s, len, i, 'c');
    if(sc > 0) return sc;
    sc = skip_typed_sentinel(s, len, i, 'n');
    if(sc > 0) return sc;
    sc = skip_typed_sentinel(s, len, i, '!');
    if(sc > 0) return sc;
    return skip_typed_sentinel(s, len, i, '~');
}

/* JS zs = " \xA0\u1680\u2000-\u200A\u202F\u205F\u3000" */
static size_t consume_js_zs(const char *s, size_t len, size_t i) {
    if(i >= len) return 0;
    unsigned char c0 = (unsigned char)s[i];
    if(c0 == 0x20) return 1; /* U+0020 */
    if(i + 1 < len && c0 == 0xC2 && (unsigned char)s[i + 1] == 0xA0) return 2; /* U+00A0 */
    if(i + 2 < len && c0 == 0xE1 && (unsigned char)s[i + 1] == 0x9A && (unsigned char)s[i + 2] == 0x80) return 3; /* U+1680 */
    if(i + 2 < len && c0 == 0xE2 && (unsigned char)s[i + 1] == 0x80 &&
       (unsigned char)s[i + 2] >= 0x80 && (unsigned char)s[i + 2] <= 0x8A) return 3; /* U+2000..U+200A */
    if(i + 2 < len && c0 == 0xE2 && (unsigned char)s[i + 1] == 0x80 && (unsigned char)s[i + 2] == 0xAF) return 3; /* U+202F */
    if(i + 2 < len && c0 == 0xE2 && (unsigned char)s[i + 1] == 0x81 && (unsigned char)s[i + 2] == 0x9F) return 3; /* U+205F */
    if(i + 2 < len && c0 == 0xE3 && (unsigned char)s[i + 1] == 0x80 && (unsigned char)s[i + 2] == 0x80) return 3; /* U+3000 */
    return 0;
}

static bool ext_lookahead_ok(const char *s, size_t len, size_t i) {
    if(i >= len) return true; /* closing ']' is outside parser_scan inner segment */
    unsigned char c = (unsigned char)s[i];
    if(c == '[' || c == ']' || c == '<' || c == '>' || c == '"') return true;
    if(c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v') return true;
    if(consume_js_zs(s, len, i) > 0) return true;
    if(c == 0 && i + 1 < len && s[i + 1] >= '0' && s[i + 1] <= '9') return true;
    return false;
}

/* JS text group: [^\]\x01-\x08\x0A-\x1F\uFFFD]* */
static bool ext_text_is_valid(const char *s, size_t len) {
    for(size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)s[i];
        if(c == ']') return false;
        if(c >= 0x01 && c <= 0x08) return false;
        if(c >= 0x0A && c <= 0x1F) return false;
        if(i + 2 < len && c == 0xEF && (unsigned char)s[i + 1] == 0xBF && (unsigned char)s[i + 2] == 0xBD)
            return false; /* U+FFFD */
    }
    return true;
}

/* Build a minimal magic-link-url token (URL child of an ext-link or in-file marker).
 * Returns the token (already pushed to accum). */
static Token *build_magic_link_token(const char *url, size_t url_len, Accum *accum) {
	Token *t= token_new(TOKEN_MAGIC_LINK, "ext-link-url");
	if(!t) return NULL;
	/* Append URL into the persistent tokens arena and use a stable view */
	const char *url_view = wiki_thread_buf_append_to_tokens(url, url_len);
	token_append_text_n(t, url_view, url_len);
	accum_push(accum, t);
	return t;
}

/* Build an ext-link token: TOKEN_EXT_LINK containing [url_tok, (opt) ext-link-text] */
static Token *build_ext_link_token(Token *url_tok,
																	 const char *space, size_t space_len,
																	 const char *text, size_t text_len,
																	 Accum *accum) {
	Token *ext= token_new(TOKEN_EXT_LINK, "ext-link");
	if(!ext) return NULL;
	/* store separator as owned string for now (refactor later to use tokens arena) */
	ext->data.ext_link.space= malloc(space_len + 1);
	if(!ext->data.ext_link.space) {
		token_free(ext);
		return NULL;
	}
	if(space && space_len > 0) memcpy(ext->data.ext_link.space, space, space_len);
	ext->data.ext_link.space[space_len]= '\0';

	token_append_child(ext, url_tok);

	if(text_len > 0) {
		Token *inner= token_new(TOKEN_PLAIN, "ext-link-text");
		if(!inner) {
			token_free(ext);
			return NULL;
		}
		/* Ensure the ext-link-text points into the persistent tokens arena */
		const char *text_view = wiki_thread_buf_append_to_tokens(text, text_len);
		token_append_text_n(inner, text_view, text_len);
		accum_push(accum, inner);
		token_append_child(ext, inner);
	}

	accum_push(accum, ext);
	return ext;
}

/* JS parity: ExtLinkToken constructor checks
 *   /^\0\d+f\x7F$/.test(url)
 * and, if true, reuses accum[N] (the existing MagicLinkToken) rather than creating
 * a new wrapper token.  This happens in the second-pass parseExternalLinks call on
 * image-parameter caption tokens that were already pre-processed with inFile=true.
 */
static bool try_parse_f_sentinel(const char *ptr, size_t len, size_t *out_idx) {
	if(len < 4) return false;
	if((unsigned char)ptr[0] != 0x00) return false;
	if(ptr[len - 2] != 'f') return false;
	if((unsigned char)ptr[len - 1] != 0x7F) return false;
	size_t idx= 0;
	for(size_t i= 1; i + 2 < len; i++) {
		char c= ptr[i];
		if(c < '0' || c > '9') return false;
		idx= idx * 10 + (size_t)(c - '0');
	}
	*out_idx= idx;
	return true;
}

static bool parse_external_inner(const char *inner, size_t len,
                                 const ParserConfig *cfg,
                                 const char **url_ptr, size_t *url_len,
                                 const char **space_ptr, size_t *space_len,
                                 const char **text_ptr, size_t *text_len) {
    /* Leading (?:\x00\d+[cn]\x7F)* */
    size_t i = 0;
    while(i < len) {
        size_t sc = skip_cn_sentinel(inner, len, i);
        if(sc == 0) break;
        i += sc;
    }

    size_t u0 = i;
    size_t sf = skip_typed_sentinel(inner, len, i, 'f');
    if(sf > 0) {
        i += sf;
    } else {
        bool prefixed = false;
        size_t sm = skip_typed_sentinel(inner, len, i, 'm');
        if(sm > 0) {
            i += sm;
            prefixed = true;
        }

        if(!prefixed) {
            size_t pfx = match_proto_prefix(inner + i, len - i, cfg);
            if(pfx > 0) {
                i += pfx;
            } else if(i + 1 < len && inner[i] == '/' && inner[i + 1] == '/') {
                i += 2;
            } else {
                return false;
            }

            if(i >= len || !is_url_common_byte((unsigned char)inner[i])) return false;
            i++;
        }

        while(i < len) {
            size_t sc = skip_exturl_sentinel(inner, len, i);
            if(sc > 0) { i += sc; continue; }
            if(!is_url_common_byte((unsigned char)inner[i])) break;
            i++;
        }

        if(!ext_lookahead_ok(inner, len, i)) return false;
    }

    if(i <= u0) return false;

    *url_ptr = inner + u0;
    *url_len = i - u0;

    size_t sp0 = i;
    while(i < len) {
        size_t zs = consume_js_zs(inner, len, i);
        if(zs == 0) break;
        i += zs;
    }
    *space_ptr = inner + sp0;
    *space_len = i - sp0;

    *text_ptr = inner + i;
    *text_len = len - i;
    if(!ext_text_is_valid(*text_ptr, *text_len)) return false;
    return true;
}

typedef struct {
    ThreadBuf *out;
    const ParserConfig *cfg;
    Accum *accum;
    bool in_file;
} ExtCtx;

static void ext_cb(const char *seg, size_t len, ParserSegmentKind kind, void *ud) {
    ExtCtx *c = (ExtCtx *)ud;
    if(kind == PARSER_SEG_TEXT) {
        wiki_thread_buf_append(c->out, (sz_string_view_t){ .start = seg, .length = len });
        return;
    }

    const char *url = NULL, *sp = NULL, *txt = NULL;
    size_t ulen = 0, splen = 0, tlen = 0;
    if(!parse_external_inner(seg, len, c->cfg, &url, &ulen, &sp, &splen, &txt, &tlen)) {
        wiki_thread_buf_append(c->out, (sz_string_view_t){ .start = "[", .length = 1 });
        wiki_thread_buf_append(c->out, (sz_string_view_t){ .start = seg, .length = len });
        wiki_thread_buf_append(c->out, (sz_string_view_t){ .start = "]", .length = 1 });
        return;
    }

    /* JS parity: split URL at first &lt; / &gt; entity and move suffix into text. */
    ThreadBuf *entity_text = NULL;
    for(size_t i = 0; i + 3 < ulen; i++) {
        if(url[i] != '&') continue;
        bool is_lt = (url[i + 1] == 'l' && url[i + 2] == 't' && url[i + 3] == ';');
        bool is_gt = (url[i + 1] == 'g' && url[i + 2] == 't' && url[i + 3] == ';');
        if(!is_lt && !is_gt) continue;

        entity_text = wiki_thread_buf_acquire_scratch();
        if(!entity_text) { log_fatal("thread_buffer: failed to acquire scratch in ext_cb"); abort(); }
        entity_text->len = 0;
        wiki_thread_buf_append(entity_text, (sz_string_view_t){ .start = url + i, .length = ulen - i });
        if(tlen > 0) wiki_thread_buf_append(entity_text, (sz_string_view_t){ .start = txt, .length = tlen });

        ulen = i;
        sp = "";
        splen = 0;
        txt = entity_text->buf;
        tlen = entity_text->len;
        break;
    }

    /* reuse existing build_magic_link_token/build_ext_link_token logic exactly */
    size_t before = c->accum->count;
    Token *url_tok = NULL;
    size_t f_idx = 0;
    if(!c->in_file && try_parse_f_sentinel(url, ulen, &f_idx) && f_idx < c->accum->count) {
        url_tok = c->accum->tokens[f_idx];
    } else {
        url_tok = build_magic_link_token(url, ulen, c->accum);
    }
    if(!url_tok) {
        wiki_thread_buf_append(c->out, (sz_string_view_t){ .start = "[", .length = 1 });
        wiki_thread_buf_append(c->out, (sz_string_view_t){ .start = seg, .length = len });
        wiki_thread_buf_append(c->out, (sz_string_view_t){ .start = "]", .length = 1 });
        if(entity_text) wiki_thread_buf_release_scratch(entity_text);
        return;
    }

    if(c->in_file) {
        char sent[64]; size_t slen = 0;
        work_str_sentinel(before, 'f', sent, &slen);
        wiki_thread_buf_append(c->out, (sz_string_view_t){ .start = "[", .length = 1 });
        wiki_thread_buf_append(c->out, (sz_string_view_t){ .start = sent, .length = slen });
        if(splen) wiki_thread_buf_append(c->out, (sz_string_view_t){ .start = sp, .length = splen });
        if(tlen) wiki_thread_buf_append(c->out, (sz_string_view_t){ .start = txt, .length = tlen });
        wiki_thread_buf_append(c->out, (sz_string_view_t){ .start = "]", .length = 1 });
    } else {
        Token *ext = build_ext_link_token(url_tok, sp, splen, txt, tlen, c->accum);
        if(!ext) {
            wiki_thread_buf_append(c->out, (sz_string_view_t){ .start = "[", .length = 1 });
            wiki_thread_buf_append(c->out, (sz_string_view_t){ .start = seg, .length = len });
            wiki_thread_buf_append(c->out, (sz_string_view_t){ .start = "]", .length = 1 });
            return;
        }
        char sent[64]; size_t slen = 0;
        work_str_sentinel(c->accum->count - 1, 'w', sent, &slen);
        wiki_thread_buf_append(c->out, (sz_string_view_t){ .start = sent, .length = slen });
    }

    if(entity_text) wiki_thread_buf_release_scratch(entity_text);
}

void parse_external_links(ThreadBuf *tb, const ParserConfig *cfg,
                         Accum *accum, bool in_file) {
    if(!tb || !tb->buf) return;

    /* Get the rule by stable name */
    const ParserRules *rule = wiki_parser_rules_get("rule-extlink-bracket");
    if(!rule) {
        /* Fallback to direct reference if name lookup fails */
        rule = &wiki_rule_extlink_bracket;
    }

    /* Prepare output buffer */
    ThreadBuf *out = wiki_thread_buf_acquire_scratch();
    if(!out) return;

    /* Initialize output buffer length to 0 for accumulation */
    out->len = 0;

    ExtCtx ctx = {
        .out = out,
        .cfg = cfg,
        .accum = accum,
        .in_file = in_file
    };

    /* Run callback scanner */
    parser_scan(tb->buf, tb->len, rule, ext_cb, &ctx);

    /* JS parity: with /\[(...?)\]/g, input like [[http://example.com]] or
     * [[http://example.com] can still yield an inner [http://...] ext-link
     * (the outer leading '[' and any remaining trailing ']' stay as plain text).
     * parser_scan can miss these overlapping-start cases, so run a focused
     * second pass for "[[..." forms by probing from the second '[' to the
     * first following ']'. */
    if(!in_file && out->len >= 4) {
        ThreadBuf *out2 = wiki_thread_buf_acquire_scratch();
        if(out2) {
            out2->len = 0;
            const char *src = out->buf;
            size_t slen = out->len;
            size_t last = 0;
            size_t i = 0;

            while(i + 1 < slen) {
                if(src[i] == '[' && src[i + 1] == '[') {
                    size_t j = i + 2;
                    while(j < slen && src[j] != ']') j++;

                    if(j < slen) {
                        const char *u = NULL, *sp = NULL, *txt = NULL;
                        size_t ulen = 0, splen = 0, tlen = 0;
                        const char *inner = src + i + 2;
                        size_t inner_len = j - (i + 2);
                        if(parse_external_inner(inner, inner_len, cfg, &u, &ulen, &sp, &splen, &txt, &tlen)) {
                            if(i > last) {
                                wiki_thread_buf_append(out2, (sz_string_view_t){ .start = src + last, .length = i - last });
                            }
                            wiki_thread_buf_append(out2, (sz_string_view_t){ .start = "[", .length = 1 });
                            ExtCtx inner_ctx = {
                                .out = out2,
                                .cfg = cfg,
                                .accum = accum,
                                .in_file = false
                            };
                            ext_cb(inner, inner_len, PARSER_SEG_INNER, &inner_ctx);
                            i = j + 1;
                            last = i;
                            continue;
                        }
                    }
                }
                i++;
            }

            if(last < slen) wiki_thread_buf_append(out2, (sz_string_view_t){ .start = src + last, .length = slen - last });
            wiki_thread_buf_set(out, out2->buf, out2->len);
            wiki_thread_buf_release_scratch(out2);
        }
    }

    /* Replace input buffer with output */
    wiki_thread_buf_set(tb, out->buf, out->len);
    wiki_thread_buf_release_scratch(out);
}
