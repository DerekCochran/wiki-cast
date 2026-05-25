#!/usr/bin/env node
'use strict';
// Parity test: templates, arguments, section headings (stage 1 – parseBraces).
const { runTests } = require('./helpers');

const tempTests = [
  `<gallery>
File:Sydney Skyline (5620756401).jpg|The [[Sydney central business district]] in [[Sydney]]'s [[Western Suburbs, Sydney|western suburbs
</gallery>`
];

if (process.argv[1] === __filename) {
    runTests(tempTests, { name: 'wiki-cast' });
}

module.exports = { tempTests };
