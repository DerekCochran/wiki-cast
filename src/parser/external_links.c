#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

#include "util/log.h"
#include "parser/external_links.h"
#include "string_util.h"
#include <stringzilla/stringzilla.h>
#include "token.h"
#include "util/pcre_cache.h"
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * JS regex (config version, expanded):
 *
 * \[(
 *   (?:\0\d+[cn]\x7F)*
 *   (?:
 *     \0\d+f\x7F
 *     |
 *     (?:(?:PROTO|//)extUrlCharFirst|\0\d+m\x7F)extUrlChar
 *     (?=[[\]<>"\t ZS]|\0\d)
 *   )
 * )(ZS*(?!ZS))([^\]\x01-\x08\x0A-\x1F\uFFFD]*)\]
 *
 * Where:
 *   ZS (Zs class) = " \xA0\x{1680}\x{2000}-\x{200A}\x{202F}\x{205F}\x{3000}"
 *   extUrlCharFirst = (?:\[[\da-f:.]+\]|[^\[\]<>"\x00-\x1F\x7F ZS \x{FFFD}])
 *   extUrlChar = (?:[^\[\]<>"\x00-\x1F\x7F ZS \x{FFFD}]|\x00\d+[cn!~]\x7F)*
 *
 * Flags: giu → PCRE2_CASELESS | PCRE2_UTF | PCRE2_UCP
 */

/* ZS char class fragment (space separators, for use inside [...]) */
#define ZS_CLASS " \\xA0\\x{1680}\\x{2000}-\\x{200A}\\x{202F}\\x{205F}\\x{3000}"

/* commonExtUrlChar: non-bracket, non-control, non-Zs, non-FFFD */
#define COMMON_EXT \
	"[^\\[\\]<>\"\\x00-\\x1F\\x7F" ZS_CLASS "\\x{FFFD}]"

static const char s_ext_char_first[]=
"(?:\\[[\\da-f:.]+\\]|" COMMON_EXT ")";

static const char s_ext_char[]=
"(?:" COMMON_EXT "|\\x00\\d+[cn!~]\\x7F)*";

/*
 * Build and compile the JS-parity external links regex.
 * config.protocol is substituted where PROTO appears.
 * Result is stored in cfg->regex_external_links.
 */
static pcre2_code *compile_external_links_regex(const ParserConfig *cfg) {
	if(!(cfg && cfg->protocol && cfg->protocol[0])) {
		return NULL;
	}

	/* Lazily build and cache the external-links pattern string in cfg. */
	if(!cfg->pattern_external_links || !cfg->pattern_external_links[0]) {
		const char *proto= cfg->protocol;
		size_t cap= 512 + strlen(proto) + strlen(s_ext_char_first) + strlen(s_ext_char) + 3 * sizeof(ZS_CLASS) + 1;
		char *pat= malloc(cap);
		if(!pat) return NULL;

		snprintf(pat, cap,
				 "\\["
				 "("
				 "(?:\\x00\\d+[cn]\\x7F)*"
				 "(?:"
				 "\\x00\\d+f\\x7F"
				 "|"
				 "(?:(?:%s|//)%s|\\x00\\d+m\\x7F)%s"
				 "(?=[\\[\\]<>\"\\t" ZS_CLASS "]|\\x00\\d)"
				 ")"
				 ")"
				 "([" ZS_CLASS "]*(?![" ZS_CLASS "]))"
				 "([^\\]\\x01-\\x08\\x0A-\\x1F\\x{FFFD}]*)"
				 "\\]",
				 proto, s_ext_char_first, s_ext_char);

		((ParserConfig *)cfg)->pattern_external_links = pat;
	}

	return pcre_cache_get(cfg->pattern_external_links, PCRE2_CASELESS | PCRE2_UTF | PCRE2_UCP);
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

void parse_external_links(ThreadBuf *tb, const ParserConfig *cfg,
													Accum *accum, bool in_file) {
	if(!tb || !tb->buf) return;

	/* Compile this call's protocol-aware regex (cached by pattern by pcre_cache). */
	pcre2_code *re = compile_external_links_regex(cfg);
	if(!re) return;

	pcre2_match_data *md= pcre2_match_data_create_from_pattern(re, NULL);
	if(!md) return;

	size_t out_cap= tb->len * 2 + 64;
	char *out_buf= malloc(out_cap);
	assert(out_buf);
	size_t out_len= 0;
	size_t search_at= 0;

#define ENSURE_CAP(need)                         \
	do {                                           \
		while(out_len + (size_t)(need) >= out_cap) { \
			out_cap*= 2;                               \
			out_buf= realloc(out_buf, out_cap);        \
			assert(out_buf);                           \
		}                                            \
	} while(0)

	while(search_at <= tb->len) {
		int rc= pcre2_match(re, (PCRE2_SPTR)tb->buf, (PCRE2_SIZE)tb->len,
												(PCRE2_SIZE)search_at, 0, md, NULL);
		if(rc <= 0) {
			/* No more matches — copy the rest unchanged */
			size_t rest= tb->len - search_at;
			ENSURE_CAP(rest + 1);
			memcpy(out_buf + out_len, tb->buf + search_at, rest);
			out_len+= rest;
			break;
		}

		PCRE2_SIZE *ov= pcre2_get_ovector_pointer(md);
		size_t mstart= ov[0], mend= ov[1];

		/* Copy text before this match */
		size_t before= mstart - search_at;
		ENSURE_CAP(before + 128);
		memcpy(out_buf + out_len, tb->buf + search_at, before);
		out_len+= before;

		/* Extract the 3 groups */
		const char *url_ptr= (ov[2] != PCRE2_UNSET) ? tb->buf + ov[2] : NULL;
		size_t url_len_v= (ov[2] != PCRE2_UNSET && ov[3] > ov[2]) ? ov[3] - ov[2] : 0;
		const char *spc_ptr= (ov[4] != PCRE2_UNSET) ? tb->buf + ov[4] : NULL;
		size_t spc_len= (ov[4] != PCRE2_UNSET && ov[5] > ov[4]) ? ov[5] - ov[4] : 0;
		const char *txt_ptr= (ov[6] != PCRE2_UNSET) ? tb->buf + ov[6] : NULL;
		size_t txt_len= (ov[6] != PCRE2_UNSET && ov[7] > ov[6]) ? ov[7] - ov[6] : 0;

		if(!url_ptr || url_len_v == 0) {
			/* Shouldn't happen; copy original */
			ENSURE_CAP(mend - mstart);
			memcpy(out_buf + out_len, tb->buf + mstart, mend - mstart);
			out_len+= mend - mstart;
			search_at= mend + (mend == mstart ? 1 : 0);
			continue;
		}

		/*
         * JS post-match: const mt = /&[lg]t;/u.exec(url);
         * if (mt) { space = ''; text = url.slice(mt.index) + space + text; url = url.slice(0, mt.index); }
         *
         * If url contains &lt; or &gt;, truncate url at that point and
         * prepend the rest onto text (space is cleared).
         */
		{
			/* Scan url for "&lt;" or "&gt;" — search for '&' quickly. */
			size_t truncate_at= url_len_v; /* default: no truncation */
			size_t search_pos = 0;
			while(search_pos + 3 < url_len_v) {
				const char needle = '&';
				const char *amp = sz_find_byte(url_ptr + search_pos, url_len_v - search_pos, &needle);
				if(!amp) break;
				size_t i = (size_t)(amp - url_ptr);
				size_t rem = url_len_v - i;
				if(rem >= 4 &&
				   ((strncasecmp(url_ptr + i, "&lt;", 4) == 0) ||
					(strncasecmp(url_ptr + i, "&gt;", 4) == 0))) {
					truncate_at = i;
					break;
				}
				search_pos = i + 1;
			}

			if(truncate_at < url_len_v) {
				/* The rest of the URL becomes part of text — build a combined text buffer */
				size_t extra= url_len_v - truncate_at; /* "&lt;..." portion */
				size_t new_txt_l= extra + txt_len;
				char *new_txt= malloc(new_txt_l + 1);
				if(new_txt) {
					memcpy(new_txt, url_ptr + truncate_at, extra);
					if(txt_ptr && txt_len > 0) memcpy(new_txt + extra, txt_ptr, txt_len);
					new_txt[new_txt_l]= '\0';
					/* Adjust */
					url_len_v= truncate_at;
					txt_ptr= new_txt;
					txt_len= new_txt_l;
					spc_len= 0; /* space = '' */

					/* Build tokens with adjusted values */
					size_t accum_before= accum->count;
					Token *url_tok= build_magic_link_token(url_ptr, url_len_v, accum);
					if(!url_tok) {
						free(new_txt);
						ENSURE_CAP(mend - mstart);
						memcpy(out_buf + out_len, tb->buf + mstart, mend - mstart);
						out_len+= mend - mstart;
						search_at= mend + (mend == mstart ? 1 : 0);
						continue;
					}

					if(in_file) {
						size_t sent_idx= accum_before;
						char sent_buf[64];
						size_t slen;
						work_str_sentinel(sent_idx, 'f', sent_buf, &slen);
						ENSURE_CAP(1 + slen + txt_len + 1 + 1);
						out_buf[out_len++]= '[';
						memcpy(out_buf + out_len, sent_buf, slen);
						out_len+= slen;
						if(txt_len > 0) {
							memcpy(out_buf + out_len, txt_ptr, txt_len);
							out_len+= txt_len;
						}
						out_buf[out_len++]= ']';
					} else {
						Token *ext= build_ext_link_token(url_tok, NULL, 0, txt_ptr, txt_len, accum);
						if(!ext) {
							free(new_txt);
							ENSURE_CAP(mend - mstart);
							memcpy(out_buf + out_len, tb->buf + mstart, mend - mstart);
							out_len+= mend - mstart;
							search_at= mend + (mend == mstart ? 1 : 0);
							continue;
						}
						size_t ext_idx= accum->count - 1;
						char sent_buf[64];
						size_t slen;
						work_str_sentinel(ext_idx, 'w', sent_buf, &slen);
						ENSURE_CAP(slen);
						memcpy(out_buf + out_len, sent_buf, slen);
						out_len+= slen;
					}

					free(new_txt);
					search_at= mend + (mend == mstart ? 1 : 0);
					continue;
				}
				/* malloc failed — fall through to normal handling */
			}
		}

		/* Normal handling (no &lt;/&gt; truncation) */
		{
			size_t accum_before= accum->count;
			Token *url_tok= NULL;
			size_t f_sentinel_idx;
			/* JS parity: when !in_file and URL is a pure \0<N>f\x7F sentinel,
			 * reuse the existing MagicLinkToken from accum[N] (mirrors ExtLinkToken
			 * constructor) rather than wrapping it in an extra ext-link-url node. */
			if(!in_file && try_parse_f_sentinel(url_ptr, url_len_v, &f_sentinel_idx)
							&& f_sentinel_idx < accum->count) {
				url_tok= accum->tokens[f_sentinel_idx];
			} else {
				url_tok= build_magic_link_token(url_ptr, url_len_v, accum);
				if(!url_tok) {
					ENSURE_CAP(mend - mstart);
					memcpy(out_buf + out_len, tb->buf + mstart, mend - mstart);
					out_len+= mend - mstart;
					search_at= mend + (mend == mstart ? 1 : 0);
					continue;
				}
			}

			if(in_file) {
				/*
                 * JS: return `[\0${length}f\x7F${space}${text}]`;
                 */
				size_t sent_idx= accum_before;
				char sent_buf[64];
				size_t slen;
				work_str_sentinel(sent_idx, 'f', sent_buf, &slen);
				ENSURE_CAP(1 + slen + spc_len + txt_len + 1 + 1);
				out_buf[out_len++]= '[';
				memcpy(out_buf + out_len, sent_buf, slen);
				out_len+= slen;
				if(spc_ptr && spc_len > 0) {
					memcpy(out_buf + out_len, spc_ptr, spc_len);
					out_len+= spc_len;
				}
				if(txt_ptr && txt_len > 0) {
					memcpy(out_buf + out_len, txt_ptr, txt_len);
					out_len+= txt_len;
				}
				out_buf[out_len++]= ']';
			} else {
				/*
                 * JS: new ExtLinkToken(url, space, text, config, accum);
                 *     return `\0${length}w\x7F`;
                 */
				Token *ext= build_ext_link_token(url_tok, spc_ptr, spc_len, txt_ptr, txt_len, accum);
				if(!ext) {
					ENSURE_CAP(mend - mstart);
					memcpy(out_buf + out_len, tb->buf + mstart, mend - mstart);
					out_len+= mend - mstart;
					search_at= mend + (mend == mstart ? 1 : 0);
					continue;
				}
				size_t ext_idx= accum->count - 1;
				char sent_buf[64];
				size_t slen;
				work_str_sentinel(ext_idx, 'w', sent_buf, &slen);
				ENSURE_CAP(slen);
				memcpy(out_buf + out_len, sent_buf, slen);
				out_len+= slen;
			}
		}

		search_at= mend + (mend == mstart ? 1 : 0);
	}

	out_buf[out_len]= '\0';
	wiki_thread_buf_set(tb, out_buf, out_len);
	free(out_buf);

	pcre2_match_data_free(md);
}
