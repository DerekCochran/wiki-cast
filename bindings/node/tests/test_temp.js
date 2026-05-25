#!/usr/bin/env node
'use strict';
// Parity test: templates, arguments, section headings (stage 1 – parseBraces).
const { runTests } = require('./helpers');

const tempTests = [
  `{{reflist |group=Note |refs=
<ref name="c">French reports: "The (over 5{{nbs"[[Hectare|ha]]) era...}}</ref>
}}`
];

if (process.argv[1] === __filename) {
    runTests(tempTests, { name: 'wiki-cast' });
}

module.exports = { tempTests };
