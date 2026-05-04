#!/usr/bin/env node
'use strict';

const path = require('path');
const Parser = require(path.resolve(__dirname, '..', '..', '..', 'new-js', 'dist', 'index.js'));
const { compareSample } = require('./helpers.js');

const ROOT = path.resolve(__dirname, '..', '..', '..');
const ENWIKI = path.join(ROOT, 'config', 'enwiki.json');
const JAWIKI = path.join(ROOT, 'config', 'jawiki.json');

const issues = [
  {
    id: 'A',
    title: 'Page context not propagated',
    configPath: ENWIKI,
    candidates: [
      { sample: '[[/Sub]]', page: 'Parent' },
      { sample: '[[/Sub|display text]]', page: 'Parent' },
      { sample: '[[../Child]]', page: 'Grand/Parent' },
      { sample: '[[#Section]]', page: 'Parent' },
      { sample: '{{Template}} and [[/Subpage]]', page: 'Parent' },
    ],
  },
  {
    id: 'G',
    title: 'Quote bold/italic state flags',
    configPath: ENWIKI,
    candidates: [
      { sample: "; A'''B''' : C" },
      { sample: "; outer '''bold : text''' end" },
      { sample: "; '''x''' : y" },
      { sample: "; a '''b : c''' d" },
      { sample: "; '''a''' '''b''' : c" },
    ],
  },
  {
    id: 'H',
    title: 'Fullwidth double-underscore handling',
    configPath: JAWIKI,
    candidates: [
      { sample: '＿＿目次＿＿' },
      { sample: 'text ＿＿目次＿＿ more text' },
      { sample: '  ＿＿目次＿＿  ' },
    ],
  },
  {
    id: 'I',
    title: 'Heading trailing unicode whitespace',
    configPath: ENWIKI,
    candidates: [
      { sample: '== Heading ==\u00A0' },
      { sample: '== Heading ==\u3000' },
      { sample: '== Heading ==\u2009' },
    ],
  },
];

function runCandidate(issue, candidate, idx) {
  const originalConfig = Parser.config;
  const originalParse = Parser.parse;
  const originalLog = console.log;
  const originalErr = console.error;

  try {
    Parser.config = issue.configPath;

    if (typeof candidate.page === 'string') {
      const forcedPage = candidate.page;
      Parser.parse = function patchedParse(wikitext, include, maxStage) {
        return originalParse.call(this, wikitext, forcedPage, include, maxStage);
      };
    }

    console.log = () => {};
    console.error = () => {};

    return compareSample(candidate.sample, {
      name: `probe-${issue.id}`,
      sampleIndex: idx + 1,
      sampleLabel: `[${issue.id}] ${candidate.sample.replace(/\n/g, '\\n').slice(0, 120)}`,
    });
  } finally {
    console.log = originalLog;
    console.error = originalErr;
    Parser.parse = originalParse;
    Parser.config = originalConfig;
  }
}

const summary = [];

for (const issue of issues) {
  console.log(`\n=== Issue ${issue.id}: ${issue.title} ===`);
  let foundMismatch = false;
  let firstMismatch = null;

  issue.candidates.forEach((candidate, idx) => {
    const ok = runCandidate(issue, candidate, idx);
    const status = ok ? 'MATCH' : 'MISMATCH';
    const pagePart = candidate.page ? ` page=${JSON.stringify(candidate.page)}` : '';
    console.log(`  [${idx + 1}] ${status}${pagePart} sample=${JSON.stringify(candidate.sample)}`);

    if (!ok && !foundMismatch) {
      foundMismatch = true;
      firstMismatch = candidate;
    }
  });

  summary.push({
    issue: issue.id,
    foundMismatch,
    firstMismatch,
  });
}

console.log('\n=== Probe Summary ===');
for (const row of summary) {
  if (row.foundMismatch) {
    console.log(`Issue ${row.issue}: FOUND mismatch with ${JSON.stringify(row.firstMismatch)}`);
  } else {
    console.log(`Issue ${row.issue}: no mismatch found in provided candidates`);
  }
}

const unresolved = summary.filter((x) => !x.foundMismatch).map((x) => x.issue);
if (unresolved.length > 0) {
  console.log(`\nNo reproducible mismatch yet for: ${unresolved.join(', ')}`);
  process.exitCode = 1;
}
