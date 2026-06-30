#!/usr/bin/env bash
# Run multi-seed validation for a given algorithm+config on PROSTATE_sampled
# Usage: run_validation.sh <algo> <label> <seed_start> <seed_end> <emili_args...>
# Weights are injected via env vars: W_UNDER, W_OVER, W_PTV_OVER
set -euo pipefail

ALGO="$1"; shift
LABEL="$1"; shift
SEED_START="$1"; shift
SEED_END="$1"; shift

OUTDIR="/project/resultados"
mkdir -p "$OUTDIR"

for seed in $(seq "$SEED_START" "$SEED_END"); do
    OUTFILE="${OUTDIR}/${ALGO}_${LABEL}_s${seed}.txt"
    if [ -f "$OUTFILE" ] && [ -s "$OUTFILE" ]; then
        echo "[skip] $OUTFILE exists"
        continue
    fi
    echo "[$ALGO:$LABEL] seed=$seed → $OUTFILE"
    TEMP=$(mktemp -d)
    cp -r /project/instances/PROSTATE_sampled/* "$TEMP/"
    sed -i "s/^w_under.*/w_under       ${W_UNDER:-1.0}/" "$TEMP/instance_config.txt"
    sed -i "s/^w_over.*/w_over        ${W_OVER:-0.5}/" "$TEMP/instance_config.txt"
    sed -i "s/^w_ptv_over.*/w_ptv_over    ${W_PTV_OVER:-0.5}/" "$TEMP/instance_config.txt"
    /project/build/emili "$TEMP" baoimrt 4 "$@" rnds "$seed" > "$OUTFILE" 2>&1
    rm -rf "$TEMP"
done
echo "[$ALGO:$LABEL] DONE seeds $SEED_START-$SEED_END"
