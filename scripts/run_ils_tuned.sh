#!/usr/bin/env bash
set -euo pipefail
TEMP=$(mktemp -d)
cp -r /project/instances/PROSTATE_sampled/* "$TEMP/"
sed -i 's/^w_under.*/w_under       1.01/' "$TEMP/instance_config.txt"
sed -i 's/^w_over.*/w_over        0.17/' "$TEMP/instance_config.txt"
sed -i 's/^w_ptv_over.*/w_ptv_over    0.14/' "$TEMP/instance_config.txt"
/project/build/emili "$TEMP" baoimrt 4 ils first ifirstk tmaxiter 1 nangswap tmaxiter 2 pgreedy 2 improve rnds 42 > /project/resultados/ils_tuned_s42.txt 2>&1
rm -rf "$TEMP"
