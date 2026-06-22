# Wiki-CAST — C Implementation Design Document

## Overview

`Wiki-CAST` is a pure C reimplementation of `Parser.parse(wikitext)` from the
`wikiparser-node` library (v1.38.1).  The JavaScript version lives in
`wikiparser-node-1.38.1/package/dist/`.  The C version must produce an identical token
tree for every input, and must be tested against the same test suite.

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

### Phase Purpose (Why Each Stage Exists)

The table above describes *what* each stage matches. This section explains the
*purpose* of each stage and why its position in the pipeline matters.

- **Stage -1 (pre-parse tidy):** sanitize raw input by removing parser-reserved
  bytes (`\0`, `\x7F`) so later sentinel insertion cannot collide with user text.

- **Stage 0a (`parseRedirect`):** detect page-level redirect syntax before deeper
  parsing. Redirect recognition is a root concern and must happen before nested
  constructs can rewrite surrounding text.

- **Stage 0b (`parseCommentAndExt`):** isolate comments and extension blocks early
  because these regions frequently suppress or alter normal wikitext parsing.
  Tokenizing them first prevents later stages from over-parsing their internals.

- **Stage 1 (`parseBraces`):** resolve brace-driven structure (templates,
  arguments, brace-linked heading forms). This is the primary structural pass
  that establishes template/parameter hierarchy used by many later stages.

- **Stage 2 (`parseHtml`):** tokenize known HTML tags after brace structure is
  stabilized. Doing this here avoids HTML parsing interfering with brace stack
  behavior while still exposing HTML boundaries to later text-line stages.

- **Stage 3 (`parseTable`):** parse table syntax line-by-line on the transformed
  stream. It must run before list/quote/link post-stages that depend on the
  table cell/intermediate token boundaries.

- **Stage 4 (`parseHrAndDoubleUnderscore`):** handle horizontal rules,
  double-underscore magic words, and heading finalization once table/html/comment
  boundaries are established.

- **Stage 5 (`parseLinks`):** resolve internal links/files/categories after
  higher-priority structural syntax is already tokenized, reducing false link
  captures inside templates/tables.

- **Stage 6 (`parseQuotes`):** balance bold/italic runs per line after link
  tokenization, so quote balancing operates on stabilized link text segments.

- **Stage 7 (`parseExternalLinks`):** parse bracketed external links after quote
  normalization and internal-link parsing, matching JS precedence.

- **Stage 8 (`parseMagicLinks`):** detect free URLs and RFC/PMID/ISBN forms
  after bracketed links are handled, avoiding duplicate/competing captures.

- **Stage 9 (`parseList`):** compute list nesting from line prefixes near the end
  of the pipeline, when most inline syntax has already been collapsed into
  stable sentinel/token boundaries.

- **Stage 10 (`parseConverter`):** run language variant conversion parsing last,
  preserving JS precedence for `-{...}-` handling and minimizing conflicts with
  earlier structural parsers.

- **Post-pipeline `build()`:** replace sentinel markers with accumulated tokens to
  produce the final tree shape. `build()` is not a parsing stage; it is the
  expansion/finalization step that materializes child-token relationships.

In short: earlier stages isolate high-impact structural regions, middle stages
resolve inline/token semantics, and late stages finalize line-oriented and
conversion syntax. Reordering stages is high-risk for AST parity.

### Malformed Input Handling And Recovery

Wikitext is frequently malformed (unclosed delimiters, mixed nesting, broken
tag boundaries). The parser is designed for resilient recovery with JS-parity:
prefer preserving user text and producing a valid tree over hard failure.

- **No fatal parse errors for syntax shape (all stages):** malformed constructs
  are typically left as literal text when a stage cannot prove a valid token
  boundary. This policy is enforced by stage functions only replacing validated
  matches (entrypoint: `wiki_parse_with_page`).

- **Stage-local recovery:** each stage only commits replacements for complete,
  validated matches. Partial or ambiguous matches are passed through unchanged
  so later stages can still process surrounding text.
  Stage/function mapping:
  - Stage 0a: `parse_redirect`
  - Stage 0b: `parse_comment_and_ext`, `stage0_parse_comment_and_ext_on_accum`
  - Stage 1: `parse_braces`, `stage1_parse_braces_on_accum`
  - Stage 2: `parse_html`
  - Stage 3: `parse_table`
  - Stage 4: `parse_hr_and_double_underscore`
  - Stage 5: `parse_links`
  - Stage 6: `parse_quotes_stage6_per_line` (delegates per-line to `parse_quotes`)
  - Stage 7: `parse_external_links`
  - Stage 8: `parse_magic_links`
  - Stage 9: `parse_list`
  - Stage 10: `parse_converter`

- **Unmatched open/close delimiters:** for braces, links, converter regions,
  lists, and tables, unmatched delimiters are treated conservatively as text or
  minimally tokenized fragments, matching JavaScript behavior.
  Primary handlers by grammar:
  - Braces/templates/args: Stage 1 (`parse_braces`)
  - Internal links/files/categories: Stage 5 (`parse_links`)
  - Converter blocks: Stage 10 (`parse_converter`)
  - Tables: Stage 3 (`parse_table`)
  - Lists: Stage 9 (`parse_list`)

- **Quote balancing strategy:** quote parsing may synthesize closing quote tokens
  at line end when needed so output remains structurally consistent while
  preserving the original textual intent.
  Stage/function: Stage 6 (`parse_quotes_stage6_per_line` and `parse_quotes`).

- **Comment/ext isolation first:** early tokenization of comments and extension
  regions prevents malformed internals from corrupting outer-stage parsing.
  Stage/function: Stage 0b (`parse_comment_and_ext` and
  `stage0_parse_comment_and_ext_on_accum`).

- **Line-oriented containment:** table and list parsing recovers by line,
  limiting damage from malformed syntax to local spans rather than whole-page
  collapse.
  Stage/functions: Stage 3 (`parse_table`) and Stage 9 (`parse_list`).

- **Sentinel safety:** only parser-generated sentinel patterns are expanded in
  `build()`. Raw user text that resembles malformed syntax but is not tokenized
  remains text and is not force-expanded.
  Build functions: `build_from_str` (marker scanning/expansion) and
  `build_token_recursive` (tree-wide recursive expansion).

- **Deterministic fallback path:** when mixed-content or cross-child conditions
  are risky, postprocess paths fall back to conservative reconstruction so AST
  shape remains stable and equivalent to JS expectations.
  Post-build recovery functions: `postprocess_parameter_value_inline_impl` and
  `postprocess_nested_plain`.

- **Pre-parse sanitization boundary:** parser-reserved bytes are removed before
  Stage 0 so malformed control-byte input cannot impersonate sentinels.
  Stage/function: Stage -1 tidy via `str_tidy_into` (called by
  `wiki_parse_with_page`).

Recovery principle: if a construct is not confidently valid, keep it as text;
if it is valid, tokenize it at the earliest stage that owns that grammar.

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

## Token Type vs Token Subtype

The parser intentionally stores both `type` and `subtype` on every `Token`.

- `type` identifies the concrete token class and selects the `TokenData` union
  payload branch in `include/wiki_cast/token.h`.
- `subtype` identifies the semantic role/context name used for JS-parity
  labeling, debug/json naming, and child-role disambiguation.

This split is not redundant.

- Multiple semantic roles share a broad structural `type`.
  Example: many `TOKEN_PLAIN` tokens are distinguished by subtype
  (`heading-title`, `parameter-value`, `table-inner`, `td-inner`, etc.).
- Some parser steps intentionally preserve subtype while changing or rewrapping
  type context (for example postprocess rebuild paths in `src/parse.c` use
  `token_new_with_subtype(TOKEN_PLAIN, t->subtype)`).
- Type-specific data is carried in `TokenData` and must still be keyed by
  `type` (for example quote/html/ext/redirect/ext-link payload fields).

### What `TOKEN_SUBTYPE_NONE` Means

`TOKEN_SUBTYPE_NONE` is the fallback from `token_subtype_from_name()` when a
name is null/empty/unknown (`src/token.c`). In normal parser construction,
subtype names passed to `token_new(type, name)` are mapped, and no explicit
`token_new_with_subtype(..., TOKEN_SUBTYPE_NONE)` construction is used by
current parse paths in `src/`.

So `NONE` exists as a defensive/default enum value, not as a primary semantic
state for normal AST nodes.

### Handler Strategy Guidance (Markdown Converter)

Use both levels of dispatch:

1. Prefer subtype-specific handlers first for semantic roles.
2. Fall back to type-level handlers for structural behavior and payload access.

Why not subtype-only:

- Type fallback handles unknown/future/defensive subtype values.
- Type dispatch is the natural place for behavior that depends on `TokenData`
  fields keyed by token class.
- It avoids forcing every structural case into a very large subtype-only table.

In short: subtype gives fine-grained meaning; type gives structural guarantees.
Both are required for robust conversion.

