# WikiCast AST Developer Guide

This document describes how to use the WikiCast AST (Abstract Syntax Tree) produced by
`WikiParser.parse()` to convert wikitext to other formats (Markdown, HTML, plain text,
knowledge graphs, etc.) or to extract structured information from Wikipedia/MediaWiki pages.

---

## Dispatch Strategy

Every `PyToken` node carries two fields that together determine what it represents:

| Field | Type | Purpose |
|---|---|---|
| `token.type` | `TokenType` enum | Structural class — selects the `TokenData` payload branch |
| `token.subtype` | `TokenSubType` enum | Semantic role — the context-specific name given at construction |

**The correct dispatch order is: subtype first, then type fallback.**

Subtype is the more specific signal. Many tokens share the same `type` (e.g., `TOKEN_PLAIN`)
but differ in role (heading title, parameter value, table inner, td inner, gallery line, etc.).
Some nodes have their subtype reassigned after construction (e.g., `TOKEN_TRANSCLUDE` gets
subtype `TOKEN_SUBTYPE_TEMPLATE` or `TOKEN_SUBTYPE_MAGIC_WORD` depending on what was parsed).

`TOKEN_SUBTYPE_NONE` means no subtype was assigned; fall back to type-based dispatch.

```python
subtype = getattr(token, "subtype", TokenSubType.TOKEN_SUBTYPE_NONE)
handler = subtype_handlers.get(subtype)
if handler is None:
    handler = type_handlers.get(token.type, default_passthrough)
handler(token)
```

---

## Token Payload Fields

The `TokenData` union exposes per-type fields through `PyToken` properties. Only access a
field when the token has the matching type.

| Property | Type condition | Description |
|---|---|---|
| `token.level` | `TOKEN_HEADING` | Heading level 1–6 |
| `token.comment_closed` | `TOKEN_COMMENT` | Whether `-->` was present |
| `token.html_self_closing` | `TOKEN_HTML` | `<br/>` style |
| `token.html_closing` | `TOKEN_HTML` | This is a closing tag |
| `token.html_orig_tag` | `TOKEN_HTML` | Original-case tag name |
| `token.td_inner_syntax` | `TOKEN_TD` | Separator string between attrs and inner |
| `token.dunder_case_sensitive` | `TOKEN_DOUBLE_UNDERSCORE` | Case-sensitive match |
| `token.dunder_fullwidth` | `TOKEN_DOUBLE_UNDERSCORE` | Full-width `＿＿` form |
| `token.quote_bold` | `TOKEN_QUOTE` | Marker opens/closes bold |
| `token.quote_italic` | `TOKEN_QUOTE` | Marker opens/closes italic |
| `token.redirect_pre` | `TOKEN_REDIRECT` | Leading whitespace before `#REDIRECT` |
| `token.redirect_post` | `TOKEN_REDIRECT` | Trailing whitespace |
| `token.redirect_link` | `TOKEN_REDIRECT` | Raw link target |
| `token.redirect_display` | `TOKEN_REDIRECT` | Display text after `|` if present |
| `token.ext_name` | `TOKEN_EXT` | Extension tag name (e.g. `ref`, `math`) |
| `token.ext_attr` | `TOKEN_EXT` | Raw attribute string |
| `token.ext_inner` | `TOKEN_EXT` | Raw inner content |
| `token.ext_closing` | `TOKEN_EXT` | Closing tag text |
| `token.ext_self_closing` | `TOKEN_EXT` | Self-closing form |
| `token.include_tag` | `TOKEN_INCLUDE`/`NOINCLUDE`/`ONLYINCLUDE` | Tag name string |
| `token.include_inner` | `TOKEN_INCLUDE`/`NOINCLUDE`/`ONLYINCLUDE` | Raw inner content |
| `token.image_param_raw_syntax` | subtype `IMAGE_PARAMETER` | Canonical parameter string |
| `token.ext_link_space` | `TOKEN_EXT_LINK` | Separator between URL and text |
| `token.name` | varies | Template name, tag name, or magic word name |
| `token.sep` | varies | Child join separator (`\n` or empty) |

---

## Subtype Reference

The following sections describe every `TokenSubType` value: what it represents, what children
it carries (from the canonical schema), what payload it exposes, and how to handle it in
different output contexts.

### `TOKEN_SUBTYPE_NONE`
No semantic subtype was assigned. Fall through to type-based dispatch. This happens for
tokens whose names do not appear in the subtype map (should not occur in normal parses).

**Action:** Dispatch on `token.type`.

---

### `TOKEN_SUBTYPE_ROOT`
**Type:** `TOKEN_ROOT`  
The root node of the entire parse tree. Contains all top-level content of the page.

**Children (multiple allowed):** `template`, `text`, `quote`, `link`, `heading`, `file`,
`ext`, `comment`, `table`, `list`, `category`, `html`, `ext-link`, `magic-word`,
`free-ext-link`, `dd`, `double-underscore`, `magic-link`, `noinclude`, `hr`

**For any output format:** iterate and process all children.

---

### `TOKEN_SUBTYPE_REDIRECT`
**Type:** `TOKEN_REDIRECT`  
Marks a redirect page (`#REDIRECT [[Target]]`). The page has no prose content; its only
purpose is to point to another article.

**Payload:** `token.redirect_link` — the target page title. `token.redirect_display` — display text if present.

**Children:** `redirect-syntax` (the `#REDIRECT` text), `redirect-target` (the link atom)

**Markdown / HTML / plain text:** Skip entirely. The page is a redirect, not an article.  
**Knowledge graph:** Extract `redirect_link` as a `redirectsTo` edge from this page title.

---

### `TOKEN_SUBTYPE_REDIRECT_SYNTAX`
**Type:** `TOKEN_REDIRECT_TARGET` (used with `TOKEN_SYNTAX`)  
The literal `#REDIRECT` keyword token.

**Children:** text child containing `#REDIRECT`

**All formats:** Skip — this is internal syntax, not content.

---

### `TOKEN_SUBTYPE_REDIRECT_TARGET`
**Type:** `TOKEN_REDIRECT_TARGET`  
Contains the raw link target of the redirect.

**Children:** text and/or `atom` containing the page title

**All formats:** Skip — the parent `REDIRECT` handler extracts `redirect_link` directly.  
**Knowledge graph:** Parent already handles the edge extraction.

---

### `TOKEN_SUBTYPE_COMMENT`
**Type:** `TOKEN_COMMENT`  
An HTML comment `<!-- ... -->`. The `token.comment_closed` field indicates whether `-->` was found.

**Children:** one text child containing the comment body (not shown to readers)

**Schema children:** `text` (not multiple)

**All formats:** Skip — comments are author notes, never shown to readers.  
**Exception:** If extracting metadata annotations (e.g., `<!-- TEMPLATE_DATA ... -->`),
read the text child.

---

### `TOKEN_SUBTYPE_EXT`
**Type:** `TOKEN_EXT`  
An extension tag such as `<ref>`, `<math>`, `<gallery>`, `<imagemap>`, `<poem>`, etc.

**Payload:** `token.ext_name` (tag name), `token.ext_attr` (attributes), `token.ext_inner`
(raw inner text), `token.ext_self_closing`

**Children:** `ext-attrs`, `ext-inner`

**Markdown:** Preserve as `<tagname>...</tagname>` or `<tagname/>`. For `<ref>`, suppress
(footnote rendering). For `<math>`, emit as LaTeX block. For `<gallery>`, process children.  
**HTML:** Emit as the appropriate HTML element.  
**Plain text:** For `<ref>` emit nothing; for `<poem>` emit inner text; for `<math>` emit
the LaTeX source.  
**Knowledge graph:** For `<ref>` extract citation data from inner content.

---

### `TOKEN_SUBTYPE_NOINCLUDE`
**Type:** `TOKEN_NOINCLUDE`  
`<noinclude>` block. Content IS shown when viewing the page directly but is excluded when
the template is transcluded.

**Children:** text (single text child in schema)

**Markdown / HTML / plain text:** Process children — this content appears on the page.  
**Template extraction:** Skip — this content is for direct page view only.

---

### `TOKEN_SUBTYPE_INCLUDE` / `TOKEN_SUBTYPE_INCLUDEONLY`
**Type:** `TOKEN_INCLUDE`  
`<includeonly>` block. Content is NOT shown when viewing the template page itself; it only
appears when the template is transcluded into another page.

**Children:** text

**Markdown / HTML / plain text (viewing the template page):** Skip entirely.  
**Transcluded output:** Process children.

---

### `TOKEN_SUBTYPE_ONLYINCLUDE`
**Type:** `TOKEN_ONLYINCLUDE`  
`<onlyinclude>` block. Content IS shown on the template page and is the only content
included when the template is transcluded.

**Children:** text (like noinclude)

**All formats:** Process children — content is visible.

---

### `TOKEN_SUBTYPE_TRANSLATE`
**Type:** `TOKEN_TRANSLATE`  
`<translate>` extension block used in multilingual wikis (Translatewiki). Wraps translatable
content.

**Children:** varies — same as surrounding content

**All formats:** Process children transparently; the translation markup is metadata.

---

### `TOKEN_SUBTYPE_ARG`
**Type:** `TOKEN_ARG`  
Template argument placeholder `{{{argname|default}}}`. Only meaningful when expanding a
template, not when viewing the template page itself.

**Children:** `arg-name` (name of argument), optionally `arg-default` (default value)

**Markdown / HTML / plain text (page view):** Skip — placeholder syntax, not content.  
**Template expansion:** Substitute the corresponding argument value if expanding.  
**Knowledge graph:** Record as a parameterized template node.

---

### `TOKEN_SUBTYPE_ARG_NAME`
**Type:** child of `TOKEN_ARG`  
The name part of `{{{argname|...}}}`.

**Children:** text

**All formats:** Only used when processing `TOKEN_SUBTYPE_ARG`; do not visit independently.

---

### `TOKEN_SUBTYPE_ARG_DEFAULT`
**Type:** child of `TOKEN_ARG`  
The default value part of `{{{argname|default}}}`.

**Children:** text

**All formats:** Only used when processing `TOKEN_SUBTYPE_ARG`; do not visit independently.

---

### `TOKEN_SUBTYPE_TEMPLATE`
**Type:** `TOKEN_TRANSCLUDE`  
A template invocation `{{TemplateName|param1|key=value}}`.

**Payload:** `token.name` — the resolved (normalized) template name.

**Children:** `template-name` (single), `parameter` (multiple)

**Markdown:** Templates cannot be expanded without a template engine. Serialize to a
fenced code span or block containing the JSON representation of the token tree:
```python
payload = token.to_json(False)
output.append(f"`{payload}`")
```
**HTML:** Same as Markdown unless you have a template expansion engine.  
**Plain text:** Skip or emit a placeholder like `{{TemplateName}}`.  
**Knowledge graph:** Extract template name and parameters as a structured annotation.
Use `token.name` for the template name, then walk `parameter` children for key/value pairs.

---

### `TOKEN_SUBTYPE_TEMPLATE_NAME`
**Type:** child `TOKEN_ATOM` or `TOKEN_PLAIN`  
The name part of a template invocation, e.g. `Infobox person`.

**Children:** text, comment (comment not multiple)

**All formats:** Internal to the template token. The parent handler reads
`token.name` (the normalized version). Do not emit independently.

---

### `TOKEN_SUBTYPE_MAGIC_WORD`
**Type:** `TOKEN_TRANSCLUDE`  
A parser function or magic word: `{{PAGENAME}}`, `{{#if:...|...|...}}`,
`{{formatnum:1234}}`, etc. These are evaluated server-side and produce dynamic output.

**Payload:** `token.name` — the canonical magic word or function name.

**Children:** `magic-word-name` (single), `parameter` (multiple for parser functions),
`invoke-module`, `invoke-function` (for `#invoke` Lua calls)

**Markdown / HTML / plain text:** Skip — cannot evaluate without the MediaWiki engine.
For known words like `{{PAGENAME}}`, you may substitute a placeholder.  
**Knowledge graph:** Record the function name and parameters as metadata. For `#invoke`,
record the Lua module and function names as structured data.

---

### `TOKEN_SUBTYPE_MAGIC_WORD_NAME`
**Type:** `TOKEN_SYNTAX`  
The name part of a magic word or parser function.

**Children:** text (single)

**All formats:** Internal to the magic-word token; do not emit independently.

---

### `TOKEN_SUBTYPE_PARAMETER`
**Type:** `TOKEN_PARAMETER`  
A single `|key=value` or positional `|value` parameter of a template or magic word.

**Children:** `parameter-key` (optional, single), `parameter-value` (single)

**All formats:** Internal to template or magic-word handling. When JSON-encoding a template,
walk parameters to build the argument list. When skipping templates, skip parameters too.

---

### `TOKEN_SUBTYPE_PARAMETER_KEY`
**Type:** `TOKEN_PLAIN`  
The key part of a named parameter `key=value`.

**Children:** text, comment, free-ext-link, template, magic-word, html, quote, link
(comment/free-ext-link/template/magic-word/link: not multiple)

**All formats:** Internal; extract text via `_extract_text()` when building template JSON.

---

### `TOKEN_SUBTYPE_PARAMETER_VALUE`
**Type:** `TOKEN_PLAIN`  
The value part of a parameter (positional or named).

**Children:** text, quote, template, link, magic-word, free-ext-link, ext-link, comment,
html, ext, file, list, magic-link, hr, noinclude, dd, heading, include

**All formats:** Internal; extract or recurse when building template JSON or when expanding
template arguments into surrounding content.

---

### `TOKEN_SUBTYPE_HEADING`
**Type:** `TOKEN_HEADING`  
A section heading `== Title ==`. Levels 1–6.

**Payload:** `token.level` — integer 1–6.

**Children:** `heading-title` (single), `heading-trail` (single)

**Markdown:** Emit `\n#...# ` (level `#`s) then process `heading-title` child, then `\n\n`.
Do not process `heading-trail`.  
**HTML:** Emit `<h{level}>` around the title content.  
**Plain text:** Emit the title text followed by a newline.  
**Knowledge graph:** Record as a section node with `level` and text label. Use to build
the page section hierarchy.

---

### `TOKEN_SUBTYPE_HEADING_TITLE`
**Type:** `TOKEN_PLAIN`  
The text content between the `==` markers.

**Children:** text, html, link, quote, comment, template, magic-word, magic-link
(comment/magic-word/magic-link: not multiple)

**All formats:** Recurse into children to produce the heading text. Parent heading handler
calls passthrough on this node.

---

### `TOKEN_SUBTYPE_HEADING_TRAIL`
**Type:** `TOKEN_SYNTAX`  
The closing `==` markers and any trailing whitespace after the heading.

**Children:** text, comment

**All formats:** Skip — this is wikitext syntax punctuation, not content. The heading
handler already appends newlines.

---

### `TOKEN_SUBTYPE_HTML`
**Type:** `TOKEN_HTML`  
A known sanitized HTML tag such as `<b>`, `<i>`, `<span>`, `<div>`, `<table>`, etc.

**Payload:** `token.html_orig_tag` (original-case tag name), `token.html_self_closing`,
`token.html_closing`

**Children:** `html-attrs` (single, not multiple)

**Markdown:** Map semantic tags: `<b>`/`<strong>` → `**`, `<i>`/`<em>` → `*`,
`<code>` → backtick, `<br>` → `  \n`. Pass through `<sup>`, `<sub>`, others as raw HTML.  
**HTML:** Emit the tag with attributes. Closing tokens emit `</tag>`.  
**Plain text:** Skip tags; recurse into children for text content.  
**Knowledge graph:** Track formatting spans if needed.

---

### `TOKEN_SUBTYPE_HTML_ATTRS`
**Type:** `TOKEN_ATTRIBUTES`  
Container for all attributes of an HTML tag.

**Children:** `html-attr-dirty` (multiple), `html-attr` (multiple)

**All formats:** Skip — attribute data is not part of output content (unless building HTML).
When generating HTML, extract `attr-key` and `attr-value` from `html-attr` children.

---

### `TOKEN_SUBTYPE_HTML_ATTR`
**Type:** `TOKEN_EXT_ATTR`  
A single parsed HTML attribute with key and value tokens.

**Children:** `attr-key` (single), `attr-value` (single)

**All formats:** Skip for non-HTML outputs. For HTML output, extract key and value strings.

---

### `TOKEN_SUBTYPE_HTML_ATTR_DIRTY`
**Type:** `TOKEN_EXT_ATTR_DIRTY`  
A raw, unparsed attribute fragment (malformed or complex attribute text).

**Children:** text (single)

**All formats:** Skip.

---

### `TOKEN_SUBTYPE_TABLE`
**Type:** `TOKEN_TABLE`  
A wikitext table `{| ... |}`. May contain rows, cells, captions, and inter-row content.

**Children:** `table-syntax` (multiple), `table-attrs` (single, not multiple),
`td` (multiple), `tr` (multiple), `table-inter` (single, not multiple)

**Markdown:** Collect all rows and cells; emit as GFM pipe table with header separator.  
**HTML:** Emit `<table>` with rows and cells. Extract attributes from `table-attrs`.  
**Plain text:** Emit cells separated by tabs or newlines.  
**Knowledge graph:** Treat as a structured data node; extract rows as facts.

---

### `TOKEN_SUBTYPE_TR`
**Type:** `TOKEN_TR`  
A table row `|-`.

**Children:** `table-syntax` (single, not multiple), `table-attrs` (single, not multiple),
`td` (multiple), `table-inter` (single, not multiple)

**All formats:** Collect cells from `td` children and emit as a row.

---

### `TOKEN_SUBTYPE_TD`
**Type:** `TOKEN_TD`  
A table cell `|` or header cell `!`. The `token.td_inner_syntax` field holds the separator.

**Payload:** `token.td_inner_syntax` — the syntax string between attributes and content.

**Children:** `table-syntax` (single, not multiple), `table-attrs` (single, not multiple),
`td-inner` (single, not multiple)

**All formats:** Capture output of `td-inner` child and emit as a cell value.

---

### `TOKEN_SUBTYPE_TABLE_SYNTAX`
**Type:** `TOKEN_SYNTAX`  
Wikitext table control characters (`{|`, `|}`, `|-`, `|`, `!`).

**Children:** text, comment (comment not multiple)

**All formats:** Skip — raw syntax markers, not content.

---

### `TOKEN_SUBTYPE_TABLE_ATTRS`
**Type:** `TOKEN_ATTRIBUTES`  
Attribute container for a table, row, or cell.

**Children:** `table-attr` (multiple), `table-attr-dirty` (multiple)

**All formats:** Skip for Markdown/plain text. For HTML, extract attribute key/value pairs.

---

### `TOKEN_SUBTYPE_TABLE_ATTR`
**Type:** `TOKEN_EXT_ATTR`  
A single table attribute.

**Children:** `attr-key` (single), `attr-value` (single)

**All formats:** Skip unless generating HTML.

---

### `TOKEN_SUBTYPE_TABLE_ATTR_DIRTY`
**Type:** `TOKEN_ATOM`  
An unparsed or malformed table attribute fragment.

**Children:** text, comment, template, html, ext (comment/template not multiple)

**All formats:** Skip.

---

### `TOKEN_SUBTYPE_TABLE_INTER`
**Type:** `TOKEN_PLAIN`  
Content that appears between rows in a table — typically templates or links that generate
row content dynamically.

**Children:** text, comment, template, heading, file, link, html, quote, list, ext-link,
magic-word, category (many types, multiple)

**Markdown / HTML:** Recurse into children and emit as content between rows.  
**Knowledge graph:** Process child nodes normally.

---

### `TOKEN_SUBTYPE_TABLE_INNER`
**Type:** `TOKEN_PLAIN`  
Inner content node inside a table-level plain token.

**Children:** same as `table-inter`

**All formats:** Recurse and process children.

---

### `TOKEN_SUBTYPE_TD_INNER`
**Type:** `TOKEN_PLAIN`  
The actual visible content of a table cell.

**Children:** text, html, template, ext, link, file, quote, comment, magic-word, list,
table, ext-link, magic-link, heading, category, hr, free-ext-link
(dd not allowed in this context)

**All formats:** This is real content. Recurse into all children and emit normally. The
cell output is captured and used as the cell value in the table row.

---

### `TOKEN_SUBTYPE_HR`
**Type:** `TOKEN_HR`  
A horizontal rule `----`.

**Children:** text (single — the raw `----` characters; not content)

**Markdown:** Emit `\n---\n`.  
**HTML:** Emit `<hr/>`.  
**Plain text:** Emit a line of dashes.  
**Knowledge graph:** Skip — section separator with no semantic content.

---

### `TOKEN_SUBTYPE_DOUBLE_UNDERSCORE`
**Type:** `TOKEN_DOUBLE_UNDERSCORE`  
A behavior switch such as `__TOC__`, `__NOTOC__`, `__FORCETOC__`, `__NOEDITSECTION__`.

**Payload:** `token.name` — the switch name (e.g. `TOC`, `NOTOC`).
`token.dunder_case_sensitive`, `token.dunder_fullwidth`

**Children:** text (single — the raw `__NAME__` characters)

**Markdown / HTML / plain text:** Skip — these are rendering control directives with no
visible output.  
**Knowledge graph / metadata extraction:** Record `token.name` as a page behavior flag
(e.g., `hasTOC: false` for `__NOTOC__`).

---

### `TOKEN_SUBTYPE_LINK`
**Type:** `TOKEN_LINK`  
An internal wiki link `[[Page title|display text]]` or `[[Page title]]`.

**Payload:** `token.name` — normalized page title.

**Children:** `link-target` (single, not multiple), `link-text` (single, not multiple)

**Markdown:** Emit `[display text](Page title)`. Extract target from `link-target` child
text; extract display from `link-text` child text. If no display text, use the target.  
**HTML:** Emit `<a href="/wiki/Page_title">display text</a>`.  
**Plain text:** Emit the display text (or page title if no display).  
**Knowledge graph:** Record as an outgoing `linksTo` edge from the current page to the
target page.

---

### `TOKEN_SUBTYPE_FILE`
**Type:** `TOKEN_FILE`  
A file/image embed `[[File:Example.jpg|thumb|Caption text]]`.

**Payload:** `token.name` — normalized file name.

**Children:** `link-target` (single, not multiple), `image-parameter` (multiple)

**Markdown:** Emit `![caption](filename)`. Read `link-target` for the filename; scan
`image-parameter` children for the caption (the last one with visible text).  
**HTML:** Emit `<img src="..." alt="caption"/>` with appropriate classes.  
**Plain text:** Skip or emit the caption text only.  
**Knowledge graph:** Record the image use with file name and caption as node attributes.

---

### `TOKEN_SUBTYPE_CATEGORY`
**Type:** `TOKEN_CATEGORY`  
A category link `[[Category:Name|sortkey]]`. Not displayed in article body.

**Payload:** `token.name` — normalized category name.

**Children:** `link-target` (single, not multiple), `link-text` (single — sort key, not multiple)

**Markdown / HTML / plain text:** Skip — categories appear in the sidebar, not the article body.  
**Knowledge graph:** Record as a `hasCategory` edge from the page to the category name.

---

### `TOKEN_SUBTYPE_TEXT`
**Type:** `TOKEN_TEXT` (or `TOKEN_PLAIN` with this subtype)  
A leaf text node carrying raw string content.

**Children:** none — the text itself is a string child, not a token child

**All formats:** Emit the string content directly.

---

### `TOKEN_SUBTYPE_LINK_TARGET`
**Type:** `TOKEN_ATOM`  
The target page title inside a link, file, or category token.

**Children:** text (multiple), comment (not multiple), template (multiple)

**All formats:** Do not emit independently. The parent link/file handler calls
`_extract_text()` on this node to get the target string. Comments are stripped;
templates in targets are rare but possible.

---

### `TOKEN_SUBTYPE_LINK_TEXT`
**Type:** `TOKEN_PLAIN`  
The display text portion `|display text` inside a link.

**Children:** text (multiple), html (multiple), template (multiple), quote (multiple),
ext (multiple), comment (not multiple), magic-word (not multiple)

**All formats:** The parent link handler calls `_extract_text()` or recurses into children
to produce the display label. Do not emit independently.

---

### `TOKEN_SUBTYPE_QUOTE`
**Type:** `TOKEN_QUOTE`  
A bold/italic marker (`''`, `'''`, `'''''`). These are **boundary markers**, not wrappers.
They appear as paired open/close tokens around text, not as a container.

**Payload:** `token.quote_bold` (bool), `token.quote_italic` (bool)

**Children:** text (single — the raw `''` characters; do NOT process)

**Markdown:** Emit the appropriate marker: `*` (italic), `**` (bold), `***` (bold+italic).
The same handler runs for both the opening and closing marker of each span.  
**HTML:** Track open/close state; emit `<em>`, `<strong>`, or `<em><strong>`.  
**Plain text:** Skip markers entirely.  
**Knowledge graph:** Skip.

---

### `TOKEN_SUBTYPE_EXT_LINK`
**Type:** `TOKEN_EXT_LINK`  
A bracketed external link `[https://example.com Display text]`.

**Payload:** `token.ext_link_space` — separator between URL and text.

**Children:** `ext-link-url` (single, not multiple), `ext-link-text` (single, not multiple)

**Markdown:** Emit `[display text](url)`. Extract URL from `ext-link-url` child;
extract display from `ext-link-text` child. If no text, use the URL itself.  
**HTML:** Emit `<a href="url" rel="nofollow">display text</a>`.  
**Plain text:** Emit the display text (or URL if no text).  
**Knowledge graph:** Record as an outgoing `externalLink` edge.

---

### `TOKEN_SUBTYPE_EXT_LINK_URL`
**Type:** `TOKEN_PLAIN`  
The URL part of a bracketed external link.

**Children:** text (multiple), comment (not multiple), magic-word (multiple)

**All formats:** Do not emit independently. Parent `ext-link` handler calls `_extract_text()`
to get the URL string.

---

### `TOKEN_SUBTYPE_EXT_LINK_TEXT`
**Type:** `TOKEN_PLAIN`  
The display text part of a bracketed external link.

**Children:** text (multiple), quote (multiple), template (multiple), html (multiple),
comment (not multiple), magic-word (multiple), ext (multiple)

**All formats:** Do not emit independently. Parent `ext-link` handler calls `_extract_text()`
or recurses to produce the display label.

---

### `TOKEN_SUBTYPE_MAGIC_LINK`
**Type:** `TOKEN_MAGIC_LINK`  
A free-standing URL recognized as a link (RFC NNNN, PMID NNNNN, ISBN NNNNN, bare URLs).

**Children:** text (single — the raw link text)

**Markdown:** Emit the text as-is (bare URL or identifier). For RFC/PMID/ISBN, you may
optionally wrap as `[RFC 1234](https://tools.ietf.org/html/rfc1234)`.  
**HTML:** Emit `<a href="...">RFC 1234</a>`.  
**Plain text:** Emit the text content.  
**Knowledge graph:** Record as an external reference with identifier type.

---

### `TOKEN_SUBTYPE_FREE_EXT_LINK`
**Type:** `TOKEN_MAGIC_LINK`  
A bare URL in text that the parser recognized as a free external link (not in `[brackets]`).

**Children:** text (multiple), comment (not multiple), magic-word (not multiple)

**Markdown / HTML:** Emit the URL as a bare link or wrapped in angle brackets `<url>`.  
**Plain text:** Emit the URL text.  
**Knowledge graph:** Record as an external link.

---

### `TOKEN_SUBTYPE_LIST`
**Type:** `TOKEN_LIST`  
A single list item — one `*`, `#`, `;`, or `:` prefixed line.

**Children:** text (single — the raw content; note: the spec marks this as not-multiple,
meaning the whole item text is one text child)

**Markdown:** Determine marker from the raw text prefix: `*`/`-` → `* `, `#` → `1. `.
Emit `indent + marker + content + \n`. Track nesting level for indentation.  
**HTML:** Emit `<li>...</li>` inside `<ul>` or `<ol>`. Track nesting for `<ul>`/`<ol>` open/close.  
**Plain text:** Emit `- content\n` or `N. content\n`.  
**Knowledge graph:** Skip list structure; process the text content as prose.

---

### `TOKEN_SUBTYPE_DD`
**Type:** `TOKEN_DD`  
A definition description `: text` (part of definition lists `;term :description`).

**Children:** text (single)

**Markdown:** Emit `: content\n` (definition list extension, not standard GFM) or indent the
text as a block quote.  
**HTML:** Emit `<dd>...</dd>`.  
**Plain text:** Emit the content text with indentation.

---

### `TOKEN_SUBTYPE_CONVERTER`
**Type:** `TOKEN_CONVERTER`  
A language-variant converter block `-{ flags | zh:text; zh-tw:text }-`. Used to display
different text for different script variants of Chinese, Serbian, etc.

**Children:** converter-rule (multiple), converter-flags, converter-flag (multiple)

**Markdown / HTML / plain text:** Cannot evaluate without knowing the target language.
Skip entirely or emit one rule's "to" text as a fallback.  
**Knowledge graph:** Record the variants as multilingual content alternatives.

---

### `TOKEN_SUBTYPE_CONVERTER_RULE` / `_FROM` / `_VARIANT` / `_TO` / `_FLAGS` / `_FLAG`
All internal nodes of the converter block.

**All formats:** Skip (unless implementing full language-variant rendering).

---

### `TOKEN_SUBTYPE_ATTRIBUTES`
**Type:** `TOKEN_ATTRIBUTES`  
Generic attribute container (used for both HTML and table attributes).

**Children:** `attr-key`/`attr-value` pairs

**All formats:** Skip for non-HTML outputs. For HTML, walk children to build attribute strings.

---

### `TOKEN_SUBTYPE_ATTR_EQUAL_TMP`
**Type:** internal  
Transient token representing the `=` sign during attribute parsing. Never appears in the
final built tree.

**All formats:** Skip.

---

### `TOKEN_SUBTYPE_ATTR_KEY`
**Type:** `TOKEN_ATTR_KEY`  
The key part of an HTML or extension attribute (e.g., `class`, `style`, `href`).

**Children:** text (single), template (multiple), comment (not multiple)

**All formats:** Skip for non-HTML outputs. For HTML output, extract text to get the key name.

---

### `TOKEN_SUBTYPE_ATTR_VALUE`
**Type:** `TOKEN_ATTR_VALUE`  
The value part of an HTML or extension attribute.

**Children:** text (multiple), template (multiple), link (multiple), quote (multiple),
comment (not multiple), ext-link (not multiple)

**All formats:** Skip for non-HTML outputs. For HTML output, extract text to get the value.

---

### `TOKEN_SUBTYPE_ATOM`
**Type:** `TOKEN_ATOM`  
A general-purpose text-holding node used for link targets, template names, module names, etc.

**Children:** varies by context — typically contains text children

**All formats:** Recurse into children and emit text content. Parent handlers typically use
`_extract_text()` on atom nodes rather than emitting them directly.

---

### `TOKEN_SUBTYPE_HIDDEN`
**Type:** `TOKEN_HIDDEN`  
A hidden/suppressed token used during parsing to mark text that should not be processed
by later stages.

**All formats:** Skip — internal parser state, never content.

---

### `TOKEN_SUBTYPE_EXT_ATTRS`
**Type:** `TOKEN_EXT_ATTRS`  
Attribute container for extension tags (`<ref class="...">` etc.).

**Children:** `ext-attr-dirty` (multiple), `ext-attr` (multiple)

**All formats:** Skip for non-HTML. For HTML output of extension tags, walk children to
extract attribute key/value pairs.

---

### `TOKEN_SUBTYPE_EXT_INNER`
**Type:** `TOKEN_EXT_INNER`  
The inner content of an extension tag. Can carry rich content for gallery/imagemap/poem etc.

**Children:** text, quote, template, ext-link, link, html, free-ext-link, gallery-image,
noinclude, imagemap-image, imagemap-link, list, ext, comment, magic-link, magic-word
(file/hr/table/heading: not in this context; dd: allowed)

**All formats:** Recurse into children and process each as normal content. This is where
`<gallery>` images, `<ref>` citation text, and `<poem>` verse content live.

---

### `TOKEN_SUBTYPE_EXT_ATTR_DIRTY`
**Type:** `TOKEN_EXT_ATTR_DIRTY`  
A raw/unparsed extension attribute fragment.

**Children:** text (single)

**All formats:** Skip.

---

### `TOKEN_SUBTYPE_EXT_ATTR`
**Type:** `TOKEN_EXT_ATTR`  
A single parsed extension attribute with key and value.

**Payload:** `token.ext_attr_equal` (the `=` sign and surrounding spaces),
`token.ext_attr_quotes` (quote character pair)

**Children:** `attr-key` (single), `attr-value` (single)

**All formats:** Skip for non-HTML. For HTML, extract key and value strings.

---

### `TOKEN_SUBTYPE_IMAGE_PARAMETER`
**Type:** `TOKEN_PLAIN`  
A single parameter of a file/image embed (e.g., `thumb`, `right`, `200px`, `Caption text`).

**Payload:** `token.image_param_raw_syntax` — canonical parameter string as it appears
in the original wikitext.

**Children:** link, text, template, ext, quote, html, comment, magic-word, hr, ext-link,
table, free-ext-link, file, magic-link

**Markdown:** The last non-layout parameter is typically the caption. Emit as the `alt`
text in `![alt](file)`. Layout parameters (`thumb`, `left`, `right`, `center`, `frame`,
`frameless`) and size parameters (`200px`) are skipped.  
**HTML:** Use layout params for CSS class; use size for `width`/`height`; use caption in `<figcaption>`.  
**Plain text:** Extract caption text only.  
**Knowledge graph:** Record layout, size, and caption as file attributes.

---

### `TOKEN_SUBTYPE_GALLERY_IMAGE`
**Type:** `TOKEN_FILE`  
A single image entry inside a `<gallery>` tag.

**Children:** `link-target` (single), `image-parameter` (multiple)

**Markdown:** Emit as `![caption](filename)` on its own line.  
**HTML:** Emit as `<figure><img .../><figcaption>...</figcaption></figure>`.  
**Knowledge graph:** Record as an image use.

---

### `TOKEN_SUBTYPE_IMAGEMAP_IMAGE`
**Type:** `TOKEN_FILE`  
The main image in an `<imagemap>` tag.

**Children:** `link-target` (single), `image-parameter` (multiple)

**Markdown:** Emit as `![alt](file)`.  
**HTML:** Emit as the base `<img>` with `<map>` overlay.

---

### `TOKEN_SUBTYPE_GALLERY_LINE`
**Type:** `TOKEN_PLAIN`  
A line of content within a `<gallery>` extension inner block (can be an image or a blank/comment line).

**Children:** varies — gallery-image, noinclude, text

**All formats:** Recurse and process children.

---

### `TOKEN_SUBTYPE_GALLERY_PARAM_WRAPPER`
**Type:** `TOKEN_PLAIN`  
A `<gallery>` tag option line such as `mode=packed`, `widths=200`, `heights=150`.

**All formats:** Skip — this is gallery configuration metadata, not content.

---

### `TOKEN_SUBTYPE_IMAGEMAP_LINK_INNER`
**Type:** `TOKEN_PLAIN`  
Inner content of an imagemap clickable region definition.

**All formats:** Recurse and process children.

---

### `TOKEN_SUBTYPE_IMAGEMAP_IMAGE_LINE`
**Type:** `TOKEN_PLAIN`  
The image line of an `<imagemap>` block.

**All formats:** Process children (contains the image token).

---

### `TOKEN_SUBTYPE_IMAGEMAP_LINK`
**Type:** `TOKEN_PLAIN`  
A clickable region definition line in an `<imagemap>`.

**Children:** text (single), link (not multiple), noinclude (not multiple)

**Markdown:** Skip imagemap region definitions; emit the base image.  
**HTML:** Emit as `<area>` element within a `<map>`.

---

### `TOKEN_SUBTYPE_INVOKE_MODULE`
**Type:** `TOKEN_ATOM`  
The Lua module name in `{{#invoke:Module|function}}`.

**Children:** text (single)

**All formats:** Internal to template JSON encoding. Skip independently.  
**Knowledge graph:** Record as a Lua module reference.

---

### `TOKEN_SUBTYPE_INVOKE_FUNCTION`
**Type:** `TOKEN_ATOM`  
The Lua function name in `{{#invoke:Module|function}}`.

**Children:** text (single)

**All formats:** Internal to template JSON encoding. Skip independently.  
**Knowledge graph:** Record as a Lua function reference.

---

### `TOKEN_SUBTYPE_PARAM_LINE`
**Type:** `TOKEN_PLAIN`  
A configuration parameter line inside `<gallery>` or `<imagemap>` (e.g., `widths=200px`).

**All formats:** Skip — metadata configuration, not content.

---

## Practical Output Recipes

### Markdown
- Process `root` children in order.
- Headings: `\n` + `#`×level + ` ` + heading-title content + `\n\n`.
- Links: `[text](target)` — extract target from `link-target`, text from `link-text`.
- Files: `![caption](filename)` — use last visible `image-parameter` as caption.
- Bold/italic: emit `*`/`**`/`***` at each `quote` marker boundary.
- Tables: collect all `tr`/`td` into rows, emit as GFM pipe table.
- Lists: emit `* item\n` or `1. item\n` with indentation for nesting.
- Templates: emit as `` `json` `` via `token.to_json(False)`.
- Skip: comments, categories, double-underscores, magic-words, args, include-only, redirects.

### HTML
- Headings → `<h1>`–`<h6>`.
- Links → `<a href="/wiki/target">text</a>`.
- Files → `<figure><img src="..." alt="caption"/><figcaption>caption</figcaption></figure>`.
- Bold/italic → `<strong>`/`<em>` via quote marker state tracking.
- Tables → `<table><tr><td>...</td></tr></table>` with attributes from `table-attrs`.
- HTML tags → pass through with attribute extraction.
- Templates → render placeholder or substitute if engine available.

### Plain Text
- Headings: title text + `\n\n`.
- Links/ext-links: display text only (drop URL/target).
- Files: caption text only.
- Bold/italic: plain text, drop markers.
- Tables: tab-separated cells.
- Templates/magic-words/args: skip.
- Comments/redirects/categories: skip.

### Knowledge Graph
- Every `heading` → section node with `level` and `title`.
- Every `link` → `linksTo` edge from page to `token.name` (normalized target).
- Every `category` → `hasCategory` edge.
- Every `ext-link`/`free-ext-link`/`magic-link` → `externalLink` edge.
- Every `template` → template-use node with name and parameters.
- Every `file` → image-use node with filename and caption.
- Every `redirect` → `redirectsTo` edge via `token.redirect_link`.
- Every `magic-word` with `#invoke` → Lua module call record.

---

## Walking the Tree

```python
def walk(token, visitor):
    """Generic depth-first walker. visitor(token) returns True to recurse."""
    if visitor(token):
        for child in token.children:
            if not isinstance(child, str):
                walk(child, visitor)

def extract_plain_text(token):
    """Recursively extract all text content, stripping all markup."""
    parts = []
    for child in token.children:
        if isinstance(child, str):
            parts.append(child)
        elif child.subtype not in (
            TokenSubType.TOKEN_SUBTYPE_COMMENT,
            TokenSubType.TOKEN_SUBTYPE_REDIRECT_SYNTAX,
            TokenSubType.TOKEN_SUBTYPE_HEADING_TRAIL,
        ):
            parts.append(extract_plain_text(child))
    return "".join(parts)
```
