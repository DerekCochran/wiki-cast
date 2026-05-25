#!/usr/bin/env node
'use strict';
// Parity test: templates, arguments, section headings (stage 1 – parseBraces).
const { runTests } = require('./helpers');

const tempTests = [
  `<imagemap>
File:Mustelidae-01.jpg|250px|alt=Alt text 
rect 800 1066 1599 1594 [[Honey badger |Honey badger (''Mellivora capensis'')]] 
desc none
default [[Mustelidae]]
</imagemap>`
];

if (process.argv[1] === __filename) {
    runTests(tempTests, { name: 'wiki-cast' });
}

module.exports = { tempTests };
