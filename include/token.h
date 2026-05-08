/*
 * token.h — Token node types and lifecycle.
 *
 * Mirrors dist/src/index.js and the token hierarchy under dist/src/.
 * Each token type corresponds to a JS class.  Children can be either
 * text (char *) or another Token *.
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include "util/thread_buffer.h"

/* ── Token type enum ──────────────────────────────────────────────────────── */
typedef enum {
    TOKEN_TEXT           = 0,  /* leaf text node (AstText) */
    TOKEN_ROOT,                /* root plain token */
    TOKEN_PLAIN,               /* generic plain token */
    TOKEN_COMMENT,             /* CommentToken          'c' */
    TOKEN_EXT,                 /* ExtToken              'e' */
    TOKEN_NOINCLUDE,           /* NoincludeToken        'n' */
    TOKEN_INCLUDE,             /* IncludeToken          'n' */
    TOKEN_TRANSLATE,           /* TranslateToken        'g' */
    TOKEN_ONLYINCLUDE,         /* OnlyincludeToken      'g' */
    TOKEN_ARG,                 /* ArgToken              'a' */
    TOKEN_TRANSCLUDE,          /* TranscludeToken       't' */
    TOKEN_HEADING,             /* HeadingToken          'h' */
    TOKEN_HTML,                /* HtmlToken             'x' */
    TOKEN_TABLE,               /* TableToken            'b' */
    TOKEN_TR,                  /* TrToken               'b' */
    TOKEN_TD,                  /* TdToken               'b' */
    TOKEN_HR,                  /* HrToken               'r' */
    TOKEN_DOUBLE_UNDERSCORE,   /* DoubleUnderscoreToken 'n' / 'u' */
    TOKEN_LINK,                /* LinkToken             'l' */
    TOKEN_FILE,                /* FileToken             'l' */
    TOKEN_CATEGORY,            /* CategoryToken         'l' */
    TOKEN_REDIRECT,            /* RedirectToken         'o' */
    TOKEN_REDIRECT_TARGET,     /* RedirectTargetToken */
    TOKEN_REDIRECT_SYNTAX,     /* SyntaxToken inside redirect */
    TOKEN_QUOTE,               /* QuoteToken            'q' */
    TOKEN_EXT_LINK,            /* ExtLinkToken          'w' */
    TOKEN_MAGIC_LINK,          /* MagicLinkToken        'i' / 'w' / 'f' */
    TOKEN_LIST,                /* ListToken             'd' */
    TOKEN_DD,                  /* DdToken               'd' */
    TOKEN_CONVERTER,           /* ConverterToken        'v' */
    TOKEN_PARAMETER,           /* ParameterToken (child of TranscludeToken) */
    TOKEN_ATTRIBUTES,          /* AttributesToken       'a' */
    TOKEN_SYNTAX,              /* SyntaxToken */
    TOKEN_ATOM,                /* AtomToken */
    TOKEN_HIDDEN,              /* HiddenToken */
    /* Sub-tokens for ExtToken */
    TOKEN_EXT_ATTRS,           /* AttributesToken with type "ext-attrs" */
    TOKEN_EXT_INNER,           /* plain Token with type "ext-inner" */
    TOKEN_EXT_ATTR_DIRTY,      /* AtomToken with type "ext-attr-dirty" */
    TOKEN_EXT_ATTR,            /* AttributeToken with type "ext-attr" */
    TOKEN_ATTR_KEY,            /* plain Token with type "attr-key" */
    TOKEN_ATTR_VALUE,          /* plain Token with type "attr-value" */
    TOKEN_TYPE_COUNT
} TokenType;

/* ── Child node ───────────────────────────────────────────────────────────── */
typedef struct {
    bool   is_text;       /* true → text node; false → token node */
    size_t text_len;      /* byte length of text (valid when is_text; may contain NUL sentinels) */
    union {
        const char  *text;      /* non-owning view into a ThreadBuf (or owned if text_owned) */
        struct Token *token; /* owned token pointer for token children */
    };
    bool   text_owned;    /* true if u.text was heap-allocated and must be freed */
} Child;

/* ── Per-type payload (union to save memory) ─────────────────────────────── */
typedef union {
    struct {
        int  level;                    /* 1-6 for HeadingToken */
    } heading;
    struct {
        bool closed;                   /* CommentToken: whether --> present */
    } comment;
    struct {
        bool self_closing;             /* HtmlToken */
        bool closing;
        char *orig_tag;                /* original-case tag name for toString */
    } html;
    struct {
        char *inner_syntax;            /* TdToken separator between attrs and inner */
    } td;
    struct {
        bool case_sensitive;           /* DoubleUnderscoreToken */
        bool fullwidth;                /* preserve ＿＿...＿＿ vs __...__ */
    } dunder;
    struct {
        bool bold;                     /* QuoteToken */
        bool italic;
    } quote;
    struct {
        char *pre;                     /* RedirectToken: leading whitespace */
        char *post;                    /* trailing whitespace */
        char *link;                    /* raw link target */
        char *display;                 /* optional display text (after |) */
    } redirect;
    struct {
        char *name;                    /* ExtToken: tag name */
        char *attr;                    /* attribute string */
        char *inner;                   /* inner content */
        char *closing;                 /* closing tag text */
        bool  self_closing;
    } ext;
    struct {
        char *tag;                     /* IncludeToken: "includeonly"/"noinclude" */
        char *attr;
        char *inner;
        char *closing;                 /* closing tag text, NULL if unclosed */
    } include;
    struct {
        char *equal;      /* equal sign + surrounding whitespace — owned, NULL if boolean attr */
        char  quote_open; /* opening quote ('"', '\'', or '\0' if unquoted/boolean) */
        char  quote_close;/* closing quote ('"', '\'', or '\0' if unclosed/unquoted) */
    } ext_attr;
    struct {
        char *raw_syntax; /* canonical syntax string for image-parameter toString */
    } image_param;
    struct {
        char *space;      /* ExtLinkToken separator between URL and text (may be empty) */
    } ext_link;
    struct {
        char *modifier;   /* TranscludeToken modifier prefix, e.g. "subst:" */
    } transclude;
} TokenData;

/* ── Token struct ─────────────────────────────────────────────────────────── */
typedef struct Token {
    TokenType  type;
    char      *type_name;   /* e.g. "root", "redirect", "comment" — owned */
    char      *name;        /* tag/template name where applicable — owned */
    char       sep;         /* separator for token_to_string(): '\0' or '\n' */

    Child     *children;
    size_t     child_count;
    size_t     child_cap;

    TokenData  data;        /* per-type payload */

    /* parsing state */
    unsigned   seen_epoch; /* scratch mark for graph freeing (updated by token_free) */
    int        stage;       /* last parseOnce stage executed */
    bool       include;     /* includeOnly mode */
    bool       built;       /* build() has been called */
} Token;

/* ── Lifecycle ────────────────────────────────────────────────────────────── */

/** Allocate a new token with the given type.  type_name is strdup'd. */
Token *token_new(TokenType type, const char *type_name);

/** Append a TEXT child to a token from a pointer+length substring. */
void token_append_text_n(Token *t, const char *text, size_t len);

/** Append a TOKEN child to a token.  Takes ownership of child. */
void token_append_child(Token *t, Token *child);

/** Recursively free a token and all its children. */
void token_free(Token *t);

/** Shallow-free a token: free its own memory and any text children, but do NOT
 * recursively free child tokens. This is useful when tokens are owned in a
 * flat accumulator and must be freed individually without double-freeing.
 */
void token_free_shallow(Token *t);

/** Serialize a token tree to JSON, writing to fp. */
void token_to_json(const Token *t, FILE *fp);
void token_log_json(const Token *t);

/**
 * Recursively serialise a token tree into a caller-provided buffer,
 * mirroring JS Token.prototype.toString().
 *
 * Children of a token are joined with the token's `sep` character (if
 * non-NUL) between each pair, exactly as JS `Array.join(separator)`.
 *
 * The returned pointer is owned by `tb->buf`.
 */
char *token_to_string(const Token *t, ThreadBuf *tb);

/** Return the sentinel char for a token type (e.g. 'c', 'e', 'n', 'o'…). */
char token_sentinel_char(TokenType type);
