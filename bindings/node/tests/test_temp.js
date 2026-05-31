#!/usr/bin/env node
'use strict';
// Parity test: templates, arguments, section headings (stage 1 – parseBraces).
const { runTests } = require('./helpers');

const tempTests = [
  
`{{Infobox automobile
| name == Dodge Viper SRT-10 (ZB II) ==
| manufacturer = [[Chrysler LLC]]<br>[[Chrysler Group LLC]]
|'''Coupé:''' {{convert|47.6|in|mm|abbr=on}}
|'''Convertible:''' {{convert|48.6|in|mm|abbr=on}}
}}
| weight = {{ubl
|'''SRT-10:''' {{convert|3460|lb|kg|0|abbr=on}}
|'''ACR:''' {{convert|3408|lb|kg|abbr=on}}
}}
| predecessor = [[Dodge Viper (ZB I)]]
| successor = [[Dodge Viper (VX I)]]
}}`,

];

if (process.argv[1] === __filename) {
    runTests(tempTests, { name: 'wiki-cast' });
}

module.exports = { tempTests };
