#!/usr/bin/env node
'use strict';

const fs = require('fs');
const path = require('path');

const repoRoot = path.resolve(__dirname, '..');
const origRoot = path.join(repoRoot, 'new-js');
const coverageDir = path.resolve(process.env.COVERAGE_DIR || path.join(repoRoot, '.artifacts', 'v8cov-origjs'));
const outDir = path.resolve(process.env.OUT_DIR || path.join(repoRoot, '.artifacts', 'origjs-usage'));

function walkFiles(dir) {
  const out = [];
  if (!fs.existsSync(dir)) return out;
  for (const entry of fs.readdirSync(dir, { withFileTypes: true })) {
    const full = path.join(dir, entry.name);
    if (entry.isDirectory()) {
      out.push(...walkFiles(full));
    } else if (entry.isFile()) {
      out.push(full);
    }
  }
  return out;
}

function toRel(p) {
  return path.relative(repoRoot, p).split(path.sep).join('/');
}

function fromCoverageUrl(url) {
  if (!url || typeof url !== 'string') return null;
  if (url.startsWith('file://')) {
    try {
      return decodeURIComponent(new URL(url).pathname);
    } catch {
      return null;
    }
  }
  if (path.isAbsolute(url)) return url;
  return null;
}

function csvEscape(v) {
  const s = String(v ?? '');
  if (s.includes('"') || s.includes(',') || s.includes('\n') || s.includes('\r')) {
    return '"' + s.replace(/"/g, '""') + '"';
  }
  return s;
}

function writeCsv(filePath, headers, rows) {
  const lines = [headers.map(csvEscape).join(',')];
  for (const row of rows) {
    lines.push(row.map(csvEscape).join(','));
  }
  fs.writeFileSync(filePath, lines.join('\n') + '\n', 'utf8');
}

function sortAndUniqRows(rows) {
  const sorted = [...rows].sort(
    (a, b) => String(a[0]).localeCompare(String(b[0])) || String(a[1]).localeCompare(String(b[1])) || Number(a[2]) - Number(b[2]) || Number(a[3]) - Number(b[3]),
  );
  const uniq = [];
  let lastKey = null;
  for (const row of sorted) {
    const key = row.join('\u0001');
    if (key !== lastKey) {
      uniq.push(row);
      lastKey = key;
    }
  }
  return uniq;
}

function extractRelativeSpecs(source) {
  const specs = new Set();
  const patterns = [
    /require\s*\(\s*['"]([^'"]+)['"]\s*\)/g,
    /import\s+(?:[^'";]*?\s+from\s+)?['"]([^'"]+)['"]/g,
    /export\s+\*\s+from\s+['"]([^'"]+)['"]/g,
    /export\s+\{[^}]*\}\s+from\s+['"]([^'"]+)['"]/g,
  ];
  for (const re of patterns) {
    let m;
    while ((m = re.exec(source)) !== null) {
      if (m[1] && m[1].startsWith('.')) specs.add(m[1]);
    }
  }
  return [...specs];
}

function resolveRelativeImport(fromFile, spec) {
  const base = path.resolve(path.dirname(fromFile), spec);
  const candidates = [
    base,
    `${base}.js`,
    `${base}.mjs`,
    `${base}.json`,
    path.join(base, 'index.js'),
    path.join(base, 'index.mjs'),
    path.join(base, 'index.json'),
  ];
  for (const c of candidates) {
    if (fs.existsSync(c) && fs.statSync(c).isFile()) return c;
  }
  return null;
}

function buildStaticGraph(origFiles) {
  const edgesOut = new Map();
  const edgesIn = new Map();

  for (const f of origFiles) {
    edgesOut.set(f, new Set());
    edgesIn.set(f, new Set());
  }

  for (const from of origFiles) {
    if (!/\.(js|mjs)$/i.test(from)) continue;
    let src = '';
    try {
      src = fs.readFileSync(from, 'utf8');
    } catch {
      continue;
    }
    for (const spec of extractRelativeSpecs(src)) {
      const to = resolveRelativeImport(from, spec);
      if (!to) continue;
      if (!to.startsWith(origRoot + path.sep) && to !== origRoot) continue;
      if (!edgesOut.has(to)) continue;
      edgesOut.get(from).add(to);
      edgesIn.get(to).add(from);
    }
  }

  return { edgesOut, edgesIn };
}

if (!fs.existsSync(origRoot)) {
  console.error('new-js folder not found:', origRoot);
  process.exit(1);
}
if (!fs.existsSync(coverageDir)) {
  console.error('Coverage folder not found:', coverageDir);
  console.error('Set COVERAGE_DIR or run coverage collection first.');
  process.exit(1);
}

fs.mkdirSync(outDir, { recursive: true });

const coverageFiles = walkFiles(coverageDir).filter((f) => fs.statSync(f).isFile());
const origFiles = walkFiles(origRoot).filter((f) => fs.statSync(f).isFile());
const { edgesOut, edgesIn } = buildStaticGraph(origFiles);

const fileStats = new Map();
const executedRows = [];
const nonExecutedRows = [];

for (const covFile of coverageFiles) {
  let payload;
  try {
    payload = JSON.parse(fs.readFileSync(covFile, 'utf8'));
  } catch {
    continue;
  }

  for (const result of payload.result || []) {
    const absPath = fromCoverageUrl(result.url);
    if (!absPath) continue;
    if (!(absPath === origRoot || absPath.startsWith(origRoot + path.sep))) continue;

    const rel = toRel(absPath);
    if (!fileStats.has(absPath)) {
      fileStats.set(absPath, { rel, total: 0, executed: 0, maxCount: 0 });
    }
    const stat = fileStats.get(absPath);

    for (const fn of result.functions || []) {
      const ranges = Array.isArray(fn.ranges) ? fn.ranges : [];
      const maxCount = ranges.reduce((m, r) => Math.max(m, Number(r.count || 0)), 0);
      const executed = maxCount > 0;
      const rootRange = ranges[0] || {};
      const row = [
        rel,
        fn.functionName || '(anonymous)',
        rootRange.startOffset ?? '',
        rootRange.endOffset ?? '',
        maxCount,
      ];

      stat.total += 1;
      stat.maxCount = Math.max(stat.maxCount, maxCount);
      if (executed) {
        stat.executed += 1;
        executedRows.push(row);
      } else {
        nonExecutedRows.push(row);
      }
    }
  }
}

const summaryRows = [];
for (const f of origFiles) {
  const rel = toRel(f);
  const s = fileStats.get(f) || { rel, total: 0, executed: 0, maxCount: 0 };
  const inbound = edgesIn.get(f) ? edgesIn.get(f).size : 0;
  const outbound = edgesOut.get(f) ? edgesOut.get(f).size : 0;
  const execPct = s.total > 0 ? Math.round((100 * s.executed) / s.total) : 0;
  summaryRows.push([
    rel,
    s.total,
    s.executed,
    s.total - s.executed,
    execPct,
    inbound,
    outbound,
    s.maxCount,
  ]);
}

summaryRows.sort((a, b) => Number(b[3]) - Number(a[3]));
const uniqueExecutedRows = sortAndUniqRows(executedRows);
const uniqueNonExecutedRows = sortAndUniqRows(nonExecutedRows);

const high = [];
const medium = [];
const low = [];
const entrypointAllowlist = new Set([
  'new-js/dist/index.js',
  'new-js/dist/base.js',
  'new-js/dist/src/index.js',
]);

for (const row of summaryRows) {
  const file = String(row[0]);
  const total = Number(row[1]);
  const executed = Number(row[2]);
  const unexecuted = Number(row[3]);
  const execPct = Number(row[4]);
  const inbound = Number(row[5]);

  if (!/\.(js|mjs)$/i.test(file)) continue;
  if (entrypointAllowlist.has(file)) continue;

  if (total > 0 && executed === 0 && inbound === 0) {
    high.push({ file, total, unexecuted, execPct, inbound, reason: 'No executed functions and no static inbound refs' });
  } else if (total > 0 && execPct <= 5 && inbound <= 1) {
    medium.push({ file, total, unexecuted, execPct, inbound, reason: 'Very low execution and minimal static inbound refs' });
  } else if (total > 0 && execPct <= 20) {
    low.push({ file, total, unexecuted, execPct, inbound, reason: 'Low execution share in current coverage set' });
  }
}

const candidatePath = path.join(outDir, 'candidate-delete-files.txt');
const lines = [];
lines.push('# Candidate delete list generated from runtime coverage + static relative-import graph');
lines.push('# Review manually before deleting. Dynamic/runtime reflection may hide true dependencies.');
lines.push('');

lines.push('## HIGH CONFIDENCE');
for (const c of high) {
  lines.push(`${c.file} | exec_pct=${c.execPct} | inbound=${c.inbound} | total_fn=${c.total} | unexecuted_fn=${c.unexecuted} | ${c.reason}`);
}
lines.push('');

lines.push('## MEDIUM CONFIDENCE');
for (const c of medium) {
  lines.push(`${c.file} | exec_pct=${c.execPct} | inbound=${c.inbound} | total_fn=${c.total} | unexecuted_fn=${c.unexecuted} | ${c.reason}`);
}
lines.push('');

lines.push('## LOW CONFIDENCE');
for (const c of low) {
  lines.push(`${c.file} | exec_pct=${c.execPct} | inbound=${c.inbound} | total_fn=${c.total} | unexecuted_fn=${c.unexecuted} | ${c.reason}`);
}
lines.push('');

fs.writeFileSync(candidatePath, lines.join('\n') + '\n', 'utf8');

writeCsv(
  path.join(outDir, 'executed-functions.csv'),
  ['file', 'function', 'startOffset', 'endOffset', 'maxCount'],
  uniqueExecutedRows,
);
writeCsv(
  path.join(outDir, 'non-executed-functions.csv'),
  ['file', 'function', 'startOffset', 'endOffset', 'maxCount'],
  uniqueNonExecutedRows,
);
writeCsv(
  path.join(outDir, 'file-summary.csv'),
  ['file', 'totalFunctions', 'executedFunctions', 'nonExecutedFunctions', 'execPct', 'staticInboundRefs', 'staticOutboundRefs', 'maxFunctionCount'],
  summaryRows,
);

console.log('Coverage files scanned:', coverageFiles.length);
console.log('new-js files discovered:', origFiles.length);
console.log('Output dir:', outDir);
console.log('Wrote:', path.join(outDir, 'executed-functions.csv'));
console.log('Wrote:', path.join(outDir, 'non-executed-functions.csv'));
console.log('Wrote:', path.join(outDir, 'file-summary.csv'));
console.log('Wrote:', candidatePath);
console.log('Candidates: high=' + high.length + ' medium=' + medium.length + ' low=' + low.length);
