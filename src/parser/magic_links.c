#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

#include "util/log.h"
#include "parser/magic_links.h"
#include "util/string_util.h"
#include "token.h"
#include "util/pcre_cache.h"
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * JS variables (from util/string.js and magicLinks.js):
 *
 *   zs = " \xA0\u1680\u2000-\u200A\u202F\u205F\u3000"
 *   space = `[${zs}\t]|&nbsp;|&#0*160;|&#x0*a0;`
 *   sp = `(?:${space})+`
 *   spdash = `(?:${space}|-)`
 *   extUrlCharFirst = `(?:\[[\da-f:.]+\]|${commonExtUrlChar})`
 *   extUrlChar = `(?:${commonExtUrlChar}|\0\d+[cn!~]\x7F)*`
 *   commonExtUrlChar = `[^[\]<>"\x00-\x1F\x7F ZS \uFFFD]`
 *
 * JS main regex (try-branch):
 *   (^|[^\p{L}\p{N}_])(?:(?:PROTO)(extUrlCharFirst extUrlChar)|magicLinkPattern)
 *   flags: giu
 *
 * JS fallback regex (catch-branch):
 *   (^|\W)(?:(?:PROTO)(extUrlCharFirst extUrlChar)|magicLinkPattern_fallback)
 *   flags: giu
 */

/* ZS char class (space separator Unicode category + common named chars) */
#define ZS_CLASS " \\xA0\\x{1680}\\x{2000}-\\x{200A}\\x{202F}\\x{205F}\\x{3000}"

/* commonExtUrlChar: not bracket/control/Zs/FFFD */
#define COMMON_EXT "[^\\[\\]<>\"\\x00-\\x1F\\x7F" ZS_CLASS "\\x{FFFD}]"

/* JS: extUrlCharFirst */
static const char s_ext_char_first[]=
"(?:\\[[\\da-f:.]+\\]|" COMMON_EXT ")";

/* JS: extUrlChar */
static const char s_ext_char[]=
"(?:" COMMON_EXT "|\\x00\\d+[cn!~]\\x7F)*";

/*
 * JS sp  = (?:[ZS\t]|&nbsp;|&#0*160;|&#x0*a0;)+
 * JS spdash = (?:[ZS\t]|&nbsp;|&#0*160;|&#x0*a0;|-)
 */
#define SP "(?:[" ZS_CLASS "\\t]|&nbsp;|&#0*160;|&#x0*a0;)+"
#define SPDASH "(?:[" ZS_CLASS "\\t]|&nbsp;|&#0*160;|&#x0*a0;|-)"

/*
 * JS magicLinkPattern:
 *   (?:RFC|PMID)sp\d+\b|ISBNsp(?:97[89]spdash?)?(?:\dspdash?){9}[\dx]\b
 *
 * (case-insensitive flag 'i' handles rfc/pmid/isbn)
 */
static const char s_magic_pat[]=
"(?:RFC|PMID)" SP "\\d+\\b"
									"|ISBN" SP "(?:97[89]" SPDASH "?)?(?:\\d" SPDASH "?){9}[\\dx]\\b";

/* Compile the magic-links regex lazily.
 * First tries the Unicode-aware pattern (PCRE2_UTF|PCRE2_UCP) using cfg->protocol.
 * Falls back to simpler pattern (no \p{}) if compilation fails. */
static pcre2_code *compile_magic_regex(const ParserConfig *cfg) {
	if(!(cfg && cfg->protocol && cfg->protocol[0])) return NULL;

	static int s_has_unicode = -1;
	if(s_has_unicode < 0) {
		int val = 0;
		s_has_unicode = (pcre2_config(PCRE2_CONFIG_UNICODE, &val) == 0 && val != 0) ? 1 : 0;
	}

	/* Lazily build and cache the magic-links pattern string in ParserConfig. */
	if(!cfg->pattern_magic_links || !cfg->pattern_magic_links[0]) {
		const char *proto = cfg->protocol;
		if(s_has_unicode) {
			size_t pat_cap = 128 + strlen(proto) + strlen(s_ext_char_first) + strlen(s_ext_char) + strlen(s_magic_pat) + 3 * sizeof(ZS_CLASS) + 1;
			char *pattern = malloc(pat_cap);
			assert(pattern);
			snprintf(pattern, pat_cap,
					 "(^|[^\\p{L}\\p{N}_])(?:(?:%s)(%s%s)|%s)",
					 proto, s_ext_char_first, s_ext_char, s_magic_pat);
			((ParserConfig *)cfg)->pattern_magic_links = pattern;
		} else {
			const char *magic_ascii=
				"(?:RFC|PMID)[\\s\\t]+\\d+\\b"
				"|ISBN[\\s\\t]+(?:97[89][\\s\\t-]?)?(?:\\d[\\s\\t-]?){9}[\\dx]\\b";
			const char *ext_first_ascii= "(?:\\[[\\da-f:.]+\\]|[^\\[\\]<>\"\\s])";
			const char *ext_char_ascii= "(?:[^\\[\\]<>\"\\x00\\s]|\\x00\\d+[cn!~]\\x7F)*";
			size_t pat_cap = 64 + strlen(proto) + strlen(ext_first_ascii) + strlen(ext_char_ascii) + strlen(magic_ascii) + 1;
			char *pattern = malloc(pat_cap);
			assert(pattern);
			snprintf(pattern, pat_cap,
					 "(^|\\W)(?:(?:%s)(%s%s)|%s)",
					 proto, ext_first_ascii, ext_char_ascii, magic_ascii);
			((ParserConfig *)cfg)->pattern_magic_links = pattern;
		}
	}

	if(s_has_unicode) {
		return pcre_cache_get(cfg->pattern_magic_links, PCRE2_CASELESS | PCRE2_UTF | PCRE2_UCP);
	}
	return pcre_cache_get(cfg->pattern_magic_links, PCRE2_CASELESS);
}

/* Build a MagicLinkToken and push to accum. */
static Token *build_magic_link(const char *s, size_t len,
															 const char *type_name, Accum *accum) {
	Token *t= token_new(TOKEN_MAGIC_LINK, type_name);
	if(!t) return NULL;
	/* Append text into the persistent tokens arena and use a stable view */
	const char *view = wiki_thread_buf_append_to_tokens(s, len);
	token_append_text_n(t, view, len);
	accum_push(accum, t);
	return t;
}

void parse_magic_links(ThreadBuf *tb, const ParserConfig *cfg, Accum *accum) {
	if(!tb || !tb->buf) return;

	/* Obtain cached compiled regex for this call (owned by pcre_cache). */
	pcre2_code *re = compile_magic_regex(cfg);
	if(!re) return;
	pcre2_match_data *md = pcre2_match_data_create_from_pattern(re, NULL);
	if(!md) return;

	size_t out_cap= tb->len * 2 + 64;
	ThreadBuf *tmp_out = wiki_thread_buf_acquire_scratch();
	char *out_buf = NULL;
	if(tmp_out) {
		wiki_thread_buf_reserve(tmp_out, out_cap);
		out_buf = tmp_out->buf;
	} else {
		out_buf= malloc(out_cap);
		assert(out_buf);
	}
	size_t out_len= 0;
	size_t search_at= 0;

#define ENSURE_CAP(need)                                         \
	do {                                                           \
		while(out_len + (size_t)(need) >= out_cap) {               \
			out_cap*= 2;                                           \
			if(tmp_out) {                                           \
				wiki_thread_buf_reserve(tmp_out, out_cap);         \
				out_buf = tmp_out->buf;                            \
			} else {                                               \
				out_buf= realloc(out_buf, out_cap);                \
				assert(out_buf);                                   \
			}                                                      \
		}                                                          \
	} while(0)

	while(search_at <= tb->len) {
		int rc= pcre2_match(re, (PCRE2_SPTR)tb->buf, (PCRE2_SIZE)tb->len,
												(PCRE2_SIZE)search_at, 0, md, NULL);
		if(rc <= 0) {
			size_t rest= tb->len - search_at;
			ENSURE_CAP(rest + 1);
			memcpy(out_buf + out_len, tb->buf + search_at, rest);
			out_len+= rest;
			break;
		}

		PCRE2_SIZE *ov= pcre2_get_ovector_pointer(md);
		size_t mstart= ov[0], mend= ov[1];

		/* Copy text before match */
		size_t before= mstart - search_at;
		ENSURE_CAP(before + 64);
		memcpy(out_buf + out_len, tb->buf + search_at, before);
		out_len+= before;

		/* group 1: lead */
		size_t lead_s= (rc > 1 && ov[2] != PCRE2_UNSET) ? ov[2] : mstart;
		size_t lead_e= (rc > 1 && ov[3] != PCRE2_UNSET) ? ov[3] : mstart;

		/* group 2: p1 (URL body, after protocol) */
		bool has_p1= (rc > 2 && ov[4] != PCRE2_UNSET);
		size_t url_body_s= has_p1 ? ov[4] : 0;
		size_t url_body_e= has_p1 ? ov[5] : 0;

		/* Emit the lead character */
		if(lead_e > lead_s) {
			size_t llen= lead_e - lead_s;
			ENSURE_CAP(llen + 2);
			memcpy(out_buf + out_len, tb->buf + lead_s, llen);
			out_len+= llen;
		}

		if(has_p1) {
			/*
             * URL case.
             * JS: let url = m.slice(lead.length)  → whole match minus lead
             *     p1 = capture group 2 (URL body without protocol)
             *
             * Apply JS post-match trimming:
             * 1) &[lg]t;/&nbsp;/&#... entity truncation
             * 2) trailing punctuation stripping
             * 3) if trail.length >= p1.length → bail out (return m unchanged)
             */
			const char *url_ptr= tb->buf + lead_e;
			size_t url_len= mend - lead_e;
			size_t p1_len= url_body_e - url_body_s;

			/* --- Entity truncation ---
             * JS regex exactly: /&(?:[lg]t|nbsp|#x0*(?:3[ce]|a0)|#0*(?:6[02]|160));/iu
             * Only these specific entities truncate the URL:
             *   &lt;  &gt;  &nbsp;
             *   &#x3c; &#x3e; &#xa0; (and with leading zeros)
             *   &#60;  &#62;  &#160; (and with leading zeros)
             */
			size_t entity_at= url_len; /* no truncation by default */
			for(size_t k= 0; k < url_len && k + 1 < url_len; k++) {
				if(url_ptr[k] != '&') continue;
				size_t rem= url_len - k;

				/* &lt; or &gt; — exactly 4 chars */
				if(rem >= 4 && url_ptr[k + 3] == ';' &&
					 ((strncasecmp(url_ptr + k + 1, "lt", 2) == 0) ||
						(strncasecmp(url_ptr + k + 1, "gt", 2) == 0))) {
					entity_at= k;
					break;
				}
				/* &nbsp; — exactly 6 chars */
				if(rem >= 6 && strncasecmp(url_ptr + k, "&nbsp;", 6) == 0) {
					entity_at= k;
					break;
				}
				/* &#x...;  — JS: #x0*(?:3[ce]|a0) → &#x3c; &#x3e; &#xa0; with optional leading zeros */
				if(rem >= 5 && url_ptr[k + 1] == '#' && (url_ptr[k + 2] == 'x' || url_ptr[k + 2] == 'X')) {
					/* skip leading zeros after #x */
					size_t j= k + 3;
					while(j < url_len && url_ptr[j] == '0') j++;
					size_t hex_start= j;
					/* match: 3c, 3e, a0 (case-insensitive) */
					bool ok= false;
					if(j + 2 < url_len && url_ptr[j + 2] == ';') {
						char h0= url_ptr[j], h1= url_ptr[j + 1];
						/* lowercase */
						if(h0 >= 'A' && h0 <= 'Z') h0 += 32;
						if(h1 >= 'A' && h1 <= 'Z') h1 += 32;
						ok= (h0 == '3' && (h1 == 'c' || h1 == 'e')) ||
								(h0 == 'a' && h1 == '0');
					}
					if(ok) { entity_at= k; break; }
					(void)hex_start;
					continue;
				}
				/* &#...;  — JS: #0*(?:6[02]|160) → &#60; &#62; &#160; with optional leading zeros */
				if(rem >= 4 && url_ptr[k + 1] == '#' && url_ptr[k + 2] >= '0' && url_ptr[k + 2] <= '9') {
					/* skip leading zeros */
					size_t j= k + 2;
					while(j < url_len && url_ptr[j] == '0') j++;
					bool ok= false;
					/* check for 60, 62, 160 */
					if(j + 2 < url_len && url_ptr[j + 2] == ';') {
						/* two-digit: 60 or 62 */
						ok= (url_ptr[j] == '6' && (url_ptr[j + 1] == '0' || url_ptr[j + 1] == '2'));
					} else if(j + 3 < url_len && url_ptr[j + 3] == ';') {
						/* three-digit: 160 */
						ok= (url_ptr[j] == '1' && url_ptr[j + 1] == '6' && url_ptr[j + 2] == '0');
					}
					if(ok) { entity_at= k; break; }
					continue;
				}
			}

			/* Build trail from entity-truncated part */
			size_t trail_cap= url_len + 4;
			char *trail= malloc(trail_cap);
			assert(trail);
			size_t trail_len= 0;

			if(entity_at < url_len) {
				trail_len= url_len - entity_at;
				memcpy(trail, url_ptr + entity_at, trail_len);
				url_len= entity_at;
			}

			/* --- Trailing punctuation stripping ---
             * JS sep regex:
             *   url.includes('(')  → /[^,;\\.:!?][,;\\.:!?]+$/u
             *   else               → /[^,;\\.:!?)][,;\\.:!?)]+$/u
             *
             * We strip from the right while the last char is in the punct set.
             * Edge case: ';' at end that continues an entity reference is kept
             * for 2 chars (entity correction = 2 case).
             */
			bool has_open_paren= false;
			for(size_t k= 0; k < url_len; k++) {
				if(url_ptr[k] == '(') {
					has_open_paren= true;
					break;
				}
			}

			/* build the set of strippable chars */
			while(url_len > 1) {
				char last= url_ptr[url_len - 1];
				bool strippable= (last == ',' || last == ';' || last == '\\' ||
													last == '.' || last == ':' || last == '!' || last == '?');
				if(!strippable && !has_open_paren && last == ')') strippable= true;
				if(!strippable) break;

				/* JS: if char [1] === ';' and preceding text ends with entity, correction=2,
                 * meaning we only strip 1 dot: correction increments by 1, so we keep the ';'
                 * inside the url (net strip = 0 extra). Actually JS strips from index+correction,
                 * where correction=1 normally, =2 if entity. The result is we never strip a ';'
                 * that's part of an entity. Simple approach: if last char is ';', check entity. */
				if(last == ';') {
					/* Check if this ';' closes an HTML entity */
					/* /&(?:[a-z]+|#x[\da-f]+|#\d+)$/iu */
					bool is_entity= false;
					for(size_t k= url_len - 1; k-- > 0;) {
						if(url_ptr[k] == '&') {
							is_entity= true;
							break;
						}
						char c= url_ptr[k];
						if(!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
								 (c >= '0' && c <= '9') || c == '#' || c == 'x' || c == 'X')) break;
					}
					if(is_entity) {
						/* correction=2: strip from index+2 → only strip chars AFTER the ';' */
						/* Since ';' is the last char and correction=2 means trail starts 2 chars
                         * from sepChars.index (which is the char BEFORE ';'), effectively the ';'
                         * stays in url. */
						break; /* keep ';' in url */
					}
				}

				/* Ensure non-puncture char before current run */
				/* Actually JS regex requires [^punct]punctchars+ so we need at least
                 * 1 non-strippable char before the run.  The simple approach: check
                 * that url_len-1 > 0 (guaranteed since url_len > 1 above). */
				/* move last char to trail */
				if(trail_len + 1 >= trail_cap) {
					trail_cap*= 2;
					trail= realloc(trail, trail_cap);
					assert(trail);
				}
				/* prepend to trail (insert at front) */
				memmove(trail + 1, trail, trail_len);
				trail[0]= last;
				trail_len++;
				url_len--;
			}

			/*
             * JS: if (trail.length >= p1.length) { return m; }
             * (bail out, whole match is copied unchanged)
             */
			if(trail_len >= p1_len) {
				/* undo: copy original match lead was already emitted, so we need to
                 * back up.  Since we already emitted the lead above, we need to
                 * reconstruct the full match.  Actually, per JS, return m means copy
                 * the entire match `m` (including lead). We already emitted lead, so
                 * emit rest = url_ptr[0..original url_len + trail] = from lead_e to mend. */
				size_t rest_len= mend - lead_e;
				ENSURE_CAP(rest_len + 1);
				memcpy(out_buf + out_len, tb->buf + lead_e, rest_len);
				out_len+= rest_len;
				free(trail);
				search_at= mend + (mend == mstart ? 1 : 0);
				continue;
			}

			/* Build the MagicLink token for the trimmed URL (includes protocol prefix) */
			Token *ml= build_magic_link(url_ptr, url_len, "free-ext-link", accum);
			if(ml) {
				size_t idx= accum->count - 1;
				char sent[64];
				size_t slen;
				work_str_sentinel(idx, 'w', sent, &slen);
				ENSURE_CAP(slen + trail_len + 1);
				memcpy(out_buf + out_len, sent, slen);
				out_len+= slen;
				if(trail_len > 0) {
					memcpy(out_buf + out_len, trail, trail_len);
					out_len+= trail_len;
				}
			} else {
				size_t rest_len= mend - lead_e;
				ENSURE_CAP(rest_len + 1);
				memcpy(out_buf + out_len, tb->buf + lead_e, rest_len);
				out_len+= rest_len;
			}

			free(trail);

		} else {
			/*
             * RFC/PMID/ISBN case.
             * JS: else if (!/^(?:RFC|PMID|ISBN)/u.test(url)) { return m; }
             *     new MagicLinkToken(url, 'magic-link', ...)
             *     return `${lead}\0${accum.length-1}i\x7F`
             *
             * url = m.slice(lead.length) = tb->buf[lead_e .. mend)
             */
			const char *inner_ptr= tb->buf + lead_e;
			size_t inner_len= (mend > lead_e) ? (mend - lead_e) : 0;

			/* JS: if (!/^(?:RFC|PMID|ISBN)/u.test(url)) { return m; } */
			bool is_magic=
			(inner_len >= 3 && strncmp(inner_ptr, "RFC", 3) == 0) ||
			(inner_len >= 4 && strncmp(inner_ptr, "PMID", 4) == 0) ||
			(inner_len >= 4 && strncmp(inner_ptr, "ISBN", 4) == 0);
			if(!is_magic) {
				/* return m: emit lead was done, now emit rest */
				size_t rest_len= mend - lead_e;
				ENSURE_CAP(rest_len + 1);
				memcpy(out_buf + out_len, tb->buf + lead_e, rest_len);
				out_len+= rest_len;
			} else {
				Token *ml= build_magic_link(inner_ptr, inner_len,
																		"magic-link", accum);
				if(ml) {
					size_t idx= accum->count - 1;
					char sent[64];
					size_t slen;
					work_str_sentinel(idx, 'i', sent, &slen);
					ENSURE_CAP(slen + 1);
					memcpy(out_buf + out_len, sent, slen);
					out_len+= slen;
				} else {
					size_t rest_len= mend - lead_e;
					ENSURE_CAP(rest_len + 1);
					memcpy(out_buf + out_len, tb->buf + lead_e, rest_len);
					out_len+= rest_len;
				}
			}
		}

		search_at= mend + (mend == mstart ? 1 : 0);
	}

	out_buf[out_len]= '\0';
	wiki_thread_buf_set(tb, out_buf, out_len);
	if(tmp_out) {
		wiki_thread_buf_release_scratch(tmp_out);
	} else {
		free(out_buf);
	}

	pcre2_match_data_free(md);
}
