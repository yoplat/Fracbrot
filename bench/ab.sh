#!/bin/sh
# Interleaved A/B benchmark that cancels out GPU/machine drift:
#   bench/ab.sh BIN_A BIN_B NAME [N]
# Alternates A and B runs N times (default 3), then compares best medians.
set -e
a=${1:?usage: bench/ab.sh BIN_A BIN_B NAME [N]}
b=${2:?}
name=${3:?}
n=${4:-3}
dir=$(dirname "$0")
mkdir -p "$dir/out"
rm -f "$dir/out/${name}_a_"*.json "$dir/out/${name}_b_"*.json
for i in $(seq 1 "$n"); do
  "$a" --bench "$dir/out/${name}_a_$i.json" >/dev/null
  "$b" --bench "$dir/out/${name}_b_$i.json" >/dev/null
done
python3 "$dir/compare.py" "$dir/out/${name}_a" "$dir/out/${name}_b"
