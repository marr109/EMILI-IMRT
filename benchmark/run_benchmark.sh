#!/usr/bin/env bash
# =============================================================================
# Fase 1 — Benchmark de 5 metaheurísticas × 3 valores de K en MEDIUM_test
# =============================================================================
set -euo pipefail

EMILI="./build/emili"
INST="instances/MEDIUM_test"
OUTDIR="benchmark"
SEED=42
mkdir -p "$OUTDIR"

echo "=============================================="
echo " BENCHMARK BAO — MEDIUM_test"
echo " $(date)"
echo "=============================================="
echo ""

# Each run outputs: algo K f obj_time(s) angles
for K in 3 4 5; do
    # 1) ILS puro
    NAME="ILS_K${K}"
    echo -n "[$NAME] "
    START=$(date +%s)
    ./build/emili "$INST" baoimrt "$K" ils best ifirstk tmaxiter 1 nangswap \
        tmaxiter 3 prangswap 1 improve rnds "$SEED" > "$OUTDIR/${NAME}.txt" 2>&1
    F=$(grep "Objective function value:" "$OUTDIR/${NAME}.txt" | awk '{print $NF}' | tail -1)
    T=$(( $(date +%s) - START ))
    echo "f=$F  time=${T}s"
    
    # 2) Iterated Greedy
    NAME="Greedy_K${K}"
    echo -n "[$NAME] "
    START=$(date +%s)
    ./build/emili "$INST" baoimrt "$K" ils best ifirstk tmaxiter 1 nangswap \
        tmaxiter 3 pgreedy 2 improve rnds "$SEED" > "$OUTDIR/${NAME}.txt" 2>&1
    F=$(grep "Objective function value:" "$OUTDIR/${NAME}.txt" | awk '{print $NF}' | tail -1)
    T=$(( $(date +%s) - START ))
    echo "f=$F  time=${T}s"
    
    # 3) Tabu fixed (tenure=5)
    NAME="TabuFixed_K${K}"
    echo -n "[$NAME] "
    START=$(date +%s)
    ./build/emili "$INST" baoimrt "$K" tabu best ifirstk tmaxiter 10 nangswap \
        TBao_fixed 5 rnds "$SEED" > "$OUTDIR/${NAME}.txt" 2>&1
    F=$(grep "Objective function value:" "$OUTDIR/${NAME}.txt" | awk '{print $NF}' | tail -1)
    T=$(( $(date +%s) - START ))
    echo "f=$F  time=${T}s"
    
    # 4) Tabu adaptive (tenure_min=2, tenure_max=8)
    NAME="TabuAdaptive_K${K}"
    echo -n "[$NAME] "
    START=$(date +%s)
    ./build/emili "$INST" baoimrt "$K" tabu best ifirstk tmaxiter 10 nangswap \
        TBao_adaptive 2 8 rnds "$SEED" > "$OUTDIR/${NAME}.txt" 2>&1
    F=$(grep "Objective function value:" "$OUTDIR/${NAME}.txt" | awk '{print $NF}' | tail -1)
    T=$(( $(date +%s) - START ))
    echo "f=$F  time=${T}s"

    # 5) VNS
    NAME="VNS_K${K}"
    echo -n "[$NAME] "
    START=$(date +%s)
    ./build/emili "$INST" baoimrt "$K" vns best ifirstk tmaxiter 1 nangswap \
        tmaxiter 2 bangshake 2 accng improve rnds "$SEED" > "$OUTDIR/${NAME}.txt" 2>&1
    F=$(grep "Objective function value:" "$OUTDIR/${NAME}.txt" | awk '{print $NF}' | tail -1)
    T=$(( $(date +%s) - START ))
    echo "f=$F  time=${T}s"
    
    echo ""
done

# ── Tabla resumen ──────────────────────────────────────────────────────────
echo "=============================================="
echo " RESUMEN"
echo "=============================================="
printf "%-20s %3s %12s %8s %8s %8s %8s\n" "ALGORITMO" "K" "f" "CI" "HI" "V95%" "DVH_OK"
echo "----------------------------------------------------------------------"
for f in benchmark/*.txt; do
    NAME=$(basename "$f" .txt)
    K=$(echo "$NAME" | sed 's/.*_K//')
    ALGO=$(echo "$NAME" | sed 's/_K.*//')
    OBJ=$(grep "Objective function value:" "$f" | awk '{print $NF}' | tail -1)
    CI=$(grep "Conformity Index" "$f" | awk '{print $NF}')
    HI=$(grep "Homogeneity Index" "$f" | awk '{print $NF}')
    V95=$(grep "PTV coverage" "$f" | awk '{print $NF}')
    OK_COUNT=$(grep -c "OK$" "$f" || true)
    VIOL_COUNT=$(grep -c "VIOL$" "$f" || true)
    printf "%-20s %3s %12.0f %8.3f %8.3f %8s %4s/%s\n" \
        "$ALGO" "$K" "$OBJ" "${CI:-N/A}" "${HI:-N/A}" "${V95:-N/A}" "${OK_COUNT:-0}" "$(( OK_COUNT + VIOL_COUNT ))"
done
echo "=============================================="
echo "Resultados completos en benchmark/*.txt"
