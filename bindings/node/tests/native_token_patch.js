'use strict';
const path = require('path');

const newJs = path.resolve(__dirname, '..', '..', '..', 'new-js', 'dist', 'src', 'index.js');
const { Token: NewToken } = require(newJs);
const newProto = NewToken.prototype;
if ( !(newProto && typeof newProto.parse === 'function')) {
	throw new Error('Failed to load new JavaScript implementation of Tokenizer');
}


let nativeProto = null;
try {
	nativeProto = require(path.join(__dirname, '..', 'build', 'Release', 'wikiparser-node-c-tokenizer.node'));
} catch (e) {
	nativeProto = require(path.join(__dirname, '..', 'build', 'Debug', 'wikiparser-node-c-tokenizer.node'));
}
if ( !(nativeProto && typeof nativeProto.parse === 'function')) {
	throw new Error('Failed to load native C implementation of Tokenizer');
}

module.exports = { newProto, nativeProto };
