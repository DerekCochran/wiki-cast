# Parity Issues — Research & Verification Report (Batch 2)

This document verifies each issue raised in the second batch of potential parity problems,
cross-references the actual C and JS source, records the true status, and provides
concrete test fixtures for `test_pipeline.c`.

---

## Issue A — Critical: Page Context Not Propagated to C Link/Title Parsing

### Status: **CONFIRMED — real gap**

### Evidence

**JS** (`dist/index.js` line 239, `dist/src/index.js` line 483):
```js
token.pageName = page;           // Parser.parse() stores page on root token
this.setText(parseLinks(…, this.pageName, tidy)); // stage-5 passes pageName
```

**C** (`include/parse.h` line 25, `src/parse.c` stage-5 call):
```c
Token *wiki_parse(const char *wikitext, const ParserConfig *cfg,
                  bool include, int max_stage);          // <-- no page param
…
parse_links(ws, cfg, &accum, NULL, false);               // always NULL page
```

`title_compose_resolved()` in `src/title.c` already has full subpage/relative-path
logic: `base[0]=='/'` → prepend page; `../` prefix → climb page hierarchy.
But it is never exercised from the root pipeline because page is always `NULL` or `""`.

`finalize_gallery_and_link_names()` also calls
`title_parse_half_parsed(raw, raw_len, def_ns, cfg, true, "")` — always empty page.

### Impact

Any link that starts with `/` or `../` will fail to resolve to the correct title.
`[[#Fragment]]` self-links will produce wrong names.  Token `name` fields diverge from JS.

### Recommended Fix

Add `const char *page` to `wiki_parse()` API and thread it down:
- `wiki_parse(…, const char *page)`
- `parse_links(ws, cfg, &accum, page, tidy)` at stage 5 in `parse.c`
- `finalize_gallery_and_link_names(…, page)` — pass page to all `title_parse_half_parsed` calls
- `build_redirect_token` (see Issue B) likewise

### Test Fixtures

```c
// In test_pipeline.c samples[] — add these:

// Subpage link — name must be "Parent/Sub" when page="Parent"
"[[/Sub]]",
"[[/Sub|display]]",

// Relative-parent link — name must be "Grand/Child" when page="Grand/Parent"
"[[../Child]]",

// Self-anchor only — should produce link token with name=="" or page name
"[[#Section heading]]",

// Combination: template + subpage link
"{{Template}} and [[/Sub]]",

// Category on a subpage
"[[Category:Topic]] and [[/Sub-article]]",
```

---

## Issue B — High: Redirect Target Normalization Uses NULL Config

### Status: **CONFIRMED — real gap**

### Evidence

**JS** (`dist/parser/redirect.js` line 20):
```js
// Validity check:
index_1.default.normalizeTitle(mt[3], 0, false, config, { halfParsed:true, … })
// Token construction also passes config to RedirectToken constructor
new redirect_1.RedirectToken(…, config, accum);
```

**C** (`src/parser/redirect.c` in `build_redirect_token`, line ~80):
```c
Title *parsed = title_parse_half_parsed(link, link_main_len, 0, NULL, true, "");
//                                                            ^^^^ NULL cfg
```

The validity gate in `parse_redirect()` correctly uses `cfg`:
```c
if (!title_is_valid_half_parsed(link_ptr, link_len, cfg)) { … }
```
But `build_redirect_token` — which computes `target_tok->name` — passes `NULL`.
With a NULL config: namespace alias lookup fails, interwiki resolution is skipped,
so `target_tok->name` may be wrong for redirect targets that need namespace
normalization (e.g. `#REDIRECT [[WP:Shortcut]]`, `#REDIRECT [[Wikipedia:foo]]`).

### Impact

`redirect-target` token `name` attribute diverges from JS for any target that
requires namespace-alias or interwiki resolution.

### Recommended Fix

Pass `cfg` into `build_redirect_token` and forward it to `title_parse_half_parsed`:

```c
// src/parser/redirect.c — build_redirect_token signature
static Token *build_redirect_token(
    …,
    const ParserConfig *cfg) {     // <-- add cfg
    …
    Title *parsed = title_parse_half_parsed(link, link_main_len, 0, cfg, true, "");
```

Update the single call site in `parse_redirect()` to pass `cfg`.

### Test Fixtures

```c
// Redirect with namespace alias (config-dependent namespace resolution)
"#REDIRECT [[Project:Shortcut]]",
"#REDIRECT [[WP:Foo]]",

// Redirect with interwiki (interwiki list from config)
"#REDIRECT [[en:English article]]",

// Redirect with display text (noinclude child)
"#REDIRECT [[Target page|display text]]",

// Redirect to a subpage
"#REDIRECT [[/SubPage]]",
```

---

## Issue C — High: Image Parameter Classification Lacks JS Validation Gate

### Status: **CONFIRMED — real gap**

### Evidence

**JS** `dist/src/imageParameter.js` has a `validate(key, val, config, extOrType, halfParsed)` function:
```js
switch (key) {
    case 'width':
        return !value && Boolean(val) || /^(?:\d+x?|\d*x\d+)(?:\s*px)?$/u.test(value);
    case 'link':
        // validates as URL or normalizeTitle().valid
    case 'lang':
        return (extOrType === 'svg' || extOrType === 'svgz') && !/[^a-z\d-]/u.test(value);
    case 'page':
        return (extOrType === 'djvu' || extOrType === 'djv' || extOrType === 'pdf')
               && Number(value) > 0;
    case 'alt': case 'class': case 'manualthumb':
        return true;
    default:   // numeric params like 'border', sizing
        return Boolean(value) && !isNaN(value);
}
```

**C** (`src/parser/links.c` `append_file_image_params`):
```c
if (!match_img_syntax(match_ptr, match_len, syntax, &cap_ptr, &cap_len, &has_cap))
    continue;
// If syntax matches → create image-parameter token unconditionally.
// No semantic validation of the captured value.
```

The C implementation only does structural pattern matching against the config `img`
key/value table.  It never:
- Checks that a captured width value is a valid pixel specification
- Validates that a `link=` value is a real URL or valid wiki title
- Restricts `lang=` to svg/svgz files
- Restricts `page=` to djvu/pdf files with a positive integer value
- Validates numeric parameters with `!isNaN`

Result: syntactically matching but semantically invalid parameters become
`image-parameter` nodes in C but fall through to `caption` in JS.

### Impact

For files with invalid image parameters (e.g. `[[File:Foo.jpg|page=abc]]`,
`[[File:Foo.png|lang=invalid]]`), C emits structured `image-parameter` children
while JS demotes them to `caption`.

### Recommended Fix

Port the `validate()` logic from `imageParameter.js` into C.  Key points:
- `width` key: check against regex `^(?:\d+x?|\d*x\d+)(?:\s*px)?$`
- `link` key: call `title_parse_half_parsed` (or URL check)
- `lang` key: only accept for svg/svgz extension (needs file extension from token name)
- `page` key: only accept for djvu/djv/pdf, value must be positive integer
- Default/numeric keys: `strtod` + `isnan` check

The file extension can be derived from the link target already stored in the
`FileToken`'s first child at the time `append_file_image_params` is called.

### Test Fixtures

```c
// Invalid width — should become caption, not image-parameter
"[[File:Foo.jpg|notapixelvalue]]",

// Valid width
"[[File:Foo.jpg|200px]]",
"[[File:Foo.jpg|200x150px]]",

// lang= only valid for SVG
"[[File:Diagram.svg|lang=zh]]",
"[[File:Photo.jpg|lang=zh]]",   // should be caption

// page= only valid for DJVU/PDF
"[[File:Book.djvu|page=3]]",
"[[File:Photo.jpg|page=3]]",    // should be caption

// link= with a valid wiki target
"[[File:Foo.jpg|link=Main Page]]",

// link= with empty string (valid — removes link)
"[[File:Foo.jpg|link=]]",

// Numeric border/padding (numeric default branch)
"[[File:Foo.jpg|border]]",

// Multiple params mixed valid/invalid
"[[File:Foo.jpg|200px|right|caption text]]",
"[[File:Foo.jpg|notvalid|right|caption]]",
```

---

## Issue D — Medium: Converter Stage Gating Differs

### Status: **CONFIRMED — real gap**

### Evidence

**JS** (`dist/src/index.js` line 522):
```js
#parseConverter() {
    if (this.#config.variants.length > 0) {   // <-- gated
        const { parseConverter } = require('../parser/converter');
        this.setText(parseConverter(this.firstChild.toString(), …));
    }
}
```

**C** (`src/parse.c` stages 10, 802, 1168, 1497):
```c
case 10:
    parse_converter(ws, cfg, &accum);   // always runs — no variants check
    …
parse_converter(scratch, cfg, accum);   // nested pipelines also always run
```

When `cfg->variants.count == 0` (e.g. English wiki config), the C parser still
processes `-{…}-` fragments whereas JS leaves them as plain text.

### Impact

In configs without variants, text like `-{ some text }-` will be converted to
`ConverterToken` nodes in C but remain as plain text in JS.

### Recommended Fix

Gate `parse_converter` calls with a variants count check everywhere it is called:

```c
// src/parse.c — wherever parse_converter is called
if (cfg->variants.count > 0) {
    parse_converter(ws, cfg, &accum);
}
```

Apply to all three call sites:
1. Stage 10 in the main loop
2. `run_nested_plain_pipeline` (td-inner / ext-inner)
3. `postprocess_parameter_value_inline`

### Test Fixtures

```c
// Converter syntax in an English (no-variants) config — must stay as plain text
"-{zh:简体;zh-hant:繁體}-",
"Before -{A|B}- after",

// Converter inside a template in no-variants config
"{{T|-{A|B}-}}",

// Converter inside a ref (ext-inner) in no-variants config
"<ref>-{A|B}-</ref>",
```

---

## Issue F — Bonus: Poem Ext-Inner First-Line List Processing

### Status: **LIKELY GAP — needs targeted test**

### Evidence

**JS** (`dist/src/index.js` `#parseList`):
```js
let i = type === 'root' || (type === 'ext-inner' && name === 'poem') ? 0 : 1;
for (; i < lines.length; i++) {
    lines[i] = parseList(lines[i], state, this.#config, this.#accum);
}
```
For `<poem>` ext-inner, JS starts processing from line **0** (no skip).
For all other ext-inner types, it starts from line **1** (skip first line).

**C** (`src/parse.c` `run_nested_plain_pipeline`, line ~799):
```c
if (is_td_inner) {
    parse_list_skip_first_line(scratch, cfg, accum);
} else if (is_ext_inner) {
    parse_list_skip_first_line(scratch, cfg, accum);   // always skips for ALL ext-inner
}
```

`parse_list_skip_first_line` always skips line 0 regardless of whether the
ext-inner is a `poem` tag.  This means the first list item in `<poem>` content
may be missed by C while JS processes it.

### Recommended Fix

Check the token name before skipping:
```c
bool is_poem = (t->name && strcmp(t->name, "poem") == 0);
if (!is_poem) {
    parse_list_skip_first_line(scratch, cfg, accum);
} else {
    parse_list(scratch, cfg, accum);   // process all lines for poem
}
```

---

## Issue G — Medium: QuoteToken Bold/Italic Flags Use Static Type Instead of Running Open/Close State

### Status: **CONFIRMED — real gap**

### Evidence

**JS** (`dist/parser/quotes.js`):
```js
let bold = false, italic = false;
for (let i = 1; i < length; i += 2) {
    const n = arr[i].length, isBold = n !== 2, isItalic = n !== 3,
    token = new quote_1.QuoteToken(arr[i], { bold: isBold && Boolean(bold), italic: isItalic && Boolean(italic) }, config, accum);
    if (isBold) {
        bold = !bold && token;   // ← opens or closes: false for first, truthy for second
    }
    if (isItalic) {
        italic = !italic && token;
    }
    arr[i] = `\0${accum.length - 1}q\x7F`;
}
```

So `token.bold` for a `'''` run is **`false`** on the opening token (first `'''`) and **`true`** on the closing token (second `'''`).

**C** (`src/parser/quotes.c` `build_quote_token`):
```c
t->data.quote.bold   = txt_len != 2;   // true for ALL ''' and ''''' tokens
t->data.quote.italic = txt_len != 3;   // true for ALL '' and ''''' tokens
```

C always sets `bold = true` for any `'''` token regardless of whether it opens or closes a bold span.

### Impact

The `bold`/`italic` flags are consumed by stage-9 (`parse_list`) to track open HTML-like nesting depth when deciding whether a `:` should create a `dd` token.  In JS `list.js`:

```js
const { bold, italic } = accum[idx];
if (bold) {
    update(lb);   // update(closing) — increments or decrements lt
    lb = !lb;
}
```

In JS, the **first** `'''` has `bold = false` → `if (bold)` is skipped → **`lt` is not incremented**.  
In C, the first `'''` has `bold = true` → `if (!lb) lt++` → **`lt` becomes 1** prematurely.

For a definition-list line containing bold text and a `:` separator, this causes a divergence: the `:` between the first and second `'''` is processed at `lt = 0` in JS (eligible for `dd`) but at `lt = 1` in C (suppressed).

**Demonstrated behavioural difference** for input `;A'''B''' : C`:

| Step | JS `lt` | JS result | C `lt` | C result |
|---|---|---|---|---|
| First `'''` (bold=false) | 0 | no change | 1 | lt++ |
| `:` colon | 0 → dd created | dd token | 1 → skipped | no dd |
| Second `'''` (bold=true) | 1 | update(lb=false) → lt++ | 0 | lt-- |

### Recommended Fix

Update `build_quote_token` to accept and store the running state:

```c
// In parse_quotes(), track running state like JS:
bool bold_state = false, italic_state = false;
...
// When building token for quote run of length n:
bool isBold = (n != 2), isItalic = (n != 3);
t->data.quote.bold   = isBold   && bold_state;
t->data.quote.italic = isItalic && italic_state;
if (isBold)   bold_state   = !bold_state;
if (isItalic) italic_state = !italic_state;
```

The closing `tidy` token at the end must get `bold = bold_state` (which is now
the truthy open token), not `(len != 2)`.

---

## Issue H — Medium: Fullwidth Double-Underscore Pattern Strips Wrong Number of Bytes

### Status: **CONFIRMED — real gap**

### Evidence

**JS** (`dist/parser/hrAndDoubleUnderscore.js`):
```js
config.regexHrAndDoubleUnderscore ??= new RegExp(
    `...|＿{2}(${all.filter(s => !isUnderscore(s)).map(s => s.slice(2, -2)).join('|')})＿{2}`,
    'gimu'
);
```
For a fullwidth-underscore dunder like `"＿＿目次＿＿"` (6 chars: `＿`, `＿`, `目`, `次`, `＿`, `＿`),
JS calls `s.slice(2, -2)` → removes **2 chars** from each end → inner = `"目次"`.
The pattern becomes `＿{2}(目次)＿{2}` and correctly matches `＿＿目次＿＿`.

**C** (`src/parser/hr_and_double_underscore.c` `build_hr_and_dunder_pattern`):
```c
static const char fw[] = "\xEF\xBC\xBF"; /* U+FF3F — 3 UTF-8 bytes per char */
…
pattern_append_n(&pattern, &cap, &len,
    it + (sizeof(fw) - 1U),                    // skip only 3 bytes (ONE ＿)
    it_len - 2U * (sizeof(fw) - 1U));           // remove only 6 bytes total
```

For `"＿＿目次＿＿"` = 18 UTF-8 bytes:
- C skips `it + 3` → starts at the **second** `＿`
- Length `18 - 6 = 12` bytes → captures `＿目次＿` (NOT just `目次`)

The pattern becomes `＿{2}(＿目次＿)＿{2}` which would require **3** fullwidth underscores on each side to match — the string `＿＿目次＿＿` would NOT match.

### Impact

Double-underscore magic words stored in the fullwidth form (`＿＿目次＿＿`, the Chinese
alias for `__TOC__`) will not be recognised in C.  The regex simply never fires
for them.

### Recommended Fix

Skip **two** fullwidth underscores from the start and remove **four** from the total:

```c
/* JS: s.slice(2, -2) removes 2 full-width underscores (3 bytes each) from each end */
pattern_append_n(&pattern, &cap, &len,
    it + 2U * (sizeof(fw) - 1U),               // it + 6: skip TWO ＿
    it_len - 4U * (sizeof(fw) - 1U));           // len - 12: remove FOUR ＿ total
```

Also check `is_fullwidth_wrapped_dunder` — it currently only verifies that the
string starts and ends with **one** `＿` (3 bytes).  It should verify at least
**two** on each side for a proper double-underscore item:

```c
static int is_fullwidth_wrapped_dunder(const char *s) {
    static const char fw[] = "\xEF\xBC\xBF";
    size_t fwl = sizeof(fw) - 1U;         /* 3 bytes per char */
    size_t len = s ? strlen(s) : 0;
    if (len < 4 * fwl + 1) return 0;     /* need ＿＿X＿＿ minimum */
    return memcmp(s, fw, fwl) == 0
        && memcmp(s + fwl, fw, fwl) == 0             /* second leading ＿ */
        && memcmp(s + len - fwl, fw, fwl) == 0
        && memcmp(s + len - 2 * fwl, fw, fwl) == 0;  /* second trailing ＿ */
}
```

---

## Issue I — Low: Heading Trailing-Whitespace Regex Too Narrow

### Status: **CONFIRMED — real gap**

### Evidence

**JS** (`dist/parser/hrAndDoubleUnderscore.js`):
```js
data = data.replace(
    /^((?:\0\d+[cn]\x7F)*)(={1,6})(.+)\2((?:\s|\0\d+[cn]\x7F)*)$/gmu,
    ...
);
```
The trailing-whitespace group uses `\s` which in JavaScript matches
`[ \f\n\r\t\v\u00a0\u1680\u2000–\u200a\u2028\u2029\u202f\u205f\u3000\ufeff]`.

**C** (`src/parser/hr_and_double_underscore.c`):
```c
const char *hpat =
    "^((?:\\x00\\d+[cn]\\x7F)*)(={1,6})(.+)\\2"
    "((?:[ \\t\\f\\v]|\\x00\\d+[cn]\\x7F)*)$";
```
Only `[ \t\f\v]` — missing `\r`, `\n`, and all Unicode whitespace (NBSP, thin space,
ideographic space, etc.) from the trail group.

After the pre-pass `tidy` stage, lone `\r` are removed so that case is moot.
`\n` ends the line in multiline mode so it cannot appear in a single-line trail.
The **practical impact** is for `\u00a0` (NO-BREAK SPACE, U+00A0) or other Unicode
whitespace that can appear as trailing content inside headings.

For `== Heading ==\u00a0` (line ending with NBSP after the closing `==`):
- JS: trail group matches `\u00a0`; heading recognized with trail `= "\u00a0"`
- C: trail group fails to match `\u00a0`; the `(.+)\2` backtrack fails to find a
  valid `==` prefix; heading **not recognized** — the line stays as plain text

### Recommended Fix

Replace `[ \\t\\f\\v]` with `\\s` and add `PCRE2_UCP` to the compile flags:

```c
const char *hpat =
    "^((?:\\x00\\d+[cn]\\x7F)*)(={1,6})(.+)\\2"
    "((?:\\s|\\x00\\d+[cn]\\x7F)*)$";

pcre2_code *hre = pcre2_compile(...,
    PCRE2_UTF | PCRE2_MULTILINE | PCRE2_UCP,
    ...);
```

`PCRE2_UCP` makes `\s` match the full set of Unicode "space separator" characters,
matching JavaScript's `\s` behaviour.

