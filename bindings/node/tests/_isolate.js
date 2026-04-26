'use strict';
const path = require('path');
const fs = require('fs');

function resolveWikiNodeModulesDir() {
  const candidates = [
    path.resolve(__dirname, '..', 'node_modules'),
    path.resolve(__dirname, '..', '..', '..', 'node_modules'),
  ];
  for (const dir of candidates) {
    const entry = path.join(dir, 'wikiparser-node', 'dist', 'src', 'index.js');
    if (fs.existsSync(entry)) return dir;
  }
  throw new Error('Unable to locate wikiparser-node in expected node_modules locations');
}

const wikiNmDir = resolveWikiNodeModulesDir();
const { Token } = require(wikiNmDir + '/wikiparser-node/dist/src/index');
const { Shadow } = require(wikiNmDir + '/wikiparser-node/dist/util/debug');
const Parser = require(wikiNmDir + '/wikiparser-node/dist/index');
function loadNative() {
  const candidates = [
    path.resolve(__dirname, '..', 'build', 'Release', 'wikiparser-node-c-tokenizer.node'),
    path.resolve(__dirname, '..', 'build', 'Debug', 'wikiparser-node-c-tokenizer.node'),
  ];
  for (const candidate of candidates) {
    if (fs.existsSync(candidate)) {
      return require(candidate);
    }
  }
  throw new Error('Unable to locate native addon build artifact');
}

const native = loadNative();
const DEFAULT_WIKI_CONFIG = path.join(wikiNmDir, 'wikiparser-node', 'config', 'enwiki.json');
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
