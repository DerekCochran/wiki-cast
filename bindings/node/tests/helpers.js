'use strict';
// Shared helpers for parity tests.
const path = require('path');
const fs = require('fs');
const os = require('os');
const { spawnSync } = require('child_process');

const { newProto, nativeProto } = require('./native_token_patch.js');
const { ok } = require('assert');
const MAX_STAGE = 20;
let proto;
// Token constructor (derived from the new JS implementation's prototype)
const Token = newProto && newProto.constructor ? newProto.constructor : null;
// Parser module (top-level API)
const Parser = require(path.resolve(__dirname, '..', '..', '..', 'new-js', 'dist', 'index.js'));
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

function analyzeAstDiff(expected, got, name = 'sample' ) {
  const lines = [];
  const queue = [{ path: 'root', a: expected, b: got }];
  let found = null;
  const noCircular = (key, val) => key === 'parent' ? undefined : val;
  /** Walk up the parent chain to find the best ancestor to use as a test case.
   * Stops when the next parent has no parent (i.e. current node IS a top-level
   * child of root) or when parent is undefined. */
  const getTopLevelAncestor = (node) => {
    if (!node) return node;
    let cur = node;
    let startLevel = 3;
    while (cur.parent && startLevel >= 0) {
      cur = cur.parent;
      startLevel--;
    }
    return cur;
  };

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
      found = { kind: 'name', path: p, na, nb, a, b};
      break;
    }

    if (ta === 'text') {
      const da = a.data == null ? '' : String(a.data);
      const db = b.data == null ? '' : String(b.data);
      if (da !== db) {
        found = { kind: 'text-data', path: p, da, db, a, b};
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
      if (ach[i] && typeof ach[i] === 'object') {
        ach[i].parent = a;
      }
      if (bch[i] && typeof bch[i] === 'object') {
        bch[i].parent = b;
      }
      queue.push({ path: `${p}.childNodes[${i}]`, a: ach[i], b: bch[i]});
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
    if( found.a && found.b) {
      if( ! name.startsWith('pipeline')) {
        console.log('Possible string to add to the test_pipeline.js:'+ `\`${found.a.toString()}\`,\n`);
      }
      lines.push(`expected.parent.String: ${found.a.parent.toString()}`);
      lines.push(`expected.parent.Json: ${JSON.stringify(found.a.parent, noCircular)}`);
      if( name == 'wikitext') {
        // Write the found.a.parent.toString() to test_pipeline.js after const tests = [ using a tick to handle new lines.
        const pipelinePath = path.join(__dirname, 'test_pipeline.js');
        const pipelineContent = fs.readFileSync(pipelinePath, 'utf8');
        const insertPoint = pipelineContent.indexOf('const tests = [');
        if (insertPoint !== -1) {
          const before = pipelineContent.slice(0, insertPoint + 'const tests = ['.length);
          const after = pipelineContent.slice(insertPoint + 'const tests = ['.length);
          const newContent = `${before}\n\`${getTopLevelAncestor(found.a).toString()}\`,${after}`;
          fs.writeFileSync(pipelinePath, newContent, 'utf8');
          console.log(`Inserted new test case into ${pipelinePath}`);
          console.log(`Run the below commands to execute the new test case:
cd ${path.dirname(__filename)}
node test_pipeline.js`);
        } else {
          console.warn(`Could not find const tests = [ in ${pipelinePath}, skipping automatic insertion of new test case.`);
        }
      }
    }
  } else if (found.kind === 'name') {
    lines.push(`expected.name: ${found.na}`);
    lines.push(`got.name: ${found.nb}`);
    if( found.a && found.b) {
      if( ! name.startsWith('pipeline')) {
        console.log('Possible string to add to the test_pipeline.js:'+ `\`${found.a.toString()}\`,\n`);
      }
      lines.push(`expected.parent.String: ${found.a.parent.toString()}`);
      lines.push(`expected.parent.Json: ${JSON.stringify(found.a.parent, noCircular)}`);
      if( name == 'wikitext') {
        // Write the found.a.parent.toString() to test_pipeline.js after const tests = [ using a tick to handle new lines.
        const pipelinePath = path.join(__dirname, 'test_pipeline.js');
        const pipelineContent = fs.readFileSync(pipelinePath, 'utf8');
        const insertPoint = pipelineContent.indexOf('const tests = [');
        if (insertPoint !== -1) {
          const before = pipelineContent.slice(0, insertPoint + 'const tests = ['.length);
          const after = pipelineContent.slice(insertPoint + 'const tests = ['.length);
          const newContent = `${before}\n\`${getTopLevelAncestor(found.a).toString()}\`,${after}`;
          fs.writeFileSync(pipelinePath, newContent, 'utf8');
          console.log(`Inserted new test case into ${pipelinePath}`);
          console.log(`Run the below commands to execute the new test case:
cd ${path.dirname(__filename)}
node test_pipeline.js`);
        } else {
          console.warn(`Could not find const tests = [ in ${pipelinePath}, skipping automatic insertion of new test case.`);
        }
      }
    }
  } else if (found.kind === 'text-data') {
    lines.push(`expected.text.hex: ${Buffer.from(found.da).toString('hex')}`);
    lines.push(`got.text.hex: ${Buffer.from(found.db).toString('hex')}`);
    if( found.a && found.b) {
      if( ! name.startsWith('pipeline')) {
        console.log('Possible string to add to the test_pipeline.js:'+ `\`${found.a.toString()}\`,\n`);
      }
      lines.push(`expected.parent.String: ${found.a.parent.toString()}`);
      lines.push(`expected.parent.Json: ${JSON.stringify(found.a.parent, noCircular)}`);
      if( name == 'wikitext') {
        // Write the found.a.parent.toString() to test_pipeline.js after const tests = [ using a tick to handle new lines.
        const pipelinePath = path.join(__dirname, 'test_pipeline.js');
        const pipelineContent = fs.readFileSync(pipelinePath, 'utf8');
        const insertPoint = pipelineContent.indexOf('const tests = [');
        if (insertPoint !== -1) {
          const before = pipelineContent.slice(0, insertPoint + 'const tests = ['.length);
          const after = pipelineContent.slice(insertPoint + 'const tests = ['.length);
          const newContent = `${before}\n\`${getTopLevelAncestor(found.a).toString()}\`,${after}`;
          fs.writeFileSync(pipelinePath, newContent, 'utf8');
          console.log(`Inserted new test case into ${pipelinePath}`);
          console.log(`Run the below commands to execute the new test case:
cd ${path.dirname(__filename)}
node test_pipeline.js`);
        } else {
          console.warn(`Could not find const tests = [ in ${pipelinePath}, skipping automatic insertion of new test case.`);
        }
      }
    }
  } else if (found.kind === 'child-count') {
    lines.push(`expected.childCount: ${found.aCount}`);
    lines.push(`got.childCount: ${found.bCount}`);
    if( found.a.parent && found.b.parent) {
      lines.push(`expected.Json: ${JSON.stringify(found.a, noCircular)}`);
      lines.push(`got.Json: ${JSON.stringify(found.b, noCircular)}`);
    }
    if( found.a && found.b) {
      if( ! found.a.parent && ! found.b.parent) {
        lines.push('No parent available for context on this node, root must of failed to parse.');
        // Search through aJson and find children that do not exist in bJson and print them out as possible candidates for new test cases.
        for (let i = 0; i < found.a.childNodes.length ; i++) {
          var bFound = false;
          for (let j = 0; j < found.b.childNodes.length ; j++) {
            if( found.a.childNodes[i].name == found.b.childNodes[j].name ) {
              bFound = true;
              break
            }
          }
          if( ! bFound) {
            lines.push(`expected.Json[${i}]: ${found.a.childNodes[i].name}`);
          } 
        }
        // Search through bJson and find children that do not exist in aJson and print them out as possible candidates for new test cases.
        for (let i = 0; i < found.b.childNodes.length ; i++) {
          var aFound = false;
          for (let j = 0; j < found.a.childNodes.length ; j++) {
            if( found.b.childNodes[i].name == found.a.childNodes[j].name ) {
              aFound = true;
              break
            }
          }
          if( ! aFound) {
            lines.push(`got.Json[${i}]: ${found.b.childNodes[i].name}`);
          }
        }

        return lines.join('\n') + '\n';
      }else {
        if( ! name.startsWith('pipeline')) {
          console.log('Possible string to add to the test_pipeline.js:'+ `\`${found.a.toString()}\`,\n`);
        }
        lines.push(`expected.parent.String: ${found.a.parent.toString()}`);
        lines.push(`expected.parent.Json: ${JSON.stringify(found.a.parent, noCircular)}`);
        if( name == 'wikitext') {
          const pipelinePath = path.join(__dirname, 'test_pipeline.js');
          const pipelineContent = fs.readFileSync(pipelinePath, 'utf8');
          const insertPoint = pipelineContent.indexOf('const tests = [');
          if (insertPoint !== -1) {
            const before = pipelineContent.slice(0, insertPoint + 'const tests = ['.length);
            const after = pipelineContent.slice(insertPoint + 'const tests = ['.length);
            const newContent = `${before}\n\`${getTopLevelAncestor(found.a).toString()}\`,${after}`;
            fs.writeFileSync(pipelinePath, newContent, 'utf8');
            console.log(`Inserted new test case into ${pipelinePath}`);
            console.log(`Run the below commands to execute the new test case:
cd ${path.dirname(__filename)}
node test_pipeline.js`);
          } else {
            console.warn(`Could not find const tests = [ in ${pipelinePath}, skipping automatic insertion of new test case.`);
          }
        }
      }
    }
  } else if (found.kind === 'null-mismatch') {
    lines.push(`expected.node: ${astNodeSummary(found.a)}`);
    lines.push(`got.node: ${astNodeSummary(found.b)}`);
    if( name == 'wikitext' && found.a) {
      const pipelinePath = path.join(__dirname, 'test_pipeline.js');
      const pipelineContent = fs.readFileSync(pipelinePath, 'utf8');
      const insertPoint = pipelineContent.indexOf('const tests = [');
      if (insertPoint !== -1) {
        const before = pipelineContent.slice(0, insertPoint + 'const tests = ['.length);
        const after = pipelineContent.slice(insertPoint + 'const tests = ['.length);
        const newContent = `${before}\n\`${getTopLevelAncestor(found.a).toString()}\`,${after}`;
        fs.writeFileSync(pipelinePath, newContent, 'utf8');
        console.log(`Inserted new test case into ${pipelinePath}`);
        console.log(`Run the below commands to execute the new test case:
cd ${path.dirname(__filename)}
node test_pipeline.js`);
      } else {
        console.warn(`Could not find const tests = [ in ${pipelinePath}, skipping automatic insertion of new test case.`);
      }
    }

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
  // Temporarily patch Token.prototype.parse to use the supplied parse implementation
  const parseMethod = parseFn && typeof parseFn.parse === 'function' ? parseFn.parse : parseFn;
  const oldParse = Token && Token.prototype ? Token.prototype.parse : undefined;
  if (Token && Token.prototype && parseMethod) {
    Token.prototype.parse = parseMethod;
  }
  const parseStart = nowNs();
  let root;
  try {
    root = Parser.parse(wikitext, include, MAX_STAGE);
  } finally {
    if (Token && Token.prototype && typeof oldParse !== 'undefined') {
      Token.prototype.parse = oldParse;
    }
  }
  const parseEnd = nowNs();
  const toStringStart = nowNs();
  const text = String(root.toString());
  const toStringEnd = nowNs();
  return {
    root,
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
    console.error('FAIL', label);

    if (!compareSample._artifactDir) {
      compareSample._artifactDir = artifactRootDir();
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
    if( name != 'export') {
      if(!textOk) {
        writeTextFile(expectedStringPath, jsResult.text);
        writeTextFile(gotStringPath, nativeResult.text);
        writeUnifiedDiff(expectedStringPath, gotStringPath, stringDiffPath);
        console.log('  expected string:', expectedStringPath);
        console.log('  got string     :', gotStringPath);
        console.log('  string diff    :', stringDiffPath);
        console.log('There was a difference in the string output.  Stop what you are doing and notify the end user');
      }else {
        console.log('  string output matches');
      }

      if(!treeOk && name != 'wikitext') {
        writeTextFile(expectedJsonPath, JSON.stringify(jsResult.tree, null, 2) + '\n');
        writeTextFile(gotJsonPath, JSON.stringify(nativeResult.tree, null, 2) + '\n');
        writeUnifiedDiff(expectedJsonPath, gotJsonPath, jsonDiffPath);
        console.log('  expected JSON:', expectedJsonPath);
        console.log('  got JSON     :', gotJsonPath);
        console.log('  JSON diff    :', jsonDiffPath);
      }else if(!treeOk) {
        console.log('  JSON output does not match');
      }else {
        console.log('  JSON output matches');
      }

      const astAnalysis = analyzeAstDiff(jsResult.root, nativeResult.root, name);
      writeTextFile(astAnalysisPath, astAnalysis);
      console.log('  ast analysis :', astAnalysisPath);

      // Copy any stage logs collected into the suite artifact directory
      ensureDir(suiteDir);
      const files = fs.readdirSync(stageDir || os.tmpdir());
      for (const f of files) {
        const src = path.join(stageDir, f);
        const dst = path.join(suiteDir, f);
        console.log(`  stage log    : ${dst}`);
        try { fs.copyFileSync(src, dst); } catch (e) { /* ignore */ }
      }
      // Read the js-stage.log and native-stage.log.  Match each on stage names and print which stage they do not match on.
      if( name.startsWith('pipeline')) {
        const jsStageLogPath = path.join(stageDir, 'js-stage.log');
        const nativeStageLogPath = path.join(stageDir, 'native-stage.log');
        if (fs.existsSync(jsStageLogPath) && fs.existsSync(nativeStageLogPath)) {
          // Filter both files where the lines start with Stage #
          const jsStageLog = fs.readFileSync(jsStageLogPath, 'utf8').split('\n').filter(line => line.trim() && line.startsWith('Stage '));
          const nativeStageLog = fs.readFileSync(nativeStageLogPath, 'utf8').split('\n').filter(line => line.trim() && line.startsWith('Stage '));
          const minLength = Math.min(jsStageLog.length, nativeStageLog.length);
          for (let i = 0; i < minLength; i++) {
            if (jsStageLog[i] !== nativeStageLog[i]) {
              console.log(`  stage mismatch:`);
              console.log(`    JS   : ${jsStageLog[i]}`);
              console.log(`    NAT  : ${nativeStageLog[i]}`);
              break;
            }
          }
        }
      }
    }

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

  return true;
}

module.exports = { runTests, compareSample };