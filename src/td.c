#include "parser/td.h"
#include "token.h"
#include "string_util.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static Token *make_attr_key(const char *key, size_t key_len, Accum *accum)
{
    Token *t = token_new(TOKEN_ATTR_KEY, "attr-key");
    if (!t) return NULL;
    token_append_text_n(t, key, key_len);
    accum_push(accum, t);
    return t;
}

static Token *make_attr_value(const char *val, size_t val_len, Accum *accum)
{
    Token *t = token_new(TOKEN_ATTR_VALUE, "attr-value");
    if (!t) return NULL;
    token_append_text_n(t, val, val_len);
    accum_push(accum, t);
    return t;
}

static Token *make_table_attr_dirty(const char *text, size_t text_len, Accum *accum)
{
    Token *t = token_new(TOKEN_ATOM, "table-attr-dirty");
    if (!t) return NULL;
    token_append_text_n(t, text, text_len);
    accum_push(accum, t);
    return t;
}

static Token *make_table_attr(const char *key, size_t key_len,
                              const char *val, size_t val_len,
                              const char *equal, size_t equal_len,
                              char quote_open, char quote_close,
                              Accum *accum)
{
    Token *t = token_new(TOKEN_EXT_ATTR, "table-attr");
    if (!t) return NULL;

    t->name = str_trim_lc(key, key_len);
    if (equal && equal_len > 0) {
        t->data.ext_attr.equal = malloc(equal_len + 1);
        assert(t->data.ext_attr.equal);
        memcpy(t->data.ext_attr.equal, equal, equal_len);
        t->data.ext_attr.equal[equal_len] = '\0';
    }
    t->data.ext_attr.quote_open = quote_open;
    t->data.ext_attr.quote_close = quote_close;

    Token *attr_key = make_attr_key(key, key_len, accum);
    if (!attr_key) { token_free(t); return NULL; }
    token_append_child(t, attr_key);

    if (val) {
        Token *attr_val = make_attr_value(val, val_len, accum);
        if (!attr_val) { token_free(t); return NULL; }
        token_append_child(t, attr_val);
    }

    accum_push(accum, t);
    return t;
}

static void parse_table_attrs(Token *attrs_tok, const char *attr_str, size_t attr_len, Accum *accum)
{
    if (!attr_str || attr_len == 0) return;

    size_t i = 0;
    char dirty_buf[4096];
    size_t dirty_len = 0;

#define FLUSH_DIRTY() do { \
    if (dirty_len > 0) { \
        Token *dt = make_table_attr_dirty(dirty_buf, dirty_len, accum); \
        if (dt) token_append_child(attrs_tok, dt); \
        dirty_len = 0; \
    } \
} while (0)

    while (i < attr_len) {
        if (attr_str[i] == '/' || attr_str[i] == ' ' || attr_str[i] == '\t' || attr_str[i] == '\n' || attr_str[i] == '\r' || attr_str[i] == '\f' || attr_str[i] == '\v') {
            dirty_buf[dirty_len++] = attr_str[i++];
            continue;
        }

        size_t key_start = i;
        while (i < attr_len && attr_str[i] != '/' && attr_str[i] != '='
               && attr_str[i] != ' ' && attr_str[i] != '\t' && attr_str[i] != '\n'
               && attr_str[i] != '\r' && attr_str[i] != '\f' && attr_str[i] != '\v') {
            i++;
        }
        size_t key_len = i - key_start;
        if (key_len == 0) {
            dirty_buf[dirty_len++] = attr_str[i++];
            continue;
        }

        const char *key = attr_str + key_start;
        unsigned char kc0 = (unsigned char)key[0];
        int valid_key = ((kc0 >= 'A' && kc0 <= 'Z') || (kc0 >= 'a' && kc0 <= 'z') || kc0 == '_' || kc0 == ':');
        for (size_t k = 1; valid_key && k < key_len; k++) {
            unsigned char kc = (unsigned char)key[k];
            valid_key = ((kc >= 'A' && kc <= 'Z') || (kc >= 'a' && kc <= 'z') || (kc >= '0' && kc <= '9') || kc == ':' || kc == '.' || kc == '_' || kc == '-');
        }
        if (!valid_key) {
            for (size_t k = 0; k < key_len; k++) dirty_buf[dirty_len++] = key[k];
            continue;
        }

        size_t ws_start = i;
        while (i < attr_len && (attr_str[i] == ' ' || attr_str[i] == '\t' || attr_str[i] == '\n' || attr_str[i] == '\r' || attr_str[i] == '\f' || attr_str[i] == '\v')) i++;

        if (i >= attr_len || attr_str[i] != '=') {
            FLUSH_DIRTY();
            Token *at = make_table_attr(key, key_len, NULL, 0, NULL, 0, '\0', '\0', accum);
            if (at) token_append_child(attrs_tok, at);
            if (ws_start < i) {
                memcpy(dirty_buf, attr_str + ws_start, i - ws_start);
                dirty_len = i - ws_start;
            }
            continue;
        }

        size_t eq_start = ws_start;
        i++;
        while (i < attr_len && (attr_str[i] == ' ' || attr_str[i] == '\t' || attr_str[i] == '\n' || attr_str[i] == '\r' || attr_str[i] == '\f' || attr_str[i] == '\v')) i++;
        size_t eq_len = i - eq_start;

        char quote_open = '\0', quote_close = '\0';
        const char *val = NULL;
        size_t val_len = 0;

        if (i < attr_len && (attr_str[i] == '"' || attr_str[i] == '\'')) {
            quote_open = attr_str[i++];
            size_t val_start = i;
            while (i < attr_len && attr_str[i] != quote_open) i++;
            val = attr_str + val_start;
            val_len = i - val_start;
            if (i < attr_len && attr_str[i] == quote_open) {
                quote_close = attr_str[i];
                i++;
            }
        } else {
            size_t val_start = i;
            while (i < attr_len && attr_str[i] != ' ' && attr_str[i] != '\t' && attr_str[i] != '\n'
                   && attr_str[i] != '\r' && attr_str[i] != '\f' && attr_str[i] != '\v') i++;
            val = attr_str + val_start;
            val_len = i - val_start;
        }

        FLUSH_DIRTY();
        Token *at = make_table_attr(key, key_len, val, val_len,
                                    attr_str + eq_start, eq_len,
                                    quote_open, quote_close,
                                    accum);
        if (at) token_append_child(attrs_tok, at);
    }

    FLUSH_DIRTY();
#undef FLUSH_DIRTY
}

static const char *cell_attr_name(const char *syntax, size_t syntax_len)
{
    char last = syntax_len > 0 ? syntax[syntax_len - 1] : '|';
    if (last == '!') return "th";
    if (last == '+') return "caption";
    return "td";
}

/* Create a TdToken with SyntaxToken, AttributesToken and inner plain token.
 * syntax_len / attr_len / inner_len are binary-safe byte lengths
 * (may contain embedded NUL sentinels). */
Token *create_td_token(const char *syntax,
                       size_t syntax_len,
                       const char *attr, size_t attr_len,
                       const char *inner_syntax, size_t inner_syntax_len,
                       const char *inner, size_t inner_len,
                       Accum *accum)
{
    Token *td = token_new(TOKEN_TD, "td");
    if (!td) return NULL;
    accum_push(accum, td);

    Token *syn = token_new(TOKEN_SYNTAX, "table-syntax");
    if (!syn) return td;
    if (syntax && syntax_len > 0) token_append_text_n(syn, syntax, syntax_len);
    accum_push(accum, syn);
    token_append_child(td, syn);

    Token *attrs = token_new(TOKEN_ATTRIBUTES, "table-attrs");
    if (!attrs) return td;
    attrs->name = strdup(cell_attr_name(syntax, syntax_len));
    parse_table_attrs(attrs, attr, attr_len, accum);
    accum_push(accum, attrs);
    token_append_child(td, attrs);

    if (inner_syntax && inner_syntax_len > 0) {
        td->data.td.inner_syntax = malloc(inner_syntax_len + 1);
        assert(td->data.td.inner_syntax);
        memcpy(td->data.td.inner_syntax, inner_syntax, inner_syntax_len);
        td->data.td.inner_syntax[inner_syntax_len] = '\0';
    } else {
        td->data.td.inner_syntax = strdup("");
        assert(td->data.td.inner_syntax);
    }

    Token *inner_tok = token_new(TOKEN_PLAIN, "td-inner");
    if (!inner_tok) return td;
    if (inner && inner_len > 0) token_append_text_n(inner_tok, inner, inner_len);
    else token_append_text_n(inner_tok, "", 0);
    accum_push(accum, inner_tok);
    token_append_child(td, inner_tok);

    return td;
}
