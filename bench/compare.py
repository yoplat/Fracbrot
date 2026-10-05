#!/usr/bin/env python3
"""Compare two `fracbrot --bench` JSON files: python3 bench/compare.py base.json new.json"""
import json
import sys


def load(path):
    with open(path) as f:
        data = json.load(f)
    return data, {s["name"]: s for s in data["scenes"]}


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 1
    base_meta, base = load(sys.argv[1])
    new_meta, new = load(sys.argv[2])
    if base_meta["resolution"] != new_meta["resolution"]:
        print(f"warning: resolution differs ({base_meta['resolution']} vs {new_meta['resolution']})")

    print(f"{'scene':<16} {'base':>9} {'new':>9} {'change':>8}   {'base p95':>9} {'new p95':>9}")
    for name, b in base.items():
        n = new.get(name)
        if not n:
            print(f"{name:<16} {b['median_ms']:>7.2f}ms {'-':>9}")
            continue
        change = (n["median_ms"] - b["median_ms"]) / b["median_ms"] * 100.0
        print(f"{name:<16} {b['median_ms']:>7.2f}ms {n['median_ms']:>7.2f}ms {change:>+7.1f}%"
              f"   {b['p95_ms']:>7.2f}ms {n['p95_ms']:>7.2f}ms")
    return 0


if __name__ == "__main__":
    sys.exit(main())
