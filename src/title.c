/*
 * title.c — Title normalization and validity (halfParsed mode).
 *
 * Mirrors the subset of dist/lib/title.js used by parseRedirect when
 * called with { halfParsed: true, temporary: true, decode: true, page: '' }.
 *
 * Title validity test (JS):
 *   this.valid = Boolean(title || this.interwiki || ...)
 *     && decodeHtml(title) === title     // no remaining HTML entities
 *     && !/^:|\0\d+[eh!+-]\x7F|[<>[\]{}|\n]|%[\da-f]{2}|(?:^|\/)\.{1,2}(?:$|\/)/iu.test(sub);
 */
#include "title.h"
#include "util/string_util.h"
#include "stringzilla/stringzilla.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* sz_lookup LUT: maps ' ' (0x20) -> '_' (0x5F); all other bytes map to themselves.
 * Used wherever spaces need to be bulk-replaced with underscores. */
static const unsigned char s_spc2under_lut[256] = {
      0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15,
     16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
    '_', 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47,
     48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63,
     64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
     80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95,
     96, 97, 98, 99,100,101,102,103,104,105,106,107,108,109,110,111,
    112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,
    128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,
    144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,
    160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,
    176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,
    192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,
    208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,
    224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,
    240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255,
};

static char *strndup0(const char *s, size_t len) {
	char *out= malloc(len + 1);
	if(!out) return NULL;
	if(len > 0) sz_copy(out, s, len);
	out[len]= '\0';
	return out;
}

static const char *title_namespace_name(const ParserConfig *cfg, int ns) {
	if(!cfg) return "";
	for(size_t i= 0; i < cfg->ns_count; i++) {
		sz_ptr_t name_start;
		sz_size_t name_len;
		sz_string_range(&cfg->namespaces[i].name, &name_start, &name_len);
		if(cfg->namespaces[i].num == ns && name_start) {
			return (const char *)name_start;
		}
	}
	return "";
}

/* Optimized: trim whitespace inline and compare with strncasecmp,
 * avoiding the malloc/free from trim_lc_n. */
static int title_lookup_namespace(const ParserConfig *cfg, const char *s, size_t len) {
	if(!cfg || !s) return 0;
	/* Trim without allocation */
	size_t start= 0, end= len;
	while(start < end && isspace((unsigned char)s[start])) start++;
	while(end > start && isspace((unsigned char)s[end - 1])) end--;
	size_t trimmed_len= end - start;
	for(size_t i= 0; i < cfg->ns_count; i++) {
		sz_ptr_t name_start;
		sz_size_t name_len;
		sz_string_range(&cfg->namespaces[i].name, &name_start, &name_len);
		if(!name_start || name_len != trimmed_len) continue;
		if(str_ci_eq_n(s + start, (const char *)name_start, trimmed_len)) {
			return cfg->namespaces[i].num;
		}
	}
	return 0;
}

/* Optimized: use sz_find() to skip directly to the 3-byte UTF-8 sequence
 * instead of testing every byte individually. */
static void title_replace_1e9a(char *s) {
	if(!s) return;
	size_t len= strlen(s);
	static const char seq[3]= {(char)0xE1, (char)0xBA, (char)0x9A};
	sz_cptr_t found= sz_find(s, len, seq, 3);
	while(found) {
		char *p= (char *)found;
		p[0]= 'A';
		p[1]= (char)0xCA;
		p[2]= (char)0xBE;
		size_t new_off= (size_t)(p + 3 - s);
		if(new_off >= len) break;
		found= sz_find(s + new_off, len - new_off, seq, 3);
	}
}

static size_t utf8_encode_codepoint(uint32_t cp, char *out) {
	if(cp < 0x80) {
		out[0]= (char)cp;
		return 1;
	}
	if(cp < 0x800) {
		out[0]= (char)(0xC0 | (cp >> 6));
		out[1]= (char)(0x80 | (cp & 0x3F));
		return 2;
	}
	if(cp < 0x10000) {
		out[0]= (char)(0xE0 | (cp >> 12));
		out[1]= (char)(0x80 | ((cp >> 6) & 0x3F));
		out[2]= (char)(0x80 | (cp & 0x3F));
		return 3;
	}
	out[0]= (char)(0xF0 | (cp >> 18));
	out[1]= (char)(0x80 | ((cp >> 12) & 0x3F));
	out[2]= (char)(0x80 | ((cp >> 6) & 0x3F));
	out[3]= (char)(0x80 | (cp & 0x3F));
	return 4;
}

static size_t title_uppercase_first_codepoint(char *s, size_t len) {
	if(len == 0) return 0;
	int char_len= utf8_char_len((unsigned char)s[0]);
	if(char_len <= 0 || (size_t)char_len > len) return len;

	uint32_t cp= 0;
	if(char_len == 1) {
		cp= (unsigned char)s[0];
	} else if(char_len == 2) {
		cp= ((uint32_t)(s[0] & 0x1F) << 6) | ((uint32_t)(unsigned char)s[1] & 0x3F);
	} else if(char_len == 3) {
		cp= ((uint32_t)(s[0] & 0x0F) << 12) | ((uint32_t)(unsigned char)s[1] & 0x3F) << 6 | ((uint32_t)(unsigned char)s[2] & 0x3F);
	} else {
		cp= ((uint32_t)(s[0] & 0x07) << 18) | ((uint32_t)(unsigned char)s[1] & 0x3F) << 12 | ((uint32_t)(unsigned char)s[2] & 0x3F) << 6 | ((uint32_t)(unsigned char)s[3] & 0x3F);
	}

	if(cp == 0x00DF) {
		const char buf[2]= {'S', 'S'};
		size_t wrote= 2;
		size_t new_len= len - (size_t)char_len + wrote;
		sz_move(s + wrote, s + char_len, len - (size_t)char_len + 1);
		sz_copy(s, buf, wrote);
		return new_len;
	}

	uint32_t up= utf8_toupper_codepoint(cp);
	if(up == cp) return len;

	char buf[4];
	size_t wrote= utf8_encode_codepoint(up, buf);
	size_t new_len= len - (size_t)char_len + wrote;
	sz_move(s + wrote, s + char_len, len - (size_t)char_len + 1);
	sz_copy(s, buf, wrote);
	return new_len;
}

static char *title_main_from_text(const char *s, size_t len) {
	char *out= strndup0(s, len);
	if(!out) return NULL;
	size_t in= 0, out_i= 0;
	while(in < len && (out[in] == '_' || isspace((unsigned char)out[in]))) in++;
	bool last_space= false;
	for(; in < len; in++) {
		char c= out[in];
		if(c == '_') c= ' ';
		if(isspace((unsigned char)c)) {
			if(!last_space) out[out_i++]= ' ';
			last_space= true;
		} else {
			out[out_i++]= c;
			last_space= false;
		}
	}
	while(out_i > 0 && out[out_i - 1] == ' ') out_i--;
	out[out_i]= '\0';
	title_replace_1e9a(out);
	out_i= title_uppercase_first_codepoint(out, out_i);
	out[out_i]= '\0';
	return out;
}

static char *title_compose_resolved(const Title *t, const char *page) {
	if(!t || !t->main || !t->prefix || !t->interwiki) return NULL;
	size_t pre_len= strlen(t->interwiki);
	size_t ns_len= strlen(t->prefix);
	size_t main_len= strlen(t->main);
	size_t base_len= pre_len + (pre_len ? 1 : 0) + ns_len + (ns_len ? 1 : 0) + main_len;
	char *base= malloc(base_len + 1);
	if(!base) return NULL;
	size_t pos= 0;
	if(pre_len > 0) {
		sz_copy(base + pos, t->interwiki, pre_len);
		pos+= pre_len;
		base[pos++]= ':';
	}
	if(ns_len > 0) {
		sz_copy(base + pos, t->prefix, ns_len);
		pos+= ns_len;
		base[pos++]= ':';
	}
	sz_copy(base + pos, t->main, main_len);
	pos+= main_len;
	base[pos]= '\0';
	/* Replace spaces with underscores using a SIMD-friendly lookup table */
	sz_lookup(base, pos, base, (const char *)s_spc2under_lut);

	if(base[0] == '/') {
		size_t page_len= page ? strlen(page) : 0;
		while(pos > 0 && base[pos - 1] == '/') pos--;
		/* JS parity: keep a single slash for slash-only targets like "/". */
		if(pos == 0) pos= 1;
		char *resolved= malloc(page_len + pos + 1);
		if(!resolved) {
			free(base);
			return NULL;
		}
		if(page_len > 0) sz_copy(resolved, page, page_len);
		sz_copy(resolved + page_len, base, pos);
		resolved[page_len + pos]= '\0';
		free(base);
		return resolved;
	}

	if(pos >= 3 && sz_equal(base, "../", 3) && page && strchr(page, '/')) {
		size_t level= 0;
		const char *sub= base;
		while((size_t)(sub - base) + 3 <= pos && sz_equal(sub, "../", 3)) {
			level++;
			sub+= 3;
		}
		size_t page_len= strlen(page);
		/* Use sz_find_byte to count '/' without a byte-by-byte loop */
		size_t dir_count= 1;
		{
			char slash_ch= '/';
			const char *pp= page;
			size_t prem= page_len;
			while(prem > 0) {
				sz_cptr_t nsl= sz_find_byte(pp, prem, &slash_ch);
				if(!nsl) break;
				dir_count++;
				pp= nsl + 1;
				prem= page_len - (size_t)(pp - page);
			}
		}
		if(dir_count > level) {
			size_t keep= page_len;
			size_t drops= level;
			while(keep > 0 && drops > 0) {
				keep--;
				if(page[keep] == '/') drops--;
			}
			size_t sub_len= strlen(sub);
			char *resolved= malloc(keep + (sub_len ? 1 : 0) + sub_len + 1);
			if(!resolved) {
				free(base);
				return NULL;
			}
			sz_copy(resolved, page, keep);
			size_t out= keep;
			if(sub_len > 0 && out > 0 && resolved[out - 1] != '/') resolved[out++]= '/';
			if(sub_len > 0) {
				sz_copy(resolved + out, sub, sub_len);
				out+= sub_len;
			}
			resolved[out]= '\0';
			free(base);
			return resolved;
		}
	}

	return base;
}

/* ── Helpers ─────────────────────────────────────────────────────────────── */

/** True if byte b is a hex digit. */
static bool is_hex(unsigned char b) {
	return (b >= '0' && b <= '9') || (b >= 'a' && b <= 'f') || (b >= 'A' && b <= 'F');
}

/**
 * True if s contains any character that makes a title invalid:
 *   ^: | [<>[\]{}|\n] | \0\d+[eh!+-]\x7F | %[0-9a-f]{2} | path ./ or ../
 */
/* Optimized: sz_find_byteset() to jump to the next interesting byte (SIMD),
 * then sz_find_byte() for the slash-based path-traversal check. */
static bool title_has_invalid_chars(const char *s, size_t len) {
	/* Check for leading colon */
	if(len > 0 && s[0] == ':') return true;

	/* Byteset of all characters that need special handling */
	sz_byteset_t inv_set;
	sz_byteset_init(&inv_set);
	sz_byteset_add(&inv_set, '<');
	sz_byteset_add(&inv_set, '>');
	sz_byteset_add(&inv_set, '[');
	sz_byteset_add(&inv_set, ']');
	sz_byteset_add(&inv_set, '{');
	sz_byteset_add(&inv_set, '}');
	sz_byteset_add(&inv_set, '|');
	sz_byteset_add(&inv_set, '\n');
	sz_byteset_add(&inv_set, '\0');
	sz_byteset_add(&inv_set, '%');

	const char *ptr= s;
	size_t rem= len;
	while(rem > 0) {
		sz_cptr_t found= sz_find_byteset(ptr, rem, &inv_set);
		if(!found) break;
		size_t offset= (size_t)(found - s);
		unsigned char c= (unsigned char)*found;

		if(c == '<' || c == '>' || c == '[' || c == ']' ||
		   c == '{' || c == '}' || c == '|' || c == '\n') {
			return true;
		}
		/* Sentinel markers \0\d+[eh!+-]\x7F */
		if(c == '\0') {
			size_t k= offset + 1;
			while(k < len && s[k] >= '0' && s[k] <= '9') k++;
			if(k < len) {
				char tc= s[k];
				if((tc == 'e' || tc == 'h' || tc == '!' || tc == '+' || tc == '-') &&
				   k + 1 < len && (unsigned char)s[k + 1] == '\x7F') {
					return true;
				}
			}
		}
		/* %XX URL percent encoding */
		if(c == '%' && offset + 2 < len &&
		   is_hex((unsigned char)s[offset + 1]) && is_hex((unsigned char)s[offset + 2])) {
			return true;
		}
		ptr= found + 1;
		rem= len - (size_t)(ptr - s);
	}

	/* Check for path components . and .. */
	{
		if(len == 1 && s[0] == '.') {
			return true;
		}
		if(len >= 2 && s[0] == '.' &&
		   (s[1] == '/' || (s[1] == '.' && (len == 2 || s[2] == '/')))) {
			return true;
		}
		/* Use sz_find_byte() to jump to each '/' quickly */
		char slash_ch= '/';
		const char *sp= s;
		size_t srem= len;
		while(srem > 1) {
			sz_cptr_t sl= sz_find_byte(sp, srem, &slash_ch);
			if(!sl) break;
			size_t i= (size_t)(sl - s);
			if(i + 1 < len && s[i + 1] == '.') {
				if(i + 2 >= len || s[i + 2] == '/') return true;
				if(i + 2 < len && s[i + 2] == '.' &&
				   (i + 3 >= len || s[i + 3] == '/')) return true;
			}
			sp= sl + 1;
			srem= len - (size_t)(sp - s);
		}
	}
	return false;
}

/* JS parity for decode:true in Title constructor: decode valid %XX bytes but
 * preserve malformed '%' literals. */
/* Optimized: sz_find_byte() to locate '%' fast (SIMD), then bulk-copy clean
 * segments with sz_copy() between percent-encoded bytes. */
static char *title_try_percent_decode(const char *s, size_t len, size_t *out_len) {
	/* Fast check: any '%' in the string? */
	char pct_ch= '%';
	sz_cptr_t first_pct= sz_find_byte(s, len, &pct_ch);

	if(!first_pct) {
		char *copy= malloc(len + 1);
		if(!copy) return NULL;
		sz_copy(copy, s, len);
		copy[len]= '\0';
		if(out_len) *out_len= len;
		return copy;
	}

	/* Decode: bulk-copy clean segments; decode valid percent-encoded bytes.
	 * Malformed '%' is preserved as a literal byte. */
	char *out= malloc(len + 1);
	if(!out) return NULL;
	size_t j= 0;
	const char *p= s;
	size_t rem= len;
	while(rem > 0) {
		sz_cptr_t np= sz_find_byte(p, rem, &pct_ch);
		if(!np) {
			sz_copy(out + j, p, rem);
			j+= rem;
			break;
		}
		size_t seg= (size_t)(np - p);
		if(seg > 0) {
			sz_copy(out + j, p, seg);
			j+= seg;
		}
		size_t off= (size_t)(np - s);
		if(off + 2 < len && is_hex((unsigned char)np[1]) && is_hex((unsigned char)np[2])) {
			unsigned char hi= (unsigned char)np[1];
			unsigned char lo= (unsigned char)np[2];
			unsigned char hv= (unsigned char)(hi <= '9' ? hi - '0' : (tolower(hi) - 'a' + 10));
			unsigned char lv= (unsigned char)(lo <= '9' ? lo - '0' : (tolower(lo) - 'a' + 10));
			out[j++]= (char)((hv << 4) | lv);
			p= np + 3;
		} else {
			out[j++]= '%';
			p= np + 1;
		}
		rem= len - (size_t)(p - s);
	}
	out[j]= '\0';
	if(out_len) *out_len= j;
	return out;
}

/* ── Public API ─────────────────────────────────────────────────────────── */

bool title_is_valid_half_parsed(const char *raw, size_t raw_len,
																const ParserConfig *cfg) {
	Title *t= title_parse_half_parsed(raw, raw_len, 0, cfg, true, "");
	bool valid= t && t->valid;
	title_free(t);
	return valid;
}

Title *title_parse_half_parsed(const char *raw, size_t raw_len,
															 int default_ns,
															 const ParserConfig *cfg,
															 bool self_link,
															 const char *page) {
	if(!raw) return NULL;

	Title *t= calloc(1, sizeof(*t));
	if(!t) return NULL;
	t->prefix= strdup("");
	t->interwiki= strdup("");
	if(!t->prefix || !t->interwiki) {
		title_free(t);
		return NULL;
	}

	size_t t0= 0, t1= raw_len;
	while(t0 < t1 && isspace((unsigned char)raw[t0])) t0++;
	while(t1 > t0 && isspace((unsigned char)raw[t1 - 1])) t1--;
	bool subpage= (t1 - t0 >= 3 && raw[t0] == '.' && raw[t0 + 1] == '.' && raw[t0 + 2] == '/');
	bool page_subpage= (page && *page && raw_len - t0 >= 1 && raw[t0] == '/');

	size_t decoded_len= 0;
	char *pct_decoded= title_try_percent_decode(raw, raw_len, &decoded_len);
	if(!pct_decoded) {
		title_free(t);
		return NULL;
	}

	size_t html_len= 0;
	char *html_decoded= str_decode_html_basic(pct_decoded, decoded_len, &html_len);
	free(pct_decoded);
	if(!html_decoded) {
		title_free(t);
		return NULL;
	}

	char *norm= malloc(html_len + 1);
	if(!norm) {
		free(html_decoded);
		title_free(t);
		return NULL;
	}

	size_t nlen= 0;
	bool last_space= false;
	for(size_t i= 0; i < html_len; i++) {
		unsigned char c= (unsigned char)html_decoded[i];
		bool is_space= (c == '_' || c == ' ');
		/* JS decodeHtml() parity: U+00A0 is normalized to ASCII space. */
		if(!is_space && c == 0xC2 && i + 1 < html_len &&
		   (unsigned char)html_decoded[i + 1] == 0xA0) {
			is_space= true;
			i++;
		}
		if(is_space) {
			if(!last_space) norm[nlen++]= ' ';
			last_space= true;
		} else {
			norm[nlen++]= (char)c;
			last_space= false;
		}
	}
	norm[nlen]= '\0';
	free(html_decoded);

	size_t start= 0, end= nlen;
	while(start < end && isspace((unsigned char)norm[start])) start++;
	while(end > start && isspace((unsigned char)norm[end - 1])) end--;

	int ns= default_ns;
	const char *title= norm + start;
	size_t title_len= end - start;
	/* Preserve the original title length before stripping leading "../" segments
	 * so validity checks can mirror JS (which treats a non-empty original
	 * title as significant even if the remaining "sub" is empty). */
	size_t orig_title_len = title_len;

	if(subpage || page_subpage) {
		ns= 0;
	} else {
		if(title_len > 0 && title[0] == ':') {
			ns= 0;
			title++;
			title_len--;
			while(title_len > 0 && isspace((unsigned char)title[0])) {
				title++;
				title_len--;
			}
		}

		if(default_ns == 0) {
			size_t iw_consumed= 0;
			char *iw= str_extract_interwiki(title, title_len, cfg, &iw_consumed);
			if(iw) {
				free(t->interwiki);
				t->interwiki= iw;
				if(iw_consumed <= title_len) {
					title+= iw_consumed;
					title_len-= iw_consumed;
				}
			}
		}

		char colon_ch_= ':';
		const char *colon= (const char *)sz_find_byte(title, title_len, &colon_ch_);
		if(colon) {
			int found= title_lookup_namespace(cfg, title, (size_t)(colon - title));
			if(found) {
				ns= found;
				title_len-= (size_t)(colon + 1 - title);
				title= colon + 1;
				while(title_len > 0 && isspace((unsigned char)title[0])) {
					title++;
					title_len--;
				}
			}
		}
	}
	t->ns= ns;

	/* Use sz_find_byte to jump directly to '#' */
	size_t hash= title_len;
	{
		char hash_ch= '#';
		sz_cptr_t hp= sz_find_byte(title, title_len, &hash_ch);
		if(hp) hash= (size_t)(hp - title);
	}
	if(hash < title_len) {
		const char *fragment= title + hash + 1;
		size_t fragment_len= title_len - hash - 1;
		while(fragment_len > 0 && isspace((unsigned char)fragment[fragment_len - 1])) fragment_len--;
		size_t frag_dec_len= 0;
		char *frag_pct= title_try_percent_decode(fragment, fragment_len, &frag_dec_len);
		if(frag_pct) {
			char *frag_html= str_decode_html_basic(frag_pct, frag_dec_len, NULL);
			free(frag_pct);
			if(frag_html) {
				size_t flen= strlen(frag_html);
				while(flen > 0 && isspace((unsigned char)frag_html[flen - 1])) flen--;
				/* Replace spaces in fragment with underscores via lookup table */
				sz_lookup(frag_html, flen, frag_html, (const char *)s_spc2under_lut);
				frag_html[flen]= '\0';
				t->fragment= frag_html;
			}
		}
		title_len= hash;
		while(title_len > 0 && isspace((unsigned char)title[title_len - 1])) title_len--;
	}

	t->main= title_main_from_text(title, title_len);
	if(!t->main) {
		free(norm);
		title_free(t);
		return NULL;
	}

	free(t->prefix);
	t->prefix= strdup(title_namespace_name(cfg, ns));
	if(!t->prefix) {
		free(norm);
		title_free(t);
		return NULL;
	}

	size_t level= 0;
	const char *sub= title;
	if(subpage) {
		while(title_len >= 3 && strncmp(sub, "../", 3) == 0) {
			level++;
			sub+= 3;
			title_len-= 3;
		}
	}

	size_t decoded_again_len= 0;
	char *decoded_again= str_decode_html_basic(title, title_len, &decoded_again_len);
	bool html_idempotent= decoded_again && decoded_again_len == title_len && sz_equal(decoded_again, title, title_len);
	free(decoded_again);

	bool page_ok= true;
	if(level > 0 && page != NULL) {
		/* Use sz_find_byte to count '/' without iterating byte-by-byte */
		size_t page_parts= 1;
		size_t plen= strlen(page);
		char slash_ch= '/';
		const char *pp= page;
		size_t prem= plen;
		while(prem > 0) {
			sz_cptr_t nsl= sz_find_byte(pp, prem, &slash_ch);
			if(!nsl) break;
			page_parts++;
			pp= nsl + 1;
			prem= plen - (size_t)(pp - page);
		}
		page_ok= page_parts > level;
	}

	/* JS parity with tightening: treat the original title-length as
	 * significant only for subpage ("../") inputs. This avoids accepting
	 * namespace-only titles like "File:" while still accepting "../". */
	t->valid= ((title_len > 0) || t->interwiki[0] != '\0' || (self_link && t->ns == 0 && t->fragment != NULL) || (subpage && orig_title_len > 0)) && html_idempotent && page_ok && !title_has_invalid_chars(sub, title_len);

	t->title= title_compose_resolved(t, page);
	if(!t->title) {
		free(norm);
		title_free(t);
		return NULL;
	}

	free(norm);
	return t;
}

char *title_normalize(const char *raw, size_t raw_len) {
	if(!raw || raw_len == 0) return strdup("");

	char *decoded= str_decode_html_basic(raw, raw_len, NULL);
	if(!decoded) return NULL;
	size_t decoded_len= strlen(decoded);

	char *result= malloc(decoded_len + 1);
	if(!result) return NULL;

	/* Replace spaces with underscores via SIMD-friendly lookup table */
	sz_lookup(result, decoded_len, decoded, (const char *)s_spc2under_lut);
	result[decoded_len]= '\0';
	free(decoded);

	/* Trim leading/trailing underscores (were spaces) */
	size_t start= 0, end= decoded_len;
	while(start < end && result[start] == '_') start++;
	while(end > start && result[end - 1] == '_') end--;

	/* Collapse internal runs of underscores */
	size_t out= 0;
	bool prev_under= false;
	for(size_t i= start; i < end; i++) {
		if(result[i] == '_') {
			if(!prev_under) result[out++]= '_';
			prev_under= true;
		} else {
			result[out++]= result[i];
			prev_under= false;
		}
	}
	result[out]= '\0';

	/* JS parity edge case: normalize U+1E9A (ẚ) -> "Aʾ".
	 * Delegates to title_replace_1e9a which uses sz_find() (vectorized). */
	title_replace_1e9a(result);

	/*
     * JS parity: Title.main uppercases the first character of the main part
     * (after namespace/interwiki prefix parsing), not necessarily byte 0 of
     * the full title string.
     */
	out= title_uppercase_first_codepoint(result, out);

	/* Use sz_find_byte to locate the first ':' */
	size_t cap_at= 0;
	{
		char colon_ch= ':';
		sz_cptr_t cp= sz_find_byte(result, out, &colon_ch);
		if(cp) cap_at= (size_t)(cp - result) + 1;
	}
	if(cap_at < out) {
		out= cap_at + title_uppercase_first_codepoint(result + cap_at, out - cap_at);
	}

	return result;
}

void title_free(Title *t) {
	if(!t) return;
	free(t->main);
	free(t->prefix);
	free(t->fragment);
	free(t->interwiki);
	free(t->title);
	free(t);
}
