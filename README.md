# Wiki CAST

This is a C implementation to create an abstract systax tree (AST) for wikipedia.  This common format can then be used to transform or read the data as needed.  It was created by using the [wikiparser-node](https://github.com/bhsd-harry/wikiparser-node) project as a template.  However, the design is quickly diverging based upon different needs.

## Implementation

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

# run Valgrind Callgrind (very slow, detailed)
sudo ./scripts/profile_wikitext.sh --callgrind --config node_modules/wikiparser-node/config/enwiki.json --testsdir tests/wikitext
callgrind_annotate /home/djc/git/wiki-cast/build_profile/callgrind.out --auto=yes
sudo chown djc:djc -R bindings/node/build

# run gprof
sudo ./scripts/profile_wikitext.sh --gprof --config node_modules/wikiparser-node/config/enwiki.json --testsdir tests/wikitext
sudo chown djc:djc -R bindings/node/build

```

- **Interpretation:** Start with the flamegraph to find heavy call stacks. For hotspots, run Callgrind on a smaller reproducer to inspect callers/callees in detail (open `callgrind.out` with `kcachegrind`). Use `perf report` and `perf script` for quick sampling summaries.

Notes: the helper script creates/uses `build_profile/` — you can delete it after profiling. Ensure `perf`, `valgrind`, and `gprof` are installed as needed.

