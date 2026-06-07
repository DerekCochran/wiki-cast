#!/usr/bin/env node
'use strict';

const fs = require('fs');
const path = require('path');

const { castTests } = require(path.join(__dirname, '..', '..', 'bindings', 'node', 'tests', 'test_cast.js'));
const { parsoidTests } = require(path.join(__dirname, '..', '..', 'bindings', 'node', 'tests', 'test_parsoid.js'));

const outDir = path.join(__dirname, '..', 'input');

function ensureDir(dir) {
  fs.mkdirSync(dir, { recursive: true });
}

function toPayload(name, samples) {
  return {
    name,
    generatedAt: new Date().toISOString(),
    count: samples.length,
    samples: samples.map((wikitext, i) => ({
      sampleLabel: `${name}:${i + 1}`,
      wikitext: String(wikitext),
    })),
  };
}

function writeJson(filePath, payload) {
  fs.writeFileSync(filePath, JSON.stringify(payload, null, 2) + '\n', 'utf8');
  console.log(`wrote ${filePath} (${payload.count} samples)`);
}

function main() {
  ensureDir(outDir);

  const castPath = path.join(outDir, 'test_cast.samples.json');
  const parsoidPath = path.join(outDir, 'test_parsoid.samples.json');

  writeJson(castPath, toPayload('test_cast', castTests));
  writeJson(parsoidPath, toPayload('test_parsoid', parsoidTests));
}

main();
