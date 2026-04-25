'use strict';
// Shared helpers for parity tests.
const path = require('path');
const fs = require('fs');
const os = require('os');
const { spawnSync } = require('child_process');
const Module = require('module');

// Resolve wikiparser-node from this package's node_modules so tests run
// against the local `extern_tokenizer` copy of `wikiparser-node`.
const wikiNmDir = path.resolve(__dirname, '..', 'node_modules');
const _origPaths = Module._nodeModulePaths;
Module._nodeModulePaths = function(from) {
  return [wikiNmDir, ...(_origPaths.call(this, from) || [])];
};

// Load the native patch exactly once from the local tests directory.
const patchPath = path.resolve(__dirname, 'native_token_patch.js');
const patch = require(patchPath);

const { Token } = require(path.join(wikiNmDir, 'wikiparser-node', 'dist', 'src', 'index.js'));
const proto = Token.prototype;
const Parser = require(path.join(wikiNmDir, 'wikiparser-node', 'dist', 'index.js'));

if (!proto || !proto.__orig_parse) {
  console.error('Original JS parse() not saved on prototype; aborting');
  process.exit(0);
}

const MAX_STAGE = 11;
const LAST_SAMPLE_PATH = '/tmp/wiki_latest_test_input.txt';
const PERF_LOG_PATH = '/tmp/wikitext_perf.txt';
const DEFAULT_WIKI_CONFIG = path.resolve(__dirname, '..', '..', 'node_modules', 'wikiparser-node', 'config', 'enwiki.json');

if (!process.env.WIKI_CONFIG) {
  process.env.WIKI_CONFIG = DEFAULT_WIKI_CONFIG;
}

Parser.config = process.env.WIKI_CONFIG;

function writeLatestSampleCheckpoint(wikitext, opts) {
  const payload = [
    '# latest native_binding parity input',
    `# timestamp: ${new Date().toISOString()}`,
    `# include: ${Boolean(opts && opts.include)}`,
    `# tidy: ${Boolean(opts && opts.tidy)}`,
    '',
    String(wikitext),
    '',
  ].join('\n');

  /* Overwrite on every sample so the file always contains the latest attempted input. */
  fs.writeFileSync(LAST_SAMPLE_PATH, payload, 'utf8');
}

function ensureDir(dir) {
  fs.mkdirSync(dir, { recursive: true });
}

function sanitizeName(s) {
  return String(s)
    .replace(/[^a-zA-Z0-9._-]+/g, '_')
    .replace(/^_+|_+$/g, '')
    .slice(0, 80) || 'sample';
}

function resolveSuiteName(opts = {}) {
  if (opts && typeof opts.name === 'string' && opts.name.trim()) {
    return sanitizeName(opts.name);
  }
  if (opts && typeof opts.suite === 'string' && opts.suite.trim()) {
    return sanitizeName(opts.suite);
  }
  return 'samples';
}

function writeTextFile(filePath, content) {
  fs.writeFileSync(filePath, String(content), 'utf8');
}

function appendTextFile(filePath, content) {
  fs.appendFileSync(filePath, String(content), 'utf8');
}

function nowNs() {
  return process.hrtime.bigint();
}

function nsToMsRounded(ns) {
  return Math.round(Number(ns) / 1e6);
}

function formatMs(ms) {
  const s = String(ms);
  return s.length >= 3 ? `${s}ms` : `${s.padStart(3, '0')}ms`;
}

function appendPerfLine(name, sampleIndex, jsTiming, nativeTiming) {
  const jsParseMs = Number(jsTiming && jsTiming.parseMs) || 0;
  const jsToStringMs = Number(jsTiming && jsTiming.toStringMs) || 0;
  if (jsParseMs <= 25 && jsToStringMs <= 25) {
    return;
  }

  const label = `${sanitizeName(name)}-${sampleIndex}`;
  const line = `${label} parse: ${formatMs(jsTiming.parseMs)} ${formatMs(nativeTiming.parseMs)}, toString: ${formatMs(jsTiming.toStringMs)} ${formatMs(nativeTiming.toStringMs)}\n`;
  appendTextFile(PERF_LOG_PATH, line);
}

function writeUnifiedDiff(expectedPath, gotPath, diffPath) {
  const res = spawnSync('diff', ['--unified=3', '--label', 'expected', '--label', 'got', expectedPath, gotPath], {
    encoding: 'utf8',
  });
  if (res.error) {
    writeTextFile(diffPath, `diff tool unavailable: ${res.error.message}\n`);
    return;
  }
  const body = res.stdout || '';
  /* GNU diff exits 1 when files differ, 0 when same. */
  if (res.status === 0) {
    writeTextFile(diffPath, 'No differences.\n');
  } else {
    writeTextFile(diffPath, body || (res.stderr || 'Diff generation failed.\n'));
  }
}

function astNodeSummary(node) {
  if (!node) return 'null';
  if (node.type === 'text') return `text(${JSON.stringify(node.data).slice(0, 40)})`;
  return `${node.type}${node.name != null ? `:${node.name}` : ''}`;
}

function analyzeAstDiff(expected, got) {
  const lines = [];
  const queue = [{ path: 'root', a: expected, b: got }];
  let found = null;

  while (queue.length > 0) {
    const cur = queue.shift();
    const { path: p, a, b } = cur;
    if (a == null || b == null) {
      found = { kind: 'null-mismatch', path: p, a, b };
      break;
    }

    const ta = a.type == null ? 'undefined' : String(a.type);
    const tb = b.type == null ? 'undefined' : String(b.type);
    if (ta !== tb) {
      found = { kind: 'type', path: p, ta, tb, a, b };
      break;
    }

    const na = a.name == null ? null : String(a.name);
    const nb = b.name == null ? null : String(b.name);
    if (na !== nb) {
      found = { kind: 'name', path: p, na, nb, a, b };
      break;
    }

    if (ta === 'text') {
      const da = a.data == null ? '' : String(a.data);
      const db = b.data == null ? '' : String(b.data);
      if (da !== db) {
        found = { kind: 'text-data', path: p, da, db, a, b };
        break;
      }
      continue;
    }

    const ach = Array.isArray(a.childNodes) ? a.childNodes : [];
    const bch = Array.isArray(b.childNodes) ? b.childNodes : [];
    if (ach.length !== bch.length) {
      found = { kind: 'child-count', path: p, aCount: ach.length, bCount: bch.length, a, b };
      break;
    }

    for (let i = 0; i < ach.length; i++) {
      queue.push({ path: `${p}.childNodes[${i}]`, a: ach[i], b: bch[i] });
    }
  }

  lines.push('AST analysis report');
  if (!found) {
    lines.push('No structural differences found by analyzer.');
    return lines.join('\n') + '\n';
  }

  lines.push(`first_mismatch.kind: ${found.kind}`);
  lines.push(`first_mismatch.path: ${found.path}`);
  if (found.kind === 'type') {
    lines.push(`expected.type: ${found.ta}`);
    lines.push(`got.type: ${found.tb}`);
  } else if (found.kind === 'name') {
    lines.push(`expected.name: ${found.na}`);
    lines.push(`got.name: ${found.nb}`);
  } else if (found.kind === 'text-data') {
    lines.push(`expected.text.hex: ${Buffer.from(found.da).toString('hex')}`);
    lines.push(`got.text.hex: ${Buffer.from(found.db).toString('hex')}`);
  } else if (found.kind === 'child-count') {
    lines.push(`expected.childCount: ${found.aCount}`);
    lines.push(`got.childCount: ${found.bCount}`);
  } else if (found.kind === 'null-mismatch') {
    lines.push(`expected.node: ${astNodeSummary(found.a)}`);
    lines.push(`got.node: ${astNodeSummary(found.b)}`);
  }

  lines.push(`expected.node.summary: ${astNodeSummary(found.a)}`);
  lines.push(`got.node.summary: ${astNodeSummary(found.b)}`);
  return lines.join('\n') + '\n';
}

function artifactRootDir() {
  const ts = new Date().toISOString().replace(/[:.]/g, '-');
  const dir = path.join(os.tmpdir(), `wiki_js_parity_${ts}`);
  ensureDir(dir);
  return dir;
}

/**
 * Recursively serialise a Token/AstText node to a plain object for comparison.
 * We capture type, name (where present), and childNodes recursively.
 * Text nodes expose their raw data string.
 */
function nodeToJSON(node) {
  if (!node) return null;
  if (node.type === 'text') {
    return { type: 'text', data: String(node.data) };
  }
  return {
    type: String(node.type),
    name: node.name != null ? String(node.name) : undefined,
    childNodes: Array.from(node.childNodes || []).map(nodeToJSON),
  };
}

/**
 * Run the full parse pipeline on a fresh Token and return
 * { text: string, tree: object }.
 * @param {string} wikitext
 * @param {Function} parseFn  Either proto.__orig_parse (JS baseline) or token.parse
 * @param {boolean} include
 * @param {boolean} tidy
 */
function runParse(wikitext, parseFn, include = false, tidy = false) {
  // Call the high-level Parser.parse exactly like production. When the
  // caller wants the original JS baseline (proto.__orig_parse), temporarily
  // restore proto.parse to the original implementation so Parser.parse
  // exercises the JS codepath.
  const saved = proto.parse;
  const needRestore = parseFn === proto.__orig_parse;
  if (needRestore) proto.parse = proto.__orig_parse;
  try {
    const parseStart = nowNs();
    const root = Parser.parse(wikitext, include, MAX_STAGE);
    const parseEnd = nowNs();
    const toStringStart = nowNs();
    const text = String(root.toString());
    const toStringEnd = nowNs();
    return {
      text,
      tree: nodeToJSON(root),
      timing: {
        parseMs: nsToMsRounded(parseEnd - parseStart),
        toStringMs: nsToMsRounded(toStringEnd - toStringStart),
      },
    };
  } finally {
    if (needRestore) proto.parse = saved;
  }
}

/**
 * Compare JS baseline vs native for one sample.
 * Returns true if both match.
 * Prints OK / FAIL to stdout with diagnostics on failure.
 */
function compareSample(wikitext, { include = false, tidy = false, name = 'samples', sampleIndex = 1 } = {}) {
  writeLatestSampleCheckpoint(wikitext, { include, tidy });

  const label = JSON.stringify(wikitext.slice(0, 70));

  let jsResult, nativeResult;

  try {
    jsResult = runParse(wikitext, proto.__orig_parse, include, tidy);
  } catch (e) {
    console.log('ERROR (JS)  ', label, e && e.message);
    return false;
  }

  try {
    nativeResult = runParse(wikitext, proto.parse, include, tidy);
  } catch (e) {
    console.log('ERROR (NAT) ', label, e && e.message);
    return false;
  }

  appendPerfLine(name, sampleIndex, jsResult.timing, nativeResult.timing);

  const textOk = jsResult.text === nativeResult.text;
  const treeJs = JSON.stringify(jsResult.tree);
  const treeNat = JSON.stringify(nativeResult.tree);
  const treeOk = treeJs === treeNat;

  const ok = textOk && treeOk;
  if (!ok) {
    console.log('FAIL', label);
  }

  if (!ok) {
    if (!compareSample._artifactDir) {
      compareSample._artifactDir = artifactRootDir();
      console.log('  artifacts root:', compareSample._artifactDir);
    }

    const suiteDir = path.join(compareSample._artifactDir, sanitizeName(name));
    ensureDir(suiteDir);

    const n = String(sampleIndex).padStart(4, '0');
    const expectedStringPath = path.join(suiteDir, `expected.string.${n}.txt`);
    const gotStringPath = path.join(suiteDir, `got.string.${n}.txt`);
    const stringDiffPath = path.join(suiteDir, `string.diff.${n}.txt`);
    const expectedJsonPath = path.join(suiteDir, `expected.tree.${n}.json`);
    const gotJsonPath = path.join(suiteDir, `got.tree.${n}.json`);
    const jsonDiffPath = path.join(suiteDir, `tree.diff.${n}.txt`);
    const astAnalysisPath = path.join(suiteDir, `ast.analysis.${n}.txt`);
    const inputPath = path.join(suiteDir, `input.wikitext.${n}.txt`);

    writeTextFile(inputPath, wikitext);
    writeTextFile(expectedStringPath, jsResult.text);
    writeTextFile(gotStringPath, nativeResult.text);
    writeUnifiedDiff(expectedStringPath, gotStringPath, stringDiffPath);

    writeTextFile(expectedJsonPath, JSON.stringify(jsResult.tree, null, 2) + '\n');
    writeTextFile(gotJsonPath, JSON.stringify(nativeResult.tree, null, 2) + '\n');
    writeUnifiedDiff(expectedJsonPath, gotJsonPath, jsonDiffPath);

    const astAnalysis = analyzeAstDiff(jsResult.tree, nativeResult.tree);
    writeTextFile(astAnalysisPath, astAnalysis);

    console.log('  expected string:', expectedStringPath);
    console.log('  got string     :', gotStringPath);
    console.log('  string diff    :', stringDiffPath);
    console.log('  expected json  :', expectedJsonPath);
    console.log('  got json       :', gotJsonPath);
    console.log('  tree diff      :', jsonDiffPath);
    console.log('  ast analysis   :', astAnalysisPath);
  }

  return ok;
}

/**
 * Run a list of samples, print a summary, exit 0 on all-pass or 2 on any failure.
 * @param {string[]} samples
 * @param {object}   [opts]
 */
function runTests(samples, opts = {}) {
  const include = Boolean(opts && opts.include);
  const tidy = Boolean(opts && opts.tidy);
  const suiteName = resolveSuiteName(opts);
  let passed = 0;
  let failed = 0;
  for (let i = 0; i < samples.length; i++) {
    const ok = compareSample(samples[i], {
      include,
      tidy,
      name: suiteName,
      sampleIndex: i + 1,
    });
    if (ok) passed++;
    else failed++;
  }
  const total = samples.length;
  console.log(`SUMMARY [${suiteName}] passed=${passed} failed=${failed} total=${total}`);
  if (failed > 0) {
    process.exit(2);
  }
  process.exit(0);
}

module.exports = { runTests, compareSample, nodeToJSON, Token, proto, patch, MAX_STAGE };