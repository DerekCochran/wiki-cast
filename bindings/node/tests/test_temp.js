#!/usr/bin/env node
'use strict';
// Parity test: templates, arguments, section headings (stage 1 – parseBraces).
const { runTests } = require('./helpers');

const tempTests = [
  `{{map caption |location_color=dark green|region=Europe |region_color=dark grey |subregion=the European Union |subregion_color=green |<br>legend=EU-Hungary.svg}}`
];

if (process.argv[1] === __filename) {
    runTests(tempTests, { name: 'wiki-cast' });
}

module.exports = { tempTests };
