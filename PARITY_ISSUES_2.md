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

## Issue E — Medium: Gallery/Imagemap File-Target Parsing Uses Reduced Pipeline

### Status: **CONFIRMED — real gap; `parse_gallery_caption_fragment` is also DEAD CODE**

### Evidence

**JS**: Gallery image lines go through the full `Token.parse()` chain, meaning all
stages 0–9 run on the caption text.

**C** `parse_gallery_image_line` in `src/parse.c` (line ~506):
```c
parse_braces(&tmp_tb, cfg, accum);
parse_links(&tmp_tb, cfg, accum, NULL, false);
// Missing: comment_and_ext, parse_html, parse_quotes, parse_external_links,
//          parse_magic_links, parse_list, parse_converter
```

`parse_imagemap_image_line` has the same reduced pipeline.

Furthermore, `parse_gallery_caption_fragment()` (line ~418) exists with a fuller
pipeline (comment_and_ext → braces → html → links → quotes → external_links →
magic_links) but is **never called anywhere** — it is dead code.  The gallery
image caption is never processed through it.

### Impact

Gallery image captions containing comments (`<!-- -->`), HTML tags, bold/italic
(`''…''`), external links, or list syntax will tokenize differently between
C and JS.

### Recommended Fix

In `parse_gallery_image_line`, after `parse_links`, add the remaining stages that
JS would apply:
```c
parse_comment_and_ext(&tmp_tb, cfg, accum, false);   // before braces
parse_braces(&tmp_tb, cfg, accum);
parse_html(&tmp_tb, cfg, accum);
parse_links(&tmp_tb, cfg, accum, NULL, false);
// quotes are applied per-line to the caption fragment only — use scratch
parse_quotes_stage6_per_line(&tmp_tb, cfg, accum);   // needs to be accessible
parse_external_links(&tmp_tb, cfg, accum, true);     // inFile=true for gallery
parse_magic_links(&tmp_tb, cfg, accum);
```

Same fix for `parse_imagemap_image_line`.

Remove or wire in `parse_gallery_caption_fragment` to avoid it being dead code.

### Test Fixtures

```c
// Gallery with comment in caption
"<gallery>\nFile:Foo.jpg|Caption <!-- hidden --> text\n</gallery>",

// Gallery with bold/italic in caption (requires parse_quotes)
"<gallery>\nFile:Foo.jpg|''italic'' caption\n</gallery>",
"<gallery>\nFile:Foo.jpg|'''bold''' caption\n</gallery>",

// Gallery with external link in caption
"<gallery>\nFile:Foo.jpg|See [http://example.com link]\n</gallery>",

// Gallery with magic link in caption
"<gallery>\nFile:Foo.jpg|See RFC 2119\n</gallery>",

// Gallery with template in filename line
"<gallery>\nFile:Foo.jpg|{{Template|arg}} caption\n</gallery>",

// Imagemap with comment in image line
"<imagemap>\nFile:Foo.jpg|thumb|Description <!-- note -->\npoly 0 0 10 10 [[Target]]\n</imagemap>",

// Imagemap caption with italic
"<imagemap>\nFile:Foo.jpg|''italic'' caption\npoly 0 0 10 10 [[Link]]\n</imagemap>",
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

