# Exhaustive Parity Analysis: `braces.js` vs. `braces.c`

This document details an exhaustive, step-by-step analysis comparing the logic of the JavaScript implementation (`braces.js`) and the C implementation (`braces.c`) to achieve line-by-line functional parity.

## 1. High-Level Goal

The objective is to ensure that for any given wikitext input, both the JS and C parsers produce an identical token tree structure, including the exact sentinel markers (`\0<n><ch>\x7F`) used to represent tokens. The parsing process is complex, involving iterative regex replacement, state management (link stack), and specialized token construction.

## 2. Component Breakdown and Edge Case Evaluation

### Component A: `getSymbol` Function Parity (Sentinel Determination)

This function is critical as it determines the sentinel character (`ch`) for tokens.

**JS Logic Summary:**
1.  Apply `removeComment` and `trimLc` to the input string `s`.
2.  Check if the resulting canonical name exists in a predefined `Map` (`marks`) for specific magic words (`!`, `!!`, `(!`, etc.).
3.  If not found, check against regex patterns for URL-like names (`filepath:`, `fullurl:`, etc.) $\rightarrow$ returns `'m'`.
4.  Check against regex patterns for variable definitions (`#vardefine:`).
5.  Default to `'t'`.

**C Logic Summary:**
1.  Applies `str_remove_comment` and trimming.
2.  It then performs extensive logic involving `cfg` (ParserConfig) to derive `canonical`, `base_orig`, and `base_lc` by splitting the name on `:` and checking against `cfg->parser_function_sensitive` and `cfg->parser_function_insensitive` maps.
3.  It then checks for magic words, URL patterns, and `#vardefine:`.

**Discrepancy & Challenge:**
The C implementation introduces a dependency on `cfg` and complex string manipulation (`base_orig`, `base_lc`) that is not explicitly present in the simplified JS snippet provided. The JS snippet relies on direct string matching after trimming/lowercasing. If the C code's complex canonicalization logic leads to a match in `cfg` when the JS code would have failed the map/regex checks, parity is broken.

**Edge Case:** A template name like `MyTemplate:Name` might be handled differently. JS checks the lowercased, trimmed string against the map first. C checks the raw, trimmed string, then potentially the base parts derived from the colon.

**Conclusion for Parity:** The C implementation must be simplified to mirror the JS flow: **Trim $\rightarrow$ Lowercase $\rightarrow$ Check Map $\rightarrow$ Check Regex $\rightarrow$ Default.** The complex `cfg` lookups should be removed or strictly validated against the JS behavior.

### Component B: `build_template_token` Parity (Token Construction)

This function constructs the token object based on whether it is an argument (`ArgToken`) or a transclude (`TranscludeToken`).

**JS Logic Summary:**
1.  Splits the inner content by `|`.
2.  For each part, it checks for an `=` sign.
3.  If `=` exists, it splits into `[key, value]`.
4.  If no `=`, the whole part is treated as a positional argument.
5.  It uses `string_1.restore` to handle embedded sentinels during text restoration.

**C Logic Summary:**
1.  Uses `build_from_inner` to split by `|` and restore text.
2.  The C implementation explicitly models the `ArgToken` structure: `[arg-name, arg-default?, hidden*]`.
3.  It correctly splits on `=` to determine key/value pairs.
4.  It handles positional arguments by assigning sequential integers (`positional++`) to the parameter name.

**Discrepancy & Challenge:**
The C implementation's explicit construction of `ArgToken` children (name, default, hidden) is a structural interpretation. The JS code implies this structure but relies on the underlying `TranscludeToken` constructor to handle the resulting parts array. The C code's logic for assigning positional names (`"1"`, `"2"`, etc.) must exactly match how the JS version determines the positional index.

**Edge Case:** A template part containing only an equals sign (`=`) or only a pipe (`|`) must be handled gracefully without crashing or misinterpreting the structure.

**Conclusion for Parity:** The C implementation's argument parsing logic (splitting on `=` and handling positional vs. named) appears structurally sound, provided the `positional` counter increments exactly as the JS version does when a positional argument is encountered.

### Component C: `parseBraces` Loop Parity (The Main Driver)

This involves the iterative replacement loop and the final state machine loop.

**JS Logic Summary:**
1.  **Pass 1 (Simple Args):** Uses a regex (`reReplace`'s first alternative) to find innermost, non-nested `{{{...}}}` and replaces them immediately with a sentinel (`\0<n><ch>\x7F`).
2.  **Pass 2 (Main Loop):** Iteratively replaces all remaining matches (`reReplace`) until the string stabilizes.
3.  **State Machine:** After replacement, a second regex loop processes remaining structural elements (`[[`, `-{`, `\n`, `}}`, etc.) using a stack to manage context (links, headings).

**C Logic Summary:**
1.  **Pass 1 (Simple Args):** Calls `parse_simple_args` using PCRE2, which performs the same innermost replacement logic.
2.  **Pass 2 (Main Loop):** Uses a `do...while` loop driven by `pcre2_match` on the main regex (`cfg->regex_braces`).
3.  **State Machine:** The C code uses `linkStack` and `link_stack_lens` to park matches (`[[...]]`, `-{...}-`) and replaces them with numeric sentinels (`\0<N>\x7F`). It then restores these parked items at the end.

**Discrepancy & Challenge:**
The C implementation's use of `linkStack` and the convergence check (`prev_buf` vs `out_buf`) is a necessary adaptation for C's string handling when dealing with embedded sentinels, which is not explicitly visible in the JS replacement callback structure. The core logic must ensure that when the C code replaces a match with a sentinel, the *content* of that match is correctly stored in the `linkStack` for later restoration, exactly as the JS code would have implicitly handled it by leaving the raw text in the string until the final build phase.

**Edge Case:** The convergence check (`prev_buf_len == out_len && memcmp(...) == 0`) is crucial. If this check fails to trigger when it should, the loop will run infinitely or terminate prematurely.

## 3. Summary of Required Actions for Parity

To achieve full parity, the following high-level changes are necessary:

1.  **Refactor `braces_get_symbol`:** Remove the complex `cfg` dependency and strictly implement the JS logic: Trim $\rightarrow$ Lowercase $\rightarrow$ Map Check $\rightarrow$ Regex Check $\rightarrow$ Default.
2.  **Verify `build_template_token`:** Ensure the argument parsing logic correctly maps positional arguments to sequential indices (`1`, `2`, ...) exactly as the JS version does when no `=` is present.
3.  **Validate Regex Patterns:** Confirm that the PCRE2 patterns used in `parse_simple_args` and `parse_braces` are functionally equivalent to the JS regexes, paying close attention to lookarounds (`(?<!\{)` vs. lack thereof).