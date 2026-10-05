#!/bin/sh
# Runs the benchmark N times (default 3) headlessly: bench/run.sh NAME [N]
# Writes bench/out/NAME_1.json ... NAME_N.json; compare with bench/compare.py.
set -e
name=${1:?usage: bench/run.sh NAME [N]}
n=${2:-3}
dir=$(dirname "$0")
mkdir -p "$dir/out"
for i in $(seq 1 "$n"); do
  "$dir/../build/release/fracbrot" --bench "$dir/out/${name}_$i.json" >/dev/null
done
echo "wrote $dir/out/${name}_1..$n.json"
