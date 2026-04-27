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
const fs = require('fs');
/* Resolve the exact Token implementation to patch.
 * Prefer the installed `wikiparser-node/dist/src/index.js` used by helpers.js
 * so Parser.parse() and Token.prototype share the same module instance.
 * Keep orig-js candidates as fallback for local scaffolding. */
function resolveTokenModulePath() {
	const candidates = [
		path.resolve(__dirname, '..', 'node_modules', 'wikiparser-node', 'dist', 'src', 'index.js'),
		path.resolve(__dirname, '..', '..', '..', 'node_modules', 'wikiparser-node', 'dist', 'src', 'index.js'),
		path.resolve(__dirname, '..', '..', '..', 'orig-js', 'dist', 'src', 'index.js'),
		path.resolve(__dirname, '..', 'orig-js', 'dist', 'src', 'index.js'),
		path.resolve(__dirname, '..', '..', '..', 'orig-js', 'src', 'src', 'index.js'),
		path.resolve(__dirname, '..', 'orig-js', 'src', 'src', 'index.js'),
		path.resolve(__dirname, '..', '..', '..', 'orig-js', 'src', 'index.js'),
		path.resolve(__dirname, '..', 'orig-js', 'src', 'index.js'),
	];

	for (const file of candidates) {
		if (fs.existsSync(file)) return file;
	}

	throw new Error('Unable to locate Token module (expected wikiparser-node/dist/src/index.js or orig-js fallback)');
}

const tokenModulePath = resolveTokenModulePath();
const { Token } = require(tokenModulePath);
const proto = Token.prototype;

// ── Save the original JS parse method ────────────────────────────────────────
proto.__orig_parse = proto.parse;

// Try to load the optional native addon. If found and exports a `parse`
// function, replace `Token.prototype.parse` with the native implementation.
// Otherwise leave the JS implementation (scaffolding mode).
let native = null;
try {
	// Prefer the addon artifact node-gyp actually rebuilds.
	native = require(path.join(__dirname, '..', 'build', 'Release', 'wikiparser-node-c-tokenizer.node'));
} catch (e) {
	try {
		native = require(path.join(__dirname, '..', 'build', 'Release', 'bindings/node.node'));
	} catch (e2) {
		try {
			native = require(path.join(__dirname, '..', 'build', 'Debug', 'wikiparser-node-c-tokenizer.node'));
		} catch (e3) {
			native = require(path.join(__dirname, '..', 'build', 'Debug', 'bindings/node.node'));
		}
	}
}
if (native && typeof native.parse === 'function') {
	// store the native parse function separately and install it as the active
	// `parse` implementation on the prototype so tests can call either.
	proto.__native_parse = native.parse;
	proto.parse = native.parse;
}

module.exports = { Token, proto };
