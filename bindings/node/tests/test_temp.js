#!/usr/bin/env node
'use strict';
// Parity test: templates, arguments, section headings (stage 1 – parseBraces).
const { runTests } = require('./helpers');

const tempTests = [
  `[<!-- http://perso.univ-rennes1.fr/antoine.chambert-loir/DJVU/ -->https://www.irphe.fr/~clanet/otherpaperfile/articles/Galois/N0029062_PDF_1_84.pdf Œuvres Mathématiques]`
];

if (process.argv[1] === __filename) {
    runTests(tempTests, { name: 'wiki-cast' });
}

module.exports = { tempTests };
