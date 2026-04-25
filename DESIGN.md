# extern_tokenizer — C Implementation Design Document

## Overview

`extern_tokenizer` is a pure C reimplementation of `Parser.parse(wikitext)` from the
`wikiparser-node` library (v1.38.1).  The JavaScript version lives in
`wikiparser-node-1.38.1/package/dist/`.  The C version must produce an identical token
tree for every input, and must be tested against the same test suite.

The implementation **does not** use Node N-API or call back into JavaScript.  It is a
standalone C library that takes wikitext as a `const char *` (UTF-8) plus a parsed
config object and returns the root token tree as a pointer to a `Token` struct.

---

## Parsing Pipeline

The JavaScript parser processes wikitext in **11 sequential stages** (0–10).  Each
stage mutates a "current string" by replacing matched substrings with opaque
`\0<index><char>\x7F` sentinel markers and pushing the corresponding Token into an
accumulator array.  After all 11 stages the markers are expanded back into a tree
(`build()`).

| Stage | JS function              | What it does                                                     |
|-------|--------------------------|------------------------------------------------------------------|
| -1    | *(pre-parse)*            | Remove `\0` and `\x7F` from raw input                           |
| 0     | `parseRedirect` then `parseCommentAndExt` | Redirect detection; HTML comments; extension tags (`<ref>`, etc.) |
| 1     | `parseBraces`            | Templates `{{…}}`, arguments `{{{…}}}`, headings `=…=`, `[[`, `-{` |
| 2     | `parseHtml`              | HTML tags (sanitized list from config)                           |
| 3     | `parseTable`             | Wikitext tables `{|…|}`                                          |
| 4     | `parseHrAndDoubleUnderscore` | `----` horizontal rules, `__TOC__` etc., heading finalization |
| 5     | `parseLinks`             | Internal links `[[…]]`, files, categories                        |
| 6     | `parseQuotes`            | Bold/italic `''`/`'''` balancing                                 |
| 7     | `parseExternalLinks`     | Bracketed external links `[url text]`                           |
| 8     | `parseMagicLinks`        | Free URLs, RFC/PMID/ISBN                                         |
| 9     | `parseList`              | List items `*`, `#`, `;`, `:`                                    |
| 10    | `parseConverter`         | Language-variant converter `-{…}-`                               |

After stage 10: `build()` replaces every `\0<n><ch>\x7F` marker in text nodes with
the accumulated child token, forming the final tree.

### Sentinel Marker System

The string `\0<n><ch>\x7F` is used throughout:
- `<n>` — decimal integer index into the accumulator array
- `<ch>` — single ASCII character identifying the token type (see table below)

| char | Token type                                        |
|------|---------------------------------------------------|
| `!`  | `{{!}}` pipe placeholder                          |
| `{`  | `{{(!}}` open-brace placeholder                   |
| `}`  | `{{!)}}` close-brace placeholder                  |
| `-`  | `{{!-}}` dash placeholder                         |
| `+`  | `{{!!}}` double-pipe                              |
| `~`  | `{{=}}` equals                                    |
| `a`  | AttributeToken                                    |
| `b`  | TableToken                                        |
| `c`  | CommentToken                                      |
| `d`  | ListToken / DdToken                               |
| `e`  | ExtToken (extension tag)                          |
| `f`  | MagicLinkToken inside image parameter             |
| `g`  | TranslateToken / OnlyincludeToken                 |
| `h`  | HeadingToken                                      |
| `i`  | MagicLinkToken (RFC/PMID/ISBN)                    |
| `l`  | LinkToken                                         |
| `m`  | server/fullurl/canonicalurl/filepath magic word   |
| `n`  | NoincludeToken / IncludeToken / TvarToken / DoubleUnderscoreToken |
| `o`  | RedirectToken                                     |
| `q`  | QuoteToken                                        |
| `r`  | HrToken                                           |
| `s`  | `{{{|subst:}}}` substitution                      |
| `t`  | ArgToken / TranscludeToken                        |
| `u`  | `__TOC__`                                         |
| `v`  | ConverterToken                                    |
| `w`  | ExtLinkToken / free external link                 |
| `x`  | HtmlToken                                         |

---

## Core Data Structures

### String View (no-copy substrings)

```c
/* A non-owning view into the original input buffer. */
typedef struct {
    const char *ptr;   /* pointer into the wikitext input */
    size_t       len;  /* byte length (UTF-8) */
} StrView;
```

All intermediate strings produced during parsing are represented as dynamically
allocated C strings (`char *`) only where necessary.  Where a substring of the
accumulator working string is sufficient, a `StrView` is used instead.

### Token Node

```c
typedef enum {
    TOKEN_TEXT = 0,     /* leaf text node (AstText equivalent) */
    TOKEN_PLAIN,        /* plain Token (root, table-inter, etc.) */
    TOKEN_COMMENT,
    TOKEN_EXT,
    TOKEN_NOINCLUDE,
    TOKEN_INCLUDE,
    TOKEN_TRANSLATE,
    TOKEN_ONLYINCLUDE,
    TOKEN_ARG,
    TOKEN_TRANSCLUDE,
    TOKEN_HEADING,
    TOKEN_HTML,
    TOKEN_TABLE,
    TOKEN_TR,
    TOKEN_TD,
    TOKEN_HR,
    TOKEN_DOUBLE_UNDERSCORE,
    TOKEN_LINK,
    TOKEN_FILE,
    TOKEN_CATEGORY,
    TOKEN_REDIRECT,
    TOKEN_REDIRECT_TARGET,
    TOKEN_QUOTE,
    TOKEN_EXT_LINK,
    TOKEN_MAGIC_LINK,
    TOKEN_LIST,
    TOKEN_DD,
    TOKEN_CONVERTER,
    TOKEN_CONVERTER_FLAGS,
    TOKEN_CONVERTER_RULE,
    TOKEN_PARAMETER,
    TOKEN_ATTRIBUTES,
    TOKEN_SYNTAX,
    TOKEN_ATOM,
    TOKEN_HIDDEN,
    /* … additional types as needed */
    TOKEN_TYPE_COUNT
} TokenType;

typedef struct Token Token;
struct Token {
    TokenType    type;
    char        *type_name;   /* e.g. "root", "heading", "link", … */
    char        *name;        /* tag/template name where applicable */

    /* child nodes — either text or Token */
    struct {
        bool      is_text;
        union {
            char  *text;       /* owned string for text nodes */
            Token *token;      /* owned pointer for token children */
        };
    } *children;
    size_t        child_count;
    size_t        child_cap;

    /* per-type payload (union to save memory) */
    union {
        struct { int level; }                heading;    /* HeadingToken */
        struct { bool closed; }              comment;    /* CommentToken */
        struct { bool self_closing; bool closing; } html; /* HtmlToken */
        struct { bool case_sensitive; }      dunder;     /* DoubleUnderscoreToken */
        struct { bool bold; bool italic; }   quote;      /* QuoteToken */
    } data;

    /* parsing state (mirrors JS #stage / #include / #built) */
    int           stage;
    bool          include;
    bool          built;
};
```

### Accumulator

```c
typedef struct {
    Token  **tokens;   /* pointer array into heap-allocated tokens */
    size_t   count;
    size_t   cap;
} Accum;
```

### Parser Config

```c
typedef struct {
    /* extension tag names, lowercased */
    const char **ext;
    size_t       ext_count;

    /* HTML element lists: html[0]=normal, html[1]=li-like, html[2]=void */
    const char **html[3];
    size_t       html_count[3];

    /* namespace map: ns_num -> ns_name */
    int         *ns_nums;
    const char **ns_names;
    size_t       ns_count;

    /* redirection keywords */
    const char **redirection;
    size_t       redirection_count;

    /* double-underscore keywords */
    const char **double_underscore_insensitive;  /* e.g. "toc", "notoc" */
    size_t       dunder_insensitive_count;
    const char **double_underscore_sensitive;
    size_t       dunder_sensitive_count;

    /* parser functions (magic words) */
    const char **parser_functions[4];  /* [0]=case-insensitive, [1]=case-sensitive, etc. */

    /* protocol list for external links */
    const char  *protocol_regex;   /* pre-built regex fragment, e.g. "https?:|ftp:" */

    /* variant list for language converter */
    const char **variants;
    size_t       variant_count;

    /* excludes list */
    const char **excludes;
    size_t       exclude_count;

    /* cached compiled regexes (populated lazily) */
    void        *regex_redirect;
    void        *regex_ext;          /* one per (includeOnly) */
    void        *regex_external_links;
    void        *regex_magic_links;
    void        *regex_hr_and_dunder;
    void        *regex_converter;
    void        *regex_links;
} ParserConfig;
```

Config is loaded from a JSON file (e.g. `config/enwiki.json`) once at startup using
the bundled `cJSON` library and then held for the lifetime of the process.

---

## Regex Strategy

The JavaScript implementation relies heavily on `RegExp`.  The C implementation will
use **PCRE2** (Perl-Compatible Regular Expressions 2) which supports:
- Unicode (`PCRE2_UTF`)
- Named groups
- Lookaheads / lookbehinds
- `\p{…}` Unicode properties

All regex patterns are compiled once (when the config is first used) and reused across
calls.  The compiled patterns are stored in `ParserConfig` as `pcre2_code *` cast to
`void *` so the header does not depend on pcre2 headers directly.

No external parsing library (PEG, ANTLR, etc.) is used; the parsing logic is
implemented by hand, mirroring the JS source.

---

## Memory Management

- **No strdup unless necessary.** Parser functions operate on the working string
  buffer in-place where possible.  When a stage must insert a sentinel, the buffer
  is reallocated with `realloc`.
- **Token children** are `char *` (owned) for text nodes and `Token *` (owned) for
  subtokens.  `token_free` recursively frees all children.
- **Accumulator** holds non-owning `Token *` pointers.  The root token owns all
  tokens (they are allocated inside `token_new` and linked as children during
  `build()`).
- **Config** is owned by the caller; `config_free` releases all regex-compiled
  patterns and allocated strings.
- **Working strings** in each parser stage are allocated by the callee and freed by
  the caller (`parse.c`) before adopting the new string.

---

## Build System

```

    CMakeLists.txt          # top-level; builds libwikiparser.a + test executables
    parser/CMakeLists.txt   # compiles parser stage files
    src/CMakeLists.txt      # compiles token type files
    tests/CMakeLists.txt    # compiles and runs test executables
```

Dependencies:
- `pcre2` (≥ 10.30) — regex engine
- `cjson` (bundled or system) — JSON config loading and fixture serialization
