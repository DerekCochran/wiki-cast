#include "parser/tr.h"
#include "string_util.h"
#include "token.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

static Token *make_attr_key(const char *key, size_t key_len, Accum *accum) {
	Token *t= token_new(TOKEN_ATTR_KEY, "attr-key");
	if(!t) return NULL;
	token_append_text_n(t, key, key_len);
	accum_push(accum, t);
	return t;
}

static Token *make_attr_value(const char *val, size_t val_len, Accum *accum) {
	Token *t= token_new(TOKEN_ATTR_VALUE, "attr-value");
	if(!t) return NULL;
	token_append_text_n(t, val, val_len);
	accum_push(accum, t);
	return t;
}

static Token *make_table_attr_dirty(const char *text, size_t text_len, Accum *accum) {
	Token *t= token_new(TOKEN_ATOM, "table-attr-dirty");
	if(!t) return NULL;
	token_append_text_n(t, text, text_len);
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
		t->data.ext_attr.equal= malloc(equal_len + 1);
		assert(t->data.ext_attr.equal);
		memcpy(t->data.ext_attr.equal, equal, equal_len);
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

/* JS parity: /^(?:[\w:]|\0\d+t\x7F)(?:[\w:.-]|\0\d+t\x7F)*$/u */
static bool is_valid_attr_key(const char *k, size_t klen) {
	size_t i= 0;
	if(i >= klen) return false;

	if((unsigned char)k[i] == 0x00) {
		i++;
		if(i >= klen) return false;
		if(k[i] < '0' || k[i] > '9') return false;
		while(i < klen && k[i] >= '0' && k[i] <= '9') i++;
		if(i >= klen || k[i] != 't') return false;
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
			if(i >= klen || k[i] != 't') return false;
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

static void parse_table_attrs(Token *attrs_tok, const char *attr_str, size_t attr_len, Accum *accum) {
	if(!attr_str || attr_len == 0) return;

	/* JS parity: dynamic boolean attrs should produce table-attr token. */
	if(memchr(attr_str, '=', attr_len) == NULL) {
		size_t first= 0;
		while(first < attr_len && (attr_str[first] == ' ' || attr_str[first] == '\t' || attr_str[first] == '\n' || attr_str[first] == '\r' || attr_str[first] == '\f' || attr_str[first] == '\v')) first++;
		size_t last= attr_len;
		while(last > first && (attr_str[last - 1] == ' ' || attr_str[last - 1] == '\t' || attr_str[last - 1] == '\n' || attr_str[last - 1] == '\r' || attr_str[last - 1] == '\f' || attr_str[last - 1] == '\v')) last--;

		if(last > first) {
			const char *k= attr_str + first;
			size_t klen= last - first;
			int has_space= 0;
			for(size_t p= 0; p < klen; p++) {
				char ch= k[p];
				if(ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\f' || ch == '\v') {
					has_space= 1;
					break;
				}
			}
			bool dynamic_key= memchr(k, '\0', klen) != NULL || (klen >= 2 && k[0] == '{' && k[1] == '{') || (klen >= 2 && k[0] == '-' && k[1] == '{');
			if(dynamic_key && !has_space && is_valid_attr_key(k, klen)) {
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
		if(attr_str[i] == '/' || attr_str[i] == ' ' || attr_str[i] == '\t' || attr_str[i] == '\n' || attr_str[i] == '\r' || attr_str[i] == '\f' || attr_str[i] == '\v') {
			dirty_buf[dirty_len++]= attr_str[i++];
			continue;
		}

		size_t key_start= i;
		while(i < attr_len && attr_str[i] != '/' && attr_str[i] != '=' && attr_str[i] != ' ' && attr_str[i] != '\t' && attr_str[i] != '\n' && attr_str[i] != '\r' && attr_str[i] != '\f' && attr_str[i] != '\v') {
			i++;
		}
		size_t key_len= i - key_start;
		if(key_len == 0) {
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
			bool dynamic_key= memchr(key, '\0', key_len) != NULL || (key_len >= 2 && key[0] == '{' && key[1] == '{') || (key_len >= 2 && key[0] == '-' && key[1] == '{');
			if(!dynamic_key || !is_valid_attr_key(key, key_len)) {
				for(size_t k= 0; k < key_len; k++) dirty_buf[dirty_len++]= key[k];
				continue;
			}
		}

		size_t ws_start= i;
		while(i < attr_len && (attr_str[i] == ' ' || attr_str[i] == '\t' || attr_str[i] == '\n' || attr_str[i] == '\r' || attr_str[i] == '\f' || attr_str[i] == '\v')) i++;

		if(i >= attr_len || attr_str[i] != '=') {
			FLUSH_DIRTY();
			Token *at= make_table_attr(key, key_len, NULL, 0, NULL, 0, '\0', '\0', accum);
			if(at) token_append_child(attrs_tok, at);
			if(ws_start < i) {
				memcpy(dirty_buf, attr_str + ws_start, i - ws_start);
				dirty_len= i - ws_start;
			}
			continue;
		}

		size_t eq_start= ws_start;
		i++;
		while(i < attr_len && (attr_str[i] == ' ' || attr_str[i] == '\t' || attr_str[i] == '\n' || attr_str[i] == '\r' || attr_str[i] == '\f' || attr_str[i] == '\v')) i++;
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
			while(i < attr_len && attr_str[i] != ' ' && attr_str[i] != '\t' && attr_str[i] != '\n' && attr_str[i] != '\r' && attr_str[i] != '\f' && attr_str[i] != '\v') i++;
			val= attr_str + val_start;
			val_len= i - val_start;
		}

		FLUSH_DIRTY();
		Token *at= make_table_attr(key, key_len, val, val_len,
															 attr_str + eq_start, eq_len,
															 quote_open, quote_close,
															 accum);
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
	if(syntax && syntax_len > 0) token_append_text_n(syn, syntax, syntax_len);
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
