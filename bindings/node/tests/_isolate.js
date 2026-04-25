'use strict';
const path = require('path');
const wikiNmDir = path.resolve(__dirname, '..', 'node_modules');
const { Token } = require(wikiNmDir + '/wikiparser-node/dist/src/index');
const { Shadow } = require(wikiNmDir + '/wikiparser-node/dist/util/debug');
const Parser = require(wikiNmDir + '/wikiparser-node/dist/index');
const native = require(path.resolve(__dirname, '..', 'build', 'Release', 'native_binding.node'));
const DEFAULT_WIKI_CONFIG = path.resolve(__dirname, '..', '..', 'node_modules', 'wikiparser-node', 'config', 'enwiki.json');
const proto = Token.prototype;

if (!process.env.WIKI_CONFIG) {
  process.env.WIKI_CONFIG = DEFAULT_WIKI_CONFIG;
}
Parser.config = process.env.WIKI_CONFIG;

proto.__orig_parse = proto.parse;
proto.parse = native.parse;

function nodeToJSON(node) {
  if (!node) return null;
  if (node.type === 'text') return { type: 'text', data: String(node.data) };
  return { type: String(node.type), name: node.name != null ? String(node.name) : undefined, childNodes: Array.from(node.childNodes || []).map(nodeToJSON) };
}

console.log('--- step1: JS parse ---');
proto.parse = proto.__orig_parse;
const jsRoot = Parser.parse('{{Template}}', false, 11);
const jsText = String(jsRoot.toString());
console.log('JS ok, text=', jsText);
proto.parse = native.parse;

console.log('--- step2: native parse ---');
const nativeRoot = Parser.parse('{{Template}}', false, 11);
console.log('native parse returned, type=', nativeRoot && nativeRoot.type);
const nativeText = String(nativeRoot.toString());
console.log('toString ok:', nativeText);
const nativeTree = nodeToJSON(nativeRoot);
console.log('nodeToJSON ok, type=', nativeTree && nativeTree.type);
console.log('ALL DONE');
