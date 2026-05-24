# Callback Parser Scaffolding Per Regex

## Build and test

```bash
cd ~/git/wikiparser-node-c-tokenizer
cmake --build build -j"$(nproc)"
node bindings/node/tests/test_parsoid.js
node bindings/node/tests/test_wikitext.js
cat /tmp/wikitext_perf.txt | tail -65 | python3 scripts/average_parse.py
```
