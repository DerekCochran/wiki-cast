# extern_tokenizer

A C reimplementation of the `Parser.parse(wikitext)` pipeline from
`wikiparser-node` (v1.38.1). The goal is a byte-for-byte compatible parser
that produces the same token tree as the JavaScript implementation while
providing a standalone C library (`libwikitokenizer.a`) and test binaries.

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

On Debian/Ubuntu you can install the essentials with:

```bash
sudo apt-get update
sudo apt-get install build-essential cmake pkg-config libpcre2-dev libcjson-dev
```

**Build**

```bash
cd extern_tokenizer
mkdir -p build
cd build
cmake ..
make -j
```

This produces `libwikitokenizer.a` and test binaries (for example
`test_stage0`) in `extern_tokenizer/build`.

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

`wiki_parse()` is safe to call from multiple threads simultaneously.  To
minimise per-call heap allocations each thread maintains a pair of reusable
buffers (main and scratch) managed by `wiki_thread_buf_get()`:

- **main** — holds the `str_tidy()` copy of the raw wikitext input; the
  tidied text is then copied into the `WorkStr` for stage processing.
- **scratch** — reserved for future use inside parser stages.

Buffers are allocated on first use per thread and freed either when the thread
exits or when `wiki_thread_buf_finalize_all()` is called at shutdown.

**Shrink policy**

After a large document is parsed, the thread's main buffer may be very large.
At the start of each `wiki_parse()` call, if the buffer capacity exceeds the
shrink threshold AND the incoming input is smaller than the target size, the
buffer is freed and reallocated at the target size.  The four thresholds are
controlled by environment variables (all values in megabytes):

| Variable | Default | Meaning |
|---|---|---|
| `TOKENIZER_THREAD_BUFFER_MAIN_SHRINK_SIZE_MB` | `10` | If main buffer is larger than this, consider shrinking |
| `TOKENIZER_THREAD_BUFFER_MAIN_TARGET_SIZE_MB` | `5` | Shrink main buffer down to this size |
| `TOKENIZER_THREAD_BUFFER_SCRATCH_SHRINK_SIZE_MB` | `10` | If scratch buffer is larger than this, shrink it |
| `TOKENIZER_THREAD_BUFFER_SCRATCH_TARGET_SIZE_MB` | `5` | Shrink scratch buffer down to this size |

**Shutdown / finalize**

Call `wiki_thread_buf_finalize_all()` (declared in `include/thread_buffer.h`)
once when the library is no longer needed.  It frees the inner buffers of
every thread that ever called `wiki_parse()`.  The `ThreadBuffers` struct
itself is left alive in TLS and freed by the pthread destructor when each
thread exits, avoiding any cross-thread `free()` of memory the destructor
still references.

**Buffer ownership rule**

`thread_buffer` is the **sole owner** of `ThreadBuf` memory.  All allocation,
reallocation, and deallocation of `ThreadBuf.buf` must go through
`wiki_thread_buf_reserve()`.  No other code — parser stages, string utilities,
or helper functions — may call `malloc`, `realloc`, or `free` on a
`ThreadBuf.buf` pointer directly.

Functions that write into a thread buffer (e.g. `str_tidy_into()`) accept a
plain `char *` pointer and a capacity value.  They must assert that the
provided capacity is sufficient and then write into the buffer without
touching its allocation.  Callers are required to call
`wiki_thread_buf_reserve()` before passing the buffer to such functions.

**Development notes**

- New parser stages and token types should mirror the JS layout in
  `wikiparser-node/dist/` and be placed under `src/parser/` or `src/` as
  appropriate. Update `CMakeLists.txt` to include new sources.
- Regexes are compiled with PCRE2 once and cached in the `ParserConfig`.
- Working strings contain embedded NUL bytes due to sentinel markers — code
  uses explicit length-aware buffers rather than C NUL-terminated strings.
- When adding tokens, follow the accumulator index convention: record the
  accumulator index *before* pushing the new token so sentinels point at the
  correct slot.


## Sanitizers (ASan / UBSan / TSan)

We recommend running compiler sanitizers during development to catch memory
errors, undefined behavior, and data races early. Create a separate build
directory for each sanitizer (do not try to combine AddressSanitizer and
ThreadSanitizer in the same binary).

1) Undefined Behavior Sanitizer (UBSan)

```bash
cd extern_tokenizer
rm -rf build_ubsan && mkdir -p build_ubsan && cd build_ubsan
cmake \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_C_FLAGS="-g -O1 -fsanitize=undefined -fno-sanitize-recover=all -fno-omit-frame-pointer" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=undefined" ..
make -j

# run a C-stage test (point WIKI_CONFIG at your wikiparser package config)
WIKI_CONFIG=../../wikiparser-node-1.38.1/package/config/default.json ./test_stage0
```

2) ThreadSanitizer (TSan) — data race detection

```bash
cd extern_tokenizer
rm -rf build_tsan && mkdir -p build_tsan && cd build_tsan
cmake \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_C_FLAGS="-g -O1 -fsanitize=thread -fno-omit-frame-pointer" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=thread" ..
make -j

# run the same test binary under TSan
TSAN_OPTIONS="report_thread_leaks=1" \
  WIKI_CONFIG=../../wikiparser-node-1.38.1/package/config/default.json ./test_stage0
```

3) AddressSanitizer (ASan) — heap/stack buffer overflows, use separately

```bash
cd extern_tokenizer
rm -rf build_asan && mkdir -p build_asan && cd build_asan
cmake \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_C_FLAGS="-g -O1 -fsanitize=address -fno-omit-frame-pointer" \
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
