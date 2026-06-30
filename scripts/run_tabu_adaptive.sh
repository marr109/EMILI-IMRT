#!/usr/bin/env bash
set -euo pipefail
echo "=== Tabu adaptive — PROSTATE K=4 seed=42 ==="
TEMP=$(mktemp -d)
cp -r /project/instances/PROSTATE_sampled/* "$TEMP/"
sed -i 's/^w_under.*/w_under       1.02/' "$TEMP/instance_config.txt"
sed -i 's/^w_over.*/w_over        0.04/' "$TEMP/instance_config.txt"
sed -i 's/^w_ptv_over.*/w_ptv_over    0.23/' "$TEMP/instance_config.txt"
/project/build/emili "$TEMP" baoimrt 4 tabu first ifirstk tmaxiter 6 nangswap TBao_adaptive 3 18 rnds 42
rm -rf "$TEMP"
