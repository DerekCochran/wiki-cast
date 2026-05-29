#include "parser/table.h"
#include "build.h"
#include "parser/td.h"
#include "parser/tr.h"
#include "util/string_util.h"
#include "stringzilla/stringzilla.h"
#include "table_token.h"
#include "token.h"
#include "util/thread_buffer.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int contains_literal_seq(const char *s, size_t len, const char *needle, size_t nlen) {
	if(nlen == 0 || len < nlen) return 0;
	return sz_find(s, len, needle, nlen) != NULL;
}

static Token *make_attr_key(const char *key, size_t key_len, Accum *accum) {
	Token *t= token_new(TOKEN_ATTR_KEY, "attr-key");
	if(!t) return NULL;
	if(key_len > 0) {
		token_append_text_n(t, key, key_len);
	} else {
		token_append_text_n(t, NULL, 0);
	}
	accum_push(accum, t);
	return t;
}

static Token *make_attr_value(const char *val, size_t val_len, Accum *accum) {
	Token *t= token_new(TOKEN_ATTR_VALUE, "attr-value");
	if(!t) return NULL;
	if(val_len > 0) {
		token_append_text_n(t, val, val_len);
	} else {
		token_append_text_n(t, NULL, 0);
	}
	accum_push(accum, t);
	return t;
}

static size_t sentinel_at(const char *buf, size_t len, size_t i, char type);

static bool table_attr_key_is_dynamic(const char *key, size_t key_len) {
	return sz_find_byte(key, key_len, "\0") != NULL ||
		(key_len >= 2 && key[0] == '{' && key[1] == '{') ||
		(key_len >= 2 && key[0] == '-' && key[1] == '{');
}

static size_t table_attr_gap_len_at(const char *s, size_t len, size_t i) {
	if(i >= len) return 0;
	size_t ws= 0;
	unsigned char c= (unsigned char)s[i];
	if(c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v') ws= 1;
	if(ws > 0) return ws;
	if(i + 1 < len && c == 0xC2 && (unsigned char)s[i + 1] == 0xA0) return 2;
	ws= sentinel_at(s, len, i, 'c');
	if(ws > 0) return ws;
	return sentinel_at(s, len, i, 'n');
}

static size_t table_ws_len_at(const char *s, size_t len, size_t i) {
	if(i >= len) return 0;
	unsigned char c= (unsigned char)s[i];
	if(c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v') return 1;
	if(i + 1 < len && c == 0xC2 && (unsigned char)s[i + 1] == 0xA0) return 2;
	return 0;
}

static size_t table_trim_ws_end(const char *s, size_t end) {
	while(end > 0) {
		if(end >= 2 && (unsigned char)s[end - 2] == 0xC2 && (unsigned char)s[end - 1] == 0xA0) {
			end-= 2;
			continue;
		}
		unsigned char c= (unsigned char)s[end - 1];
		if(c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v') {
			end--;
			continue;
		}
		break;
	}
	return end;
}

static Token *make_table_attr_dirty(const char *text, size_t text_len, Accum *accum) {
	Token *t= token_new(TOKEN_ATOM, "table-attr-dirty");
	if(!t) return NULL;
	if(text_len > 0) {
		token_append_text_n(t, text, text_len);
	} else {
		token_append_text_n(t, NULL, 0);
	}
	accum_push(accum, t);
	return t;
}

static Token *make_table_attr(const char *key, size_t key_len,
															const char *val, size_t val_len,
															const char *equal, size_t equal_len,
															char quote_open, char quote_close,
															Accum *accum) {
	Token *t= token_new(TOKEN_EXT_ATTR, "table-attr");
	if(!t) return NULL;

	 t->name= str_trim_lc(key, key_len);
	if(equal && equal_len > 0) {
		char *equal_owned= build_normalize_attr_equal(equal, equal_len, accum);
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
		/* JS parity: boolean table attrs still include empty attr-value child. */
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
 * JS parity: key validity check matching
 * /^(?:[\w:]|\0\d+[tas]\x7F)(?:[\w:.-]|\0\d+[tas]\x7F)*$/u
 * C stage-1 may emit arg-like sentinels ('a'/'s') in places where JS carries
 * transclusion-like sentinels, so accept t/a/s here for parity.
 * Any other non-[\w:.-] character makes the key invalid.
 */
static bool is_valid_attr_key(const char *k, size_t klen) {
	size_t i= 0;
	if(i >= klen) return false;

	/* first character: [\w:] or \0\d+[tas]\x7F */
	if((unsigned char)k[i] == 0x00) {
		/* template sentinel */
		i++;
		if(i >= klen) return false;
		if(k[i] < '0' || k[i] > '9') return false;
		while(i < klen && k[i] >= '0' && k[i] <= '9') i++;
		if(i >= klen || (k[i] != 't' && k[i] != 'a' && k[i] != 's')) return false;
		i++;
		if(i >= klen || (unsigned char)k[i] != 0x7F) return false;
		i++;
	} else {
		unsigned char c= (unsigned char)k[i];
		if(!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
				 (c >= '0' && c <= '9') || c == '_' || c == ':'))
			return false;
		i++;
	}

	/* continuation: [\w:.-] or \0\d+[tas]\x7F */
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
			unsigned char c= (unsigned char)k[i];
			if(!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
					 (c >= '0' && c <= '9') || c == '_' || c == ':' ||
					 c == '.' || c == '-'))
				return false;
			i++;
		}
	}
	return true;
}

static bool is_valid_attr_key_after_comment_trim(const char *k, size_t klen) {
	if(!k || klen == 0) return false;
	size_t clean_len= 0;
	char *clean= str_remove_comment(k, klen, &clean_len);
	if(!clean) return false;

	size_t start= 0;
	while(start < clean_len && (clean[start] == ' ' || clean[start] == '\t' || clean[start] == '\n' || clean[start] == '\r' || clean[start] == '\f' || clean[start] == '\v')) start++;
	size_t end= clean_len;
	while(end > start && (clean[end - 1] == ' ' || clean[end - 1] == '\t' || clean[end - 1] == '\n' || clean[end - 1] == '\r' || clean[end - 1] == '\f' || clean[end - 1] == '\v')) end--;

	bool ok= (end > start) && is_valid_attr_key(clean + start, end - start);
	free(clean);
	return ok;
}

/* Returns the byte length of the sentinel starting at buf[i] if its type
 * byte matches `type`, otherwise 0.
 * A valid sentinel: buf[i]=='\0', one or more ASCII digits, type byte, '\x7F'.
 * Minimum length: 4 bytes ('\0' + '0' + type + '\x7F'). */
static size_t sentinel_at(const char *buf, size_t len, size_t i, char type) {
	if (i >= len || (unsigned char)buf[i] != 0x00) return 0;
	size_t j = i + 1;
	while (j < len && (unsigned char)buf[j] >= '0' && (unsigned char)buf[j] <= '9') j++;
	if (j == i + 1) return 0;                        /* need at least one digit */
	if (j >= len || buf[j] != type) return 0;
	if (j + 1 >= len || (unsigned char)buf[j + 1] != 0x7F) return 0;
	return j + 2 - i;
}

/* Advance through leading whitespace (' ', '\t', '\r', '\n') and through
 * \x00\d+[cno]\x7F sentinels.  Returns the number of bytes consumed. */
size_t table_lead_skip(const char *line, size_t len) {
	size_t i = 0;
	while (i < len) {
		unsigned char c = (unsigned char)line[i];
		if (c == ' ' || c == '\t' || c == '\r' || c == '\n') { i++; continue; }
		if (c == 0x00) {
			size_t sl;
			if ((sl = sentinel_at(line, len, i, 'c'))) { i += sl; continue; }
			if ((sl = sentinel_at(line, len, i, 'n'))) { i += sl; continue; }
			if ((sl = sentinel_at(line, len, i, 'o'))) { i += sl; continue; }
		}
		break;
	}
	return i;
}

/* Parse table-start opener like: ^(:*)((?:\s|sentinels)*)(\{\||\{(?:sent)*\x00\d+!\x7F|\x00\d+\{\x7F)(.*)$ */
bool table_start_parse(const char *line, size_t len, TableStartResult *out) {
	if (!line || !out) return false;
	size_t i = 0;

	/* (:*) — leading colons */
	out->colon_count = 0;
	while (i < len && line[i] == ':') { out->colon_count++; i++; }

	/* ((?:\s|\x00\d+[cn]\x7F)*) — pre-whitespace / [cn] sentinels */
	size_t pre_start = i;
	while (i < len) {
		unsigned char c = (unsigned char)line[i];
		if (c == ' ' || c == '\t' || c == '\r' || c == '\n') { i++; continue; }
		size_t sl;
		if (c == 0x00) {
			if ((sl = sentinel_at(line, len, i, 'c'))) { i += sl; continue; }
			if ((sl = sentinel_at(line, len, i, 'n'))) { i += sl; continue; }
		}
		break;
	}
	out->pre_ws     = line + pre_start;
	out->pre_ws_len = i - pre_start;

	/* Try opener 1: literal {| */
	if (i + 1 < len && line[i] == '{' && line[i + 1] == '|') {
		out->opener_is_lbrace_sentinel = false;
		out->opener_has_bang_sentinel  = false;
		out->opener     = line + i;
		out->opener_len = 2;
		out->rest       = line + i + 2;
		out->rest_len   = len  - i - 2;
		return true;
	}

	/* Try opener 2: { + zero-or-more [cn] sentinels + \x00\d+!\x7F */
	if (i < len && line[i] == '{') {
		size_t j = i + 1;
		/* consume zero or more [cn] sentinels */
		while (j < len) {
			size_t sl;
			if ((unsigned char)line[j] == 0x00 &&
				((sl = sentinel_at(line, len, j, 'c')) ||
				 (sl = sentinel_at(line, len, j, 'n')))) {
				j += sl;
			} else {
				break;
			}
		}
		/* must be followed by \x00\d+!\x7F */
		size_t sl = sentinel_at(line, len, j, '!');
		if (sl) {
			size_t end = j + sl;
			out->opener_is_lbrace_sentinel = false;
			out->opener_has_bang_sentinel  = true;
			out->opener     = line + i;
			out->opener_len = end - i;
			out->rest       = line + end;
			out->rest_len   = len  - end;
			return true;
		}
	}

	/* Try opener 3: \x00\d+\{\x7F sentinel */
	{
		size_t sl = sentinel_at(line, len, i, '{');
		if (sl) {
			out->opener_is_lbrace_sentinel = true;
			out->opener_has_bang_sentinel  = false;
			out->opener     = line + i;
			out->opener_len = sl;
			out->rest       = line + i + sl;
			out->rest_len   = len  - i - sl;
			return true;
		}
	}

	return false;
}

bool table_line_classify(const char *line, size_t len, TableLineResult *out) {
	if (!line || !out || len == 0) return false;
	out->is_th    = false;
	out->has_plus = false;

	/* GROUP 1: table close — literal '|}' or sentinel+ '}' or sentinel '}' sentinel */
	/* literal |} */
	if (len >= 2 && line[0] == '|' && line[1] == '}') {
		out->kind = TABLE_LINE_CLOSE;
		out->rest = line + 2;
		out->rest_len = len - 2;
		return true;
	}
	/* sentinel '!' followed by '}' */
	size_t sl = sentinel_at(line, len, 0, '!');
	if (sl && sl < len && line[sl] == '}') {
		out->kind = TABLE_LINE_CLOSE;
		out->rest = line + sl + 1;
		out->rest_len = len - sl - 1;
		return true;
	}
	/* sentinel '}' */
	sl = sentinel_at(line, len, 0, '}');
	if (sl) {
		out->kind = TABLE_LINE_CLOSE;
		out->rest = line + sl;
		out->rest_len = len - sl;
		return true;
	}

	/* GROUP 2: row separator */
	if (len >= 2 && line[0] == '|' && line[1] == '-') {
		size_t j = 2;
		while (j < len && line[j] == '-') j++;
		out->kind = TABLE_LINE_ROW;
		out->rest = line + j;
		out->rest_len = len - j;
		return true;
	}
	sl = sentinel_at(line, len, 0, '!');
	if (sl && sl < len && line[sl] == '-') {
		size_t j = sl + 1;
		while (j < len && line[j] == '-') j++;
		out->kind = TABLE_LINE_ROW;
		out->rest = line + j;
		out->rest_len = len - j;
		return true;
	}
	sl = sentinel_at(line, len, 0, '-');
	if (sl) {
		size_t j = sl;
		while (j < len && line[j] == '-') j++;
		out->kind = TABLE_LINE_ROW;
		out->rest = line + j;
		out->rest_len = len - j;
		return true;
	}

	/* GROUP 3: cell opener */
	if (line[0] == '!') {
		out->kind = TABLE_LINE_CELL;
		out->is_th = true;
		out->rest = line + 1;
		out->rest_len = len - 1;
		return true;
	}
	if (line[0] == '|') {
		out->kind = TABLE_LINE_CELL;
		out->has_plus = (len >= 2 && line[1] == '+');
		size_t skip = out->has_plus ? 2 : 1;
		out->rest = line + skip;
		out->rest_len = len - skip;
		return true;
	}
	sl = sentinel_at(line, len, 0, '!');
	if (sl) {
		out->kind = TABLE_LINE_CELL;
		out->has_plus = (sl < len && line[sl] == '+');
		size_t skip = sl + (out->has_plus ? 1 : 0);
		out->rest = line + skip;
		out->rest_len = len - skip;
		return true;
	}

	return false;
}

/* Scan `buf` for cell separators.  `include_double_bang` = true for th rows. */
void table_sep_scan(const char *buf,
					size_t      len,
					bool        include_double_bang,
					TableSepCb  cb,
					void       *user_data) {
	if (!buf || !cb) return;
	size_t i = 0;
	char cand[3] = { '!', '|', '\0' };

	while (i < len) {
		const char *found = sz_find_byte_from(buf + i, len - i, cand, 3);
		if (!found) break;
		size_t p = (size_t)(found - buf);
		char c = buf[p];

		if (c == '!' && include_double_bang) {
			if (p + 1 < len && buf[p + 1] == '!') {
				cb(TABLE_SEP_DOUBLE_BANG, p, 2, user_data);
				i = p + 2;
				continue;
			}
			i = p + 1;
			continue;
		}

		if (c == '|') {
			/* || → double pipe */
			if (p + 1 < len && buf[p + 1] == '|') {
				cb(TABLE_SEP_DOUBLE_PIPE, p, 2, user_data);
				i = p + 2;
				continue;
			}
			/* | + \x00\d+!\x7F → double pipe */
			size_t sl = sentinel_at(buf, len, p + 1, '!');
			if (sl) {
				cb(TABLE_SEP_DOUBLE_PIPE, p, 1 + sl, user_data);
				i = p + 1 + sl;
				continue;
			}
			i = p + 1;
			continue;
		}

		if ((unsigned char)c == 0x00) {
			/* \x00\d+!\x7F */
			size_t sl = sentinel_at(buf, len, p, '!');
			if (sl) {
				size_t after = p + sl;
				/* sent + | */
				if (after < len && buf[after] == '|') {
					cb(TABLE_SEP_DOUBLE_PIPE, p, sl + 1, user_data);
					i = after + 1;
					continue;
				}
				size_t sl2 = sentinel_at(buf, len, after, '!');
				if (sl2) {
					cb(TABLE_SEP_DOUBLE_PIPE, p, sl + sl2, user_data);
					i = after + sl2;
					continue;
				}
				/* lone '!' sentinel — not a separator, skip */
				i = after;
				continue;
			}
			/* \x00\d+\+\x7F — the {{!!}} double-pipe sentinel */
			sl = sentinel_at(buf, len, p, '+');
			if (sl) {
				cb(TABLE_SEP_PLUS_SENTINEL, p, sl, user_data);
				i = p + sl;
				continue;
			}
			/* some other sentinel — skip one byte and keep scanning */
			i = p + 1;
			continue;
		}

		i = p + 1;
	}
}

/* Find the FIRST '|' or \x00\d+!\x7F in `buf`. Fires cb once and returns true.
 * Returns false if none found. */
bool td_inner_sep_find(const char *buf, size_t len, TdInnerSepCb cb, void *user_data) {
	if (!buf || !cb) return false;
	size_t i = 0;
	char cand[2] = { '|', '\0' };

	while (i < len) {
		const char *found = sz_find_byte_from(buf + i, len - i, cand, 2);
		if (!found) return false;
		size_t p = (size_t)(found - buf);

		if (buf[p] == '|') {
			cb(false, p, 1, user_data);
			return true;
		}

		/* '\x00': try \x00\d+!\x7F only */
		size_t sl = sentinel_at(buf, len, p, '!');
		if (sl) {
			cb(true, p, sl, user_data);
			return true;
		}

		/* other sentinel — skip and continue */
		i = p + 1;
	}
	return false;
}

typedef struct {
	bool found;
	bool is_sent;
	size_t pos;
	size_t sep_len;
} InnerSepCtx;

static void td_inner_sep_cb_bridge(bool is_sentinel, size_t pos, size_t sep_len, void *ud) {
	InnerSepCtx *c = (InnerSepCtx *)ud;
	c->found = true;
	c->is_sent = is_sentinel;
	c->pos = pos;
	c->sep_len = sep_len;
}

static bool table_attr_has_equal_marker(const char *s, size_t len) {
	if(!s || len == 0) return false;
	if(sz_find_byte(s, len, "=") != NULL) return true;
	for(size_t i= 0; i < len; i++) {
		size_t sl= sentinel_at(s, len, i, '~');
		if(sl) return true;
	}
	return false;
}

static bool table_attr_normalize_equal_syntax_tb(const char *eq, size_t eq_len, ThreadBuf *out_tb) {
	if(!eq || !out_tb) return false;

	size_t need= 0;
	for(size_t i= 0; i < eq_len;) {
		size_t sl= sentinel_at(eq, eq_len, i, '~');
		if(sl) {
			need+= 5; /* {{=}} */
			i+= sl;
			continue;
		}
		need++;
		i++;
	}

	wiki_thread_buf_reserve(out_tb, need);
	out_tb->len= 0;

	for(size_t i= 0; i < eq_len;) {
		size_t sl= sentinel_at(eq, eq_len, i, '~');
		if(sl) {
			sz_copy(out_tb->buf + out_tb->len, "{{=}}", 5);
			out_tb->len+= 5;
			i+= sl;
			continue;
		}
		out_tb->buf[out_tb->len++]= eq[i++];
	}
	out_tb->buf[out_tb->len]= '\0';
	return true;
}

static void parse_table_attrs(Token *attrs_tok, const char *attr_str, size_t attr_len, Accum *accum) {
	if(!attr_str || attr_len == 0) return;
	ThreadBuf *eq_norm_tb= wiki_thread_buf_acquire_scratch();

	/* JS parity: dynamic boolean attrs should produce table-attr token. */
	if(!table_attr_has_equal_marker(attr_str, attr_len)) {
		size_t first= 0;
		while(first < attr_len) {
			size_t ws= table_ws_len_at(attr_str, attr_len, first);
			if(ws == 0) break;
			first+= ws;
		}
		size_t last= attr_len;
		last= table_trim_ws_end(attr_str, last);

		if(last > first) {
			const char *k= attr_str + first;
			size_t klen= last - first;
			int has_space= 0;
			for(size_t p= 0; p < klen; p++) {
				if(table_ws_len_at(k, klen, p) > 0) {
					has_space= 1;
					break;
				}
			}
			bool dynamic_key= table_attr_key_is_dynamic(k, klen);
			if(dynamic_key && !has_space) {
				/* JS parity: only create table-attr for keys that pass the JS
                 * validity test /^(?:[\w:]|\0\d+t\x7F)(?:[\w:.-]|\0\d+t\x7F)*$/u.
                 * Anything else (e.g. "\01t\x7F|}") becomes table-attr-dirty. */
				if(!is_valid_attr_key_after_comment_trim(k, klen)) {
					Token *d= make_table_attr_dirty(attr_str, attr_len, accum);
					if(d) token_append_child(attrs_tok, d);
					return;
				}
				if(first > 0) {
					Token *d0= make_table_attr_dirty(attr_str, first, accum);
					if(d0) token_append_child(attrs_tok, d0);
				}
				Token *at= make_table_attr(k, klen, NULL, 0, NULL, 0, '\0', '\0', accum);
				if(at) token_append_child(attrs_tok, at);
				if(last < attr_len) {
					Token *d1= make_table_attr_dirty(attr_str + last, attr_len - last, accum);
					if(d1) token_append_child(attrs_tok, d1);
				}
				return;
			}
		}
	}

	size_t i= 0;
	size_t dirty_start= (size_t)-1;
	size_t dirty_end= 0;

#define FLUSH_DIRTY()                                                \
	do {                                                               \
		if(dirty_start != (size_t)-1 && dirty_end > dirty_start) {      \
			Token *dt= make_table_attr_dirty(attr_str + dirty_start, dirty_end - dirty_start, accum); \
			if(dt) token_append_child(attrs_tok, dt);                      \
			dirty_start= (size_t)-1;                                       \
			dirty_end= 0;                                                  \
		}                                                                \
	} while(0)

#define APPEND_DIRTY_SPAN(off, span_len)                                         \
	do {                                                                           \
		size_t _off= (off);                                                           \
		size_t _len= (span_len);                                                      \
		if(_len > 0) {                                                                 \
			if(dirty_start == (size_t)-1) {                                              \
				dirty_start= _off;                                                           \
				dirty_end= _off + _len;                                                     \
			} else if(dirty_end == _off) {                                               \
				dirty_end+= _len;                                                           \
			} else {                                                                      \
				FLUSH_DIRTY();                                                              \
				dirty_start= _off;                                                          \
				dirty_end= _off + _len;                                                    \
			}                                                                             \
		}                                                                               \
	} while(0)

	while(i < attr_len) {
		size_t ws_take= table_ws_len_at(attr_str, attr_len, i);
		if(attr_str[i] == '/' || ws_take > 0) {
			if(ws_take > 0) {
				APPEND_DIRTY_SPAN(i, ws_take);
				i+= ws_take;
			} else {
				APPEND_DIRTY_SPAN(i, 1);
				i++;
			}
			continue;
		}

		size_t key_start= i;
		while(i < attr_len) {
			if(table_ws_len_at(attr_str, attr_len, i) > 0) break;
			if(attr_str[i] == '/' || attr_str[i] == '=') {
				break;
			}
			if(sentinel_at(attr_str, attr_len, i, '~')) break;
			i++;
		}
		size_t key_len= i - key_start;
		if(key_len == 0) {
			size_t eq_take= sentinel_at(attr_str, attr_len, i, '~');
			if(i < attr_len && (attr_str[i] == '=' || eq_take > 0)) {
				if(eq_take == 0) eq_take= 1;
				APPEND_DIRTY_SPAN(i, eq_take);
				i+= eq_take;
				if(i < attr_len && (attr_str[i] == '"' || attr_str[i] == '\'')) {
					size_t vstart= i;
					char q= attr_str[i++];
					while(i < attr_len) {
						if(attr_str[i++] == q) break;
					}
					APPEND_DIRTY_SPAN(vstart, i - vstart);
				} else {
					size_t vstart= i;
					while(i < attr_len && table_ws_len_at(attr_str, attr_len, i) == 0) {
						i++;
					}
					APPEND_DIRTY_SPAN(vstart, i - vstart);
				}
				continue;
			}
			APPEND_DIRTY_SPAN(i, 1);
			i++;
			continue;
		}

		const char *key= attr_str + key_start;
		unsigned char kc0= (unsigned char)key[0];
		int valid_key= ((kc0 >= 'A' && kc0 <= 'Z') || (kc0 >= 'a' && kc0 <= 'z') ||
							 (kc0 >= '0' && kc0 <= '9') || kc0 == '_' || kc0 == ':');
		for(size_t k= 1; valid_key && k < key_len; k++) {
			unsigned char kc= (unsigned char)key[k];
			valid_key= ((kc >= 'A' && kc <= 'Z') || (kc >= 'a' && kc <= 'z') || (kc >= '0' && kc <= '9') || kc == ':' || kc == '.' || kc == '_' || kc == '-');
		}
		if(!valid_key) {
			bool dynamic_key= table_attr_key_is_dynamic(key, key_len);
			if(!dynamic_key || !is_valid_attr_key_after_comment_trim(key, key_len)) {
				APPEND_DIRTY_SPAN(key_start, key_len);
				/* JS parity: when an invalid key is immediately followed by '=value',
				 * keep the whole chunk dirty instead of treating value as a new attr key. */
				size_t eq_sl= (i < attr_len) ? sentinel_at(attr_str, attr_len, i, '~') : 0;
				if(i < attr_len && (attr_str[i] == '=' || eq_sl > 0)) {
					size_t eq_take= (attr_str[i] == '=') ? 1 : eq_sl;
					APPEND_DIRTY_SPAN(i, eq_take);
					i+= eq_take;
					if(i < attr_len && (attr_str[i] == '"' || attr_str[i] == '\'')) {
						size_t vstart= i;
						char q= attr_str[i++];
						while(i < attr_len) {
							if(attr_str[i++] == q) break;
						}
						APPEND_DIRTY_SPAN(vstart, i - vstart);
					} else {
						size_t vstart= i;
						while(i < attr_len && table_ws_len_at(attr_str, attr_len, i) == 0) {
							i++;
						}
						APPEND_DIRTY_SPAN(vstart, i - vstart);
					}
				}
				continue;
			}
		}

		size_t ws_start= i;
		while(i < attr_len) {
			size_t gap= table_attr_gap_len_at(attr_str, attr_len, i);
			if(gap == 0) break;
			i+= gap;
		}

		size_t eq_sl= (i < attr_len) ? sentinel_at(attr_str, attr_len, i, '~') : 0;
		if(i >= attr_len || (attr_str[i] != '=' && eq_sl == 0)) {
			FLUSH_DIRTY();
			Token *at= make_table_attr(key, key_len, NULL, 0, NULL, 0, '\0', '\0', accum);
			if(at) token_append_child(attrs_tok, at);
			if(ws_start < i) {
				APPEND_DIRTY_SPAN(ws_start, i - ws_start);
			}
			continue;
		}

		size_t eq_start= ws_start;
		size_t eq_marker_len= (attr_str[i] == '=') ? 1 : eq_sl;
		bool eq_has_magic= (eq_marker_len > 1);
		i+= eq_marker_len;
		while(i < attr_len) {
			size_t gap= table_attr_gap_len_at(attr_str, attr_len, i);
			if(gap == 0) break;
			i+= gap;
		}
		size_t eq_len= i - eq_start;

		char quote_open= '\0', quote_close= '\0';
		const char *val= NULL;
		size_t val_len= 0;

		if(i < attr_len && (attr_str[i] == '"' || attr_str[i] == '\'')) {
			quote_open= attr_str[i++];
			size_t val_start= i;
			while(i < attr_len && attr_str[i] != quote_open) i++;
			val= attr_str + val_start;
			val_len= i - val_start;
			if(i < attr_len && attr_str[i] == quote_open) {
				quote_close= attr_str[i];
				i++;
			}
		} else {
			size_t val_start= i;
			while(i < attr_len && table_ws_len_at(attr_str, attr_len, i) == 0) i++;
			val= attr_str + val_start;
			val_len= i - val_start;
		}

		FLUSH_DIRTY();
		const char *eq_ptr= attr_str + eq_start;
		size_t eq_use_len= eq_len;
		bool eq_norm_ok= false;
		if(eq_has_magic) {
			eq_norm_ok= table_attr_normalize_equal_syntax_tb(attr_str + eq_start, eq_len, eq_norm_tb);
			if(eq_norm_ok) {
				eq_ptr= eq_norm_tb->buf;
				eq_use_len= eq_norm_tb->len;
			}
		}
		Token *at= make_table_attr(key, key_len, val, val_len,
											 eq_ptr, eq_use_len,
															 quote_open, quote_close,
															 accum);
		(void)eq_norm_ok;
		if(at) token_append_child(attrs_tok, at);
	}

	FLUSH_DIRTY();
	wiki_thread_buf_release_scratch(eq_norm_tb);
#undef APPEND_DIRTY_SPAN
#undef FLUSH_DIRTY
}

typedef struct {
	Token **items;
	size_t count;
	size_t cap;
} TokStack;

static void stack_init(TokStack *st) {
	st->items= NULL;
	st->count= 0;
	st->cap= 0;
}

static void stack_push(TokStack *st, Token *t) {
	if(st->count >= st->cap) {
		size_t nc= st->cap ? st->cap * 2 : 16;
		st->items= realloc(st->items, nc * sizeof(Token *));
		assert(st->items);
		st->cap= nc;
	}
	st->items[st->count++]= t;
}

static Token *stack_pop(TokStack *st) {
	if(!st || st->count == 0) return NULL;
	return st->items[--st->count];
}

static Token *stack_peek(const TokStack *st) {
	if(!st || st->count == 0) return NULL;
	return st->items[st->count - 1];
}

static void stack_free(TokStack *st) {
	free(st->items);
	st->items= NULL;
	st->count= 0;
	st->cap= 0;
}

static int token_is_tr_like(const Token *tok) {
	if(!tok || tok->child_count == 0) return 0;
	const Child *last= &tok->children[tok->child_count - 1];
	if(last->is_text || !last->token) return 0;
	return last->token->type != TOKEN_PLAIN;
}

static Token *js_pop(Token *top, TokStack *st) {
	if(top && top->type == TOKEN_TD) return stack_pop(st);
	return top;
}

static void out_append(char **out_buf, size_t *out_len, size_t *out_cap,
											 const char *s, size_t n) {
	if(n == 0) return;
	while(*out_len + n + 1 >= *out_cap) {
		*out_cap*= 2;
		*out_buf= realloc(*out_buf, *out_cap);
		assert(*out_buf);
	}
	sz_copy(*out_buf + *out_len, s, n);
	*out_len+= n;
}

/* Find next separator in buf starting at `start`. Returns true and fills
 * pos_outlen_out on match, false otherwise.
 */
static bool table_sep_find(const char *buf, size_t len, size_t start,
						   bool include_double_bang,
						   size_t *pos_out, size_t *len_out) {
	if (!buf || !pos_out || !len_out) return false;
	if (start >= len) return false;
	size_t i = start;
	while (i < len) {
		const char cand[3] = { '!', '|', '\0' };
		const char *found = sz_find_byte_from(buf + i, len - i, cand, 3);
		if (!found) return false;
		size_t p = (size_t)(found - buf);
		char c = buf[p];

		if (c == '!' && include_double_bang) {
			if (p + 1 < len && buf[p + 1] == '!') {
				*pos_out = p; *len_out = 2; return true;
			}
			i = p + 1; continue;
		}

		if (c == '|') {
			if (p + 1 < len && buf[p + 1] == '|') { *pos_out = p; *len_out = 2; return true; }
			size_t sl = sentinel_at(buf, len, p + 1, '!');
			if (sl) { *pos_out = p; *len_out = 1 + sl; return true; }
			i = p + 1; continue;
		}

		if ((unsigned char)c == 0x00) {
			size_t sl = sentinel_at(buf, len, p, '!');
			if (sl) {
				size_t after = p + sl;
				if (after < len && buf[after] == '|') { *pos_out = p; *len_out = sl + 1; return true; }
				size_t sl2 = sentinel_at(buf, len, after, '!');
				if (sl2) { *pos_out = p; *len_out = sl + sl2; return true; }
				i = after; continue;
			}
			size_t slp = sentinel_at(buf, len, p, '+');
			if (slp) { *pos_out = p; *len_out = slp; return true; }
			i = p + 1; continue;
		}

		i = p + 1;
	}
	return false;
}

static void push_text_like_js(char **out_buf, size_t *out_len, size_t *out_cap,
															const char *s, size_t n,
															Token *top, const ParserConfig *cfg, Accum *accum) {
	(void)cfg;
	if(n == 0) return;
	if(!top) {
		out_append(out_buf, out_len, out_cap, s, n);
		return;
	}

	if(token_is_tr_like(top)) {
		Token *inter= token_new(TOKEN_PLAIN, "table-inter");
		if(!inter) return;
		inter->stage= 3;
		if(n > 0) {
			token_append_text_n(inter, s, n);
		} else {
			token_append_text_n(inter, NULL, 0);
		}
		accum_push(accum, inter);
		token_append_child(top, inter);
		return;
	}

	if(top->child_count > 0) {
		Child *last= &top->children[top->child_count - 1];
		if(!last->is_text && last->token) {
			/* JS parity: lastChild.setText(lastChild.toString() + str)
             * This means we must concatenate into the last text child of
             * last->token, not add a new one. */
			Token *inner= last->token;
			if(inner->child_count > 0) {
				Child *inner_last= &inner->children[inner->child_count - 1];
				if(inner_last->is_text) {
					ThreadBuf *scratch = wiki_thread_buf_acquire_scratch();
					wiki_thread_buf_set(scratch, inner_last->text, inner_last->text_len);
										wiki_thread_buf_append(scratch, (sz_string_view_t){ .start = s, .length = n });
					char *merged_owned = malloc(scratch->len + 1);
					if(!merged_owned) {
						wiki_thread_buf_release_scratch(scratch);
						return;
					}
					if(scratch->len > 0) sz_copy(merged_owned, scratch->buf, scratch->len);
					merged_owned[scratch->len]= '\0';
					if(inner_last->text_owned && inner_last->text) free((void*)inner_last->text);
					inner_last->text = merged_owned;
					inner_last->text_len = scratch->len;
					inner_last->text_owned = true;
					wiki_thread_buf_release_scratch(scratch);
					return;
				}
			}
			if(n > 0) {
				token_append_text_n(inner, s, n);
			} else {
				token_append_text_n(inner, NULL, 0);
			}
			return;
		}
	}

		if(n > 0) {
			token_append_text_n(top, s, n);
		} else {
			token_append_text_n(top, NULL, 0);
		}
}

static Token *create_table_token(const char *syntax, size_t syntax_len,
																 const char *attr, size_t attr_len,
																 Accum *accum) {
	Token *table= token_new(TOKEN_TABLE, "table");
	if(!table) return NULL;
	accum_push(accum, table);

	Token *syn= token_new(TOKEN_SYNTAX, "table-syntax");
	if(syn) {
		if(syntax_len > 0) {
			token_append_text_n(syn, syntax, syntax_len);
		} else {
			token_append_text_n(syn, NULL, 0);
		}
		accum_push(accum, syn);
		token_append_child(table, syn);
	}

	Token *attrs= token_new(TOKEN_ATTRIBUTES, "table-attrs");
	if(attrs) {
			attrs->name= strdup("table");
		parse_table_attrs(attrs, attr, attr_len, accum);
		accum_push(accum, attrs);
		token_append_child(table, attrs);
	}

	return table;
}

void parse_table(ThreadBuf *tb, const ParserConfig *cfg, Accum *accum) {
	(void)cfg;
	if(!tb || !tb->buf) return;
	size_t line_count= 1;
	for(size_t i= 0; i < tb->len; i++)
		if(tb->buf[i] == '\n') line_count++;

	const char **lines_ptr= malloc(line_count * sizeof(char *));
	size_t *lines_len= malloc(line_count * sizeof(size_t));
	assert(lines_ptr && lines_len);

	size_t li= 0;
	size_t start= 0;
	for(size_t i= 0; i <= tb->len; i++) {
		if(i == tb->len || tb->buf[i] == '\n') {
			lines_ptr[li]= tb->buf + start;
			lines_len[li]= i - start;
			li++;
			start= i + 1;
		}
	}
	line_count= li;

	size_t out_cap= tb->len * 2 + 128;
	char *out_buf= malloc(out_cap);
	assert(out_buf);
	size_t out_len= 0;

	TokStack st;
	stack_init(&st);
	ThreadBuf *line_scratch= wiki_thread_buf_acquire_scratch();

	for(size_t i= 0; i < line_count; i++) {
		const char *out_line= lines_ptr[i];
		size_t out_line_len= lines_len[i];
		Token *top= stack_pop(&st);

		size_t spaces_len = table_lead_skip(out_line, out_line_len);
		const char *line= out_line + spaces_len;
		size_t line_len= out_line_len - spaces_len;

		TableStartResult tsr;
		bool start_rc = table_start_parse(line, line_len, &tsr);
		if(start_rc) {
			while(top && top->type != TOKEN_TD) top= stack_pop(&st);

			const char *indent = line;
			size_t indent_len = tsr.colon_count;
			const char *more = tsr.pre_ws;
			size_t more_len = tsr.pre_ws_len;
			const char *syn = tsr.opener;
			size_t syn_len = tsr.opener_len;
			const char *attr = tsr.rest ? tsr.rest : "";
			size_t attr_len = tsr.rest_len;

			size_t dd_idx= 0;
			int has_dd= 0;
			if(indent_len > 0) {
				Token *dd= token_new(TOKEN_DD, "dd");
				if(dd) {
					if(indent_len > 0) {
						token_append_text_n(dd, indent, indent_len);
					} else {
						token_append_text_n(dd, NULL, 0);
					}
					accum_push(accum, dd);
					dd_idx= accum->count - 1;
					has_dd= 1;
				}
			}

			size_t table_idx= accum->count;
			Token *table= create_table_token(syn, syn_len, attr, attr_len, accum);

			wiki_thread_buf_set(line_scratch, "\n", 1);
			if(spaces_len > 0) {
				wiki_thread_buf_append(line_scratch, (sz_string_view_t){ .start = out_line, .length = spaces_len });
			}
			if(has_dd) {
				char m[64];
				size_t ml= 0;
				work_str_sentinel(dd_idx, 'd', m, &ml);
				wiki_thread_buf_append(line_scratch, (sz_string_view_t){ .start = m, .length = ml });
			}
			if(more_len > 0) {
				wiki_thread_buf_append(line_scratch, (sz_string_view_t){ .start = more, .length = more_len });
			}
			if(table) {
				char m[64];
				size_t ml= 0;
				work_str_sentinel(table_idx, 'b', m, &ml);
				wiki_thread_buf_append(line_scratch, (sz_string_view_t){ .start = m, .length = ml });
			}

			push_text_like_js(&out_buf, &out_len, &out_cap, line_scratch->buf, line_scratch->len, top, cfg, accum);

			if(top) stack_push(&st, top);
			if(table) stack_push(&st, table);
			continue;
		}
		if(!top) {
			out_append(&out_buf, &out_len, &out_cap, "\n", 1);
			out_append(&out_buf, &out_len, &out_cap, out_line, out_line_len);
			continue;
		}

		TableLineResult lres;
		if (!table_line_classify(line, line_len, &lres)) {
			wiki_thread_buf_set(line_scratch, "\n", 1);
			wiki_thread_buf_append(line_scratch, (sz_string_view_t){ .start = out_line, .length = out_line_len });
			push_text_like_js(&out_buf, &out_len, &out_cap, line_scratch->buf, line_scratch->len, top, cfg, accum);
			stack_push(&st, top);
			continue;
		}

		const char *attr = lres.rest ? lres.rest : "";
		size_t attr_len = lres.rest_len;

		if (lres.kind == TABLE_LINE_CLOSE) {
			while(top && top->type != TOKEN_TABLE) top = stack_pop(&st);
			if(top) {
				size_t clos_len = line_len - attr_len;
				wiki_thread_buf_set(line_scratch, "\n", 1);
				if(spaces_len > 0) wiki_thread_buf_append(line_scratch, (sz_string_view_t){ .start = out_line, .length = spaces_len });
				if(clos_len > 0) wiki_thread_buf_append(line_scratch, (sz_string_view_t){ .start = line, .length = clos_len });

				Token *clos = token_new(TOKEN_SYNTAX, "table-syntax");
				if(clos) {
					if(line_scratch->len > 0) {
						token_append_text_n(clos, line_scratch->buf, line_scratch->len);
					} else {
						token_append_text_n(clos, NULL, 0);
					}
					accum_push(accum, clos);
					token_append_child(top, clos);
				}
			}
			push_text_like_js(&out_buf, &out_len, &out_cap, attr, attr_len, stack_peek(&st), cfg, accum);
		} else if (lres.kind == TABLE_LINE_ROW) {
			top= js_pop(top, &st);
			if(top && top->type == TOKEN_TR) top= stack_pop(&st);

			size_t row_len = line_len - attr_len;
			wiki_thread_buf_set(line_scratch, "\n", 1);
			if(spaces_len > 0) wiki_thread_buf_append(line_scratch, (sz_string_view_t){ .start = out_line, .length = spaces_len });
			if(row_len > 0) wiki_thread_buf_append(line_scratch, (sz_string_view_t){ .start = line, .length = row_len });

			Token *tr = create_tr_token(line_scratch->buf, line_scratch->len, attr, attr_len, accum);
			if(top && tr) token_append_child(top, tr);
			if(top) stack_push(&st, top);
			if(tr) stack_push(&st, tr);
		} else {
			top= js_pop(top, &st);

			const char *cell = line;
			size_t cell_len = line_len - attr_len;
			bool include_double_bang = (cell_len == 1 && cell[0] == '!');

			size_t last_index = 0;
			wiki_thread_buf_set(line_scratch, "\n", 1);
			if(spaces_len > 0) wiki_thread_buf_append(line_scratch, (sz_string_view_t){ .start = out_line, .length = spaces_len });
			if(cell_len > 0) wiki_thread_buf_append(line_scratch, (sz_string_view_t){ .start = cell, .length = cell_len });
			const char *last_syn_ptr = line_scratch->buf;
			size_t last_syn_len = line_scratch->len;

			size_t scan = 0;
			for(;;) {
				size_t sep_pos = attr_len;
				size_t sep_len = 0;
				bool found = table_sep_find(attr, attr_len, scan, include_double_bang, &sep_pos, &sep_len);

				size_t seg_len = sep_pos - last_index;
				const char *seg = attr + last_index;

				const char *cell_attrs = "";
				size_t cell_attrs_len = 0;
				const char *inner_syntax = "";
				size_t inner_syntax_len = 0;
				const char *inner = seg;
				size_t inner_len = seg_len;

				InnerSepCtx ic = {0};
				td_inner_sep_find(seg, seg_len, td_inner_sep_cb_bridge, &ic);
				if(ic.found) {
					cell_attrs = seg;
					cell_attrs_len = ic.pos;
					inner_syntax = seg + ic.pos;
					inner_syntax_len = ic.sep_len;
					inner = seg + ic.pos + ic.sep_len;
					inner_len = seg_len - (ic.pos + ic.sep_len);

					
										if(contains_literal_seq(cell_attrs, cell_attrs_len, "[[", 2) || contains_literal_seq(cell_attrs, cell_attrs_len, "-{", 2)) {
						cell_attrs = "";
						cell_attrs_len = 0;
						inner_syntax = "";
						inner_syntax_len = 0;
						inner = seg;
						inner_len = seg_len;
					}
				}

				Token *td = create_td_token(last_syn_ptr, last_syn_len,
											 cell_attrs, cell_attrs_len,
											 inner_syntax, inner_syntax_len,
											 inner, inner_len,
											 accum);
				if(top && td) token_append_child(top, td);

				if(!found || sep_len == 0) {
					if(top) stack_push(&st, top);
					if(td) stack_push(&st, td);
					break;
				}

				last_syn_ptr = attr + sep_pos;
				last_syn_len = sep_len;

				last_index = sep_pos + sep_len;
				scan = last_index;
			}
		}
	}

	out_buf[out_len]= '\0';
	if(out_len > 0) {
		wiki_thread_buf_set(tb, out_buf + 1, out_len - 1);
	} else {
		wiki_thread_buf_set(tb, out_buf, 0);
	}
	free(out_buf);
	wiki_thread_buf_release_scratch(line_scratch);
	stack_free(&st);
	free(lines_ptr);
	free(lines_len);
}
