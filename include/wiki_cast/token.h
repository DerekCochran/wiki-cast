/*
 * wiki_cast/token.h — Token node types and lifecycle.
 *
 * Mirrors dist/src/index.js and the token hierarchy under dist/src/.
 * Each token type corresponds to a JS class.  Children can be either
 * text (char *) or another Token *.
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include "stringzilla/types.h"

// The ThreadBuff items should not be used external from the library, but we need to declare it here for the token_to_string() API.
typedef struct ThreadBuf ThreadBuf;

// ============================================================================
// 1. MASTER LISTS (The single source of truth for your tokens)
// ============================================================================

#define TOKEN_TYPE_LIST(TOKEN_TYPE) \
    TOKEN_TYPE(TOKEN_TEXT)          /* leaf text node (AstText) */ \
    TOKEN_TYPE(TOKEN_ROOT)          /* root plain token */ \
    TOKEN_TYPE(TOKEN_PLAIN) /* generic plain token */ \
    TOKEN_TYPE(TOKEN_COMMENT) /* CommentToken          'c' */ \
    TOKEN_TYPE(TOKEN_EXT) /* ExtToken              'e' */ \
    TOKEN_TYPE(TOKEN_NOINCLUDE) /* NoIncludeToken        'n' */ \
    TOKEN_TYPE(TOKEN_INCLUDE) /* IncludeToken          'i' */ \
    TOKEN_TYPE(TOKEN_TRANSLATE) /* TranslateToken        't' */ \
    TOKEN_TYPE(TOKEN_ONLYINCLUDE) /* OnlyIncludeToken      'o' */ \
    TOKEN_TYPE(TOKEN_ARG) /* ArgToken              'a' */ \
    TOKEN_TYPE(TOKEN_TRANSCLUDE) /* TranscludeToken       'x' */ \
    TOKEN_TYPE(TOKEN_HEADING) /* HeadingToken          'h' */ \
    TOKEN_TYPE(TOKEN_HTML) /* HtmlToken             'H' */ \
    TOKEN_TYPE(TOKEN_TABLE) /* TableToken            'T' */ \
    TOKEN_TYPE(TOKEN_TR) /* TableRowToken         'R' */ \
    TOKEN_TYPE(TOKEN_TD) /* TableCellToken        'C' */ \
    TOKEN_TYPE(TOKEN_HR) /* HorizontalRuleToken   'r' */ \
    TOKEN_TYPE(TOKEN_DOUBLE_UNDERSCORE) /* DoubleUnderscoreToken 'u' */ \
    TOKEN_TYPE(TOKEN_LINK) /* LinkToken             'l' */ \
    TOKEN_TYPE(TOKEN_FILE) /* FileToken             'f' */ \
    TOKEN_TYPE(TOKEN_CATEGORY) /* CategoryToken         'g' */ \
    TOKEN_TYPE(TOKEN_REDIRECT) /* RedirectToken         'd' */ \
    TOKEN_TYPE(TOKEN_REDIRECT_TARGET) /* RedirectTargetToken   'D' */ \
    TOKEN_TYPE(TOKEN_REDIRECT_SYNTAX) /* RedirectSyntaxToken   'S' */ \
    TOKEN_TYPE(TOKEN_QUOTE) /* QuoteToken            'q' */ \
    TOKEN_TYPE(TOKEN_EXT_LINK) /* ExtLinkToken          'x' */ \
    TOKEN_TYPE(TOKEN_MAGIC_LINK) /* MagicLinkToken        'm' */ \
    TOKEN_TYPE(TOKEN_LIST) /* ListToken             'L' */ \
    TOKEN_TYPE(TOKEN_DD) /* DefinitionDescriptionToken 'D' */ \
    TOKEN_TYPE(TOKEN_CONVERTER) /* ConverterToken        'c' */ \
    TOKEN_TYPE(TOKEN_PARAMETER) /* ParameterToken        'p' */ \
    TOKEN_TYPE(TOKEN_ATTRIBUTES) /* AttributesToken       'A' */ \
    TOKEN_TYPE(TOKEN_SYNTAX) /* SyntaxToken           's' */ \
    TOKEN_TYPE(TOKEN_ATOM) /* AtomToken             'a' */ \
    TOKEN_TYPE(TOKEN_HIDDEN) /* HiddenToken           'h' */ \
    TOKEN_TYPE(TOKEN_EXT_ATTRS) /* ExtAttrsToken         'E' */ \
    TOKEN_TYPE(TOKEN_EXT_INNER) /* ExtInnerToken         'I' */ \
    TOKEN_TYPE(TOKEN_EXT_ATTR_DIRTY) /* ExtAttrDirtyToken     'D' */ \
    TOKEN_TYPE(TOKEN_EXT_ATTR) /* ExtAttrToken          'A' */ \
    TOKEN_TYPE(TOKEN_ATTR_KEY) /* AttrKeyToken          'K' */ \
    TOKEN_TYPE(TOKEN_ATTR_VALUE) /* AttrValueToken        'V' */

#define TOKEN_SUBTYPE_LIST(TOKEN_SUBTYPE) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_NONE) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_ROOT) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_REDIRECT) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_REDIRECT_SYNTAX) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_REDIRECT_TARGET) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_COMMENT) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_EXT) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_NOINCLUDE) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_INCLUDE) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_INCLUDEONLY) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_ONLYINCLUDE) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_TRANSLATE) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_ARG) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_ARG_NAME) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_ARG_DEFAULT) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_TEMPLATE) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_MAGIC_WORD) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_MAGIC_WORD_NAME) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_PARAMETER) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_PARAMETER_KEY) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_PARAMETER_VALUE) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_HEADING) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_HEADING_TITLE) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_HEADING_TRAIL) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_HTML) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_HTML_ATTRS) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_HTML_ATTR) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_HTML_ATTR_DIRTY) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_TABLE) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_TR) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_TD) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_TABLE_SYNTAX) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_TABLE_ATTRS) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_TABLE_ATTR) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_TABLE_ATTR_DIRTY) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_TABLE_INTER) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_TABLE_INNER) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_TD_INNER) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_HR) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_DOUBLE_UNDERSCORE) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_LINK) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_FILE) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_CATEGORY) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_TEXT) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_LINK_TARGET) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_LINK_TEXT) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_QUOTE) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_EXT_LINK) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_EXT_LINK_URL) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_EXT_LINK_TEXT) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_MAGIC_LINK) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_FREE_EXT_LINK) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_LIST) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_DD) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_CONVERTER) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_CONVERTER_RULE) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_CONVERTER_RULE_FROM) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_CONVERTER_RULE_VARIANT) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_CONVERTER_RULE_TO) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_CONVERTER_FLAGS) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_CONVERTER_FLAG) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_ATTRIBUTES) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_ATTR_EQUAL_TMP) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_ATTR_KEY) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_ATTR_VALUE) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_ATOM) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_HIDDEN) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_EXT_ATTRS) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_EXT_INNER) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_EXT_ATTR_DIRTY) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_EXT_ATTR) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_IMAGE_PARAMETER) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_GALLERY_IMAGE) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_IMAGEMAP_IMAGE) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_GALLERY_LINE) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_GALLERY_PARAM_WRAPPER) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_IMAGEMAP_LINK_INNER) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_IMAGEMAP_IMAGE_LINE) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_IMAGEMAP_LINK) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_TEMPLATE_NAME) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_INVOKE_MODULE) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_INVOKE_FUNCTION) \
    TOKEN_SUBTYPE(TOKEN_SUBTYPE_PARAM_LINE)

// ============================================================================
// 2. ENUM GENERATION
// ============================================================================

typedef enum TokenType {
    // Force the first item to explicitly start at 0
#define DEFINE_TYPE(name) name,
    TOKEN_TYPE_LIST(DEFINE_TYPE)
#undef DEFINE_TYPE
    TOKEN_TYPE_COUNT
} TokenType;

typedef enum TokenSubType {
#define DEFINE_SUBTYPE(name) name,
    TOKEN_SUBTYPE_LIST(DEFINE_SUBTYPE)
#undef DEFINE_SUBTYPE
    TOKEN_SUBTYPE_COUNT
} TokenSubType;


// ============================================================================
// 3. STRINGIFICATION FUNCTION IMPLEMENTATIONS
// ============================================================================

// inline static lets us write the implementation directly inside the header 
// without causing "duplicate symbol" errors during compilation linking.

inline static const char* get_token_type_name(int type) {
    switch (type) {
#define CASE_TYPE(name) case name: return #name;
        TOKEN_TYPE_LIST(CASE_TYPE)
#undef CASE_TYPE
        default: return "TOKEN_UNKNOWN";
    }
}

inline static const char* get_token_subtype_name(int subtype) {
    switch (subtype) {
#define CASE_SUBTYPE(name) case name: return #name;
        TOKEN_SUBTYPE_LIST(CASE_SUBTYPE)
#undef CASE_SUBTYPE
        default: return "TOKEN_SUBTYPE_UNKNOWN";
    }
}

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
        sz_string_view_t orig_tag;     /* original-case tag name for toString */
    } html;
    struct {
        sz_string_view_t inner_syntax; /* TdToken separator between attrs and inner */
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
        sz_string_view_t pre;          /* RedirectToken: leading whitespace */
        sz_string_view_t post;         /* trailing whitespace */
        sz_string_view_t link;         /* raw link target */
        sz_string_view_t display;      /* optional display text (after |) */
    } redirect;
    struct {
        sz_string_view_t name;         /* ExtToken: tag name */
        sz_string_view_t attr;         /* attribute string */
        sz_string_view_t inner;        /* inner content */
        sz_string_view_t closing;      /* closing tag text */
        bool  self_closing;
    } ext;
    struct {
        sz_string_view_t tag;          /* IncludeToken: "includeonly"/"noinclude" */
        sz_string_view_t attr;
        sz_string_view_t inner;
        sz_string_view_t closing;      /* closing tag text, NULL if unclosed */
    } include;
    struct {
        sz_string_view_t equal; /* equal sign + surrounding whitespace — owned, empty if boolean attr */
        char  quote_open;       /* opening quote ('"', '\'', or '\0' if unquoted/boolean) */
        char  quote_close;      /* closing quote ('"', '\'', or '\0' if unclosed/unquoted) */
    } ext_attr;
    struct {
        sz_string_view_t raw_syntax;  /* canonical syntax string for image-parameter toString */
    } image_param;
    struct {
        sz_string_view_t space;  /* ExtLinkToken separator between URL and text (may be empty) */
    } ext_link;
    struct {
        bool magic_pipe;  /* LinkBaseToken delimiter was \0\d+!\x7F ({{!}}) */
    } link;
    struct {
        sz_string_view_t modifier; /* TranscludeToken modifier prefix, e.g. "subst:" */
    } transclude;
} TokenData;

/* ── Token struct ─────────────────────────────────────────────────────────── */
typedef struct Token {
    TokenType  type;
    TokenSubType subtype;
    char      *name;        /* tag/template name where applicable; always owned */
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
    bool       ext_inner_context; /* cached: token is nested under ext-inner */
    size_t     accum_index; /* index in Accum when present; (size_t)-1 when not in Accum */
    unsigned   inline_seen_epoch; /* dedupe marker for inline postprocess passes */
} Token;

/* ── Lifecycle ────────────────────────────────────────────────────────────── */

/** Allocate a new token with the given type and subtype name. */
Token *token_new(TokenType type, const char *type_name);

/** Allocate a new token with an explicit subtype enum. */
Token *token_new_with_subtype(TokenType type, TokenSubType subtype);

/** Append a TEXT child to a token from a pointer+length substring. */
void token_append_text_owned(Token *t, const char *text, size_t len);

/** Append a TEXT child by copying from an sz_string_view_t.
 *  The string content is copied and owned by the child (text_owned = true). */
void token_append_text_view_owned(Token *t, sz_string_view_t view);

/** Append a TOKEN child to a token.  Takes ownership of child. */
void token_append_child(Token *t, Token *child);

/** Replace a token name with an owned C string, updating the string-view length. */
void token_set_name_owned(Token *t, char *name);

/** Clear a token name, freeing any owned string and resetting the string view. */
void token_clear_name(Token *t);

/** Recursively free a token and all its children. */
void token_free(Token *t);

/** Shallow-free a token: free its own memory and any text children, but do NOT
 * recursively free child tokens. This is useful when tokens are owned in a
 * flat accumulator and must be freed individually without double-freeing.
 */
void token_free_shallow(Token *t);

/** Serialize a token tree to JSON, writing to fp. 
 * This is used ONLY to test between the wikiparser-node and this tokenizer
*/
char *json_stringify_wikiparser_node(const Token *t, const bool pretty);

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
char *token_to_string_external(const Token *t);

/** Return the sentinel char for a token type (e.g. 'c', 'e', 'n', 'o'…). */
char token_sentinel_char(TokenType type);

/** Return the canonical subtype name used for JSON/debug/toString parity. */
sz_string_view_t token_subtype_name(TokenSubType subtype);

/** Map a legacy subtype string name to a TokenSubType enum. */
TokenSubType token_subtype_from_name(const char *type_name);
