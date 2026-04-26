**Native Binding**

- **Purpose**: Thin N-API shim that exposes a `parse()` entry point to JavaScript so tests can exercise the native C parser implementation in-process. Current scaffold implementation forwards to the saved JavaScript `__orig_parse` so the test harness remains green until C stages are implemented.

**Build**

- Prerequisites: `node` (>= 18), `npm`, `python3`, and a C build toolchain (`make`, `g++`). On Debian/Ubuntu: `sudo apt install build-essential python3`.

**Run tests (Node integration parity)**

- Run the lightweight native-binding parity tests:

```
cd bindings/node
node tests/run_all.js
```

- The test harness loads `bindings/node/tests/native_token_patch.js` which will attempt to `require('./build/Release/bindings/node.node')` (or Debug) and, when present, replace `Token.prototype.parse` with the native `parse` function. When the addon is absent or not exporting `parse`, tests fall back to the original JS implementation.

**Files of interest**

- Native shim: [bindings/node/src/addon.c](bindings/node/src/addon.c)
- Build: [bindings/node/binding.gyp](bindings/node/binding.gyp)
- Test harness & loader: [bindings/node/tests/native_token_patch.js](bindings/node/tests/native_token_patch.js)
- Parity tests runner: [bindings/node/tests/run_all.js](bindings/node/tests/run_all.js)

**Notes & Troubleshooting**

- If a test reports `Cannot find module 'wikiparser-node'`, either run `npm install` in `bindings/node` or create the symlink to the repo `node_modules` shown above.
- If `node-gyp` fails, ensure Python and build tools are available and that your Node.js version is supported by the installed node-gyp.
- The suite intentionally does not require the standalone CLI binary; the CLI-related test is excluded from this runner.

**Next steps for native integration**

- Implement C wrappers that call `wiki_parse()` directly and marshal results to JS (currently `addon.c` forwards to JS). Rebuild with `npm rebuild` and re-run `node tests/run_all.js`.
