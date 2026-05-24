'use strict';
// Shared helpers for parity tests.
const path = require('path');
const fs = require('fs');
const os = require('os');
const { spawnSync } = require('child_process');
const { compareAST } = require('./compareAST');
const { buildJsAst } = require('./buildJsAst');

const wikiparser = require(path.join(__dirname, '..', '..', '..', 'new-js', 'dist', 'index.js'));
// Get the native parser by looking in the Release directory fist, then Debug if not found.  This allows running tests in both dev and prod builds without changing the test code.
let nativeParser;
try {  
  nativeParser = require(path.join(__dirname, '..', 'build', 'Release', 'wiki-cast.node'));
}catch(e) {
   nativeParser = require(path.join(__dirname, '..', 'build', 'Debug', 'wiki-cast.node'));
}

const MAX_STAGE = 10;
const LAST_SAMPLE_PATH = '/tmp/wiki_latest_test_input.txt';
const PERF_LOG_PATH = '/tmp/wikitext_perf.txt';
const DEFAULT_WIKI_CONFIG = path.join(__dirname, '..', '..', '..', 'config', 'enwiki.json');

wikiparser.config = String(DEFAULT_WIKI_CONFIG);
nativeParser.config = String(DEFAULT_WIKI_CONFIG);

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

// Parent is an array of { js: jsToken, c: ncToken }
function getCmpPath(parents) {
  return parents.map(p => `${p.js.type}(${p.js.name || ''})`).join(' > ');
}

function analyzeAstDiff(cmp, name = 'sample', wikitext = '') {
  const lines = [];

  lines.push('AST analysis report');
  if (cmp && cmp.success) {
    lines.push('No structural differences found by analyzer.');
    return lines.join('\n') + '\n';
  }

  lines.push(`first_mismatch.kind: ${cmp.kind}`);
  lines.push(`first_mismatch.path: ${getCmpPath(cmp.parents)}`);
  lines.push(`first_mismatch.reason: ${cmp.reason}`);
  lines.push(`expected.String: ${cmp.jsToken.toString()}`);
  lines.push(`expected.Json: ${JSON.stringify(buildJsAst(cmp.jsToken))}`);
  lines.push(`actual.String: ${cmp.ncToken.toString()}`);
  lines.push(`actual.Json: ${JSON.stringify(cmp.ncToken, Object.getOwnPropertyNames(cmp.ncToken))}`);
  return lines.join('\n') + '\n';
}

function getWikiTextSmallesDiff(jsToken, ncToken, parents) {
  const testStr = String(jsToken.toString());
  if( jsToken.type === 'root' ) {
    return testStr;
  }

  let ok = true;
  let jsResult, nativeResult;
  try {
    jsResult = runParse(testStr, wikiparser, false, false, 'js');
  } catch (e) {
    console.log('ERROR (JS)  ', e && e.message, e && e.stack);
    return undefined;
  }

  try {
    nativeResult = runParse(Buffer.from(testStr, 'utf-8'), nativeParser, false, false, 'native');
  } catch (e) {
    console.log('ERROR (NAT) ', e && e.message, e && e.stack);
    return undefined;
  }

  const textOk = jsResult.text === nativeResult.text;
  const cmp = compareAST(jsResult.root, nativeResult.root);
  ok = textOk && cmp.success;
  if( !ok ) {
    return testStr;
  }
  const parent = parents[parents.length - 1];
  return getWikiTextSmallesDiff(parent.js, parent.nc, parents.slice(0, -1));
}

function artifactRootDir() {
  const ts = new Date().toISOString().replace(/[:.]/g, '-');
  const dir = path.join(os.tmpdir(), `wiki_js_parity_${ts}`);
  ensureDir(dir);
  return dir;
}

/**
 * Run the full parse pipeline on a fresh Token and return
 * { text: string, tree: object }.
 * @param {string} wikitext
 * @param {Function} parseFn  Either proto.__orig_parse (JS baseline) or token.parse
 * @param {boolean} include
 * @param {boolean} tidy
 */
function runParse(wikitext, parser, include = false, tidy = false, runLabel = 'parse') {
  const parseStart = process.hrtime.bigint();
  const root = parser.parse(wikitext, include, MAX_STAGE);
  const parseEnd = process.hrtime.bigint();
  const toStringStart = process.hrtime.bigint();
  const text = String(root.toString());
  const toStringEnd = process.hrtime.bigint();
  return {
    root,
    text,
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
  const stageDir = path.join(os.tmpdir(), `wiki_stage_${Date.now()}_${process.pid}_${Math.random().toString(36).slice(2,8)}`);
  ensureDir(stageDir);
  const prevStageDir = process.env.WIKI_STAGE_LOG_DIR;
  const prevStageFlag = process.env.WIKI_STAGE_LOG;
  process.env.WIKI_STAGE_LOG_DIR = stageDir;

  try {
    try {
      jsResult = runParse(wikitext, wikiparser, include, tidy, 'js');
    } catch (e) {
      console.log('ERROR (JS)  ', label, e && e.message, e && e.stack);
      return false;
    }

    try {
      nativeResult = runParse(Buffer.from(wikitext, 'utf-8'), nativeParser, include, tidy, 'native');
    } catch (e) {
      console.log('ERROR (NAT) ', label, e && e.message, e && e.stack);
      return false;
    }
  } finally {
    /* restore any previous env */
    if (prevStageDir === undefined) delete process.env.WIKI_STAGE_LOG_DIR; else process.env.WIKI_STAGE_LOG_DIR = prevStageDir;
    if (prevStageFlag === undefined) delete process.env.WIKI_STAGE_LOG; else process.env.WIKI_STAGE_LOG = prevStageFlag;
  }

  appendPerfLine(name, sampleIndex, jsResult.timing, nativeResult.timing);
  // Append to a text file the JSON AST for JS and native along with the input using writeTextFile
  // fs.appendFileSync(path.join(__dirname, name+'_ast.txt'), "'"+ wikitext + "'\t'" + JSON.stringify(nativeResult.root) + "'\n", { flag: 'a' });

  const textOk = jsResult.text === nativeResult.text;
  const cmp = compareAST(jsResult.root, nativeResult.root);
  const ok = textOk && cmp.success;

  if (!ok) {
    console.error('FAIL', label);

    if( name != 'export') {
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
      console.log('  input string:', inputPath);
      writeTextFile(inputPath, wikitext);
      if(!textOk) {
        writeTextFile(expectedStringPath, jsResult.text);
        writeTextFile(gotStringPath, nativeResult.text);
        writeUnifiedDiff(expectedStringPath, gotStringPath, stringDiffPath);
        console.log('  expected string:', expectedStringPath);
        console.log('  got string     :', gotStringPath);
        console.log('  string diff    :', stringDiffPath);
        console.log(`There was a difference in the string output.  ${jsResult.text.length} vs ${nativeResult.text.length} characters.`);
      }else {
        console.log('  string output matches');
      }

      if(!cmp.success) {
        writeTextFile(expectedJsonPath, JSON.stringify(buildJsAst(jsResult.root), null, 2) + '\n');
        writeTextFile(gotJsonPath, JSON.stringify(nativeResult.root, null, 2) + '\n');
        writeUnifiedDiff(expectedJsonPath, gotJsonPath, jsonDiffPath);
        console.log('  expected JSON:', expectedJsonPath);
        console.log('  got JSON     :', gotJsonPath);
        console.log('  JSON diff    :', jsonDiffPath);
        const astAnalysis = analyzeAstDiff(cmp, name, wikitext);
        // If wikitext is under 50 characters, print the AST analysis to the console as well for easier debugging of small samples.
        if( wikitext.length <= 100 || JSON.stringify(buildJsAst(cmp.jsToken)).length <= 1000 ) {
          console.log(astAnalysis);
        }else {
          writeTextFile(astAnalysisPath, astAnalysis);
          console.log('  ast analysis :', astAnalysisPath);
        }
      }else if(!cmp.success) {
        console.log('  JSON output does not match');
      }else {
        console.log('  JSON output matches');
      }

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
      if( ! name.startsWith('export') && ! name.startsWith('wikitext')) {
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

      if( name.startsWith('wikitext')) {
        const smallestDiff = getWikiTextSmallesDiff(cmp.jsToken, cmp.ncToken, cmp.parents);
        if( smallestDiff && smallestDiff.length <= 1500  ) {
          console.log('\n`' + smallestDiff + '`,\n');
        }else {
           console.log('Smallest wikitext that produces a difference is too large to print to console, see above artifact files for details.');
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
  // These are specific for debugging if a env is set
  process.env.DEBUG_PARAM_VALUES = 'true';

  const include = Boolean(opts && opts.include);
  const tidy = Boolean(opts && opts.tidy);
  const suiteName = resolveSuiteName(opts);
  //fs.writeFileSync(path.join(__dirname, suiteName+'_ast.txt'), '', 'utf8');
  let passed = 0;
  let failed = 0;
  for (let i = 0; i < samples.length; i++) {
    // Create/clear the file at path.join(__dirname, name+'_ast.txt')
    const ok = compareSample(samples[i], {
      include,
      tidy,
      name: suiteName,
      sampleIndex: i + 1,
    });
    if (ok) passed++;
    else failed++;
  }
  // const total = samples.length;
  // console.log(`SUMMARY [${suiteName}] passed=${passed} failed=${failed} total=${total}`);
  if (failed > 0) {
    //process.exit(failed);
    return false;
  }

  return true;
}

module.exports = { runTests, compareSample };