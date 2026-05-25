#!/usr/bin/env node
'use strict';
// Parity test: templates, arguments, section headings (stage 1 – parseBraces).
const { runTests } = require('./helpers');

const tempTests = [
  `{| class=wikitable
|+ Occitan words and their French, Catalan and Spanish cognates
|-
!  scope="col" rowspan=2 | English
!! scope="col" colspan=2 | Cognate of French
!! scope="col" colspan=3 | Cognate of Catalan and Spanish
|-
!  scope="col" | Occitan
!! scope="col" | French
!! scope="col" | Occitan
!! scope="col" | Catalan
!! scope="col" | Spanish
|-
| broom || style{{=}}"background: Gainsboro" | {{lang|oc|balaja}} || {{lang|fr|balai}} || style{{=}}"background: Gainsboro" | {{lang|oc|escoba}} || {{lang|ca|escombra}} || {{lang|es|escoba}}
|-
|}`
];

if (process.argv[1] === __filename) {
    runTests(tempTests, { name: 'wiki-cast' });
}

module.exports = { tempTests };
