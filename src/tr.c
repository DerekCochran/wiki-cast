#include "parser/tr.h"
#include "build.h"
#include "util/string_util.h"
#include "stringzilla/stringzilla.h"
#include "token.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

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

/* JS parity: /^(?:[\w:]|\0\d+[tas]\x7F)(?:[\w:.-]|\0\d+[tas]\x7F)*$/u */
static bool is_valid_attr_key(const char *k, size_t klen) {
	size_t i= 0;
	if(i >= klen) return false;

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
				 (c >= '0' && c <= '9') || c == '_' || c == ':'))
			return false;
		i++;
	}

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

static size_t table_ws_len_at(const char *s, size_t len, size_t i) {
	if(i >= len) return 0;
	unsigned char c= (unsigned char)s[i];
	if(c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v') return 1;
	/* JS regex \s parity for common table inputs: NBSP (U+00A0). */
	if(i + 1 < len && c == 0xC2 && (unsigned char)s[i + 1] == 0xA0) return 2;
	return 0;
}

static size_t sentinel_at(const char *buf, size_t len, size_t i, char type);

static bool table_attr_key_is_dynamic(const char *key, size_t key_len) {
	return sz_find_byte(key, key_len, "\0") != NULL ||
		(key_len >= 2 && key[0] == '{' && key[1] == '{') ||
		(key_len >= 2 && key[0] == '-' && key[1] == '{');
}

static size_t table_attr_gap_len_at(const char *s, size_t len, size_t i) {
	size_t ws= table_ws_len_at(s, len, i);
	if(ws > 0) return ws;
	ws= sentinel_at(s, len, i, 'c');
	if(ws > 0) return ws;
	return sentinel_at(s, len, i, 'n');
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

/* Returns the byte length of the sentinel starting at buf[i] if its type
 * byte matches `type`, otherwise 0.
 * A valid sentinel: buf[i]=='\0', one or more ASCII digits, type byte, '\x7F'. */
static size_t sentinel_at(const char *buf, size_t len, size_t i, char type) {
	if(i >= len || (unsigned char)buf[i] != 0x00) return 0;
	size_t j= i + 1;
	while(j < len && (unsigned char)buf[j] >= '0' && (unsigned char)buf[j] <= '9') j++;
	if(j == i + 1) return 0;
	if(j >= len || buf[j] != type) return 0;
	if(j + 1 >= len || (unsigned char)buf[j + 1] != 0x7F) return 0;
	return j + 2 - i;
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

static char *table_attr_normalize_equal_syntax(const char *eq, size_t eq_len, size_t *out_len) {
	if(!eq) return NULL;

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

	char *out= malloc(need + 1);
	if(!out) return NULL;

	size_t p= 0;
	for(size_t i= 0; i < eq_len;) {
		size_t sl= sentinel_at(eq, eq_len, i, '~');
		if(sl) {
			sz_copy(out + p, "{{=}}", 5);
			p+= 5;
			i+= sl;
			continue;
		}
		out[p++]= eq[i++];
	}
	out[p]= '\0';
	if(out_len) *out_len= p;
	return out;
}

static void parse_table_attrs(Token *attrs_tok, const char *attr_str, size_t attr_len, Accum *accum) {
	if(!attr_str || attr_len == 0) return;

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
			if(dynamic_key && !has_space && is_valid_attr_key_after_comment_trim(k, klen)) {
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
	char dirty_buf[4096];
	size_t dirty_len= 0;

#define FLUSH_DIRTY()                                                \
	do {                                                               \
		if(dirty_len > 0) {                                              \
			Token *dt= make_table_attr_dirty(dirty_buf, dirty_len, accum); \
			if(dt) token_append_child(attrs_tok, dt);                      \
			dirty_len= 0;                                                  \
		}                                                                \
	} while(0)

	while(i < attr_len) {
		size_t ws_take= table_ws_len_at(attr_str, attr_len, i);
		if(attr_str[i] == '/' || ws_take > 0) {
			if(ws_take > 0) {
				sz_copy(dirty_buf + dirty_len, attr_str + i, ws_take);
				dirty_len+= ws_take;
				i+= ws_take;
			} else {
				dirty_buf[dirty_len++]= attr_str[i++];
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
				sz_copy(dirty_buf + dirty_len, attr_str + i, eq_take);
				dirty_len+= eq_take;
				i+= eq_take;
				if(i < attr_len && (attr_str[i] == '"' || attr_str[i] == '\'')) {
					char q= attr_str[i];
					dirty_buf[dirty_len++]= attr_str[i++];
					while(i < attr_len) {
						dirty_buf[dirty_len++]= attr_str[i];
						if(attr_str[i++] == q) break;
					}
				} else {
					while(i < attr_len && table_ws_len_at(attr_str, attr_len, i) == 0) {
						dirty_buf[dirty_len++]= attr_str[i++];
					}
				}
				continue;
			}
			dirty_buf[dirty_len++]= attr_str[i++];
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
				for(size_t k= 0; k < key_len; k++) dirty_buf[dirty_len++]= key[k];
				/* JS parity: when an invalid key is immediately followed by '=value',
				 * keep the whole chunk dirty instead of treating value as a new attr key. */
				size_t eq_sl= (i < attr_len) ? sentinel_at(attr_str, attr_len, i, '~') : 0;
				if(i < attr_len && (attr_str[i] == '=' || eq_sl > 0)) {
					size_t eq_take= (attr_str[i] == '=') ? 1 : eq_sl;
					sz_copy(dirty_buf + dirty_len, attr_str + i, eq_take);
					dirty_len+= eq_take;
					i+= eq_take;
					if(i < attr_len && (attr_str[i] == '"' || attr_str[i] == '\'')) {
						char q= attr_str[i];
						dirty_buf[dirty_len++]= attr_str[i++];
						while(i < attr_len) {
							dirty_buf[dirty_len++]= attr_str[i];
							if(attr_str[i++] == q) break;
						}
					} else {
						while(i < attr_len && table_ws_len_at(attr_str, attr_len, i) == 0) {
							dirty_buf[dirty_len++]= attr_str[i++];
						}
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
				sz_copy(dirty_buf, attr_str + ws_start, i - ws_start);
				dirty_len= i - ws_start;
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
		char *eq_norm= NULL;
		if(eq_has_magic) {
			eq_norm= table_attr_normalize_equal_syntax(attr_str + eq_start, eq_len, &eq_use_len);
			if(eq_norm) eq_ptr= eq_norm;
		}
		Token *at= make_table_attr(key, key_len, val, val_len,
														 eq_ptr, eq_use_len,
															 quote_open, quote_close,
															 accum);
		if(eq_norm) free(eq_norm);
		if(at) token_append_child(attrs_tok, at);
	}

	FLUSH_DIRTY();
#undef FLUSH_DIRTY
}

/* Create a TrToken with a SyntaxToken and AttributesToken children. */
Token *create_tr_token(const char *syntax, size_t syntax_len,
											 const char *attr, size_t attr_len,
											 Accum *accum) {
	Token *tr= token_new(TOKEN_TR, "tr");
	if(!tr) return NULL;
	accum_push(accum, tr);

	Token *syn= token_new(TOKEN_SYNTAX, "table-syntax");
	if(!syn) return tr;
	if(syntax && syntax_len > 0) {
		token_append_text_n(syn, syntax, syntax_len);
	}
	accum_push(accum, syn);
	token_append_child(tr, syn);

	Token *attrs= token_new(TOKEN_ATTRIBUTES, "table-attrs");
	if(!attrs) return tr;
	attrs->name= strdup("tr");
	parse_table_attrs(attrs, attr, attr_len, accum);
	accum_push(accum, attrs);
	token_append_child(tr, attrs);

	return tr;
}
