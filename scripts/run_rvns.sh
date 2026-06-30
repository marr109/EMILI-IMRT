#!/usr/bin/env bash
set -euo pipefail
echo "=== rVNS ablation — PROSTATE K=4 seed=42 ==="
TEMP=$(mktemp -d)
cp -r /project/instances/PROSTATE_sampled/* "$TEMP/"
sed -i 's/^w_under.*/w_under       1.02/' "$TEMP/instance_config.txt"
sed -i 's/^w_over.*/w_over        0.19/' "$TEMP/instance_config.txt"
sed -i 's/^w_ptv_over.*/w_ptv_over    0.10/' "$TEMP/instance_config.txt"
echo "rVNS (nols, irandomk, bangshake=2, outer_iter=2):"
/project/build/emili "$TEMP" baoimrt 4 vns nols irandomk tmaxiter 2 bangshake 2 accng improve rnds 42
rm -rf "$TEMP"
