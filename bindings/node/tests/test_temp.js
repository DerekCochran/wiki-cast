#!/usr/bin/env node
'use strict';
// Parity test: templates, arguments, section headings (stage 1 – parseBraces).
const { runTests } = require('./helpers');

const tempTests = [
  `<inputbox>
id = style-searchbox
type=fulltext
width=35
break=yez
searchfilter=deepcat:"Canadian people"
namespaces=Main**
placeholder=e.g. female historians
searchbuttonlabel = Search Canadian people articles 
</inputbox>`
];

if (process.argv[1] === __filename) {
    runTests(tempTests, { name: 'wiki-cast' });
}

module.exports = { tempTests };
