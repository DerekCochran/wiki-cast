---
description: Convert from JS to C
tools: [execute/getTerminalOutput, execute/awaitTerminal, execute/runInTerminal, read/problems, read/readFile, read/terminalSelection, read/terminalLastCommand, edit/createDirectory, edit/createFile, edit/editFiles, search/textSearch, vscode.mermaid-chat-features/renderMermaidDiagram, ms-vscode.cpp-devtools/GetDiagnostics_CMakeTools]
---

You are a coding agent tasked with converting JavaScript code to C. The code has been written and we are testing it.  The original JavaScript code is in the `new-js` directory, and the new C code is in the `src` directory.

## Coding Rules
- This must be a straight C implementation that takes a wikitext as input and outputs the same token tree as the JavaScript version.
- There must be parity with the JavaScript version.
  - The file structure and organization should mirror the JavaScript version, with equivalent functions and data structures.
  - We will be running the same test suite against both implementations to verify that they produce identical outputs for a wide range of inputs, including edge cases.
- The logic from the JS must be duplicated in C.  Wikitext is notorious for its complexity and edge cases, so fidelity to the original logic is crucial.
- You should not introduce any new features or optimizations that change the behavior of the code; the goal is to replicate the existing functionality in C, not to improve or modify it.
- The pipeline of parsing stages must be preserved in the same order as the JavaScript version, with each stage performing the same transformations and token creations.
- The usage of the config must have exact parity with the JavaScript version, including how configuration options affect parsing behavior and token types.
- **Memory management**:
  - `ThreadBuffers` holds two distinct named buffers plus scratch slots:
    - `stage_buffer` — the wikitext working string. Each parse stage reads it, transforms it, and writes a new version back via `wiki_thread_buf_set`. It is freely replaced after every stage. **Tokens must never hold pointers into `stage_buffer`.**
    - `tokens_buffer` — an append-only arena that persists for the entire parse call. When a stage extracts text for a token it appends that text to `tokens_buffer` via `wiki_thread_buf_append` and stores the resulting pointer as a non-owning view in the token child. `tokens_buffer` is never replaced or freed until after the token tree is destroyed.
  - Each token uses the `token_append_text_n` method to store a non-owning view (pointer + length) into `tokens_buffer` without copying.
  - There is a `thread_buffer.scratch` pool that can be used for temporary string manipulation during parsing. The scratch space is at the thread level, so do not call any other functions that use scratch at the same time to avoid conflicts. Tokens must never reference scratch buffers.

## extern_tokenizer — C Implementation Instructions

### Overview

`extern_tokenizer` is a pure C reimplementation of `Parser.parse(wikitext)` from the
`wikiparser-node` library (v1.38.1).  The JavaScript version lives in
`new-js`.  The C version must produce an identical token
tree for every input, and must be tested against the same test suite.

### Parsing Pipeline

Each stage is a function that takes the current wikitext string (which may already
contain `\0<n><ch>\x7F` sentinel markers from earlier stages), processes it, and
returns a new string with newly-found tokens replaced by fresh sentinels. Tokens are pushed into the shared `accum` array as they are created. `build()` is called once after all stages to expand every sentinel into the actual child-token tree.

#### Sentinel Marker System

The string `\0<n><ch>\x7F` is used throughout:
- `<n>` — decimal integer index into the accumulator array
- `<ch>` — single ASCII character identifying the token type

| `ch` | Token type produced |
|------|---------------------|
| `o`  | `redirect` |
| `c`  | `comment` |
| `e`  | ext tag (`<ref>` etc.) |
| `n`  | `noinclude` / `double-underscore` / `include` |
| `g`  | `onlyinclude` / `translate` |
| `t`  | `template` |
| `!`  | `magic-word` (`{{!}}`) |
| `+`  | `magic-word` (`{{!!}}`) |
| `{`  | `magic-word` (`{{(!}}`) |
| `}`  | `magic-word` (`{{!)}}`) |
| `-`  | `magic-word` (`{{!-}}`) |
| `~`  | `magic-word` (`{{=}}`) |
| `m`  | `magic-word` (filepath / server) |
| `s`  | `magic-word` (subst) |
| `a`  | `arg` |
| `h`  | `heading` |
| `x`  | html tag |
| `b`  | `table` / `tr` / `td` |
| `r`  | `hr` |
| `u`  | `double-underscore` (TOC alias) |
| `l`  | `link` / `file` / `category` |
| `q`  | `quote` |
| `w`  | `ext-link` / `free-ext-link` |
| `f`  | `free-ext-link` (in-file) |
| `i`  | `magic-link` (RFC / PMID / ISBN) |
| `d`  | `list` / `dd` |
| `v`  | `converter` |

#### Stage −1 — Pre-parse (`tidy` in JS)

- JS `Parser.parse()` first calls `tidy(wikitext)`.
- `tidy` removes bare `\0` (NUL), `\x7F` (DEL), and trailing carriage-return bytes matched by `/[\0\x7F]|\r$/gmu`.

#### Stage 0a — `parseRedirect`  (root token only)

Regex (from `config.redirection`): `^(\s*)((?:<redirect-words>)\s*(?::\s*)?)\[\[([^\n|\]]+)(\|.*?)?\]\](\s*)`

- If the entire input starts with a redirect keyword followed by `[[target]]`, create one `RedirectToken` and replace the match with `\0<n>o\x7F`.
- If not a redirect, leave the string unchanged and return `false`.

#### Stage 0b — `parseCommentAndExt`

Regex covers: HTML comments, `<noinclude>`, `<includeonly>`, `<onlyinclude>` wrappers, and all configured extension tags (e.g. `<ref>`, `<gallery>`, `<poem>`, …).

- `<!--…-->` → `CommentToken` → `\0<n>c\x7F`
- `<ref …>…</ref>` (and other ext tags) → `ExtToken` → `\0<n>e\x7F`
- `<noinclude>…</noinclude>` → `NoincludeToken` → `\0<n>n\x7F`
- `<includeonly>…</includeonly>` → `IncludeToken` → `\0<n>n\x7F`
- `<onlyinclude>…</onlyinclude>` → `OnlyincludeToken` → `\0<n>g\x7F`
- When `includeOnly=true` and `<onlyinclude>` tags are present, content *outside* them is wrapped in `NoincludeToken` and only the inner portions are kept.

#### Stage 1 — `parseBraces`

Two-pass approach:

**Pass 1 — pre-replace simple `{{…}}`:**
Regex `\{\{(…)\}\}(?!\})` finds innermost non-nested double-brace templates with no newlines or inner braces. Each match → `TranscludeToken` → `\0<n><ch>\x7F` (or pushed onto a link-stack for `[[` / `-{`).

**Pass 2 — state-machine loop with explicit stack:**
Regex source combines: `^…={1,6}` (heading start), `\[\[`, `-\{(?!\{)`, `\{{2,}` (when `}}` still exists in string).
Closes: `\]\]`, `\}-`, `\n` (heading close), `|`, `=`, `\}{2,}`.

For each match:
1. `]]` or `}-` — close a `[[` / `-{` entry in the link-stack; restore its text.
2. `\n` — if top-of-stack is `=…=` heading: test regex `^(={1,6})(.+)\1(trail)$`; if valid, create `HeadingToken` → `\0<n>h\x7F`.
3. `|` or `=` (inside template) — record pipe/equals position to split args later.
4. `}}` — close a `{{` entry; split accumulated parts on `|` / `=` to build:
   - 3-brace `{{{…}}}` → `ArgToken` → `\0<n>a\x7F`
   - 2-brace `{{…}}` → `TranscludeToken` → `\0<n><ch>\x7F` where `<ch>` depends on `getSymbol()` (magic-word check vs. template)
5. `{{…` (open) — push onto stack.

**`TranscludeToken` constructor** (called for `{{…}}`):
- Splits title on `:` to detect magic words (from `config.parserFunction` / `config.variable`)
- If it is a magic word: type = `magic-word`; first child = `SyntaxToken` with type `magic-word-name`; remaining parts become `ParameterToken` children.
- Special magic words `{{!}}`, `{{=}}`, `{{!!}}`, `{{(!}}`, `{{!)}}`, `{{!-}}`, `{{server}}` use a lookup table to get their sentinel character.
- If it is a normal template: type = `template`; name gets the namespace prefix (`Template:`) via `normalizeTitle`; first child = `AtomToken` with type `template-name`; remaining parts become `ParameterToken` children.

**`ArgToken` constructor** (called for `{{{…}}}`):
- First part → `AtomToken` with type `arg-name` (child 0)
- Second part (if present) → `Token` with type `arg-default` (child 1)
- Further parts → `HiddenToken` (child 2+)
- Sentinel char = `a`.

**`HeadingToken` constructor** (called during stage 1 and finalized in stage 4):
- Child 0 = `Token` with type `heading-title` (the text between `=` markers)
- Child 1 = `SyntaxToken` with type `heading-trail` (whitespace after closing `=`)
- Sentinel char = `h`.

**`ParameterToken` constructor** (shared by template args and magic-word args):
- Child 0 = `Token` with type `parameter-key` (empty for positional args, key text for named)
- Child 1 = `Token` with type `parameter-value`
- Anonymous (positional) args: key is assigned a sequential integer (1, 2, …); key token has no text content.

#### Stage 2 — `parseHtml`

- Matches known HTML tags from `config.html` (normal tags, void tags, deprecated tags).
- Creates `HtmlToken` → `\0<n>x\x7F`.
- Handles attributes, self-closing, and nested parsing of children.

#### Stage 3 — `parseTable`

Operates on the **first text child** of the token (not the wikitext string directly).
Split into lines; line-by-line state machine with a table stack:

- Line starts with `{|` (or sentinel variant) → `TableToken` → `\0<n>b\x7F`; push onto stack.
- Line starts with `|}` → close current table; pop stack.
- Line starts with `|-` → `TrToken` (table row) → `\0<n>b\x7F`.
- Line starts with `|`, `||`, `!`, `!!` → one or more `TdToken` (table cell) → `\0<n>b\x7F` each.
- Indented table opening (`:`prefix) → also creates `DdToken` → `\0<n>d\x7F` for the indent.
- Non-matching lines are appended as plain text into the current table/row.

#### Stage 4 — `parseHrAndDoubleUnderscore`

Operates on the **first text child** of the token.

- `^(comment-sentinels*)(-{4,})` → `HrToken` → `\0<n>r\x7F`
- `__(TOC|NOTOC|…)__` → `DoubleUnderscoreToken` → `\0<n>u\x7F` (TOC alias) or `\0<n>n\x7F`
- Heading finalization: regex `^(={1,6})(.+)\1(trail)$` on remaining text → `HeadingToken` → `\0<n>h\x7F`
  *(This is the primary heading creation path; stage 1 only creates headings for multi-line inputs that require the brace-stack parser.)*

#### Stage 5 — `parseLinks`

Splits the wikitext on `[[`; for each fragment:

- Regex extracts: `link` (target), optional `delimiter` (`|` or `!` sentinel), optional `text`, `after`.
- Validation: `normalizeTitle(link)` — skips if invalid, if link starts with a protocol URL, or if link contains certain sentinels.
- Namespace dispatch:
  - NS 6 (File/Image) and no interwiki → `FileToken` → `\0<n>l\x7F`; text portion is further parsed by `parseExternalLinks`.
  - NS 14 (Category) and no interwiki → `CategoryToken` → `\0<n>l\x7F`
  - Otherwise → `LinkToken` → `\0<n>l\x7F`
- `text` portion is always passed through `parseQuotes` first (stage 6 logic applied inline).
- Force-colon prefix (`:File:…`) → always `LinkToken`, never file/category.

**`LinkToken` / `FileToken` / `CategoryToken` constructor:**
- Child 0 = token with type `link-target`
- Child 1 = token with type `link-text` (only if `text` was present or delimiter existed)

#### Stage 6 — `parseQuotes`

Called per line by the stage dispatcher (`text.split('\n')`, parse each line, then join).
Within each line, splits on `/''{2,}/`:

- Counts runs of `''` (italic) and `'''` (bold); 4-quote runs → promote to `'''` + prepend `'`; 5+ → `'''''`.
- Balances odd counts: picks one run to demote from bold to italic based on position heuristics.
- Each quote run → `QuoteToken` → `\0<n>q\x7F`
- If unclosed bold/italic remain at end, appends a synthetic closing `QuoteToken`.

#### Stage 7 — `parseExternalLinks`

Regex: `\[(url)(space)(text)\]` where `url` matches configured protocols or `//`.

- Normal bracketed external link `[url text]` → `ExtLinkToken` → `\0<n>w\x7F`
  - Children: `ext-link-url` token, `ext-link-text` token
- When called from `parseLinks` for File images (`inFile=true`): URL portion → `MagicLinkToken` → `[\0<n>f\x7F space text]` (brackets kept, URL replaced)
- HTML entities `&lt;` / `&gt;` in URL are split: entity and following text move to `text`.

#### Stage 8 — `parseMagicLinks`

Regex finds free (unbracketed) external URLs and RFC/PMID/ISBN references:

- Free URL (e.g. `https://example.com`) → `MagicLinkToken` (type `free-ext-link`) → `\0<n>w\x7F`
  - Trailing punctuation (`,;.:!?`) is stripped from URL and left as plain text.
- `RFC 1234`, `PMID 9876`, `ISBN 978-…` → `MagicLinkToken` (type `magic-link`) → `\0<n>i\x7F`
- Must be preceded by a word-boundary (not `\p{L}`, `\p{N}`, or `_`).

#### Stage 9 — `parseList`

Called **per line** (the caller splits on `\n` and calls `parseList` for each).

- Regex `^(comment-sentinels*)([;:*#]+)(\s*)` detects list prefix.
- Computes common-prefix length vs. previous line's prefix to determine nesting.
- Each segment of the prefix → `ListToken` → `\0<n>d\x7F`
- `;` (definition term) triggers a secondary scan for `:` to create `DdToken` → `\0<n>d\x7F` for the definition part.
- State object `{ lastPrefix }` is threaded across lines.

#### Stage 10 — `parseConverter`

Regex-based stack: finds `-{` … `}-` pairs (possibly nested):

- Splits inner content on `|`: left of `|` = flags (`;`-separated), right = rules.
- Rules are split on `;` (but only when followed by a variant code like `zh:`, `zh-hans:`, …, using `config.variants`).
- Each rule may have the form `variant:text` or `source=>variant:text`.
- `ConverterToken` → `\0<n>v\x7F`
  - Children: `converter-flags` token, then one `converter-rule` token per rule
  - Each `converter-rule` has `converter-rule-variant` and `converter-rule-to` children (or just `converter-rule-noconvert` for untagged rules).

#### Post-pipeline — `build()`

Walk the root token's text children; for each `\0<n><ch>\x7F` sentinel found:
- Look up `accum[n]` to get the pre-built child token.
- Replace the text segment containing the sentinel with a token-child reference.
- Recurse into every token's children and do the same (tokens themselves may contain sentinels from earlier stages in their text children).

