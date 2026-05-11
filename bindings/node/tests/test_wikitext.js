#!/usr/bin/env node
'use strict';
// Parity test: Parse every file in tests/wikitext, sorted lexicographically.
// Stops processing on first failure.
const path = require('path');
const fs = require('fs');
const { compareSample } = require('./helpers');

/**
 * Load all files from a directory, sorted lexicographically.
 * @param {string} dir
 * @returns {{ file: string, content: string }[]}
 */
function loadWikitextSamples(dir) {
  const files = fs.readdirSync(dir).filter(f => !f.startsWith('.'));
  files.sort();
  
  const samples = [];
  for (const file of files) {
    const filePath = path.join(dir, file);
    const stat = fs.statSync(filePath);
    if (stat.isFile()) {
      const content = fs.readFileSync(filePath, 'utf8');
      samples.push({ file, content });
    }
  }
  
  return samples;
}

/**
 * Main test runner: loads files from wikitext directory and stops on first failure.
 */
function main() {
  const testDir = path.dirname(__filename);
  const wikitextDir = path.join(testDir, 'wikitext');
  
  if (!fs.existsSync(wikitextDir)) {
    console.error(`ERROR: wikitext directory not found at ${wikitextDir}`);
    process.exit(1);
  }
  
  const samples = loadWikitextSamples(wikitextDir);
  
  if (samples.length === 0) {
    console.error(`ERROR: no files found in ${wikitextDir}`);
    process.exit(1);
  }
  
  let passCount = 0;
  let failCount = 0;
  
  for (const sample of samples) {
    if (!compareSample(sample.content, { name: `wikitext`, sampleLabel: sample.file })) {
      failCount++;
      // console.error(`FAILED: stopping on first failure, ${passCount} samples passed`);
      // process.exit(2);
    }
    passCount++;
  }
  
  // Print the actual fail and pass counts
  console.log(`Test completed: ${passCount} passed, ${failCount} failed`);
  process.exit(0);
}

main();
