#!/usr/bin/env bash

usage() {
  cat <<'USAGE'
Usage: compare_stage_logs.sh JS_STAGE_LOG NATIVE_STAGE_LOG

USAGE
  exit 2
}

if [[ $# -lt 2 ]]; then
  usage
fi

JS_LOG="$1"
NATIVE_LOG="$2"

if [[ ! -f "$JS_LOG" ]]; then
  echo "JS log not found: $JS_LOG" >&2
  exit 3
fi
if [[ ! -f "$NATIVE_LOG" ]]; then
  echo "Native log not found: $NATIVE_LOG" >&2
  exit 3
fi

tmp_js='/tmp/js_stage_lines.txt'
tmp_native='/tmp/native_stage_lines.txt'

# Extract lines that begin with Stage (case-insensitive) and convert to TSV: stage<TAB>content
grep -i '^Stage ' "$JS_LOG" | head -n 10 | sed -E 's/^[Ss]tage[[:space:]]*([0-9]+):[[:space:]]*(.*)$/\2/' > "$tmp_js" || true
grep -i '^Stage ' "$NATIVE_LOG" | head -n 10 | sed -E 's/^[Ss]tage[[:space:]]*([0-9]+):[[:space:]]*(.*)$/\2/' > "$tmp_native" || true

if [[ $(wc -l < "$tmp_js") -ne 10 || $(wc -l < "$tmp_native") -ne 10 ]]; then
  echo "Expected exactly 10 Stage lines in each file." >&2
  exit 4
fi

if [[ ! -s "$tmp_js" && ! -s "$tmp_native" ]]; then
  echo "No 'Stage' lines found in either file." >&2
  exit 0
fi

for s in {1..11}; do
    echo "Comparing Stage $((s - 1))..."
    
    cat ${tmp_js} | head -n ${s} | tail -n 1 > /tmp/js_side.json
    cat ${tmp_native} | head -n ${s} | tail -n 1 > /tmp/native_side.json

    echo "Running jd diff for Stage $((s - 1))..."
    jd /tmp/js_side.json /tmp/native_side.json > /tmp/jd_diff.txt
    JD_EXIT_CODE=$?
    if [[ $JD_EXIT_CODE -ne 0 ]]; then
        echo "Difference found at Stage $((s - 1)), first 3 differences printed."
        head -n $(cat /tmp/jd_diff.txt | grep -n "^@" | sed -n '3p' | cut -d: -f1) /tmp/jd_diff.txt
        exit 1
    fi
done

echo "No differences found in Stage lines."
