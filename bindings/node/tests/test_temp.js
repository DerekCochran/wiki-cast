#!/usr/bin/env node
'use strict';
// Parity test: templates, arguments, section headings (stage 1 – parseBraces).
const { runTests } = require('./helpers');

const tempTests = [
  `
{| class="wikitable" style="float:right; clear:right; text-align:center; margin-left:1em;"
|-
!colspan="3"| BQP algorithm (1 run)
|-
! {{diagonal split header|Correct<br />answer|Answer<div style{{=}}"padding-left:4em;">produced</div>}}
! {{yes}}
! {{no}}
|-
!colspan="3"| BQP algorithm (''k'' runs)
|-
! {{diagonal split header|Correct<br />answer|<div style{{=}}"padding-left:4em;">Answer</div>produced}}
! {{yes}}
! {{no}}
|-
|colspan="3" style="font-size:85%"|for some constant ''c'' > 0
|}
`,
];

if (process.argv[1] === __filename) {
    runTests(tempTests, { name: 'wiki-cast' });
}

module.exports = { tempTests };
