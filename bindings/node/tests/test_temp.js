#!/usr/bin/env node
'use strict';
// Parity test: templates, arguments, section headings (stage 1 – parseBraces).
const { runTests } = require('./helpers');

const tempTests = [
  `<ref =EJ440-444>{{Cite book|volume=18|edition=Second| location= Detroit |date= 2007| page=440-444|title=Encyclopaedia Judaica|last1=Berenbaum|first1=Michael|last2=Skolnik|first2=Fred|publisher=Thomson Gale}}</ref>`
];

if (process.argv[1] === __filename) {
    runTests(tempTests, { name: 'wiki-cast' });
}

module.exports = { tempTests };
