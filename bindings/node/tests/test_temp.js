#!/usr/bin/env node
'use strict';
// Parity test: templates, arguments, section headings (stage 1 – parseBraces).
const { runTests } = require('./helpers');

const tempTests = [
  `{{Cite tweet |user=CAL_FIRE |number=1199881723929976833 |title=#CaveFire near Highway 154 in Santa Barbara County is 3,126 acres and 40% contained. Acreage reduced due to accurate mapping. Unified Command: @LosPadresNF @SBCFireInfo @CALFIRE_SLO Evacuation Information: https://twitter.com/sbsheriff&nbsp;https://fire.ca.gov/incidents/&nbsp;pic.twitter.com/BJa6z3YLYP|first=CAL|last=FIRE|date=November 27, 2019}}`
];

if (process.argv[1] === __filename) {
    runTests(tempTests, { name: 'wiki-cast' });
}

module.exports = { tempTests };
