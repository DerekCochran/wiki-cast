#!/usr/bin/env node
'use strict';
// Parity test: templates, arguments, section headings (stage 1 – parseBraces).
const { runTests } = require('./helpers');

const tempTests = [
  `<imagemap>
Image:Menthol synthesis.png|

rect 6 14 131 92 [[myrcene]]
rect 136 46 201 63 [[diethylamine]]
rect 468 110 628 180 [[citronellal]]
rect 387 112 458 135 [[zinc bromide]]
rect 95 97 223 209 [[menthol]]

desc bottom-left
#Notes:
#Details on the new coding for clickable images is here: [[mw:Extension:ImageMap]]
#[https://web.archive.org/web/20080327003154/http://tools.wikimedia.de/~dapete/ImageMapEdit/ImageMapEdit.html?en This image editor] was used.
</imagemap>`
];

if (process.argv[1] === __filename) {
    runTests(tempTests, { name: 'wiki-cast' });
}

module.exports = { tempTests };
