#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build_profile"

usage() {
  cat <<EOF
Usage: $0 [--perf|--callgrind|--gprof] [--config CONFIG] [--testsdir TESTSDIR]

Modes:
  --perf         Build with frame pointers and run `perf` (default)
  --callgrind    Run under Valgrind Callgrind (very slow, detailed)
  --gprof        Build with `-pg` and generate a gprof report

Examples:
  ./scripts/profile_wikitext.sh --perf --config node_modules/wikiparser-node/config/enwiki.json --testsdir tests/wikitext
EOF
  exit 1
}

MODE="perf"
CONFIG="node_modules/wikiparser-node/config/enwiki.json"
TESTSDIR="tests/wikitext"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --perf) MODE=perf; shift ;;
    --callgrind) MODE=callgrind; shift ;;
    --gprof) MODE=gprof; shift ;;
    --config) CONFIG="$2"; shift 2 ;;
    --testsdir) TESTSDIR="$2"; shift 2 ;;
    -h|--help) usage ;;
    *) echo "Unknown arg: $1"; usage ;;
  esac
done

echo "Root: $ROOT_DIR"
echo "Build dir: $BUILD_DIR"
echo "Mode: $MODE"
echo "Config: $CONFIG"
echo "Tests dir: $TESTSDIR"

rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
pushd "$BUILD_DIR" >/dev/null

if [[ "$MODE" == "gprof" ]]; then
  export CFLAGS="-g -pg -O2"
  export CXXFLAGS="$CFLAGS"
  cmake -DCMAKE_BUILD_TYPE=Release "$ROOT_DIR"
elif [[ "$MODE" == "callgrind" ]]; then
  export CFLAGS="-g -fno-omit-frame-pointer -O2 -march=haswell -mno-avx512f"
  export CXXFLAGS="$CFLAGS"
  cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo "$ROOT_DIR"
else
  export CFLAGS="-g -fno-omit-frame-pointer -O2"
  export CXXFLAGS="$CFLAGS"
  cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo "$ROOT_DIR"
fi

cmake --build . -j"$(nproc)"

find_test_binary() {
  # Prefer the well-known name; otherwise find an executable named test_wikitext*
  if [[ -x ./test_wikitext ]]; then
    echo ./test_wikitext
    return 0
  fi
  local exe
  exe=$(find . -maxdepth 2 -type f -executable -name 'test_wikitext*' | head -n1 || true)
  if [[ -n "$exe" ]]; then
    echo "$exe"
    return 0
  fi
  return 1
}

BIN=$(find_test_binary) || {
  echo "Cannot find test_wikitext binary in $BUILD_DIR" >&2
  ls -la
  popd >/dev/null
  exit 1
}

echo "Using binary: $BIN"

# Resolve config and testsdir to absolute paths relative to ROOT_DIR if not absolute
if [[ "$CONFIG" = /* ]]; then
  CONFIG_PATH="$CONFIG"
else
  CONFIG_PATH="$ROOT_DIR/$CONFIG"
fi

if [[ "$TESTSDIR" = /* ]]; then
  TESTSDIR_PATH="$TESTSDIR"
else
  TESTSDIR_PATH="$ROOT_DIR/$TESTSDIR"
fi

case "$MODE" in
  perf)
    if ! command -v perf >/dev/null 2>&1; then
      echo "perf not found; please install perf." >&2
      exit 1
    fi
    perf record -F 99 -g -- "$BIN" "$CONFIG_PATH" "$TESTSDIR_PATH"
    perf script > perf.out

    # Locate FlameGraph: allow override via FLAMEGRAPH_DIR, else check common locations
    if [[ -n "${FLAMEGRAPH_DIR:-}" && -d "$FLAMEGRAPH_DIR" ]]; then
      FLAMEG_DIR="$FLAMEGRAPH_DIR"
    else
      CANDIDATES=("$ROOT_DIR/FlameGraph" "$ROOT_DIR/../FlameGraph" "$HOME/FlameGraph")
      FLAMEG_DIR=""
      for cand in "${CANDIDATES[@]}"; do
        if [[ -d "$cand" ]]; then
          FLAMEG_DIR="$cand"
          break
        fi
      done
    fi

    if [[ -n "$FLAMEG_DIR" ]]; then
      echo "Found FlameGraph in $FLAMEG_DIR — generating perf.svg"
      "$FLAMEG_DIR/stackcollapse-perf.pl" perf.out > perf.folded
      "$FLAMEG_DIR/flamegraph.pl" perf.folded > perf.svg
      echo "Flamegraph: $BUILD_DIR/perf.svg"
    else
      echo "FlameGraph not found. To generate a flamegraph, run:" >&2
      echo "  git clone https://github.com/brendangregg/FlameGraph $ROOT_DIR/FlameGraph" >&2
      echo "or clone into parent (../FlameGraph) or \$HOME/FlameGraph" >&2
      echo "then re-run this script to produce perf.svg" >&2
    fi
    ;;

  callgrind)
    if ! command -v valgrind >/dev/null 2>&1; then
      echo "valgrind not found; please install valgrind." >&2
      exit 1
    fi
    valgrind --tool=callgrind --callgrind-out-file=callgrind.out "$BIN" "$CONFIG_PATH" "$TESTSDIR_PATH"
    echo "callgrind output: $BUILD_DIR/callgrind.out (open with kcachegrind or callgrind_annotate)"
    ;;

  gprof)
    # binary built with -pg earlier
    "$BIN" "$CONFIG_PATH" "$TESTSDIR_PATH"
    if [[ -f gmon.out ]]; then
      gprof "$BIN" gmon.out > gprof.txt || true
      echo "gprof report: $BUILD_DIR/gprof.txt"
    else
      echo "gmon.out not found after running binary; gprof report unavailable." >&2
    fi
    ;;

  *)
    echo "Unknown mode: $MODE" >&2
    exit 1
    ;;
esac

popd >/dev/null
echo "Done."
