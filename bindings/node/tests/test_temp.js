#!/usr/bin/env node
'use strict';
// Parity test: templates, arguments, section headings (stage 1 – parseBraces).
const { runTests } = require('./helpers');

const tempTests = [
  `<ref name="Poznań">''{{cite web |url=http://www.poznan.pl/mim/public/publikacje/pages.html?co=list&id=19&ch=20&instance=1017&lang=pl |title=Poznań Official Website – Twin Towns|access-date=29 November 2008 |publisher={{fontcolor|Green|(in [[Polish language|{{fontcolor|Green|Polish}}]])}} [[copyright|]] 1998–2008 Urząd Miasta Poznania }}''</ref>`
];

if (process.argv[1] === __filename) {
    runTests(tempTests, { name: 'wiki-cast' });
}

module.exports = { tempTests };
