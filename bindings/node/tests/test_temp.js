#!/usr/bin/env node
'use strict';
// Parity test: templates, arguments, section headings (stage 1 – parseBraces).
const { runTests } = require('./helpers');

const tempTests = [
  `<gallery mode="packed" caption="Coloniae and Municipia image gallery">
  File:Arco Romano.jpg|Roman arch of [[Pax Iulia|]]''[[Pax Julia|Pax Iulia]]'' ([[Beja, Portugal|Beja]])
  </gallery>`
];

if (process.argv[1] === __filename) {
    runTests(tempTests, { name: 'wiki-cast' });
}

module.exports = { tempTests };
