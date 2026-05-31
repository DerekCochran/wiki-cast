#!/usr/bin/env node
'use strict';
const fs = require('fs');
const { runTests } = require('./helpers');

process.env.WIKI_STAGE_LOG_DIR = process.env.WIKI_STAGE_LOG_DIR || '/tmp/wiki_native_stage_compare';

const tempTests = [
  fs.readFileSync('/tmp/wiki_latest_failed.txt', 'utf8')
];

if (process.argv[1] === __filename) {
    runTests(tempTests, { name: 'wiki-cast' });
}

module.exports = { tempTests };
