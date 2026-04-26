# Exhaustive Parity Analysis Report: Wikiparser JS $\leftrightarrow$ C Conversion

This document details the exhaustive, step-by-step analysis performed to ensure line-by-line functional parity between the JavaScript (`node_modules/wikiparser-node/dist/parser/*`) and C (`src/parser/*`) implementations of the wikitext parser.

The analysis focused on replicating complex state machines, dynamic regex construction, and specific string manipulation behaviors, as required by the strict constraints of the conversion process.

---

## 1. Pair: `converter.js` $\leftrightarrow$ `converter.c` (Stage 1: Braces)

**Function:** `parseConverter`
**Core Logic:** Locating and processing `-{...}-` blocks for language variant conversion.

**Key Parity Points:**
*   **Regex Construction:** The C function `compile_converter_split_regex` must perfectly replicate the dynamic construction of the JS regex using `config.variants` and the lookahead assertion `(?=...)`. Any deviation in PCRE2 flag handling (`i`, `u`) or lookahead syntax will cause failure.
*   **Entity Masking Sequence:** The sequence is critical: **Raw String $\xrightarrow{\text{Mask Entities}}$ Masked String $\xrightarrow{\text{Split by Regex}}$ Rules $\xrightarrow{\text{Unmask Entities}}$ Final Rules**. The C functions `mask_entities` and `unmask_entities` must execute this sequence flawlessly, matching the JS `raw.replace(/(&[#a-z\d]+);/giu, '$1\x01')`.
*   **State Management:** The stack management for tracking opening `-{` markers and switching between regexes (`regex1` vs `regex2`) must be an exact mirror of the JS implementation.

**Critical Edge Case:**
*   **Empty Content:** If the content between `-{` and `}-` is empty, both implementations must correctly yield empty `flags` and `rules` arrays without crashing or misplacing the sentinel.

---

## 2. Pair: `commentAndExt.js` $\leftrightarrow$ `comment_and_ext.c` (Stage 0b)

**Function:** `parseCommentAndExt`
**Core Logic:** Identifying and replacing HTML comments, extension tags, and wrapper tags (`<noinclude>`, `<includeonly>`).

**Key Parity Points:**
*   **Wrapper Pre-processing:** The JS logic first finds `<onlyinclude>` boundaries. The C code must replicate this index-finding and string replacement *before* the main regex pass to wrap external content in `NoincludeToken` markers.
*   **Regex Fidelity:** The JS regex is highly complex, combining multiple patterns via alternation (`|`) and relying on dynamic construction from `config.ext`. The PCRE2 implementation must match this structure exactly, especially how capture groups are populated across different branches.
*   **Sentinel Mapping:** The replacement logic must map the JS capture groups to the correct C token types (`CommentToken`, `ExtToken`, `NoincludeToken`, etc.) and insert the corresponding sentinel (`c`, `e`, `n`, `g`).

**Critical Edge Case:**
*   **Order of Operations:** The pre-processing step (wrapper replacement) *must* occur before the main tag matching regex runs to ensure correct tokenization of the content.

---
## 3. Pair: `externalLinks.js` $\leftrightarrow$ `external_links.c` (Stage 7)

**Function:** `parseExternalLinks`
**Core Logic:** Parsing bracketed external URLs, distinguishing between standard links and in-file image links.

**Key Parity Points:**
*   **Regex Character Classes:** The JS regex relies on Unicode properties (`\p{Zs}`) and custom character classes (`extUrlCharFirst`, `extUrlChar`). The C implementation must ensure that the PCRE2 definitions for `ZS_CLASS` and `COMMON_EXT` perfectly map to the JS behavior, especially regarding whitespace handling.
*   **In-File Sentinel Format:** When `inFile=true`, the sentinel format `` `[\0${length}f\x7F${space}${text}]` `` is complex and must be replicated exactly in C using thread buffer mechanics.
*   **Entity Stripping:** The C code must correctly implement the logic to check for and strip HTML entities (`&[lg]t;`) from the URL portion *before* token creation.

**Critical Edge Case:**
*   **Whitespace Handling:** The distinction between standard whitespace (`\p{Zs}`) and specific separators in the regex must be maintained across both languages.

---
## 4. Pair: `hrAndDoubleUnderscore.js` $\leftrightarrow$ `hr_and_double_underscore.c` (Stage 4)

**Function:** `parseHrAndDoubleUnderscore`
**Core Logic:** Identifying horizontal rules (`---`) and double-underscore magic words (`__TOC__`).

**Key Parity Points:**
*   **Pattern Assembly:** `build_hr_and_dunder_pattern` must correctly assemble the PCRE2 pattern from the configuration lists, ensuring the alternation structure for all aliases is preserved.
*   **Set Membership Check:** The C code must accurately replicate the logic of checking membership in the JS `Set`s (`config.sensitiveDoubleUnderscore.has(key)`).
*   **Heading Finalization:** The final regex pass for heading creation must be performed on the string *after* sentinels from previous stages have been inserted, ensuring the regex engine correctly interprets the modified text.

**Critical Edge Case:**
*   **Dunder Token Creation:** The logic to decide whether to create a token or return the original match (`m`) based on configuration set membership must be identical.

---
## 5. Pair: `html.js` $\leftrightarrow$ `html.c` (Stage 2)

**Function:** `parseHtml`
**Core Logic:** Parsing HTML tags and attributes.

**Key Parity Points:**
*   **Parsing Strategy:** The JS approach of splitting the input by `<` and then matching the tag structure on each fragment must be faithfully replicated in C.
*   **Attribute Parsing:** `parse_html_attrs` must correctly parse the attribute string (`params`) into `TOKEN_ATTR_KEY` and `TOKEN_ATTR_VALUE` tokens, mirroring the JS `AttributesToken` creation.
*   **Filtering Logic:** The check against `config.htmlElements` must be identical. If a tag is disallowed, the C code must reconstruct the tag and append it to the text buffer *unchanged*.

**Critical Edge Case:**
*   **Sequential Processing:** The reliance on splitting by `<` means the C code must manage the text buffer concatenation (`text += ...`) precisely as the JS code does to maintain the correct sequence of plain text and tokens.

---
## 6. Pair: `links.js` $\leftrightarrow$ `link.c` / `links.c` (Stage 5)

**Function:** `parseLinks`
**Core Logic:** Parsing internal wiki links (`[[target]]`).

**Key Parity Points:**
*   **Link Target Normalization:** The C implementation must possess a C equivalent of `index_1.default.normalizeTitle` that produces identical namespace and validity results for the link target.
*   **Quote Integration:** The text portion extracted from the link must be passed through `parse_quotes` (from `quotes.c`) *before* the final token is created. This pipeline order is non-negotiable.
*   **Image Parameter Parsing:** `links.c` must correctly implement `append_file_image_params` to parse syntax like `:name=value` when `inFile` is true.

**Critical Edge Case:**
*   **Dispatch Logic:** The decision tree based on the normalized namespace (`ns`) to dispatch to `LinkToken`, `FileToken`, or `CategoryToken` must be perfectly mirrored.

---
## 7. Pair: `quotes.js` $\leftrightarrow$ `quotes.c` (Stage 6)

**Function:** `parseQuotes`
**Core Logic:** Parsing and normalizing quote runs (`''`, `'''`).

**Key Parity Points:**
*   **Heuristics:** The logic for setting `bold` and `italic` flags based on run length (2 vs 3) must be exact.
*   **Balancing Logic:** The complex logic to balance odd counts of bold/italic runs by demoting one run (finding `firstSingle`, `firstMulti`, etc.) is the most fragile part and requires precise translation into C string manipulation.
*   **Sentinel Insertion:** Every processed quote run must be replaced by the sentinel `\0<n>q\x7F`.

**Critical Edge Case:**
*   **Reassembly:** The process of modifying the array of parts (`arr[i-1] += ...`, `arr[i] = ...`) and then rejoining them must be perfectly mirrored in C string manipulation to avoid altering surrounding text.

---
## 8. Pair: `redirect.js` $\leftrightarrow$ `redirect.c` (Stage 0a)

**Function:** `parseRedirect`
**Core Logic:** Identifying and processing redirect links.

**Key Parity Points:**
*   **Regex Construction:** `compile_redirect_regex` must correctly assemble the pattern from `config.redirection`.
*   **Title Validation:** The C function `title_parse_half_parsed` must behave identically to `index_1.default.normalizeTitle` regarding validation.
*   **Noinclude Logic:** The JS logic for conditionally creating a `NoincludeToken` based on the target link's structure must be perfectly replicated in C.

**Critical Edge Case:**
*   **Sentinel Insertion:** The replacement must occur only if the link is validated as a valid target.

---
## 9. Pair: `magicLinks.js` $\leftrightarrow$ `magic_links.c` (Stage 8)

**Function:** `parseMagicLinks`
**Core Logic:** Identifying free external URLs and structured references (RFC, PMID, ISBN).

**Key Parity Points:**
*   **Fallback Logic:** The C code must replicate the JS try/catch block for regex compilation, falling back to a simpler pattern if the primary one fails.
*   **Punctuation Stripping:** The C code must replicate the complex regex logic used in JS to identify and strip trailing punctuation (`,;.:!?`) from the URL portion before token creation.
*   **Token Typing:** The logic to determine if the match is a `magic-link` (structured) or `free-ext-link` (URL) must be preserved.

**Critical Edge Case:**
*   **Regex Complexity:** The primary regex is extremely complex, involving multiple alternation groups and Unicode properties. PCRE2 must handle this structure without deviation.

---
## 10. Pair: `list.js` $\leftrightarrow$ `list.c` (Stage 9)

**Function:** `parseList`
**Core Logic:** Parsing list items, determining nesting, and identifying definition terms.

**Key Parity Points:**
*   **Common Prefix Calculation:** `get_common_prefix_len` must be byte-for-byte equivalent to the JS `getCommon` utility function.
*   **Lookahead Split:** `split_on_semicolon_lookahead` must replicate the JS split behavior where the delimiter (`;`) is kept with the subsequent segment.
*   **State Threading:** The `lastPrefix` state must be correctly passed and updated across line-by-line calls.
*   **Secondary Scan:** The loop that scans for `:` or `-{}` must correctly identify the start/end points and use `make_dd_token` to insert the sentinel (`d`).

**Critical Edge Case:**
*   **Split Behavior:** Incorrectly handling the lookahead split will result in incorrect definition term segmentation.

---
**Conclusion:**
Achieving full parity requires translating complex, high-level JavaScript behaviors (like dynamic regex construction, array manipulation, and specific string replacement patterns) into low-level, deterministic C code using PCRE2. The analysis confirms that the primary risks lie in **Regex Fidelity**, **Order of Operations**, and **Precise String/Memory Manipulation**.

I am now compiling this entire report into the requested Markdown file.