#!/usr/bin/env node
'use strict';
// Parity test: bold/italic quote balancing (stage 6 – parseQuotes).
const { runTests } = require('./helpers');

runTests([
  // Simple italic
  "This is ''italic'' text.",
  // Simple bold
  "This is '''bold''' text.",
  // Bold and italic combined
  "This is '''''bold italic''''' text.",
  // Italic then bold on the same line
  "''italic'' and '''bold''' on one line.",
  // Unbalanced italic (odd count) – MediaWiki balancing rules apply
  "''unbalanced italic",
  // Four apostrophes – treated as one leading apostrophe + bold
  "''''four apostrophes''''",
  // Six apostrophes – five active + one leading
  "''''''six apostrophes''''''",
  // Bold spanning across italic
  "'''bold ''both''' italic''",
  // Multiple quote runs on one line
  "''a'' '''b''' ''c'''",
  // First bold-start after a space (affects balancing heuristic)
  "text '''bold with space before''' text",
  // Line with only apostrophes and no other text
  "'' '''",
  // Quotes inside a template argument (handled by quotes parser per line)
  "{{Template|''arg''}}",
  // Nested link with italic display text
  "[[Page|''italic display'']]",
], { name: 'quotes' });