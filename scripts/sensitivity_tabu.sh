#!/usr/bin/env bash
# Sensitivity: Tabu with different w_over values
set -euo pipefail
WO="$1"
echo "=== Tabu sensitivity w_over=$WO ==="
TEMP=$(mktemp -d)
cp -r /project/instances/PROSTATE_sampled/* "$TEMP/"
sed -i "s/^w_under.*/w_under       1.02/" "$TEMP/instance_config.txt"
sed -i "s/^w_over.*/w_over        $WO/" "$TEMP/instance_config.txt"
sed -i "s/^w_ptv_over.*/w_ptv_over    0.23/" "$TEMP/instance_config.txt"
/project/build/emili "$TEMP" baoimrt 4 tabu first ifirstk tmaxiter 6 nangswap TBao_fixed 7 rnds 42
rm -rf "$TEMP"
