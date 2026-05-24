Take a long time to think about it.  They must be a line by line exact match.  perform an exhaustive step-by-step analysis. Break down the problem into its smallest components, evaluate potential edge cases, and challenge your own initial assumptions. Use a 'Thinking' section for this internal monologue before providing your final response.


## Evaluate simdjson

https://github.com/luizperes/simdjson_nodejs


## Implement stringzilla

https://github.com/ashvardanian/stringzilla

Override standard libs
https://github.com/ashvardanian/StringZilla/blob/main/c/stringzilla.c

#define SZ_USE_SIMD 1 // Enables SIMD acceleration (AVX, NEON, etc.)
#include "stringzilla.h"

swap libc functions for StringZilla

Compiler Flags: To get the "Zilla" speed, make sure you compile with optimization flags (like -O3) and architecture-specific flags (like -march=native) so the compiler allows the SIMD instructions to execute.

It uses "String Views" (sz_str_view_t), which are essentially a pointer and a length. This prevents unnecessary malloc calls and strcpy overhead.

patterns:
- Replace scalar marker-byte filter loops (`\0`, `\x7F`, CRLF handling) with StringZilla length-aware byte operations.
- Prefer `sz_split_any` / `sz_join` or `sz_find_byteset` plus join-style logic for raw byte filtering when the input may contain embedded NULs.
- Example: convert `str_remove_comment()` from a manual `\0`/digit/`\x7F` scanner to StringZilla boundary search and skip logic.

Original:
```c
char *str_remove_comment(const char *s, size_t len, size_t *out_len)
{
    char *result = malloc(len + 1);
    assert(result);
    size_t j = 0, i = 0;
    while (i < len) {
        if ((unsigned char)s[i] == '\0') {
            /* look for: \0 <digits> [cn] \x7F */
            size_t k = i + 1;
            while (k < len && s[k] >= '0' && s[k] <= '9') k++;
            if (k < len && (s[k] == 'c' || s[k] == 'n') &&
                k + 1 < len && (unsigned char)s[k + 1] == '\x7F') {
                /* skip the entire marker */
                i = k + 2;
                continue;
            }
        }
        result[j++] = s[i++];
    }
    result[j] = '\0';
    if (out_len) *out_len = j;
    return result;
}
```

StringZilla-style:
```c
sz_string_view_t text = {s, len};
char *result = malloc(len + 1);
assert(result);
size_t j = 0;

while (text.length > 0) {
    sz_cptr_t marker_start = sz_find_byte(text.start, text.length, (sz_cptr_t)"\0");
    if (!marker_start) {
        memcpy(result + j, text.start, text.length);
        j += text.length;
        break;
    }

    size_t prefix_len = (size_t)(marker_start - text.start);
    memcpy(result + j, text.start, prefix_len);
    j += prefix_len;

    sz_cptr_t marker_end = sz_find_byte(marker_start + 1, text.length - prefix_len - 1, (sz_cptr_t)"\x7F");
    if (!marker_end) {
        /* malformed marker: emit the rest literally */
        memcpy(result + j, marker_start, text.length - prefix_len);
        j += text.length - prefix_len;
        break;
    }

    /* Validate the sentinel contents before skipping. */
    size_t inner_len = (size_t)(marker_end - (marker_start + 1));
    bool valid = inner_len >= 2 &&
                 (marker_start[inner_len] == 'c' || marker_start[inner_len] == 'n');
    for (size_t u = 0; valid && u + 1 < inner_len; u++) {
        if (marker_start[1 + u] < '0' || marker_start[1 + u] > '9') valid = false;
    }

    if (!valid) {
        memcpy(result + j, marker_start, (size_t)(marker_end - marker_start) + 1);
        j += (size_t)(marker_end - marker_start) + 1;
        text.start = marker_end + 1;
        text.length -= prefix_len + ((size_t)(marker_end - marker_start) + 1);
        continue;
    }

    /* Skip the valid marker and continue. */
    text.start = marker_end + 1;
    text.length -= prefix_len + ((size_t)(marker_end - marker_start) + 1);
}

result[j] = '\0';
if (out_len) *out_len = j;
```

const isNode = typeof process !== 'undefined' && 
               process.versions != null && 
               process.versions.node != null;

let sz;

if (isNode) {
    // Local Base: Use the high-performance native addon
    try {
        sz = require('stringzilla');
    } catch (e) {
        console.error("StringZilla native addon not found, falling back.");
    }
} else {
    // Browser Base: Use Wasm or pure JS implementation
    // sz = await loadStringZillaWasm(); 
}


## Tokens Arena Double Storage

The `tokens` arena in `ThreadBuffers` currently stores each byte of token text up to
**twice**:

1. **Stage-parse time** — parsers such as `build_ext_inner` append raw
   (sentinel-containing) inner text to the arena so tokens hold a stable pointer before
   the stage buffer is overwritten for the next stage.
2. **Build time** — `build_from_str` reads those sentinel-containing strings, strips the
   sentinels, and appends the resolved plain-text sub-segments back into the same arena.
   The sentinel-form copy becomes dead (unreferenced) but wastes space.

**Current workaround:** The arena is pre-reserved to `2 × input_len` bytes at the start
of each parse call (`src/parse.c`, `wiki_parse_with_page`) so it never reallocates and
invalidates outstanding `const char *` text pointers held by tokens.

**Correct fix:** During stage parsing, store inner text as a heap-owned buffer
(`text_owned = true`, plain `malloc`) rather than in the tokens arena. `build_from_str`
then reads from the owned buffer and appends resolved plain-text segments to the arena
only once. The owned buffer is freed when `build_from_str` replaces the token's
children. This reduces arena usage to `≤ input_len` bytes and allows the reservation
to drop back to `1 × input_len`.

## Guard PCRE

Guard all PCRE calls with memchr or strstr for performance.

## Cache PCRE

Verify every PCRE call is cached

## Simple PCRE to lexical parser

For a simple PCRE, convert to a small lexical parser.  Do not build a state machine, unless it shows up a LOT.
typedef struct {
    const char *start_delim;
    const char *end_delim;
    const char *forbidden_start; // e.g., "{" to handle the (?<!\{)
    const char *forbidden_end;   // e.g., "}" to handle the (!\})
} DelimiterRule;

const char* generic_scanner(const char *buf, size_t len, DelimiterRule rule) {
    const char *ptr = buf;
    const char *end = buf + len;

    while ((ptr = memmem(ptr, end - ptr, rule.start_delim, strlen(rule.start_delim)))) {
        // 1. Check Negative Lookbehind: (?<!\{)
        if (rule.forbidden_start) {
            if (ptr > buf && *(ptr - 1) == rule.forbidden_start[0]) {
                ptr++; continue;
            }
        }

        const char *content_start = ptr + strlen(rule.start_delim);
        const char *match_end = memmem(content_start, end - content_start, 
                                       rule.end_delim, strlen(rule.end_delim));

        if (match_end) {
            // 2. Check Negative Lookahead: (!\})
            if (rule.forbidden_end) {
                const char *after = match_end + strlen(rule.end_delim);
                if (after < end && *after == rule.forbidden_end[0]) {
                    ptr = after; continue;
                }
            }
            
            // 3. Validation Logic (Optional)
            // Here you can check for your forbidden \n[ or \n= 
            return match_end + strlen(rule.end_delim);
        }
        ptr++;
    }
    return NULL;
}

## Split out the headers.

Public API	include/wikiparser-node-c-parser/parser.h
Private/Internal	src/*.h	#include "local_header.h"

## Python Wrapper

Write a wrapper to use C.

This involves writing a "wrapper" file in C that includes <Python.h>. You manually convert Python objects into C types, call your function, and convert the result back into a Python object.

## Node Wrapper.

Use CMake with a tool called cmake-js.

## Official builds

Tool to use: prebuildify, cibuildwheel

How it works: It creates a folder called prebuilds/ inside your package containing all the different versions. When a user installs your package, it automatically picks the right file for their OS.

name: Build and Publish
on:
  push:
    tags: ['v*'] # Only run when you push a version tag like v1.0.0

jobs:
  # JOB 1: Build Python Wheels
  build_wheels:
    runs-on: ${{ matrix.os }}
    strategy:
      matrix:
        os: [ubuntu-latest, windows-latest, macos-latest]
    steps:
      - uses: actions/checkout@v4
      - name: Build wheels
        uses: pypa/cibuildwheel@v2.16.2
        env:
          CIBW_BEFORE_BUILD: make  # Runs your Makefile to prep the C lib
      - uses: actions/upload-artifact@v4
        with:
          path: ./wheelhouse/*.whl

  # JOB 2: Build Node Prebuilds
  build_node:
    runs-on: ${{ matrix.os }}
    strategy:
      matrix:
        os: [ubuntu-latest, windows-latest, macos-latest]
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-node@v4
      - run: npm install
      - run: npx prebuildify --napi --strip
      - uses: actions/upload-artifact@v4
        with:
          path: ./prebuilds/**


# Handle disambigues

1. How to Detect Them (The "Fast" Way)
Wikipedia uses a special "Behavior Switch" (a magic word) and specific templates. In your C-scanner, look for these strings:

__DISAMBIG__: This is the most reliable technical marker. If this string is anywhere in the wikitext, it is officially a disambiguation page.

{{Disambiguation}} (or aliases like {{Dab}}, {{Disambig}}): Usually at the bottom.

2. Why they are "Data Gold" (Don't just ignore)
Even if you don't want them as entities, the content inside these pages is a structured list of "Potential Matches." For your graph, you can extract a hasVariant or resolvesTo relationship:

Subject: Mercury (Ambiguous Term)

Predicate: canReferTo

Objects: [Mercury (planet), Mercury (element), Mercury (mythology), Mercury (programming language)]

This is the secret sauce for Entity Linking. When your parser later finds a raw link like [[Mercury]] in a random article, your graph already knows there are 4+ possible meanings.