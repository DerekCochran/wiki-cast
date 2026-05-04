# wikiparser-node C Tokenizer

This project contains a drop in replacement for the [wikiparser-node](https://github.com/bhsd-harry/wikiparser-node) Token class. It overrides the parse and toString to use a C based tokenizer. It will have extra functions to assist with normalizing the data for AI.

## Purpose

Parsing the 7 million wikipedia articles is a slow process. Using a straight C tokenizer speeds it up. Initial testing shows the increase can be significan for processing 7 million records. Below is a sample, but there are opportunities for additional performance enhancements.

```text
article-1 parse: 159ms 054ms, toString: 003ms 000ms
article-2 parse: 060ms 020ms, toString: 002ms 000ms
article-3 parse: 037ms 014ms, toString: 001ms 000ms
article-4 parse: 140ms 086ms, toString: 004ms 000ms
article-5 parse: 046ms 030ms, toString: 001ms 000ms
```

Along with parsing, a straight C implementation is portable. I could be used with Python, the de-facto language for AI.

The end goal is to use this parser to normalize WikiPedia for use with AI, to create a knowledge graph and normalized text for training and RAG.

## Implementation

This is a duplicate of the functionality of [wikiparser-node](https://github.com/bhsd-harry/wikiparser-node). There are prompts to use in vs Code Copilot to convert to C and keep parity with the JS implementation. The goal is to not re-invent the wheel, but provide a cross language high performance tokenizer.

**High-level overview**

- The parser runs in a multi-stage pipeline (stages -1, 0..10). Each stage
  replaces matched substrings with opaque sentinel markers of the form
  `\0<n><ch>\x7F` and pushes the corresponding `Token` into an accumulator.
  After all stages the `build()` phase expands markers into a tree.
- The implementation uses a no-copy substring view for efficiency, PCRE2 for
  regexes, and `cJSON` for config parsing.

**Dependencies**

- C toolchain: `gcc`/`clang`, `make`, `cmake`, `pkg-config`
- PCRE2 (development headers) — recommended `pcre2` >= 10.30
- cJSON (or libcjson) development headers
- ICU (ICU4C) development headers and libraries (`icu-uc` / `libicu`)

On Debian/Ubuntu you can install the essentials with:

```bash
sudo apt-get update
sudo apt-get install build-essential cmake pkg-config libpcre2-dev libcjson-dev libicu-dev
```

On macOS (Homebrew), install:

```bash
brew install cmake pkg-config pcre2 cjson icu4c
```

The Node native addon build (`node-gyp`) also links ICU (`icuuc`, `icudata`),
so ICU is required for both CMake and Node addon builds.

**Build**

```bash
cd extern_tokenizer
mkdir -p build
cd build
cmake ..
make -j
```

This produces `libwikitokenizer.a` and test binaries (for example
`test_stage0`) in `build`.

**Run tests**

Some tests require the wikiparser JSON config. Point `WIKI_CONFIG` at the
`package/config/default.json` from the `wikiparser-node` package and run the
stage test binary:

```bash
  ./test_stage0
```

Example (repository-local path used during development):

```bash
  ./test_stage0
```

To override the default log level, set `TOKENIZER_LOG_LEVEL` before running a test binary. The default is `INFO`.

```bash
  ./test_stage0
```

Expected output (stage-0):

```
PASS [plain text]
PASS [comment closed]
PASS [comment unclosed]
PASS [ref tag simple]
PASS [noinclude tag]
PASS [redirect]

6/6 tests passed
```

**Project layout**

- `include/` - public headers (token, accum, build, config, string utilities)
- `src/` - implementation sources
- `src/parser/` - parser stage implementations (redirect, comment_and_ext, ...)
- `tests/` - C test harnesses

**Thread-local buffer pool**

`wiki_parse()` is safe to call from multiple threads simultaneously. To
minimise per-call heap allocations each thread maintains one reusable main
buffer plus a pool of reusable scratch buffers managed by `wiki_thread_buf_get()`
and the scratch acquire/release helpers:

- **main** — holds the `str_tidy()` copy of the raw wikitext input; the
  tidied text is then copied into the `WorkStr` for stage processing.
- **scratch pool** — temporary parser workspaces acquired on demand so nested
  helper functions do not step on each other's scratch storage.

Buffers are allocated on first use per thread and freed either when the thread
exits or when `wiki_thread_buf_finalize_all()` is called at shutdown.

**Shrink policy**

After a large document is parsed, the thread's main buffer may be very large.
At the start of each `wiki_parse()` call, if the buffer capacity exceeds the
shrink threshold AND the incoming input is smaller than the target size, the
buffer is freed and reallocated at the target size. The four thresholds are
controlled by environment variables (all values in megabytes):

| Variable                                         | Default | Meaning                                                |
| ------------------------------------------------ | ------- | ------------------------------------------------------ |
| `TOKENIZER_THREAD_BUFFER_MAIN_SHRINK_SIZE_MB`    | `10`    | If main buffer is larger than this, consider shrinking |
| `TOKENIZER_THREAD_BUFFER_MAIN_TARGET_SIZE_MB`    | `5`     | Shrink main buffer down to this size                   |
| `TOKENIZER_THREAD_BUFFER_SCRATCH_SHRINK_SIZE_MB` | `10`    | If a scratch buffer is larger than this, shrink it     |
| `TOKENIZER_THREAD_BUFFER_SCRATCH_TARGET_SIZE_MB` | `5`     | Shrink scratch buffers down to this size               |

**Shutdown / finalize**

Call `wiki_thread_buf_finalize_all()` (declared in `include/thread_buffer.h`)
once when the library is no longer needed. It frees the inner buffers of
every thread that ever called `wiki_parse()`. The `ThreadBuffers` struct
itself is left alive in TLS and freed by the pthread destructor when each
thread exits, avoiding any cross-thread `free()` of memory the destructor
still references.

**Buffer ownership rule**

`thread_buffer` is the **sole owner** of `ThreadBuf` memory. All allocation,
reallocation, and deallocation of `ThreadBuf.buf` must go through
`wiki_thread_buf_reserve()`. No other code — parser stages, string utilities,
or helper functions — may call `malloc`, `realloc`, or `free` on a
`ThreadBuf.buf` pointer directly.

Functions that write into a thread buffer (e.g. `str_tidy_into()`) accept a
plain `char *` pointer and a capacity value. They must assert that the
provided capacity is sufficient and then write into the buffer without
touching its allocation. Callers are required to call
`wiki_thread_buf_reserve()` before passing the buffer to such functions.

**Development notes**

- New parser stages and token types should mirror the JS layout in
  `wikiparser-node/dist/` and be placed under `src/parser/` or `src/` as
  appropriate. Update `CMakeLists.txt` to include new sources.
- Regexes are compiled with PCRE2 once and cached in the `ParserConfig`.
- Working strings contain embedded NUL bytes due to sentinel markers — code
  uses explicit length-aware buffers rather than C NUL-terminated strings.
- When adding tokens, follow the accumulator index convention: record the
  accumulator index _before_ pushing the new token so sentinels point at the
  correct slot.

## Sanitizers (ASan / UBSan / TSan)

We recommend running compiler sanitizers during development to catch memory
errors, undefined behavior, and data races early. Create a separate build
directory for each sanitizer (do not try to combine AddressSanitizer and
ThreadSanitizer in the same binary).

1. Undefined Behavior Sanitizer (UBSan)

```bash
cd extern_tokenizer
rm -rf build_ubsan && mkdir -p build_ubsan && cd build_ubsan
cmake \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_C_FLAGS="-g -O1 -march=native -mtune=native -fsanitize=undefined -fno-sanitize-recover=all -fno-omit-frame-pointer" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=undefined" ..
make -j

# run a C-stage test (point WIKI_CONFIG at your wikiparser package config)
WIKI_CONFIG=../../wikiparser-node-1.38.1/package/config/default.json ./test_stage0
```

2. ThreadSanitizer (TSan) — data race detection

```bash
cd extern_tokenizer
rm -rf build_tsan && mkdir -p build_tsan && cd build_tsan
cmake \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_C_FLAGS="-g -O1 -march=native -mtune=native -fsanitize=thread -fno-omit-frame-pointer" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=thread" ..
make -j

# run the same test binary under TSan
TSAN_OPTIONS="report_thread_leaks=1" \
  WIKI_CONFIG=../../wikiparser-node-1.38.1/package/config/default.json ./test_stage0
```

3. AddressSanitizer (ASan) — heap/stack buffer overflows, use separately

```bash
cd extern_tokenizer
rm -rf build_asan && mkdir -p build_asan && cd build_asan
cmake \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_C_FLAGS="-g -O1 -march=native -mtune=native -fsanitize=address -fno-omit-frame-pointer" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address" ..
make -j

WIKI_CONFIG=../../wikiparser-node-1.38.1/package/config/default.json ./test_stage0
```

Notes:

- Use separate build directories per sanitizer to avoid conflicting runtimes.
- If you want the sanitizer to abort on first error, add `-fno-sanitize-recover=all`
  to the `CMAKE_C_FLAGS` (useful when debugging with gdb).
- For Node-level parity tests that spawn the native CLI, make sure the test
  harness invokes the binary built in the matching `build_*` directory.

## Create stage files for AI

**C code**

```bash
cd ~/git/wikiparser-node-c-tokenizer/src

# Helper: append one C/H file into an output file
append_c_file() {
  local file="$1" out="$2"
  if [ ! -f "$file" ]; then
    echo "WARNING: missing C file: $file" >&2
    return
  fi
  local clean_name="${file#./}"
  echo "<$clean_name>" >> "$out"
  if [[ "$file" == *.h ]]; then
    clang-format -style=file "$file" | \
      sed -E 's/[[:space:]]*([;=,&|\-\+{}])[[:space:]]*/\1/g' | \
      sed ':a; /[{};&|,]$/ {N; s/\n[[:space:]]*//; ba}' >> "$out"
  else
    gcc -E -P -fpreprocessed -dD "$file" | \
      clang-format -style=file | \
      sed -E 's/[[:space:]]*([;=,&|\-\+{}])[[:space:]]*/\1/g' | \
      sed ':a; /[{};&|,]$/ {N; s/\n[[:space:]]*//; ba}' >> "$out"
  fi
  echo -e "</$clean_name>\n" >> "$out"
}

declare -A C_STAGE_FILES
C_STAGE_FILES[0]="parse.c token.c accum.c ../include/thread_buffer.h string_util.c config.c build.c parser/redirect.c parser/comment_and_ext.c parser/braces.c parser/links.c parser/html.c parser/external_links.c parser/magic_links.c title.c"
C_STAGE_FILES[1]="parse.c token.c accum.c ../include/thread_buffer.h string_util.c config.c build.c parser/braces.c"
C_STAGE_FILES[2]="parse.c token.c accum.c ../include/thread_buffer.h string_util.c config.c build.c parser/html.c"
C_STAGE_FILES[3]="parse.c token.c accum.c ../include/thread_buffer.h string_util.c config.c build.c parser/table.c td.c tr.c table_token.c"
C_STAGE_FILES[4]="parse.c token.c accum.c ../include/thread_buffer.h string_util.c config.c build.c parser/hr_and_double_underscore.c"
C_STAGE_FILES[5]="parse.c token.c accum.c ../include/thread_buffer.h string_util.c config.c build.c parser/links.c parser/link.c title.c parser/braces.c parser/comment_and_ext.c parser/html.c parser/quotes.c parser/external_links.c parser/magic_links.c"
C_STAGE_FILES[6]="parse.c token.c accum.c ../include/thread_buffer.h string_util.c config.c build.c parser/quotes.c"
C_STAGE_FILES[7]="parse.c token.c accum.c ../include/thread_buffer.h string_util.c config.c build.c parser/external_links.c"
C_STAGE_FILES[8]="parse.c token.c accum.c ../include/thread_buffer.h string_util.c config.c build.c parser/magic_links.c"
C_STAGE_FILES[9]="parse.c token.c accum.c ../include/thread_buffer.h string_util.c config.c build.c parser/list.c"
C_STAGE_FILES[10]="parse.c token.c accum.c ../include/thread_buffer.h string_util.c config.c build.c parser/converter.c"

for stage in "${!C_STAGE_FILES[@]}"; do
  out="../data/c-stage_${stage}.txt"
  rm -f "$out"
  for file in ${C_STAGE_FILES[$stage]}; do
    append_c_file "$file" "$out"
  done
  echo "Written: $out"
done
```

**JS code**

```bash
cd ~/git/wikiparser-node-c-tokenizer/new-js

# Helper: append one JS file into an output file
append_js_file() {
  local file="$1" out="$2"
  if [ ! -f "$file" ]; then
    echo "WARNING: missing JS file: $file" >&2
    return
  fi
  local clean_name="${file#./}"
  echo "<$clean_name>" >> "$out"
  cat "$file" | minify --js >> "$out"
  echo -e "</$clean_name>\n" >> "$out"
}

declare -A JS_STAGE_FILES
JS_STAGE_FILES[0]="dist/parser/redirect.js dist/base.js dist/index.js dist/src/index.js dist/util/constants.js dist/util/string.js dist/src/redirect.js dist/src/link/redirectTarget.js dist/src/link/base.js dist/src/link/index.js dist/src/tagPair/index.js dist/src/nowiki/noinclude.js dist/src/nowiki/base.js dist/src/nowiki/index.js dist/mixin/padded.js dist/mixin/fixed.js dist/lib/rect.js dist/util/lint.js"
JS_STAGE_FILES[1]="dist/parser/braces.js dist/base.js dist/index.js dist/src/index.js dist/util/constants.js dist/util/string.js dist/src/transclude.js dist/src/heading.js dist/src/arg.js dist/src/tagPair/translate.js dist/src/tagPair/include.js dist/src/tagPair/ext.js dist/src/tagPair/index.js dist/src/nowiki/noinclude.js dist/src/nowiki/comment.js dist/src/nowiki/index.js dist/src/tag/index.js dist/src/tag/tvar.js dist/src/tag/html.js dist/mixin/clone.js dist/mixin/noEscape.js dist/mixin/gapped.js dist/mixin/fixed.js dist/lib/rect.js dist/util/lint.js dist/src/converter.js dist/src/converterFlags.js dist/src/converterRule.js"
JS_STAGE_FILES[2]="dist/parser/html.js dist/base.js dist/index.js dist/src/index.js dist/util/constants.js dist/util/string.js dist/src/attributes.js dist/src/tag/html.js dist/src/tag/index.js dist/mixin/cached.js dist/mixin/attributesParent.js dist/lib/rect.js dist/util/lint.js"
JS_STAGE_FILES[3]="dist/parser/table.js dist/base.js dist/index.js dist/src/index.js dist/util/constants.js dist/util/string.js dist/src/table/index.js dist/src/table/tr.js dist/src/table/td.js dist/src/table/base.js dist/src/table/trBase.js dist/mixin/cached.js dist/mixin/attributesParent.js dist/util/lint.js dist/lib/rect.js"
JS_STAGE_FILES[4]="dist/parser/hrAndDoubleUnderscore.js dist/base.js dist/index.js dist/src/index.js dist/util/constants.js dist/util/string.js dist/src/nowiki/hr.js dist/src/nowiki/doubleUnderscore.js dist/src/nowiki/base.js dist/src/nowiki/index.js dist/src/nowiki/comment.js dist/mixin/sol.js dist/mixin/syntax.js dist/mixin/hidden.js dist/mixin/padded.js dist/util/lint.js"
JS_STAGE_FILES[5]="dist/parser/links.js dist/base.js dist/index.js dist/src/index.js dist/util/constants.js dist/util/string.js dist/src/link/index.js dist/src/link/base.js dist/src/link/file.js dist/src/link/category.js dist/src/link/galleryImage.js dist/src/link/redirectTarget.js dist/src/link/categorytree.js dist/src/tagPair/index.js dist/src/tag/index.js dist/src/magicLink.js dist/mixin/padded.js dist/mixin/cached.js dist/mixin/singleLine.js dist/util/lint.js dist/lib/rect.js"
JS_STAGE_FILES[6]="dist/parser/quotes.js dist/base.js dist/index.js dist/src/index.js dist/util/constants.js dist/util/string.js dist/src/nowiki/quote.js dist/src/nowiki/base.js dist/src/nowiki/index.js dist/mixin/syntax.js dist/mixin/cached.js dist/util/lint.js"
JS_STAGE_FILES[7]="dist/parser/externalLinks.js dist/base.js dist/index.js dist/src/index.js dist/util/constants.js dist/util/string.js dist/src/magicLink.js dist/src/link/index.js dist/src/link/base.js dist/mixin/padded.js"
JS_STAGE_FILES[8]="dist/parser/magicLinks.js dist/base.js dist/index.js dist/src/index.js dist/util/constants.js dist/util/string.js dist/src/magicLink.js dist/src/link/index.js dist/src/link/base.js dist/mixin/syntax.js dist/mixin/clone.js"
JS_STAGE_FILES[9]="dist/parser/list.js dist/base.js dist/index.js dist/src/index.js dist/util/constants.js dist/util/string.js dist/src/nowiki/list.js dist/src/nowiki/listBase.js dist/src/nowiki/dd.js dist/src/nowiki/base.js dist/src/nowiki/index.js dist/mixin/sol.js dist/mixin/syntax.js dist/util/lint.js"
JS_STAGE_FILES[10]="dist/parser/converter.js dist/base.js dist/index.js dist/src/index.js dist/util/constants.js dist/util/string.js dist/src/converter.js dist/src/converterFlags.js dist/src/converterRule.js dist/mixin/padded.js dist/mixin/noEscape.js dist/mixin/cached.js dist/util/lint.js"

for stage in "${!JS_STAGE_FILES[@]}"; do
  out="../data/js-stage_${stage}.txt"
  rm -f "$out"
  seen=()
  for file in ${JS_STAGE_FILES[$stage]}; do
    append_js_file "$file" "$out"
  done
  echo "Written: $out"
done
```
