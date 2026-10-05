#!/usr/bin/env python3
"""Compare benchmark runs: python3 bench/compare.py BASE NEW

BASE and NEW are a JSON file or a prefix like bench/out/baseline (matching
bench/out/baseline_*.json). With several runs, each scene uses its best (lowest)
median, which filters out GPU clock and background-load noise.
"""
import glob
import json
import sys


def load(spec):
    files = [spec] if spec.endswith(".json") else sorted(glob.glob(spec + "_[0-9]*.json"))
    if not files:
        sys.exit(f"no results for {spec}")
    best = {}
    for path in files:
        with open(path) as f:
            for s in json.load(f)["scenes"]:
                if s["name"] not in best or s["median_ms"] < best[s["name"]]["median_ms"]:
                    best[s["name"]] = s
    return best, len(files)


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 1
    base, nb = load(sys.argv[1])
    new, nn = load(sys.argv[2])
    print(f"best median of {nb} vs {nn} runs")
    print(f"{'scene':<16} {'base':>9} {'new':>9} {'change':>8}")
    for name, b in base.items():
        n = new.get(name)
        if not n:
            continue
        change = (n["median_ms"] - b["median_ms"]) / b["median_ms"] * 100.0
        print(f"{name:<16} {b['median_ms']:>7.2f}ms {n['median_ms']:>7.2f}ms {change:>+7.1f}%")
    return 0


if __name__ == "__main__":
    sys.exit(main())
