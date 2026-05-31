#!/usr/bin/env node
'use strict';
const fs = require('fs');
const path = require('path');
const readline = require('readline');
const { spawn } = require('child_process');
const bz2 = require('unbzip2-stream');
const { compareSampleDetailed } = require('./helpers');

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
  console.log('Usage: node test_export.js --input INPUT.jsonl[.bz2|.zst] [--start N] [--end N]');
  process.exit(1);
}

function resolveDefaultInputPath(scriptDir) {
  const candidates = [
    path.join(scriptDir, '..', '..', 'data', 'enwiki-main.jsonl.zst'),
    path.join(scriptDir, '..', '..', '..', 'data', 'enwiki-main.jsonl.zst'),
  ];

  for (const candidate of candidates) {
    if (fs.existsSync(candidate)) {
      return candidate;
    }
  }

  return candidates[0];
}

function testExport(argv) {
  const args = argv.slice(2);
  const options = {
    input: null,
    start: 0,
    end: 0, // 0 means no limit
  };

  for (let i = 0; i < args.length; i += 1) {
    const arg = args[i];
    if (arg === '--input') {
      options.input = args[++i];
    } else if (arg === '--start') {
      options.start = Number(args[++i]);
      if (Number.isNaN(options.start) || options.start < 1) {
        throw new Error('--start requires a positive integer');
      }
    } else if (arg === '--end') {
      options.end = Number(args[++i]);
      if (Number.isNaN(options.end) || options.end < 0) {
        throw new Error('--end requires a non-negative integer');
      }
    } else if (arg === '--help' || arg === '-h') {
      usage();
    } else {
      throw new Error(`Unknown argument: ${arg}`);
    }
  }

  if (options.end !== 0 && options.end < options.start) {
    throw new Error('--end must be greater than or equal to --start');
  }

  const scriptDir = path.dirname(__filename);
  const inputPath = options.input ?? resolveDefaultInputPath(scriptDir);
  const inputStream = openInputStream(inputPath);
  const reader = readline.createInterface({ input: inputStream, crlfDelay: Infinity });
  let lineNumber = 0;
  let processed = 0;
  let ok = true;
  let failCount = 0;

  function reportFailure(payload) {
    // Emit machine-readable failure lines on stdout so callers can redirect once and collect all failures.
    console.log(JSON.stringify(payload));
  }

  return new Promise((resolve, reject) => {
    reader.on('line', (line) => {
      lineNumber += 1;
      if (lineNumber < options.start) {
        return;
      }
      if (options.end !== 0 && lineNumber > options.end) {
        reader.close();
        return;
      }

      const trimmed = line.trim();
      if (!trimmed) {
        return;
      }

      let entry;
      try {
        entry = JSON.parse(trimmed);
      } catch (err) {
        reportFailure({
          kind: 'invalid-jsonl',
          line: lineNumber,
          error: err.message,
        });
        ok = false;
        failCount += 1;
        return;
      }

      let pageId;
      let title;
      let revId;
      let text;

      if (Array.isArray(entry)) {
        [pageId, title, revId, text] = entry;
      } else if (entry && typeof entry === 'object') {
        // New JSONL schema from parse_enwiki.py
        pageId = entry.id;
        title = entry.title;
        revId = entry.rev_id ?? entry.timestamp ?? '';
        text = entry.wikitext;
      }

      if (pageId === undefined || title === undefined || text === undefined) {
        reportFailure({
          kind: 'invalid-record',
          line: lineNumber,
          error: 'expected [id, title, rev_id, text] or { id, title, wikitext, ... }',
        });
        ok = false;
        failCount += 1;
        return;
      }

      const result = compareSampleDetailed(text, {
        name: 'export',
        sampleIndex: lineNumber,
        sampleLabel: `${String(title).replace(/\s+/g, '_')}.wikitext`,
      });

      const roundTripText = String(result.nativeText || '');
      const roundTripMatchesInput = roundTripText === String(text);

      if (!result.ok || !roundTripMatchesInput) {
        const saveName = `${String(title).replace(/\s+/g, '_')}.wikitext`;
        const savePath1 = path.join(scriptDir, 'wikitext', saveName);
        fs.writeFileSync(savePath1, text, 'utf8');

        const savePath2 = path.join(scriptDir, '..', '..', '..', 'tests', 'wikitext', saveName);
        fs.writeFileSync(savePath2, text, 'utf8');

        reportFailure({
          kind: 'validation-failure',
          line: lineNumber,
          pageId,
          title,
          revId,
          parityOk: Boolean(result.textOk && result.astOk),
          schemaOk: Boolean(result.schemaOk),
          roundTripOk: Boolean(roundTripMatchesInput),
          roundTripGeneratedLength: roundTripText.length,
          inputLength: String(text).length,
          schemaErrors: result.schemaErrors || [],
          saved: [savePath1, savePath2],
        });
        ok = false;
        failCount += 1;
      }

      if (processed % 10 === 0) {
        process.stderr.write(`\rProcessed ${processed + 1} lines from ${path.basename(inputPath)}`);
      }

      processed += 1;
    });

    reader.on('close', () => {
      if (processed > 0) {
        process.stderr.write('\n');
        process.stderr.write('\n');
      }
      console.log(JSON.stringify({
        kind: 'summary',
        input: inputPath,
        processed,
        failures: failCount,
        ok,
      }));
      resolve(ok);
    });

    reader.on('error', (err) => {
      reject(err);
    });
  });
}

if (process.argv[1] === __filename) {
  testExport(process.argv)
    .then((ok) => {
      if (ok) {
        console.log("Export test completed successfully");
      } else {
        console.error("Export test failed");
        process.exit(1);
      }
    })
    .catch((err) => {
      console.error(err instanceof Error ? err.message : String(err));
      process.exit(1);
    });
}


module.exports = { testExport };