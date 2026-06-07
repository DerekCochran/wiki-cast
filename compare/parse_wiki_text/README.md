# parse_wiki_text Rust Export

This app parses wikitext with `parse_wiki_text` and supports both file-based exports and streaming JSONL transforms.

## What It Does

- Parses input wikitext using `parse_wiki_text::Configuration::default().parse(...)`
- For JSON suite inputs, writes one single output JSON file per suite
- For plain file inputs, writes one output JSON per input file
- For JSONL streaming, preserves each original record and adds/replaces `parse_wiki_text`
- Stores parser-native AST in `parse_wiki_text.ast` as encoded JSON text
- Logs parse/encode metrics in a helpers-style perf log

## Build

```bash
cd /home/djc/git/wiki-cast/parse_wiki_text
cargo build --release
```

## Usage

Parse plain article files and write `same-name + .pwt.json` beside them:

```bash
cargo run --release -- /path/to/article.wikitext
```

Parse multiple plain files:

```bash
cargo run --release -- /path/to/a.txt /path/to/b.txt
```

Parse JSON sample files (from test export data):

```bash
cargo run --release -- --input-json /path/to/test_cast_samples.json --input-json /path/to/test_parsoid_samples.json
```

Generate JSON inputs from the existing Node parity suites using the temporary exporter:

```bash
node ./scripts/export_test_inputs.js
```

Then parse those generated JSON inputs:

```bash
cargo run --release -- \
  --input-json ./input/test_cast.samples.json \
  --input-json ./input/test_parsoid.samples.json
```

Parse top-level files in a wikitext directory (no subdirectories) and emit one `.pwt.json` file per source file:

```bash
cargo run --release -- \
  --wikitext-dir /home/djc/git/wiki-cast/bindings/node/tests/wikitext
```

Stream JSONL from stdin and write augmented JSONL to stdout:

```bash
cargo run --release -- --jsonl-stdin < input.jsonl > output.jsonl
```

Use a compiled binary for speed:

```bash
./target/release/parse_wiki_text_export --jsonl-stdin < input.jsonl > output.jsonl
```

## JSONL + Zstd Pipeline (Max Compression)

Recommended max-compression streaming pipeline:

```bash
zstd -dc --long=31 -T0 input.jsonl.zst \
  | ./target/release/parse_wiki_text_export --jsonl-stdin \
  | zstd -c --ultra -22 --long=31 -T0 > output.with_parse_wiki_text.jsonl.zst
```

Notes:

- `-22` is zstd maximum compression level.
- `--ultra` enables levels above `19`.
- `--long=31` is included on both decode and encode sides for long-window data.

Send outputs to a specific directory:

```bash
cargo run --release -- --out-dir /tmp/pwt-out --input-json /path/to/samples.json /path/to/article.wikitext
```

Custom perf log path:

```bash
cargo run --release -- \
  --perf-log /tmp/wikitext_perf_pwt.txt \
  /path/to/article.wikitext
```

## Output Naming

- Plain file input: `article.wikitext` -> `article.wikitext.pwt.json`
- JSON samples input: `test_cast.samples.json` -> `test_cast.samples.pwt.json` (single file)
- JSON samples input: `test_parsoid.samples.json` -> `test_parsoid.samples.pwt.json` (single file)
- Wikitext directory mode: `Article.wikitext` -> `Article.wikitext.pwt.json`

## Output JSON Shape

Suite JSON mode sample shape:

```json
{
  "parser": "parse_wiki_text",
  "suite": "test_cast",
  "sampleCount": 717,
  "samples": [
    {
      "sampleIndex": 1,
      "sampleLabel": "test_cast:1",
      "input": "...original wikitext...",
      "inputLen": 123,
      "parse_wiki_text": "{\"nodes\":[...]}",
      "metrics": {
        "parseMs": 0,
        "encodeMs": 0,
        "nodeCount": 4,
        "warningCount": 0
      }
    }
  ]
}
```

JSONL streaming mode record shape (original fields preserved):

```json
{
  "id": 123,
  "title": "Example",
  "wikitext": "...",
  "parse_wiki_text": {
    "parseMs": 0,
    "encodeMs": 0,
    "nodeCount": 42,
    "warningCount": 1,
    "ast": "{\"nodes\":[...]}",
    "warnings": [
      {
        "start": 10,
        "end": 20,
        "messageType": "InvalidLinkSyntax",
        "message": "Invalid link syntax."
      }
    ]
  }
}
```

## Notes

- `parse_wiki_text.ast` is intentionally parser-native encoded JSON text so comparison logic can evolve independently.
- The perf log includes parse and encode timings for each sample.
