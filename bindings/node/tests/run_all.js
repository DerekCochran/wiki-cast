#!/usr/bin/env node
'use strict';
// Run all parity tests and print a summary.
const { spawnSync } = require('child_process');
const path = require('path');

const tests = [
  'test_hr_and_double_underscore.js',
  'test_braces.js',
  'test_comment_and_ext.js',
  'test_converter.js',
  'test_external_links.js',
  'test_html.js',
  'test_image_ns.js',
  'test_links.js',
  'test_list.js',
  'test_magic_links.js',
  'test_quotes.js',
  'test_redirect.js',
  'test_table_attrs_ref.js',
  'test_table_sweep.js',
  'test_table.js',
  //'test_zhwiki.js',
  'test_pipeline.js',
];

const dir = __dirname;
let passed = 0, failed = 0;

for (const t of tests) {
  const result = spawnSync(process.execPath, [path.join(dir, t)], {
    encoding: 'utf8',
    stdio: 'pipe',
  });
  const ok = result.status === 0;
  if (ok) {
    passed++;
    console.log('PASS', t);
  } else {
    failed++;
    console.log('FAIL', t);
    if (result.stdout) process.stdout.write(result.stdout.split('\n').map(l => '  ' + l).join('\n'));
    if (result.stderr) process.stderr.write(result.stderr.split('\n').map(l => '  ' + l).join('\n'));
  }
}

console.log(`\n${passed} passed, ${failed} failed out of ${tests.length} test files.`);
process.exit(failed > 0 ? 2 : 0);