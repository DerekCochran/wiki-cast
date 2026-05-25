#!/usr/bin/env node
'use strict';
// Parity test: templates, arguments, section headings (stage 1 – parseBraces).
const { runTests } = require('./helpers');

const tempTests = [
  `{{refn|''StarHorse2: Fifth Expansion''
* Fiscal year ended 31 March 2010: ¥2.8&nbsp;billion<ref name="sega_mar10"/>
* Fiscal year ended 31 March 2011: ¥2&nbsp;billion<ref name="sega_mar11"/>
* Currency conversion:<ref name="xe_currency"/>
** ¥2.8 billion = $34.6039 million
** ¥2 billion = $24.7171 million
|group=n|name=StarHorse2}}`
];

if (process.argv[1] === __filename) {
    runTests(tempTests, { name: 'wiki-cast' });
}

module.exports = { tempTests };
