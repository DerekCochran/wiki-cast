#!/usr/bin/env python3
import os
from collections import defaultdict

paths = {
    'current': 'build_profile/perf.folded',
    'previous': 'build_profile_regex/perf.folded'
}

def parse_folded(path):
    sums = defaultdict(int)
    total = 0
    if not os.path.isfile(path):
        raise FileNotFoundError(path)
    with open(path, 'r', encoding='utf-8', errors='replace') as f:
        for line in f:
            line=line.rstrip('\n')
            if not line: continue
            # last token after whitespace is count
            parts = line.rsplit(None, 1)
            if len(parts) == 1:
                continue
            stack, cnt_s = parts
            try:
                cnt = int(cnt_s)
            except ValueError:
                # skip malformed
                continue
            total += cnt
            # leaf function is last element after semicolon
            leaf = stack.split(';')[-1].strip()
            sums[leaf] += cnt
    return sums, total


def top_n(sums, total, n=15):
    items = sorted(sums.items(), key=lambda kv: kv[1], reverse=True)
    out = []
    for k, v in items[:n]:
        pct = 100.0 * v / total if total else 0.0
        out.append((k, v, pct))
    return out


def main():
    results = {}
    for tag, p in paths.items():
        try:
            sums, total = parse_folded(p)
        except FileNotFoundError:
            print(f"Missing perf file: {p}")
            return
        results[tag] = {'sums': sums, 'total': total, 'top': top_n(sums, total, 30)}

    print('\nTop 15 — CURRENT (build_profile/perf.folded)')
    for name, cnt, pct in results['current']['top'][:15]:
        print(f"{cnt:12d} {pct:6.2f}%  {name}")

    print('\nTop 15 — PREVIOUS (build_profile_regex/perf.folded)')
    for name, cnt, pct in results['previous']['top'][:15]:
        print(f"{cnt:12d} {pct:6.2f}%  {name}")

    # merge keys and compute percentages
    keys = set(results['current']['sums'].keys()) | set(results['previous']['sums'].keys())
    merged = []
    tot_cur = results['current']['total']
    tot_prev = results['previous']['total']
    for k in keys:
        c = results['current']['sums'].get(k, 0)
        p = results['previous']['sums'].get(k, 0)
        pct_c = 100.0 * c / tot_cur if tot_cur else 0.0
        pct_p = 100.0 * p / tot_prev if tot_prev else 0.0
        merged.append((k, c, p, pct_c, pct_p, pct_c - pct_p))

    # sort by absolute change in pct
    merged.sort(key=lambda x: abs(x[5]), reverse=True)
    print('\nTop differences (by absolute percent change) — show top 20')
    print('delta%   cur%    prev%    cur_count    prev_count    function')
    for k, c, p, pct_c, pct_p, diff in merged[:20]:
        print(f"{diff:+7.2f}%  {pct_c:6.2f}%  {pct_p:6.2f}%  {c:12d}  {p:12d}  {k}")

if __name__ == '__main__':
    main()
