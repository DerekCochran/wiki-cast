#!/usr/bin/env node
'use strict';
// zhwiki-specific parity suite. This forces zhwiki config so language
// variants are enabled and converter behavior is fully exercised.
const path = require('path');

if (!process.env.WIKI_CONFIG) {
  process.env.WIKI_CONFIG = path.resolve(__dirname, '..', '..', '..', 'node_modules', 'wikiparser-node', 'config', 'zhwiki.json');
}

const { runTests } = require('./helpers');

runTests([
  // Baseline converter samples under a variant-enabled config
  '-{text}-',
  '-{zh-hans:简体;zh-hant:繁體}-',
  '-{zh:漢字;zh-hans:汉字;zh-hant:漢字}-',
  '-{A|zh-hans:简体;zh-hant:繁體}-',
  '-{R|raw text}-',
  '-{zh-hans:-{inner}-;zh-hant:outer}-',
  '{{Template|-{zh-hans:简;zh-hant:繁}-}}',
  "''italic'' -{ zh-hans:汉字 }- text",
  '-{}-',
  '-{|zh-hans:简体;zh-hant:繁體}-',
  // Unidirectional rule form
  '-{a=>zh-hans:简;zh-hant:繁}-',
], { name: 'zhwiki' });
