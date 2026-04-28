'use strict';
// Shared helpers for parity tests.
const path = require('path');
const fs = require('fs');
const os = require('os');
const { spawnSync } = require('child_process');

const { newProto, nativeProto } = require('./native_token_patch.js');
const MAX_STAGE = 20;
const LAST_SAMPLE_PATH = '/tmp/wiki_latest_test_input.txt';
const PERF_LOG_PATH = '/tmp/wikitext_perf.txt';
const DEFAULT_WIKI_CONFIG = path.join(__dirname, '..', '..', '..', 'config', 'enwiki.json');

if (!process.env.WIKI_CONFIG) {
  process.env.WIKI_CONFIG = DEFAULT_WIKI_CONFIG;
}

newProto.config = process.env.WIKI_CONFIG;
nativeProto.config = process.env.WIKI_CONFIG;

function writeLatestSampleCheckpoint(wikitext, opts) {
  fs.writeFileSync(LAST_SAMPLE_PATH, wikitext, 'utf8');
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
function runParse(wikitext, parseFn, include = false, tidy = false, runLabel = 'parse') {
  proto = parseFn;
  const stageLogDir = process.env.WIKI_STAGE_LOG_DIR;
  let origConsoleLog, origConsoleError, logStream;
  if (stageLogDir) {
    try {
      ensureDir(stageLogDir);
      const file = path.join(stageLogDir, `${runLabel}.${Date.now()}.${Math.random().toString(36).slice(2,8)}.console.log`);
      logStream = fs.createWriteStream(file, { flags: 'a' });
      origConsoleLog = console.log;
      origConsoleError = console.error;
      console.log = (...args) => { try { origConsoleLog.apply(console, args); } catch (e) {} ; try { logStream.write(args.map(a => String(a)).join(' ') + '\n'); } catch (e) {} };
      console.error = (...args) => { try { origConsoleError.apply(console, args); } catch (e) {} ; try { logStream.write(args.map(a => String(a)).join(' ') + '\n'); } catch (e) {} };
    } catch (e) {
      /* best-effort */
    }
  }
  const parseStart = nowNs();
  const root = Parser.parse(wikitext, include, MAX_STAGE);
  const parseEnd = nowNs();
  const toStringStart = nowNs();
  const text = String(root.toString());
  const toStringEnd = nowNs();
  if (logStream) {
    try { logStream.end(); } catch (e) {}
  }
  if (origConsoleLog) {
    console.log = origConsoleLog;
  }
  if (origConsoleError) {
    console.error = origConsoleError;
  }
  return {
    text,
    tree: nodeToJSON(root),
    timing: {
      parseMs: nsToMsRounded(parseEnd - parseStart),
      toStringMs: nsToMsRounded(toStringEnd - toStringStart),
    },
  };
}

/**
 * Compare JS baseline vs native for one sample.
 * Returns true if both match.
 * Prints OK / FAIL to stdout with diagnostics on failure.
 */
function compareSample(wikitext, { include = false, tidy = false, name = 'samples', sampleIndex = 1, sampleLabel = null } = {}) {
  writeLatestSampleCheckpoint(wikitext, { include, tidy });

  const label = sampleLabel == null ? JSON.stringify(wikitext.slice(0, 70)) : String(sampleLabel);

  let jsResult, nativeResult;
  // Prepare a per-sample stage log directory and enable stage logging
  const stageDir = path.join(os.tmpdir(), `wiki_stage_${Date.now()}_${process.pid}_${Math.random().toString(36).slice(2,8)}`);
  ensureDir(stageDir);
  const prevStageDir = process.env.WIKI_STAGE_LOG_DIR;
  const prevStageFlag = process.env.WIKI_STAGE_LOG;
  process.env.WIKI_STAGE_LOG_DIR = stageDir;
  process.env.WIKI_STAGE_LOG = '1';

  try {
    try {
      jsResult = runParse(wikitext, newProto, include, tidy, 'js');
    } catch (e) {
      console.log('ERROR (JS)  ', label, e && e.message);
      return false;
    }

    try {
      nativeResult = runParse(wikitext, nativeProto, include, tidy, 'native');
    } catch (e) {
      console.log('ERROR (NAT) ', label, e && e.message);
      return false;
    }
  } finally {
    /* restore any previous env */
    if (prevStageDir === undefined) delete process.env.WIKI_STAGE_LOG_DIR; else process.env.WIKI_STAGE_LOG_DIR = prevStageDir;
    if (prevStageFlag === undefined) delete process.env.WIKI_STAGE_LOG; else process.env.WIKI_STAGE_LOG = prevStageFlag;
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

    writeTextFile(expectedJsonPath, JSON.stringify(jsResult.tree) + '\n');
    writeTextFile(gotJsonPath, JSON.stringify(nativeResult.tree) + '\n');
    writeUnifiedDiff(expectedJsonPath, gotJsonPath, jsonDiffPath);

    const astAnalysis = analyzeAstDiff(jsResult.tree, nativeResult.tree);
    writeTextFile(astAnalysisPath, astAnalysis);

    // Copy any stage logs collected into the suite artifact directory
    try {
      const stageLogsDst = path.join(suiteDir, 'stage-logs');
      ensureDir(stageLogsDst);
      const files = fs.readdirSync(stageDir || os.tmpdir());
      for (const f of files) {
        const src = path.join(stageDir, f);
        const dst = path.join(stageLogsDst, f);
        try { fs.copyFileSync(src, dst); } catch (e) { /* ignore */ }
      }
    } catch (e) {
      /* best-effort */
    }

    // Produce consolidated single-file artifacts for expected and got
    try {
      const expectedFullPath = path.join(suiteDir, `expected.full.${n}.txt`);
      const gotFullPath = path.join(suiteDir, `got.full.${n}.txt`);

      function appendHeader(fp, hdr) {
        fs.appendFileSync(fp, `==== ${hdr} ====` + '\n', 'utf8');
      }

      // Build expected.full
      try {
        fs.writeFileSync(expectedFullPath, '', 'utf8');
        appendHeader(expectedFullPath, 'INPUT');
        fs.appendFileSync(expectedFullPath, fs.readFileSync(inputPath, 'utf8') + '\n', 'utf8');

        appendHeader(expectedFullPath, 'EXPECTED STRING');
        fs.appendFileSync(expectedFullPath, fs.readFileSync(expectedStringPath, 'utf8') + '\n', 'utf8');

        appendHeader(expectedFullPath, 'EXPECTED JSON');
        fs.appendFileSync(expectedFullPath, fs.readFileSync(expectedJsonPath, 'utf8') + '\n', 'utf8');

        appendHeader(expectedFullPath, 'STAGE LOGS');
        const sl = fs.readdirSync(stageDir || os.tmpdir());
        for (const f of sl) {
          try {
            fs.appendFileSync(expectedFullPath, `-- ${f} --\n`, 'utf8');
            const data = fs.readFileSync(path.join(stageDir, f));
            fs.appendFileSync(expectedFullPath, data);
            if (!String(data).endsWith('\n')) fs.appendFileSync(expectedFullPath, '\n');
          } catch (e) {
            /* ignore per-file read errors */
          }
        }
      } catch (e) {
        /* best-effort */
      }

      // Build got.full
      try {
        fs.writeFileSync(gotFullPath, '', 'utf8');
        appendHeader(gotFullPath, 'INPUT');
        fs.appendFileSync(gotFullPath, fs.readFileSync(inputPath, 'utf8') + '\n', 'utf8');

        appendHeader(gotFullPath, 'GOT STRING');
        fs.appendFileSync(gotFullPath, fs.readFileSync(gotStringPath, 'utf8') + '\n', 'utf8');

        appendHeader(gotFullPath, 'GOT JSON');
        fs.appendFileSync(gotFullPath, fs.readFileSync(gotJsonPath, 'utf8') + '\n', 'utf8');

        appendHeader(gotFullPath, 'STAGE LOGS');
        const sl2 = fs.readdirSync(stageDir || os.tmpdir());
        for (const f of sl2) {
          try {
            fs.appendFileSync(gotFullPath, `-- ${f} --\n`, 'utf8');
            const data = fs.readFileSync(path.join(stageDir, f));
            fs.appendFileSync(gotFullPath, data);
            if (!String(data).endsWith('\n')) fs.appendFileSync(gotFullPath, '\n');
          } catch (e) {
            /* ignore */
          }
        }
      } catch (e) {
        /* best-effort */
      }
        // Print locations of consolidated artifacts and stage logs for easy triage
        try {
          console.log('  expected full  :', expectedFullPath);
          console.log('  got full       :', gotFullPath);
          console.log('  stage logs     :', path.join(suiteDir, 'stage-logs'));
        } catch (e) {
          /* best-effort */
        }
    } catch (e) {
      /* best-effort overall */
    }

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

module.exports = { runTests };