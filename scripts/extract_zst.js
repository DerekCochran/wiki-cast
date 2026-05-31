#!/usr/bin/env node
'use strict';
const fs = require('fs');
const path = require('path');
const readline = require('readline');
const { spawn } = require('child_process');
const bz2 = require('unbzip2-stream');

const DEFAULT_INDICES = [
  594843,
  2073019,
  6045871,
  6063446,
  5091913,
  2131333,
  2152923,
  2152937,
  2152983,
  2152992,
  2205317,
  2354683,
  3451048,
  6354047,
  5447889,
  6469333,
  6479869,
  5577329,
  4701747,
  6562144,
  3771349,
  3772592,
  3773360,
  4770863,
  3845806,
  4784147,
  3879768,
  2955190,
  3894558,
  3939403,
  5838514,
  4951644,
  5865127,
  6764310,
  5881315,
  4998691,
  5914447,
  5914454,
  5914460,
  5930254,
  5933424,
  6823184,
  5975289,
  6870855,
  6951667,
];

function openInputStream(inputPath) {
  if (!inputPath || inputPath === '-') {
    // Read decompressed JSONL from STDIN (e.g. `lbzip2 -dc file.bz2 | node ... --input -`)
    try {
      process.stdin.setEncoding('utf8');
    } catch (e) {
      // ignore if stdin is not a TTY or already configured
    }
    return process.stdin;
  }

  if (inputPath.endsWith('.bz2')) {
    const src = fs.createReadStream(inputPath);
    const decompressor = bz2();
    src.on('error', (err) => {
      console.error(`Unable to read compressed input: ${err.message}`);
      process.exit(1);
    });
    decompressor.on('error', (err) => {
      console.error(`Unable to decompress bz2 input: ${err.message}`);
      process.exit(1);
    });
    return src.pipe(decompressor);
  }

  // If input ends in zst, use the system zstd binary for fast decompression.
  if (inputPath.endsWith('.zst')) {
    const zstd = spawn('zstd', ['-dc', '-T1', '--long=31', inputPath], {
      stdio: ['ignore', 'pipe', 'pipe'],
    });
    let stderr = '';

    zstd.on('error', (err) => {
      console.error(`Unable to spawn zstd: ${err.message}`);
      process.exit(1);
    });

    zstd.stderr.setEncoding('utf8');
    zstd.stderr.on('data', (chunk) => {
      stderr += chunk;
    });

    zstd.on('close', (code, signal) => {
      // Ignore SIGPIPE when downstream closes early (e.g. --end limit reached).
      if (code !== 0 && signal !== 'SIGPIPE') {
        const details = stderr.trim();
        console.error(details ? `Unable to decompress zst input: ${details}` : `Unable to decompress zst input: zstd exited with code ${code}`);
        process.exit(1);
      }
    });

    zstd.stdout.setEncoding('utf8');
    return zstd.stdout;
  }

  return fs.createReadStream(inputPath, { encoding: 'utf8' });
}

function usage() {
  console.log('Usage: node extract_zst.js [--input INPUT.jsonl[.bz2|.zst]] [--index N ...] [--indexes N1,N2,...]');
  console.log('');
  console.log('If no --index/--indexes are provided, built-in default indexes are used.');
  process.exit(1);
}

function sanitizeFileName(title) {
  return String(title)
    .trim()
    .replace(/[\\/:*?"<>|]+/g, '_')
    .replace(/\s+/g, '_')
    .replace(/_+/g, '_');
}

function parsePositiveInteger(value, optionName) {
  const n = Number(value);
  if (!Number.isInteger(n) || n < 1) {
    throw new Error(`${optionName} requires a positive integer`);
  }
  return n;
}

function resolveDefaultInputPath(scriptDir) {
  const candidates = [
    path.join(scriptDir, '..', 'data', 'enwiki-main.jsonl.zst'),
  ];

  for (const candidate of candidates) {
    if (fs.existsSync(candidate)) {
      return candidate;
    }
  }

  return candidates[0];
}

function extractByIndex(argv) {
  const args = argv.slice(2);
  const options = {
    input: null,
    indices: new Set(DEFAULT_INDICES),
  };

  for (let i = 0; i < args.length; i += 1) {
    const arg = args[i];
    if (arg === '--input') {
      options.input = args[++i];
    } else if (arg === '--index') {
      if (options.indices === null) {
        options.indices = new Set();
      }
      const value = args[++i];
      options.indices.add(parsePositiveInteger(value, '--index'));
    } else if (arg === '--indexes') {
      const value = String(args[++i] || '').trim();
      if (!value) {
        throw new Error('--indexes requires a comma-separated list of integers');
      }
      if (options.indices === null) {
        options.indices = new Set();
      }
      for (const part of value.split(',')) {
        const trimmed = part.trim();
        if (!trimmed) {
          continue;
        }
        options.indices.add(parsePositiveInteger(trimmed, '--indexes'));
      }
    } else if (arg === '--no-default-indices') {
      options.indices = new Set();
    } else if (arg === '--help' || arg === '-h') {
      usage();
    } else {
      throw new Error(`Unknown argument: ${arg}`);
    }
  }

  if (!options.indices || options.indices.size === 0) {
    throw new Error('No indexes configured. Provide --index/--indexes or omit --no-default-indices.');
  }

  const scriptDir = path.dirname(__filename);
  const inputPath = options.input ?? resolveDefaultInputPath(scriptDir);
  const outputDir = path.join(scriptDir, '..', 'bindings', 'node', 'tests', 'wikitext');
  fs.mkdirSync(outputDir, { recursive: true });

  const sortedTargets = Array.from(options.indices).sort((a, b) => a - b);
  const pendingTargets = new Set(sortedTargets);
  const maxTarget = sortedTargets[sortedTargets.length - 1];

  const inputStream = openInputStream(inputPath);
  const reader = readline.createInterface({ input: inputStream, crlfDelay: Infinity });
  let lineNumber = 0;
  let extracted = 0;
  const saved = [];

  return new Promise((resolve, reject) => {
    reader.on('line', (line) => {
      lineNumber += 1;
      if (!pendingTargets.has(lineNumber)) {
        if (lineNumber % 100000 === 0) {
          process.stderr.write(`\rScanned ${lineNumber} lines from ${path.basename(inputPath)} (extracted ${extracted}/${sortedTargets.length})`);
        }
        if (lineNumber > maxTarget && pendingTargets.size === 0) {
          reader.close();
        }
        return;
      }

      const trimmed = line.trim();
      if (!trimmed) {
        pendingTargets.delete(lineNumber);
        return;
      }

      let entry;
      try {
        entry = JSON.parse(trimmed);
      } catch (err) {
        pendingTargets.delete(lineNumber);
        console.error(`Line ${lineNumber}: invalid JSON (${err.message})`);
        return;
      }

      let pageId;
      let title;
      let text;

      if (Array.isArray(entry)) {
        [pageId, title, , text] = entry;
      } else if (entry && typeof entry === 'object') {
        pageId = entry.id;
        title = entry.title;
        text = entry.wikitext;
      }

      if (pageId === undefined || title === undefined || text === undefined) {
        pendingTargets.delete(lineNumber);
        console.error(`Line ${lineNumber}: invalid record shape`);
        return;
      }

      const saveName = `${sanitizeFileName(title)}.wikitext`;
      const savePath = path.join(outputDir, saveName);
      fs.writeFileSync(savePath, String(text), 'utf8');
      extracted += 1;
      saved.push({ line: lineNumber, pageId, title, path: savePath });
      pendingTargets.delete(lineNumber);

      process.stderr.write(`\rScanned ${lineNumber} lines from ${path.basename(inputPath)} (extracted ${extracted}/${sortedTargets.length})`);

      if (pendingTargets.size === 0) {
        reader.close();
      }
    });

    reader.on('close', () => {
      if (lineNumber > 0) {
        process.stderr.write('\n');
        process.stderr.write('\n');
      }

      const missing = sortedTargets.filter((n) => pendingTargets.has(n));
      console.log(JSON.stringify({
        kind: 'summary',
        input: inputPath,
        outputDir,
        scannedLines: lineNumber,
        requested: sortedTargets.length,
        extracted,
        missing,
        saved,
      }));
      resolve(missing.length === 0);
    });

    reader.on('error', (err) => {
      reject(err);
    });
  });
}

if (process.argv[1] === __filename) {
  extractByIndex(process.argv)
    .then((ok) => {
      if (ok) {
        console.log('Extraction completed successfully');
      } else {
        console.error('Extraction completed with missing indexes');
        process.exit(1);
      }
    })
    .catch((err) => {
      console.error(err instanceof Error ? err.message : String(err));
      process.exit(1);
    });
}


module.exports = { extractByIndex };