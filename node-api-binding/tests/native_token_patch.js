'use strict';
/**
 * native_token_patch.js
 *
 * Patches Token.prototype so that tests can compare:
 *   - proto.__orig_parse : original JavaScript implementation
 *   - proto.parse        : native C-backed implementation (via N-API addon)
 *
 * During scaffolding (stages not yet implemented in C) we fall back to the
 * JavaScript implementation for that stage so that all existing tests pass.
 * As each stage is ported, the corresponding case in the native dispatch
 * table is switched from JS fallback to the C binding.
 */
const path = require('path');
/* Prefer the repo-level `node_modules` directory (one level above `tests`).
 * This mirrors `helpers.js` which arranges module paths to include the
 * extern_tokenizer/node_modules directory. */
const wikiNmDir = path.resolve(__dirname, '..', 'node_modules');
const { Token } = require(path.join(wikiNmDir, 'wikiparser-node', 'dist', 'src', 'index.js'));
const proto = Token.prototype;

// ── Save the original JS parse method ────────────────────────────────────────
proto.__orig_parse = proto.parse;

// Try to load the optional native addon. If found and exports a `parse`
// function, replace `Token.prototype.parse` with the native implementation.
// Otherwise leave the JS implementation (scaffolding mode).
let native = null;
try {
	// Prefer release build, fall back to debug build.
	native = require(path.join(__dirname, '..', 'build', 'Release', 'native_binding.node'));
} catch (e) {
	try {
		native = require(path.join(__dirname, '..', 'build', 'Debug', 'native_binding.node'));
	} catch (e2) {
		native = null;
	}
}
if (native && typeof native.parse === 'function') {
	// store the native parse function separately and install it as the active
	// `parse` implementation on the prototype so tests can call either.
	proto.__native_parse = native.parse;
	proto.parse = native.parse;
}

module.exports = { Token, proto };
