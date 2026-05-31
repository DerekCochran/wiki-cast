# Wiki CAST

This is a C based abstract systax tree (AST) for wikipedia.  This common format can then be used to transform the data as needed.  It was created by using the [wikiparser-node](https://github.com/bhsd-harry/wikiparser-node) project as a template.  However, the design is quickly diverging based upon different needs.  The goal is to emulate the Parsoid parsing, including all known issues and problems.

## Pipe Dream

The long-term vision is to make this parser a single source of truth for
wikitext parsing across ecosystems.

- Use one high-performance C parser core instead of many divergent parser
  implementations.
- Expose a [stable AST contract](./config/wiki-cast.json) that language bindings and tools can consume.
- Let projects in Node, PHP, Python, and other runtimes share the same parsing
  behavior and edge-case handling.
- Reduce duplicated parser maintenance and avoid drift between "whacky"
  implementations.

If this matures, projects like Parsoid or other wiki tooling could integrate
the same core parser through native bindings or service interfaces.

## Short term goal

The process of creating Wiki-CAST generated a large amount of tests.  We can use these tests to compare output from wikiparser-node, parse_wiki_text and parsoid to find additional issues in all 4 products.

## Issues

This product is still in a pre-release phase with undocumented discrepencies between wikiparser-node.  The testing for other parsers has not been completed, or even started.  If you see an issue you want resolved, feel free to enter it.  However, I may not get to it with the other work needed.

## Performance

This is based on the [english wikipedia download](https://dumps.wikimedia.org/enwiki/latest/) for 7,176,400 samples.

| Parser          | Min  | Max     | Mean | Stnd Dev | p50 | p95  | p99  |
|-----------------|------|---------|------|----------|-----|------|------|
| wikiparser-node | 0.0  | 1711.0  | 8.41 |  21.0378 | 4.0 | 28.0 | 92.0 |
| wiki-cast       | 0.0  | 67640.0 | 1.29 |  32.9396 | 1.0 |  5.0 | 13.0 |

Speedup (mean first/second): 6.4837x  
Mean percent change (second vs first): -84.58%

**NOTE**: REDO!!  This is based on a debug build, but it does show the max has gone down.

| Parser          | Min  | Max     | Mean | Stnd Dev | p50 | p95  | p99   |
|-----------------|------|---------|------|----------|-----|------|-------|
| wikiparser-node | 0.0  | 2150.0  | 9.09 |  23.5031 | 4.0 | 31.0 | 101.0 |
| wiki-cast       | 0.0  | 368.0   | 2.51 |   5.2563 | 1.0 |  9.0 |  24.0 |

Speedup (mean first/second): 3.6187x
Mean percent change (second vs first): -72.37%

## Testing

This closely matches the wikiparser-node AST generated.  There is a [custom JSON creation](./bindings/node/src/addon.c) to match the shape of its AST.  On the last test executed, there were 523 wikitext articles out of 7,176,400 million that did not match.

This testing is just phase 1.  It gets WIKI-CAST to match at least one implementation and gives us a large number of tests to verify against other implementeations.  However, Parsoid is the source of truth.  We must [verify against it](https://github.com/DerekCochran/wiki-cast/issues/9) if we are to meet the pipe dream.

The testing against Parsoid will only be a "small" sample based on their tests and issues in implementing the english wikipedia.  We can also gain additional tests by comparing to other implementations, such as [parse_wiki_text](https://github.com/lovasoa/parse_wiki_text)

## Languages

The current implementation has only tested against the english wikipedia.  This meets my intiial needs, but to match the pipe dream, all languages must be verified.  This is a time consuming and costly effort.  With the right financial support, I hope to accomplish this goal.

## Implementation

This implementation was based on wikiparser-node and had fixes implemented incrementally with the smallest code change possible. This means the code does not follow KISS or DRY principles.  A refactor is needed for performance, simplicity and readability.

**High-level overview**

- The parser runs in a multi-stage pipeline (stages -1, 0..10). Each stage
  replaces matched substrings with opaque sentinel markers of the form
  `\0<n><ch>\x7F` and pushes the corresponding `Token` into an accumulator.
  After all stages the `build()` phase expands markers into a tree.

**Dependencies**

- C toolchain: `gcc`/`clang`, `make`, `cmake`, `pkg-config`
- cJSON (or libcjson) development headers
- ICU (ICU4C) development headers and libraries (`icu-uc` / `libicu`)

On Debian/Ubuntu you can install the essentials with:

```bash
sudo apt-get update
sudo apt-get install build-essential cmake pkg-config libcjson-dev libicu-dev
```

On macOS (Homebrew), install:

```bash
brew install cmake pkg-config cjson icu4c
```

The Node native addon build (`node-gyp`) also links ICU (`icuuc`, `icudata`),
so ICU is required for both CMake and Node addon builds.

**Build**

```bash
rm -rf build
mkdir -p build
```
The project defaults to a Release build for single-config generators. Examples:

Release (default):

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```

Debug (unoptimised, good for stepping/debugging):

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j"$(nproc)"
```

## Node native addon (bindings/node)

The Node native addon is built as part of the CMake `node_binding` target. The addon build is invoked with `node-gyp` using `--release` by default; when CMake is configured for `Debug` the addon will be built with `node-gyp --debug`.

To build the addon manually from `bindings/node`:

```bash
# Release
cd bindings/node
npx node-gyp rebuild --release

# Debug
npx node-gyp rebuild --debug
```

**Project layout**

- `include/` - public headers
- `src/` - implementation sources
- `src/parser/` - parser stage implementations (redirect, comment_and_ext, ...)
- `tests/` - C test harnesses

## Sanitizers (ASan / UBSan / TSan)

We recommend running compiler sanitizers during development to catch memory
errors, undefined behavior, and data races early. Create a separate build
directory for each sanitizer (do not try to combine AddressSanitizer and
ThreadSanitizer in the same binary).

1. Undefined Behavior Sanitizer (UBSan)

```bash
cd ~/git/wiki-cast
rm -rf build_ubsan && mkdir -p build_ubsan && cd build_ubsan
cmake \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_C_FLAGS="-g -O1 -march=native -mtune=native -fsanitize=undefined -fno-sanitize-recover=all -fno-omit-frame-pointer" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=undefined" ..
make -j

# run a C-stage test (point WIKI_CONFIG at your wikiparser package config)
cd ..
WIKI_CONFIG=./config/enwiki.json ./build_ubsan/test_wikitext
```

2. ThreadSanitizer (TSan) — data race detection

```bash
cd ~/git/wiki-cast
rm -rf build_tsan && mkdir -p build_tsan && cd build_tsan
cmake \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_C_FLAGS="-g -O1 -march=native -mtune=native -fsanitize=thread -fno-omit-frame-pointer" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=thread" ..
make -j

cd ..
TSAN_OPTIONS="report_thread_leaks=1" WIKI_CONFIG=./config/enwiki.json ./build_tsan/test_wikitext
```

3. AddressSanitizer (ASan) — heap/stack buffer overflows, use separately

```bash
cd ~/git/wiki-cast
rm -rf build_asan && mkdir -p build_asan && cd build_asan
cmake \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_C_FLAGS="-g -O1 -march=native -mtune=native -fsanitize=address -fno-omit-frame-pointer" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address" ..
make -j

cd ..
WIKI_CONFIG=./config/enwiki.json ./build_asan/test_wikitext
```

## Profiling 

- **Tooling:** `perf` (recommended), `valgrind` (callgrind), `gprof` (fallback).
- **Script:** `scripts/profile_wikitext.sh` — builds with profiling flags and runs the chosen profiler.

- **Quick start:** clone FlameGraph (optional) and run the helper:

```bash
# clone FlameGraph for flamegraph generation (optional)
git clone https://github.com/brendangregg/FlameGraph FlameGraph

# run perf (default — may require sudo)
sudo ./scripts/profile_wikitext.sh --perf --config node_modules/wikiparser-node/config/enwiki.json --testsdir tests/wikitext
sudo chown djc:djc -R bindings/node/build
sudo chown djc:djc -R build_profile

# run Valgrind Callgrind (very slow, detailed)
sudo ./scripts/profile_wikitext.sh --callgrind --config node_modules/wikiparser-node/config/enwiki.json --testsdir tests/wikitext
callgrind_annotate /home/djc/git/wiki-cast/build_profile/callgrind.out --auto=yes
sudo chown djc:djc -R bindings/node/build
sudo chown djc:djc -R build_profile

# run gprof
sudo ./scripts/profile_wikitext.sh --gprof --config node_modules/wikiparser-node/config/enwiki.json --testsdir tests/wikitext
sudo chown djc:djc -R bindings/node/build
sudo chown djc:djc -R build_profile

```

- **Interpretation:** Start with the flamegraph to find heavy call stacks. For hotspots, run Callgrind on a smaller reproducer to inspect callers/callees in detail (open `callgrind.out` with `kcachegrind`). Use `perf report` and `perf script` for quick sampling summaries.

Notes: the helper script creates/uses `build_profile/` — you can delete it after profiling. Ensure `perf`, `valgrind`, and `gprof` are installed as needed.

