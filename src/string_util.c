/*
 * string_util.c — String helpers mirroring dist/util/string.js.
 */
#include "util/string_util.h"
#include "config.h"
#include "stringzilla/stringzilla.h"
#include <unicode/uchar.h>
#include <unicode/utf8.h>
#include <assert.h>
#include <ctype.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* lowercasing LUT for bulk operations */
static unsigned char s_tolower_lut[256];
static int s_tolower_lut_init = 0;

static void ensure_tolower_lut(void) {
	if (!s_tolower_lut_init) {
		for (int i = 0; i < 256; ++i) s_tolower_lut[i] = (unsigned char)tolower((unsigned char)i);
		s_tolower_lut_init = 1;
	}
}


/* ── Sentinel marker formatting ──────────────────────────────────────────── */

void work_str_sentinel(size_t index, char ch, char *marker_buf, size_t *marker_len) {
	/* Format: \0<decimal index><ch>\x7F */
	marker_buf[0]= '\0';
	int n= snprintf(marker_buf + 1, 28, "%zu%c", index, ch);
	marker_buf[1 + n]= '\x7F';
	if(marker_len) *marker_len= (size_t)(2 + n);
	/* The null byte at position 0 is intentional (it IS part of the sentinel).
     * The 'len' returned should be passed to memcpy, not strlen. */
}

/* ── tidy ─────────────────────────────────────────────────────────────────── */

char *str_tidy(const char *s, size_t len, size_t *out_len) {
	/* Delegate to str_tidy_into to avoid duplicating logic. Allocate a
	 * caller-sized buffer (len + 1) as the conservative maximum. */
	char *result= malloc(len + 1);
	assert(result);
	str_tidy_into(s, len, result, len + 1, out_len);
	return result;
}

void str_tidy_into(const char *s, size_t len,
									 char *buf, size_t cap,
									 size_t *out_len) {
	/* The caller must have reserved at least len+1 bytes via
     * wiki_thread_buf_reserve().  Buffer ownership and resizing belong
     * exclusively to the thread_buffer layer. */
	assert(buf && cap >= len + 1);
	const char *p = s;
	const char *end = s + len;
	size_t j = 0;

	sz_byteset_t set;
	sz_byteset_init(&set);
	sz_byteset_add(&set, '\0');
	sz_byteset_add(&set, '\x7F');
	sz_byteset_add(&set, '\r');

	while(p < end) {
		const char *found = sz_find_byteset(p, (size_t)(end - p), &set);
		if(!found) {
			size_t rem = (size_t)(end - p);
			assert(j + rem + 1 <= cap);
			if(rem) sz_copy(buf + j, p, rem);
			j += rem;
			break;
		}
		/* copy [p, found) */
		size_t seg = (size_t)(found - p);
		if(seg) {
			assert(j + seg + 1 <= cap);
			sz_copy(buf + j, p, seg);
			j += seg;
		}

		/* Decide whether to skip or keep the found byte.  CR is only
		 * removed when it is immediately before a LF or at end-of-input. */
		unsigned char fc = (unsigned char)*found;
		/* Keep only a CR that is not immediately followed by LF and not at end */
		if (fc == '\r' && !(found + 1 == end || *(found + 1) == '\n')) {
			buf[j++] = '\r';
		}
		p = found + 1;
	}
	buf[j]= '\0';
	if(out_len) *out_len= j;
}

/* ── removeComment ──────────────────────────────────────────────────────── */
/*
 * JS pattern: /\0\d+[cn]\x7F/gu
 * Removes half-parsed comment-like tokens (sentinel markers with type 'c' or 'n').
 */
char *str_remove_comment(const char *s, size_t len, size_t *out_len) {
	char *result= malloc(len + 1);
	assert(result);
	size_t j= 0;
	const char *p = s;
	const char *end = s + len;
	char needle = '\0';

	while(p < end) {
		const char *found = sz_find_byte(p, (size_t)(end - p), &needle);
		if(!found) {
			size_t rem = (size_t)(end - p);
			if(rem) sz_copy(result + j, p, rem);
			j += rem;
			break;
		}
		/* copy [p, found) */
		size_t seg = (size_t)(found - p);
		if(seg) sz_copy(result + j, p, seg);
		j += seg;

		/* Look for sentinel: \0 <digits> [cn] \x7F */
		const char *k = found + 1;
		const char *not_digit = sz_find_byte_not_from(k, (size_t)(end - k), "0123456789", 10);
		if(not_digit == NULL) k = end; else k = not_digit;
		if(k < end && (k > found + 1) && (*k == 'c' || *k == 'n') &&
		   (k + 1 < end && (unsigned char)*(k + 1) == '\x7F')) {
			/* Skip the entire marker */
			p = k + 2;
			continue;
		}

		/* Not a comment sentinel — emit the literal byte and continue. */
		result[j++]= *found;
		p = found + 1;
	}
	result[j]= '\0';
	if(out_len) *out_len= j;
	return result;
}

/* ── Sentinel scanning helpers (\x00<digits>[exhbru]\x7F) ─────────────── */

bool sentinel_scan_next(const char *buf, size_t len, size_t *pos,
						size_t *out_n, char *out_type, size_t *out_total_len) {
	if(!buf || !pos || *pos >= len) return false;
	size_t i = *pos;
	while(i < len) {
		const char *found = sz_find_byte(buf + i, len - i, "\0");
		if(!found) return false;
		size_t p = (size_t)(found - buf);
		size_t j = p + 1;
		if(j >= len || !(buf[j] >= '0' && buf[j] <= '9')) { i = p + 1; continue; }
		size_t k = j;
		while(k < len && buf[k] >= '0' && buf[k] <= '9') k++;
		if(k >= len) { i = p + 1; continue; }
		char t = buf[k];
		if(strchr(SENTINEL_TYPES, t) == NULL) { i = p + 1; continue; }
		if(k + 1 >= len || (unsigned char)buf[k + 1] != '\x7F') { i = p + 1; continue; }

		/* parse decimal */
		size_t n = 0;
		for(size_t d = j; d < k; ++d) {
			n = n * 10 + (size_t)(buf[d] - '0');
		}
		size_t total = (k + 2) - p; /* includes NUL .. DEL */
		*pos = p + total;
		if(out_n) *out_n = n;
		if(out_type) *out_type = t;
		if(out_total_len) *out_total_len = total;
		return true;
	}
	return false;
}

void sentinel_scan(const char *buf, size_t len, SentinelScanCb cb, void *user_data) {
	if(!buf || !cb) return;
	size_t i = 0;
	while(i < len) {
		const char *found = sz_find_byte(buf + i, len - i, "\0");
		if(!found) return;
		size_t p = (size_t)(found - buf);
		size_t j = p + 1;
		if(j >= len || !(buf[j] >= '0' && buf[j] <= '9')) { i = p + 1; continue; }
		size_t k = j;
		while(k < len && buf[k] >= '0' && buf[k] <= '9') k++;
		if(k >= len) { i = p + 1; continue; }
		char t = buf[k];
		if(strchr(SENTINEL_TYPES, t) == NULL) { i = p + 1; continue; }
		if(k + 1 >= len || (unsigned char)buf[k + 1] != '\x7F') { i = p + 1; continue; }

		size_t n = 0;
		for(size_t d = j; d < k; ++d) n = n * 10 + (size_t)(buf[d] - '0');
		size_t total = (k + 2) - p;
		cb(p, total, n, t, user_data);
		i = p + total;
	}
}

/* ── trimLc ─────────────────────────────────────────────────────────────── */

char *str_trim_lc(const char *s, size_t len) {
	/* Trim leading whitespace */
	size_t start= 0;
	while(start < len && isspace((unsigned char)s[start])) start++;
	/* Trim trailing whitespace */
	size_t end= len;
	while(end > start && isspace((unsigned char)s[end - 1])) end--;

	size_t out_len= end - start;
	char *result= malloc(out_len + 1);
	assert(result);
	ensure_tolower_lut();
	if(out_len > 0) sz_lookup(result, out_len, s + start, (const char *)s_tolower_lut);
	result[out_len]= '\0';
	return result;
}

/* ── decodeHtmlBasic ────────────────────────────────────────────────────── */

/* Named HTML entities we handle (mirrors JS names object) */
static const struct {
	const char *name;
	char ch;
} HTML_NAMES[]= {
{"lt", '<'},
{"gt", '>'},
{"lbrack", '['},
{"rbrack", ']'},
{"lbrace", '{'},
{"rbrace", '}'},
{"nbsp", ' '}, /* narrow no-break space → space */
{"amp", '&'},
{"quot", '"'},
};
#define HTML_NAMES_COUNT ((int)(sizeof(HTML_NAMES) / sizeof(HTML_NAMES[0])))

static char *encode_codepoint(uint32_t cp, char *out, int *bytes_written) {
	/* Encode a Unicode codepoint as UTF-8 */
	if(cp < 0x80) {
		out[0]= (char)cp;
		*bytes_written= 1;
	} else if(cp < 0x800) {
		out[0]= (char)(0xC0 | (cp >> 6));
		out[1]= (char)(0x80 | (cp & 0x3F));
		*bytes_written= 2;
	} else if(cp < 0x10000) {
		out[0]= (char)(0xE0 | (cp >> 12));
		out[1]= (char)(0x80 | ((cp >> 6) & 0x3F));
		out[2]= (char)(0x80 | (cp & 0x3F));
		*bytes_written= 3;
	} else {
		out[0]= (char)(0xF0 | (cp >> 18));
		out[1]= (char)(0x80 | ((cp >> 12) & 0x3F));
		out[2]= (char)(0x80 | ((cp >> 6) & 0x3F));
		out[3]= (char)(0x80 | (cp & 0x3F));
		*bytes_written= 4;
	}
	return out;
}

char *str_decode_html_basic(const char *s, size_t len, size_t *out_len) {
	/* Allocate generous buffer: worst case same length + some slack */
	size_t cap= len + 4;
	char *result= malloc(cap);
	assert(result);
	size_t j= 0;

	for(size_t i= 0; i < len;) {
		if(s[i] != '&') {
			result[j++]= s[i++];
			continue;
		}
		/* Find closing semicolon */
		size_t k= i + 1;
		while(k < len && s[k] != ';' && s[k] != '&' && s[k] != '\n') k++;
		if(k >= len || s[k] != ';') {
			/* No closing semicolon — emit literally */
			result[j++]= s[i++];
			continue;
		}
		const char *ref= s + i + 1;
		size_t rlen= k - i - 1;

		if(rlen > 0 && ref[0] == '#') {
			/* Numeric character reference */
			uint32_t cp= 0;
			bool is_hex= (rlen > 1 && (ref[1] == 'x' || ref[1] == 'X'));
			const char *digits= ref + (is_hex ? 2 : 1);
			size_t dlen= rlen - (is_hex ? 2 : 1);
			for(size_t d= 0; d < dlen; d++) {
				char dc= digits[d];
				if(is_hex) {
					if(dc >= '0' && dc <= '9')
						cp= cp * 16 + (uint32_t)(dc - '0');
					else if(dc >= 'a' && dc <= 'f')
						cp= cp * 16 + (uint32_t)(dc - 'a' + 10);
					else if(dc >= 'A' && dc <= 'F')
						cp= cp * 16 + (uint32_t)(dc - 'A' + 10);
					else {
						cp= 0;
						break;
					}
				} else {
					if(dc >= '0' && dc <= '9')
						cp= cp * 10 + (uint32_t)(dc - '0');
					else {
						cp= 0;
						break;
					}
				}
			}
			if(cp > 0 && cp <= 0x10FFFF) {
				char tmp[4];
				int nb= 0;
				encode_codepoint(cp, tmp, &nb);
				/* Grow result if needed */
				while(j + (size_t)nb + 1 > cap) {
					cap*= 2;
					result= realloc(result, cap);
					assert(result);
				}
				sz_copy(result + j, tmp, (size_t)nb);
				j+= (size_t)nb;
				i= k + 1;
				continue;
			}
		} else {
			/* Possibly a named entity */
			/* We do a case-insensitive search in our small table */
			char lower_ref[16]= {0};
			size_t copy_len= rlen < 15 ? rlen : 15;
			ensure_tolower_lut();
			if(copy_len > 0) sz_lookup(lower_ref, copy_len, ref, (const char *)s_tolower_lut);

			bool found= false;
			for(int n= 0; n < HTML_NAMES_COUNT; n++) {
				if(strcmp(lower_ref, HTML_NAMES[n].name) == 0) {
					result[j++]= HTML_NAMES[n].ch;
					i= k + 1;
					found= true;
					break;
				}
			}
			if(found) continue;
		}
		/* Unknown or invalid entity — emit literally */
		result[j++]= s[i++];
	}
	result[j]= '\0';
	if(out_len) *out_len= j;
	return result;
}

/* ── restore ────────────────────────────────────────────────────────────── */
/*
 * JS: s.replace(/\0(\d+)\x7F/gu, (_, p1) => stack[p1])
 */
char *str_restore(const char *s, size_t len,
									const char **stack, size_t stack_count,
									const size_t *stack_lengths,
									size_t *out_len) {
	/* Two-pass: first count output size, then emit */
	size_t cap= len * 2 + 1;
	char *result= malloc(cap);
	assert(result);
	size_t j= 0;
	const char *p = s;
	const char *end = s + len;
	char needle = '\0';

	while(p < end) {
		const char *found = sz_find_byte(p, (size_t)(end - p), &needle);
		if(!found) {
			size_t rem = (size_t)(end - p);
			if(j + rem + 1 > cap) {
				while(j + rem + 1 > cap) { cap*= 2; result= realloc(result, cap); assert(result); }
			}
			if(rem) sz_copy(result + j, p, rem);
			j += rem;
			break;
		}

		/* copy [p, found) */
		size_t seg = (size_t)(found - p);
		if(seg) {
			if(j + seg + 1 > cap) {
				while(j + seg + 1 > cap) { cap*= 2; result= realloc(result, cap); assert(result); }
			}
			sz_copy(result + j, p, seg);
			j += seg;
		}

		/* Try to parse \0<digits>\x7F */
		const char *k = found + 1;
		while(k < end && *k >= '0' && *k <= '9') k++;
		if(k < end && k > found + 1 && (unsigned char)*k == '\x7F') {
			size_t idx = 0;
			for(const char *d = found + 1; d < k; ++d) idx = idx * 10 + (size_t)(*d - '0');
			if(idx < stack_count && stack[idx]) {
				const char *rep = stack[idx];
				size_t replen = (stack_lengths && stack_lengths[idx]) ? stack_lengths[idx] : strlen(rep);
				if(j + replen + 1 > cap) {
					while(j + replen + 1 > cap) { cap*= 2; result= realloc(result, cap); assert(result); }
				}
				sz_copy(result + j, rep, replen);
				j += replen;
				p = k + 1;
				continue;
			}
		}

		/* Fallback: emit the literal \0 byte and advance. */
		if(j + 2 > cap) { cap*= 2; result= realloc(result, cap); assert(result); }
		result[j++]= *found;
		p = found + 1;
	}
	result[j]= '\0';
	if(out_len) *out_len= j;
	return result;
}

/* ── UTF-8 helpers ───────────────────────────────────────────────────────── */

int utf8_char_len(unsigned char c) {
	if(c < 0x80) return 1;
	int trails= U8_COUNT_TRAIL_BYTES(c);
	if(trails < 1 || trails > 3) return 1;
	return trails + 1;
}

uint32_t utf8_tolower_codepoint(uint32_t cp) {
	if(cp > 0x10FFFF) return cp;
	return (uint32_t)u_tolower((UChar32)cp);
}

uint32_t utf8_toupper_codepoint(uint32_t cp) {
	if(cp > 0x10FFFF) return cp;
	return (uint32_t)u_toupper((UChar32)cp);
}

/* ── str_istr ────────────────────────────────────────────────────────────── */

const char *str_istr(const char *haystack, size_t hlen,
										 const char *needle, size_t nlen) {
	if(nlen == 0) return haystack;
	if(nlen > hlen) return NULL;
	ensure_tolower_lut();
	sz_size_t matched_len = 0;
	sz_cptr_t res = sz_utf8_case_insensitive_find(haystack, hlen, needle, nlen, NULL, &matched_len);
	if(res == SZ_NULL_CHAR) return NULL;
	return (const char *)res;
}

char *str_extract_interwiki(const char *s, size_t len, const ParserConfig *cfg, size_t *consumed) {
	if(!s || len == 0 || !cfg) {
		if(consumed) *consumed= 0;
		return NULL;
	}
	if(cfg->interwiki.count == 0) {
		if(consumed) *consumed= 0;
		return NULL;
	}

	/* Build normalized buffer (underscores -> spaces) and a map from temp
     * index to original raw byte index so we can compute consumed bytes. */
	char *temp= malloc(len + 1);
	assert(temp);
	size_t *pos_map= malloc((len + 1) * sizeof(size_t));
	assert(pos_map);
	size_t tlen= 0;
	/* build temp and pos_map using sz_copy-like behavior for efficiency */
	for(size_t i= 0; i < len; i++) {
		temp[tlen]= (s[i] == '_') ? ' ' : s[i];
		pos_map[tlen]= i;
		tlen++;
	}
	temp[tlen]= '\0';

	/* Trim leading whitespace */
	const char *ws_chars = " \t\n\v\f\r";
	const char *temp_end = temp + tlen;
	const char *p = sz_find_byte_not_from(temp, tlen, ws_chars, 6);
	if(!p) {
		free(temp);
		free(pos_map);
		if(consumed) *consumed= 0;
		return NULL;
	}
	size_t start = (size_t)(p - temp);

	/* Optional leading colon (force prefix removal) */
	if(start < tlen && temp[start] == ':') {
		p = temp + start + 1;
		const char *q = sz_find_byte_not_from(p, (size_t)(temp_end - p), ws_chars, 6);
		if(!q) {
			free(temp);
			free(pos_map);
			if(consumed) *consumed= 0;
			return NULL;
		}
		start = (size_t)(q - temp);
	}

	for(size_t k= 0; k < cfg->interwiki.count; k++) {
		const char *iw= cfg->interwiki.items[k];
		if(!iw) continue;
		size_t iwlen= strlen(iw);
		if(start + iwlen > tlen) continue;
		bool match= true;
		for(size_t j= 0; j < iwlen; j++) {
			if(tolower((unsigned char)temp[start + j]) != tolower((unsigned char)iw[j])) {
				match= false;
				break;
			}
		}
		if(!match) continue;
		size_t j= start + iwlen;
		while(j < tlen && isspace((unsigned char)temp[j])) j++;
		if(j < tlen && temp[j] == ':') {
			/* matched prefix + optional spaces + ':'; consumed bytes end at raw index of ':' + 1 */
			size_t raw_consumed= pos_map[j] + 1;
			char *out= strdup(iw);
			if(out) {
				for(char *p= out; *p; ++p) *p= (char)tolower((unsigned char)*p);
			}
			free(temp);
			free(pos_map);
			if(consumed) *consumed= raw_consumed;
			return out;
		}
	}

	free(temp);
	free(pos_map);
	if(consumed) *consumed= 0;
	return NULL;
}

/* ── URL scanning helpers (shared with external_links.c) ──────────────── */

bool is_url_common_byte(unsigned char c) {
	/* JS extUrlChar: non-control, non-bracket, non-quote, non-Zs, non-FFFD */
	if(c <= 0x20 || c == 0x7F) return false;
	if(c == '[' || c == ']' || c == '<' || c == '>' || c == '"') return false;
	return true;
}

size_t match_proto_prefix(const char *s, size_t len, const ParserConfig *cfg) {
	if(!cfg || !cfg->protocol_items_valid || cfg->protocol_items.count == 0) return 0;
	for(size_t pi = 0; pi < cfg->protocol_items.count; pi++) {
		const char *tok = cfg->protocol_items.items[pi];
		if(!tok) continue;
		size_t tlen = strlen(tok);
		if(tlen > 0 && tlen <= len) {
			bool ok = true;
			for(size_t i = 0; i < tlen; i++) {
				if(tolower((unsigned char)s[i]) != tolower((unsigned char)tok[i])) {
					ok = false;
					break;
				}
			}
			if(ok) return tlen;
		}
	}
	return 0;
}
