#!/usr/bin/env node
'use strict';
// Parity test: templates, arguments, section headings (stage 1 – parseBraces).
const { runTests } = require('./helpers');

const tempTests = [
  `<ref name="Eaton-sep" |pages=286 |date=July 2025}}<ref name="Eaton 2004">{{cite book |last=Eaton |first=Richard M. |title=Temple desecration and Muslim states in medieval India |date=2004 |publisher=Hope India Publications |isbn=978-8178710273 |location=Gurgaon |pages=31–49 |quote=For, while  }}</ref>`
];

if (process.argv[1] === __filename) {
    runTests(tempTests, { name: 'wiki-cast' });
}

module.exports = { tempTests };
