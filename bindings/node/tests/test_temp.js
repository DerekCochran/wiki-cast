#!/usr/bin/env node
'use strict';
// Parity test: templates, arguments, section headings (stage 1 – parseBraces).
const { runTests } = require('./helpers');

const tempTests = [
  `* ﻿{{cite web |url= https://reason.com/2019/10/31/its-that-old-looney-tuner-lysander-spooner/ |title= It's That Old Looney Tuner, Lysander Spooner |last= Bagge |first= Peter |date= November 2019 |website= Reason Magazine |format= Comic strip| access-date= }}`
];

if (process.argv[1] === __filename) {
    runTests(tempTests, { name: 'wiki-cast' });
}

module.exports = { tempTests };
