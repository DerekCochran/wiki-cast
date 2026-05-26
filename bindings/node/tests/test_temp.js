#!/usr/bin/env node
'use strict';
// Parity test: templates, arguments, section headings (stage 1 – parseBraces).
const { runTests } = require('./helpers');

const tempTests = [
  `<ref = name=lance>[https://www.theglobeandmail.com/sports/more-sports/lance-armstrong-building-deception-upon-lies/article4500335/] " Lance Armstrong: building deception upon lies" ''Globe and Mail'', BRUCE DOWBIGGIN.</ref>`
];

if (process.argv[1] === __filename) {
    runTests(tempTests, { name: 'wiki-cast' });
}

module.exports = { tempTests };
