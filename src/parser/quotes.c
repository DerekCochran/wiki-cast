#include "util/log.h"
#include "parser/quotes.h"
#include "util/string_util.h"
#include "stringzilla/stringzilla.h"
#include "token.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
	char *s;			 /* pointer to bytes (may contain NUL for sentinels) */
	size_t len;		 /* length in bytes */
	bool is_quote; /* whether this part was a matched apostrophe run */
} Part;

/* Helper: create a QuoteToken with its inner text equal to the raw quote run. */
static Token *build_quote_token(const char *txt, size_t txt_len, Accum *accum) {
	Token *t= token_new(TOKEN_QUOTE, "quote");
	if(!t) return NULL;
	/* Set bold/italic flags by run length heuristics (matches JS getters) */
	t->data.quote.bold= txt_len != 2;
	t->data.quote.italic= txt_len != 3;

	if(txt && txt_len > 0) {
		const char *view = wiki_thread_buf_append_to_tokens(txt, txt_len);
		token_append_text_n(t, view, txt_len);
	} else {
		token_append_text_n(t, "", 0);
	}

	accum_push(accum, t);
	return t;
}

/* Append raw bytes (not necessarily NUL-terminated) to a Part's string. */
static int part_append_bytes(Part *p, const char *add, size_t addlen) {
	if(!p) return -1;
	char *newbuf= malloc(p->len + addlen + 1);
	if(!newbuf) return -1;
	if(p->s && p->len > 0) memcpy(newbuf, p->s, p->len);
	if(add && addlen > 0) memcpy(newbuf + p->len, add, addlen);
	newbuf[p->len + addlen]= '\0';
	free(p->s);
	p->s= newbuf;
	p->len+= addlen;
	return 0;
}

void quote_scan(const char *line, size_t len, QuoteRunCb cb, void *user_data) {
	if (!line || !cb || len < 2) return;
	size_t i = 0;
	const char apos = '\'';
	while (i < len) {
		const char *found = sz_find_byte(line + i, len - i, &apos);
		if (!found) break;
		size_t start = (size_t)(found - line);
		size_t run_len = 0;
		while (start + run_len < len && line[start + run_len] == '\'') run_len++;
		i = start + run_len;          /* skip past the entire run */
		if (run_len >= 2)
			cb(start, run_len, user_data);
	}

}

/* Callback context used by quote_parts_cb. We keep a pointer to the
 * `parts` variable so the callback can realloc and update it. */
typedef struct {
	Part  **parts_ptr;
	size_t *parts_len_ptr;
	size_t *parts_cap_ptr;
	const char *buf;
	size_t buf_len;
	size_t last_pos;
} QuotePartsCtx;

static void quote_parts_cb(size_t pos, size_t run_len, void *ud) {
	QuotePartsCtx *c = (QuotePartsCtx *)ud;
	/* Ensure capacity for two new entries (text + quote) */
	if((*c->parts_len_ptr + 2) >= *c->parts_cap_ptr) {
		size_t newcap = (*c->parts_cap_ptr) * 2;
		*c->parts_ptr = realloc(*c->parts_ptr, newcap * sizeof(Part));
		assert(*c->parts_ptr);
		*c->parts_cap_ptr = newcap;
	}
	Part *arr = *c->parts_ptr;

	/* Push text segment (may be empty to mimic JS split semantics) */
	size_t text_start = c->last_pos;
	size_t text_len = (pos > text_start) ? (pos - text_start) : 0;
	size_t idx = *c->parts_len_ptr;
	arr[idx].s = NULL;
	arr[idx].len = 0;
	arr[idx].is_quote = false;
	if(text_len > 0) {
		arr[idx].s = malloc(text_len + 1);
		memcpy(arr[idx].s, c->buf + text_start, text_len);
		arr[idx].s[text_len] = '\0';
		arr[idx].len = text_len;
	} else {
		arr[idx].s = malloc(1);
		arr[idx].s[0] = '\0';
		arr[idx].len = 0;
	}
	(*c->parts_len_ptr)++;

	/* Push quote run */
	idx = *c->parts_len_ptr;
	size_t rl = run_len;
	arr[idx].s = malloc(rl + 1);
	memcpy(arr[idx].s, c->buf + pos, rl);
	arr[idx].s[rl] = '\0';
	arr[idx].len = rl;
	arr[idx].is_quote = true;
	(*c->parts_len_ptr)++;

	c->last_pos = pos + rl;
}

void parse_quotes(ThreadBuf *tb, const ParserConfig *cfg, Accum *accum, bool tidy) {
	(void)cfg;
	if(!tb || !tb->buf) return;

	/* Split input into alternating text / quote parts using quote_scan. */
	size_t parts_cap= 32, parts_len= 0;
	Part *parts= calloc(parts_cap, sizeof(Part));
	assert(parts);

	

	QuotePartsCtx qctx = { &parts, &parts_len, &parts_cap, tb->buf, tb->len, 0 };
	quote_scan(tb->buf, tb->len, quote_parts_cb, &qctx);

	/* Trailing text */
	if(qctx.last_pos <= tb->len) {
		size_t text_len = tb->len - qctx.last_pos;
		if(parts_len >= parts_cap) {
			parts_cap *= 2;
			parts = realloc(parts, parts_cap * sizeof(Part));
			assert(parts);
		}
		parts[parts_len].s = malloc(text_len + 1);
		if(text_len > 0) memcpy(parts[parts_len].s, tb->buf + qctx.last_pos, text_len);
		parts[parts_len].s[text_len] = '\0';
		parts[parts_len].len = text_len;
		parts[parts_len].is_quote = false;
		parts_len++;
	}

	/* If no quote runs were found, nothing to do */
	bool any_quote= false;
	for(size_t k= 0; k < parts_len; k++)
		if(parts[k].is_quote) {
			any_quote= true;
			break;
		}
	if(!any_quote) {
		for(size_t k= 0; k < parts_len; k++) free(parts[k].s);
		free(parts);
		return;
	}

	/* Apply JS-equivalent balancing logic */
	int nBold= 0, nItalic= 0;
	ssize_t firstSingle= -1, firstMulti= -1, firstSpace= -1;

	/* Quote parts are at odd indices (0=text,1=quote,2=text,3=quote,...) */
	for(size_t i= 1; i + 0 < parts_len; i+= 2) {
		size_t len= parts[i].len;
		switch(len) {
		case 2:
			nItalic++;
			break;
		case 4: {
			/* arr[i - 1] += `'` */
			size_t prev= i - 1;
			part_append_bytes(&parts[prev], "'", 1);
			/* arr[i] = "'''" */
			free(parts[i].s);
			parts[i].s= strdup("'''");
			parts[i].len= 3;
			__attribute__((fallthrough));
		}
		case 3:
			nBold++;
			if(firstSingle != -1) break;
			{
				size_t prev= i - 1;
				size_t plen= parts[prev].len;
				const char *pstr= parts[prev].s;
				bool endsWithSpace= (plen > 0 && pstr[plen - 1] == ' ');
				bool secondLastSpace= (plen >= 2 && pstr[plen - 2] == ' ');
				if(endsWithSpace) {
					if(firstMulti == -1 && firstSpace == -1) firstSpace= (ssize_t)i;
				} else if(secondLastSpace) {
					firstSingle= (ssize_t)i;
				} else {
					if(firstMulti == -1) firstMulti= (ssize_t)i;
				}
			}
			break;
		default: {
			/* arr[i - 1] += `'`.repeat(len - 5); arr[i] = "'''''"; nItalic++, nBold++ */
			size_t prev= i - 1;
			size_t rep= (len > 5) ? (len - 5) : 0;
			if(rep > 0) {
				char *buf= malloc(rep);
				for(size_t z= 0; z < rep; z++) buf[z]= '\'';
				part_append_bytes(&parts[prev], buf, rep);
				free(buf);
			}
			free(parts[i].s);
			parts[i].s= strdup("'''''");
			parts[i].len= 5;
			nItalic++;
			nBold++;
		}
		}
	}

	if((nItalic % 2) == 1 && (nBold % 2) == 1) {
		ssize_t pick= (firstSingle != -1) ? firstSingle : ((firstMulti != -1) ? firstMulti : firstSpace);
		if(pick != -1) {
			size_t idx= (size_t)pick;
			/* arr[i] = "''"; arr[i-1] += "'" */
			free(parts[idx].s);
			parts[idx].s= strdup("''");
			parts[idx].len= 2;
			part_append_bytes(&parts[idx - 1], "'", 1);
		}
	}

	/* Replace quote parts with sentinel markers and create QuoteTokens */
	Token *bold_open= NULL;
	Token *italic_open= NULL;

	for(size_t i= 1; i + 0 < parts_len; i+= 2) {
		size_t n= parts[i].len;
		bool isBold= (n != 2);
		bool isItalic= (n != 3);

		/* Build token (this pushes it into accum) */
		Token *tok= build_quote_token(parts[i].s, parts[i].len, accum);
		if(!tok) continue; /* on allocation error, skip replacing */

		/* Generate marker for accum index (last pushed) */
		size_t tok_idx= accum->count - 1;
		char marker_buf[64];
		size_t marker_len= 0;
		work_str_sentinel(tok_idx, token_sentinel_char(TOKEN_QUOTE), marker_buf, &marker_len);

		/* Replace parts[i] content with marker bytes */
		free(parts[i].s);
		parts[i].s= malloc(marker_len);
		memcpy(parts[i].s, marker_buf, marker_len);
		parts[i].len= marker_len;
		parts[i].is_quote= false; /* now a sentinel */

		/* Toggle open/close state (we do not implement JS attribute linking here) */
		if(isBold) {
			bold_open= bold_open ? NULL : tok;
		}
		if(isItalic) {
			italic_open= italic_open ? NULL : tok;
		}
	}

	/* tidy behavior: append closing marker if tidy and unmatched remains */
	if(tidy && (bold_open || italic_open)) {
		/* Build extra closing token text like JS: (bold? "'''" : '') + (italic? "''" : '') */
		char extra_buf[8];
		size_t elen= 0;
		if(bold_open) {
			extra_buf[elen++]= '\'';
			extra_buf[elen++]= '\'';
			extra_buf[elen++]= '\'';
		}
		if(italic_open) {
			extra_buf[elen++]= '\'';
			extra_buf[elen++]= '\'';
		}
		Token *tok= build_quote_token(extra_buf, elen, accum);
		if(tok) {
			size_t tok_idx= accum->count - 1;
			char marker_buf[64];
			size_t marker_len= 0;
			work_str_sentinel(tok_idx, token_sentinel_char(TOKEN_QUOTE), marker_buf, &marker_len);
			/* append a new part with marker */
			if(parts_len >= parts_cap) {
				parts_cap*= 2;
				parts= realloc(parts, parts_cap * sizeof(Part));
				assert(parts);
			}
			parts[parts_len].s= malloc(marker_len);
			memcpy(parts[parts_len].s, marker_buf, marker_len);
			parts[parts_len].len= marker_len;
			parts[parts_len].is_quote= false;
			parts_len++;
		}
	}

	/* Join parts into output buffer (use scratch if available to avoid heap) */
	size_t out_len= 0;
	for(size_t k= 0; k < parts_len; k++) out_len+= parts[k].len;
	ThreadBuf *tmp_out = wiki_thread_buf_acquire_scratch();
	if(tmp_out) {
		wiki_thread_buf_reserve(tmp_out, out_len + 1);
		size_t out_pos= 0;
		for(size_t k= 0; k < parts_len; k++) {
			if(parts[k].len > 0) {
				memcpy(tmp_out->buf + out_pos, parts[k].s, parts[k].len);
				out_pos+= parts[k].len;
			}
		}
		tmp_out->buf[out_pos]= '\0';
		tmp_out->len = out_pos;
		wiki_thread_buf_set(tb, tmp_out->buf, out_pos);
		wiki_thread_buf_release_scratch(tmp_out);
	} else {
		char *out_buf= malloc(out_len + 1);
		size_t out_pos= 0;
		for(size_t k= 0; k < parts_len; k++) {
			if(parts[k].len > 0) {
				memcpy(out_buf + out_pos, parts[k].s, parts[k].len);
				out_pos+= parts[k].len;
			}
		}
		out_buf[out_pos]= '\0';
		/* Adopt into working string */
		wiki_thread_buf_set(tb, out_buf, out_pos);
		free(out_buf);
	}
	for(size_t k= 0; k < parts_len; k++) free(parts[k].s);
	free(parts);
}
