#!/usr/bin/env bash
# Cross-validation on PROSTATE_sampled2, seed=42
set -euo pipefail

run_xval() {
    local ALGO="$1" LABEL="$2" W_U="$3" W_O="$4" W_P="$5"
    shift 5
    local TEMP=$(mktemp -d)
    cp -r /project/instances/PROSTATE_sampled2/* "$TEMP/"
    sed -i "s/^w_under.*/w_under       $W_U/" "$TEMP/instance_config.txt"
    sed -i "s/^w_over.*/w_over        $W_O/" "$TEMP/instance_config.txt"
    sed -i "s/^w_ptv_over.*/w_ptv_over    $W_P/" "$TEMP/instance_config.txt"
    echo "[xval] $ALGO $LABEL"
    /project/build/emili "$TEMP" baoimrt 4 "$@" rnds 42
    rm -rf "$TEMP"
}

case "$1" in
    ils_tuned)   run_xval ils tuned   1.01 0.17 0.14 ils first ifirstk tmaxiter 1 nangswap tmaxiter 2 pgreedy 2 improve ;;
    ils_base)    run_xval ils base    1.0  0.5  0.5  ils first ifirstk tmaxiter 1 nangswap tmaxiter 2 prangswap 1 improve ;;
    vns_tuned)   run_xval vns tuned   1.02 0.19 0.10 vns first irandomk tmaxiter 1 nangswap tmaxiter 2 bangshake 2 accng improve ;;
    vns_base)    run_xval vns base    1.0  0.5  0.5  vns first ifirstk tmaxiter 1 nangswap tmaxiter 2 bangshake 2 accng improve ;;
    tabu_tuned)  run_xval tabu tuned  1.02 0.04 0.23 tabu first ifirstk tmaxiter 6 nangswap TBao_fixed 7 ;;
    tabu_base)   run_xval tabu base   1.0  0.5  0.5  tabu first ifirstk tmaxiter 10 nangswap TBao_fixed 5 ;;
    *) echo "Usage: $0 {ils|vns|tabu}_{tuned|base}"; exit 1 ;;
esac
