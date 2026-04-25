**Native Binding**

- **Purpose**: Thin N-API shim that exposes a `parse()` entry point to JavaScript so tests can exercise the native C parser implementation in-process. Current scaffold implementation forwards to the saved JavaScript `__orig_parse` so the test harness remains green until C stages are implemented.

**Build**

- Prerequisites: `node` (>= 18), `npm`, `python3`, and a C build toolchain (`make`, `g++`). On Debian/Ubuntu: `sudo apt install build-essential python3`.
- To build the addon and install dependencies:

```
cd extern_tokenizer/native_binding
npm install
```

- If you prefer using the repository's already-installed `node_modules` (developing in-tree), point the tests at the repo `node_modules`:

```
# from repository root
ln -sfn ./extern_tokenizer/node_modules ./extern_tokenizer/native_binding/node_modules
```

**Run tests (Node integration parity)**

- Run the lightweight native-binding parity tests:

```
cd extern_tokenizer/native_binding
node tests/run_all.js
```

- The test harness loads `native_binding/tests/native_token_patch.js` which will attempt to `require('./build/Release/native_binding.node')` (or Debug) and, when present, replace `Token.prototype.parse` with the native `parse` function. When the addon is absent or not exporting `parse`, tests fall back to the original JS implementation.

**Files of interest**

- Native shim: [native_binding/src/addon.c](native_binding/src/addon.c)
- Build: [native_binding/binding.gyp](native_binding/binding.gyp)
- Test harness & loader: [native_binding/tests/native_token_patch.js](native_binding/tests/native_token_patch.js)
- Parity tests runner: [native_binding/tests/run_all.js](native_binding/tests/run_all.js)

**Notes & Troubleshooting**

- If a test reports `Cannot find module 'wikiparser-node'`, either run `npm install` in `native_binding` or create the symlink to the repo `node_modules` shown above.
- If `node-gyp` fails, ensure Python and build tools are available and that your Node.js version is supported by the installed node-gyp.
- The suite intentionally does not require the standalone CLI binary; the CLI-related test is excluded from this runner.

**Next steps for native integration**

- Implement C wrappers that call `wiki_parse()` directly and marshal results to JS (currently `addon.c` forwards to JS). Rebuild with `npm rebuild` and re-run `node tests/run_all.js`.
