#define PCRE2_CODE_UNIT_WIDTH 8
#include <pcre2.h>

#include "parser/table.h"
#include "parser/td.h"
#include "parser/tr.h"
#include "string_util.h"
#include "table_token.h"
#include "token.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static pcre2_code *s_re_table_lead= NULL;
static pcre2_code *s_re_table_start= NULL;
static pcre2_code *s_re_table_line= NULL;
static pcre2_code *s_re_table_sep_th= NULL;
static pcre2_code *s_re_table_sep_td= NULL;
static pcre2_code *s_re_td_inner_sep= NULL;

static pcre2_code *compile_table_regex(const char *pattern) {
	PCRE2_SIZE err_offset;
	int err_code;
	pcre2_code *re= pcre2_compile((PCRE2_SPTR)pattern, PCRE2_ZERO_TERMINATED,
																PCRE2_UTF | PCRE2_UCP,
																&err_code, &err_offset, NULL);
	assert(re);
	return re;
}

static void ensure_table_regexes(void) {
	if(!s_re_table_lead) {
		s_re_table_lead= compile_table_regex("^(?:\\s|\\x00\\d+[cno]\\x7F)*");
		s_re_table_start= compile_table_regex("^(:*)((?:\\s|\\x00\\d+[cn]\\x7F)*)(\\{\\||\\{(?:\\x00\\d+[cn]\\x7F)*\\x00\\d+!\\x7F|\\x00\\d+\\{\\x7F)(.*)$");
		s_re_table_line= compile_table_regex("^(?:(\\|\\}|\\x00\\d+!\\x7F\\}|\\x00\\d+\\}\\x7F)|(\\|-+|\\x00\\d+!\\x7F-+|\\x00\\d+-\\x7F-*)(?!-)|(!|(?:\\||\\x00\\d+!\\x7F)\\+?))(.*)$");
		s_re_table_sep_th= compile_table_regex("!!|(?:\\||\\x00\\d+!\\x7F){2}|\\x00\\d+\\+\\x7F");
		s_re_table_sep_td= compile_table_regex("(?:\\||\\x00\\d+!\\x7F){2}|\\x00\\d+\\+\\x7F");
		s_re_td_inner_sep= compile_table_regex("\\||\\x00\\d+!\\x7F");
	}
}

static int match_regex(pcre2_code *re, const char *s, size_t len, size_t start,
											 pcre2_match_data **md_out) {
	pcre2_match_data *md= pcre2_match_data_create_from_pattern(re, NULL);
	if(!md) return -1;
	int rc= pcre2_match(re, (PCRE2_SPTR)s, len, start, 0, md, NULL);
	if(rc < 0) {
		pcre2_match_data_free(md);
		return rc;
	}
	*md_out= md;
	return rc;
}

static int contains_literal_seq(const char *s, size_t len, const char *needle) {
	size_t nlen= strlen(needle);
	if(nlen == 0 || len < nlen) return 0;
	for(size_t i= 0; i + nlen <= len; i++) {
		if(memcmp(s + i, needle, nlen) == 0) return 1;
	}
	return 0;
}

static size_t sentinel_len_if(const char *s, size_t len, size_t pos, char marker) {
	if(!s || pos >= len || s[pos] != '\0') return 0;
	size_t k= pos + 1;
	size_t digits= 0;
	while(k < len && s[k] >= '0' && s[k] <= '9') {
		k++;
		digits++;
	}
	if(digits == 0) return 0;
	if(k + 1 >= len) return 0;
	if(s[k] != marker) return 0;
	if((unsigned char)s[k + 1] != 0x7F) return 0;
	return (k + 2) - pos;
}

static size_t pipe_unit_len(const char *s, size_t len, size_t pos) {
	if(!s || pos >= len) return 0;
	if(s[pos] == '|') return 1;
	return sentinel_len_if(s, len, pos, '!');
}

static size_t cell_sep_len(const char *s, size_t len, size_t pos, char cell_char) {
	if(!s || pos >= len) return 0;

	if(cell_char == '!' && pos + 1 < len && s[pos] == '!' && s[pos + 1] == '!') {
		return 2;
	}

	size_t u1= pipe_unit_len(s, len, pos);
	if(u1 > 0) {
		size_t u2= pipe_unit_len(s, len, pos + u1);
		if(u2 > 0) return u1 + u2;
	}

	return sentinel_len_if(s, len, pos, '+');
}

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

/**
 * JS parity: key validity check matching
 * /^(?:[\w:]|\0\d+t\x7F)(?:[\w:.-]|\0\d+t\x7F)*$/u
 * A template sentinel \0<digits>t\x7F is valid as key start or continuation.
 * Any other non-[\w:.-] character makes the key invalid.
 */
static bool is_valid_attr_key(const char *k, size_t klen) {
	size_t i= 0;
	if(i >= klen) return false;

	/* first character: [\w:] or \0\d+t\x7F */
	if((unsigned char)k[i] == 0x00) {
		/* template sentinel */
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

	/* continuation: [\w:.-] or \0\d+t\x7F */
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
			if(dynamic_key && !has_space) {
				/* JS parity: only create table-attr for keys that pass the JS
                 * validity test /^(?:[\w:]|\0\d+t\x7F)(?:[\w:.-]|\0\d+t\x7F)*$/u.
                 * Anything else (e.g. "\01t\x7F|}") becomes table-attr-dirty. */
				if(!is_valid_attr_key(k, klen)) {
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
	memcpy(*out_buf + *out_len, s, n);
	*out_len+= n;
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
		token_append_text_n(inter, s, n);
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
					size_t new_len= inner_last->text_len + n;
					char *merged= malloc(new_len + 1);
					assert(merged);
					memcpy(merged, inner_last->text, inner_last->text_len);
					memcpy(merged + inner_last->text_len, s, n);
					merged[new_len]= '\0';
					free(inner_last->text);
					inner_last->text= merged;
					inner_last->text_len= new_len;
					return;
				}
			}
			token_append_text_n(inner, s, n);
			return;
		}
	}

	token_append_text_n(top, s, n);
}

static Token *create_table_token(const char *syntax, size_t syntax_len,
																 const char *attr, size_t attr_len,
																 Accum *accum) {
	Token *table= token_new(TOKEN_TABLE, "table");
	if(!table) return NULL;
	accum_push(accum, table);

	Token *syn= token_new(TOKEN_SYNTAX, "table-syntax");
	if(syn) {
		token_append_text_n(syn, syntax, syntax_len);
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
	ensure_table_regexes();

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

	for(size_t i= 0; i < line_count; i++) {
		const char *out_line= lines_ptr[i];
		size_t out_line_len= lines_len[i];
		Token *top= stack_pop(&st);

		pcre2_match_data *lead_md= NULL;
		size_t spaces_len= 0;
		int lead_rc= match_regex(s_re_table_lead, out_line, out_line_len, 0, &lead_md);
		if(lead_rc > 0) {
			PCRE2_SIZE *ov= pcre2_get_ovector_pointer(lead_md);
			spaces_len= ov[1];
		}
		if(lead_md) pcre2_match_data_free(lead_md);

		const char *line= out_line + spaces_len;
		size_t line_len= out_line_len - spaces_len;

		pcre2_match_data *start_md= NULL;
		int start_rc= match_regex(s_re_table_start, line, line_len, 0, &start_md);
		if(start_rc > 0) {
			while(top && top->type != TOKEN_TD) top= stack_pop(&st);

			PCRE2_SIZE *ov= pcre2_get_ovector_pointer(start_md);
			size_t indent_s= (start_rc > 1 && ov[2] != PCRE2_UNSET) ? ov[2] : 0;
			size_t indent_e= (start_rc > 1 && ov[3] != PCRE2_UNSET) ? ov[3] : 0;
			size_t more_s= (start_rc > 2 && ov[4] != PCRE2_UNSET) ? ov[4] : 0;
			size_t more_e= (start_rc > 2 && ov[5] != PCRE2_UNSET) ? ov[5] : 0;
			size_t syn_s= (start_rc > 3 && ov[6] != PCRE2_UNSET) ? ov[6] : 0;
			size_t syn_e= (start_rc > 3 && ov[7] != PCRE2_UNSET) ? ov[7] : 0;
			size_t attr_s= (start_rc > 4 && ov[8] != PCRE2_UNSET) ? ov[8] : 0;
			size_t attr_e= (start_rc > 4 && ov[9] != PCRE2_UNSET) ? ov[9] : 0;

			const char *indent= line + indent_s;
			size_t indent_len= indent_e - indent_s;
			const char *more= line + more_s;
			size_t more_len= more_e - more_s;
			const char *syn= line + syn_s;
			size_t syn_len= syn_e - syn_s;
			const char *attr= (attr_e > attr_s) ? (line + attr_s) : "";
			size_t attr_len= (attr_e > attr_s) ? (attr_e - attr_s) : 0;

			size_t dd_idx= 0;
			int has_dd= 0;
			if(indent_len > 0) {
				Token *dd= token_new(TOKEN_DD, "dd");
				if(dd) {
					token_append_text_n(dd, indent, indent_len);
					accum_push(accum, dd);
					dd_idx= accum->count - 1;
					has_dd= 1;
				}
			}

			size_t table_idx= accum->count;
			Token *table= create_table_token(syn, syn_len, attr, attr_len, accum);

			size_t pre_cap= 4 + spaces_len + more_len + 64 + 64;
			char *pre= malloc(pre_cap);
			assert(pre);
			size_t plen= 0;
			pre[plen++]= '\n';
			if(spaces_len > 0) {
				memcpy(pre + plen, out_line, spaces_len);
				plen+= spaces_len;
			}
			if(has_dd) {
				char m[64];
				size_t ml= 0;
				work_str_sentinel(dd_idx, 'd', m, &ml);
				memcpy(pre + plen, m, ml);
				plen+= ml;
			}
			if(more_len > 0) {
				memcpy(pre + plen, more, more_len);
				plen+= more_len;
			}
			if(table) {
				char m[64];
				size_t ml= 0;
				work_str_sentinel(table_idx, 'b', m, &ml);
				memcpy(pre + plen, m, ml);
				plen+= ml;
			}

			push_text_like_js(&out_buf, &out_len, &out_cap, pre, plen, top, cfg, accum);
			free(pre);

			if(top) stack_push(&st, top);
			if(table) stack_push(&st, table);
			pcre2_match_data_free(start_md);
			continue;
		}
		if(start_md) pcre2_match_data_free(start_md);

		if(!top) {
			out_append(&out_buf, &out_len, &out_cap, "\n", 1);
			out_append(&out_buf, &out_len, &out_cap, out_line, out_line_len);
			continue;
		}

		pcre2_match_data *line_md= NULL;
		int line_rc= match_regex(s_re_table_line, line, line_len, 0, &line_md);
		if(line_rc <= 0) {
			size_t n= out_line_len + 1;
			char *tmp= malloc(n);
			tmp[0]= '\n';
			memcpy(tmp + 1, out_line, out_line_len);
			push_text_like_js(&out_buf, &out_len, &out_cap, tmp, n, top, cfg, accum);
			free(tmp);
			stack_push(&st, top);
			if(line_md) pcre2_match_data_free(line_md);
			continue;
		}

		PCRE2_SIZE *ov= pcre2_get_ovector_pointer(line_md);
		size_t closing_s= (line_rc > 1 && ov[2] != PCRE2_UNSET) ? ov[2] : 0;
		size_t closing_e= (line_rc > 1 && ov[3] != PCRE2_UNSET) ? ov[3] : 0;
		size_t row_s= (line_rc > 2 && ov[4] != PCRE2_UNSET) ? ov[4] : 0;
		size_t row_e= (line_rc > 2 && ov[5] != PCRE2_UNSET) ? ov[5] : 0;
		size_t cell_s= (line_rc > 3 && ov[6] != PCRE2_UNSET) ? ov[6] : 0;
		size_t cell_e= (line_rc > 3 && ov[7] != PCRE2_UNSET) ? ov[7] : 0;
		size_t attr_s= (line_rc > 4 && ov[8] != PCRE2_UNSET) ? ov[8] : 0;
		size_t attr_e= (line_rc > 4 && ov[9] != PCRE2_UNSET) ? ov[9] : 0;

		const char *attr= (attr_e > attr_s) ? (line + attr_s) : "";
		size_t attr_len= (attr_e > attr_s) ? (attr_e - attr_s) : 0;

		if(closing_e > closing_s) {
			while(top && top->type != TOKEN_TABLE) top= stack_pop(&st);
			if(top) {
				size_t clos_len= closing_e - closing_s;
				size_t syn_len= 1 + spaces_len + clos_len;
				char *syn= malloc(syn_len);
				syn[0]= '\n';
				if(spaces_len > 0) memcpy(syn + 1, out_line, spaces_len);
				memcpy(syn + 1 + spaces_len, line + closing_s, clos_len);

				Token *clos= token_new(TOKEN_SYNTAX, "table-syntax");
				if(clos) {
					token_append_text_n(clos, syn, syn_len);
					accum_push(accum, clos);
					token_append_child(top, clos);
				}
				free(syn);
			}
			push_text_like_js(&out_buf, &out_len, &out_cap, attr, attr_len, stack_peek(&st), cfg, accum);
		} else if(row_e > row_s) {
			top= js_pop(top, &st);
			if(top && top->type == TOKEN_TR) top= stack_pop(&st);

			size_t row_len= row_e - row_s;
			size_t syn_len= 1 + spaces_len + row_len;
			char *syn= malloc(syn_len);
			syn[0]= '\n';
			if(spaces_len > 0) memcpy(syn + 1, out_line, spaces_len);
			memcpy(syn + 1 + spaces_len, line + row_s, row_len);

			Token *tr= create_tr_token(syn, syn_len, attr, attr_len, accum);
			free(syn);
			if(top && tr) token_append_child(top, tr);
			if(top) stack_push(&st, top);
			if(tr) stack_push(&st, tr);
		} else {
			top= js_pop(top, &st);

			const char *cell= line + cell_s;
			size_t cell_len= cell_e - cell_s;
			pcre2_code *sep_re= (cell_len == 1 && cell[0] == '!') ? s_re_table_sep_th : s_re_table_sep_td;

			size_t last_index= 0;
			size_t last_syn_len= 1 + spaces_len + cell_len;
			char *last_syn= malloc(last_syn_len);
			last_syn[0]= '\n';
			if(spaces_len > 0) memcpy(last_syn + 1, out_line, spaces_len);
			memcpy(last_syn + 1 + spaces_len, cell, cell_len);

			size_t scan= 0;
			while(1) {
				pcre2_match_data *sep_md= NULL;
				int sep_rc= match_regex(sep_re, attr, attr_len, scan, &sep_md);
				size_t sep_pos= attr_len;
				size_t sep_len= 0;
				if(sep_rc > 0) {
					PCRE2_SIZE *sov= pcre2_get_ovector_pointer(sep_md);
					sep_pos= sov[0];
					sep_len= sov[1] - sov[0];
				}
				if(sep_md) pcre2_match_data_free(sep_md);

				size_t seg_len= sep_pos - last_index;
				const char *seg= attr + last_index;

				const char *cell_attrs= "";
				size_t cell_attrs_len= 0;
				const char *inner_syntax= "";
				size_t inner_syntax_len= 0;
				const char *inner= seg;
				size_t inner_len= seg_len;

				pcre2_match_data *inner_md= NULL;
				int inner_rc= match_regex(s_re_td_inner_sep, seg, seg_len, 0, &inner_md);
				if(inner_rc > 0) {
					PCRE2_SIZE *iov= pcre2_get_ovector_pointer(inner_md);
					cell_attrs= seg;
					cell_attrs_len= iov[0];
					inner_syntax= seg + iov[0];
					inner_syntax_len= iov[1] - iov[0];
					inner= seg + iov[1];
					inner_len= seg_len - iov[1];

					if(contains_literal_seq(cell_attrs, cell_attrs_len, "[[") || contains_literal_seq(cell_attrs, cell_attrs_len, "-{")) {
						cell_attrs= "";
						cell_attrs_len= 0;
						inner_syntax= "";
						inner_syntax_len= 0;
						inner= seg;
						inner_len= seg_len;
					}
				}
				if(inner_md) pcre2_match_data_free(inner_md);

				Token *td= create_td_token(last_syn, last_syn_len,
																	 cell_attrs, cell_attrs_len,
																	 inner_syntax, inner_syntax_len,
																	 inner, inner_len,
																	 accum);
				if(top && td) token_append_child(top, td);

				if(sep_len == 0) {
					if(top) stack_push(&st, top);
					if(td) stack_push(&st, td);
					break;
				}

				free(last_syn);
				last_syn_len= sep_len;
				last_syn= malloc(last_syn_len);
				memcpy(last_syn, attr + sep_pos, sep_len);

				last_index= sep_pos + sep_len;
				scan= last_index;
			}
			free(last_syn);
		}

		pcre2_match_data_free(line_md);
	}

	out_buf[out_len]= '\0';
	if(out_len > 0) {
		wiki_thread_buf_set(tb, out_buf + 1, out_len - 1);
	} else {
		wiki_thread_buf_set(tb, out_buf, 0);
	}
	free(out_buf);
	stack_free(&st);
	free(lines_ptr);
	free(lines_len);
}
