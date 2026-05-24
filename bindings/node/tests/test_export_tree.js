#!/usr/bin/env node
'use strict';
const fs = require('fs');
const path = require('path');
const readline = require('readline');
const bz2 = require('unbzip2-stream');
const { SimpleZSTD } = require('simple-zstd'); // For future support of .zst files

const wikiparser = require(path.join(__dirname, '..', '..', '..', 'new-js', 'dist', 'index.js'));
wikiparser.config = String(path.join(__dirname, '..', '..', '..', 'config', 'enwiki.json'));

let zstdInstance = null;

async function openInputStream(inputPath) {
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

  if (inputPath.endsWith('.zst')) {
    zstdInstance = await SimpleZSTD.create({
      decompressQueueSize: 1, // Pre-spawn 6 decompression processes
    });
    const src = fs.createReadStream(inputPath);
    const decompressor = await zstdInstance.decompress();
    src.on('error', (err) => {
      console.error(`Unable to read compressed input: ${err.message}`);
      if (zstdInstance) zstdInstance.destroy();
      process.exit(1);
    });
    decompressor.on('error', (err) => {
      console.error(`Unable to decompress zst input: ${err.message}`);
      if (zstdInstance) zstdInstance.destroy();
      process.exit(1);
    });
    return src.pipe(decompressor);
  }

  return fs.createReadStream(inputPath, { encoding: 'utf8' });
}

function usage() {
  console.log('Usage: node test_export.js --input INPUT.jsonl[.bz2] [--start N] [--end N]');
  process.exit(1);
}

function resolveDefaultInputPath(scriptDir) {
  const candidates = [
    path.join(scriptDir, '..', '..', 'data', 'enwiki.jsonl.zst'),
    path.join(scriptDir, '..', '..', '..', 'data', 'enwiki.jsonl.zst'),
  ];

  for (const candidate of candidates) {
    if (fs.existsSync(candidate)) {
      return candidate;
    }
  }

  return candidates[0];
}

function documentChildren(node, wikiChildren) {
  if( wikiChildren[node.type] === undefined) {
    wikiChildren[node.type] = {};
  }
  if (node.childNodes.length > 0) {
    const multipleTypes = {};
    for (const child of node.childNodes) {
      if( multipleTypes[child.type] === undefined) {
        multipleTypes[child.type] = false;
      } else {
        multipleTypes[child.type] = true;
      }
      documentChildren(child, wikiChildren);
    }
    for( const type in multipleTypes) {
      if( ! wikiChildren[node.type][type] ) {
        wikiChildren[node.type][type] = multipleTypes[type];
      }
    }
  }
}

async function main(argv) {
  const args = argv.slice(2);
  const options = {
    input: null,
    start: 0,
    end: 1_000_000, // 0 means no limit
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
  const inputStream = await openInputStream(inputPath);
  const reader = readline.createInterface({ input: inputStream, crlfDelay: Infinity });
  let lineNumber = 0;
  let processed = 0;

  // Dictionary of keys with the values being another dictionary of string with boolean values.
  const wikiChildren = {};

  for await (const line of reader) {
    lineNumber += 1;
    if (lineNumber < options.start) {
      continue;
    }
    if (options.end !== 0 && lineNumber > options.end) {
      break;
    }

    const trimmed = line.trim();
    if (!trimmed) {
      continue;
    }

    let entry;
    try {
      entry = JSON.parse(trimmed);
    } catch (err) {
      console.error(`Skipping invalid line ${lineNumber}: ${err.message}`);
      continue;
    }

    const [pageId, title, revId, text] = entry;

    if (pageId === undefined || title === undefined || revId === undefined || text === undefined) {
      console.error(`Skipping invalid record on line ${lineNumber}: expected [id, title, rev_id, text]`);
      continue;
    }

    const root = wikiparser.parse(text, false, 22);
    /*
     * We want to process root to get all the childred in
    */
   documentChildren(root, wikiChildren);

    if (processed % 10 === 0) {
      process.stderr.write(`\rProcessed ${processed + 1} lines from ${path.basename(inputPath)}`);
    }

    processed += 1;
  }

  if (processed > 0) {
    process.stderr.write('\n');
  }
  if (processed > 0) {
    process.stderr.write('\n');
  }

  console.log(`All ${processed} samples passed`);
  // Output the wikiChildren dictionary as JSON to stdout
  console.log(JSON.stringify(wikiChildren, null, 2));

  if (zstdInstance) {
    zstdInstance.destroy();
  }

  process.exit(0);
}

main(process.argv).catch((err) => {
  console.error(err instanceof Error ? err.message : String(err));
  process.exit(1);
});
