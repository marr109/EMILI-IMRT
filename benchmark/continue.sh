#!/usr/bin/env bash
# Continuación benchmark — desde Greedy_K4
set -euo pipefail

INST="instances/MEDIUM_test"
SEED=42

for K in 4 5; do
    # 2) Iterated Greedy
    NAME="Greedy_K${K}"
    echo -n "[$NAME] "
    START=$(date +%s)
    ./build/emili "$INST" baoimrt "$K" ils best ifirstk tmaxiter 1 nangswap \
        tmaxiter 3 pgreedy 2 improve rnds "$SEED" > "benchmark/${NAME}.txt" 2>&1
    T=$(( $(date +%s) - START ))
    F=$(grep "Objective function value:" "benchmark/${NAME}.txt" | awk '{print $NF}' | tail -1)
    echo "f=$F  time=${T}s"
    
    # 3) Tabu fixed
    NAME="TabuFixed_K${K}"
    echo -n "[$NAME] "
    START=$(date +%s)
    ./build/emili "$INST" baoimrt "$K" tabu best ifirstk tmaxiter 10 nangswap \
        TBao_fixed 5 rnds "$SEED" > "benchmark/${NAME}.txt" 2>&1
    T=$(( $(date +%s) - START ))
    F=$(grep "Objective function value:" "benchmark/${NAME}.txt" | awk '{print $NF}' | tail -1)
    echo "f=$F  time=${T}s"
    
    # 4) Tabu adaptive
    NAME="TabuAdaptive_K${K}"
    echo -n "[$NAME] "
    START=$(date +%s)
    ./build/emili "$INST" baoimrt "$K" tabu best ifirstk tmaxiter 10 nangswap \
        TBao_adaptive 2 8 rnds "$SEED" > "benchmark/${NAME}.txt" 2>&1
    T=$(( $(date +%s) - START ))
    F=$(grep "Objective function value:" "benchmark/${NAME}.txt" | awk '{print $NF}' | tail -1)
    echo "f=$F  time=${T}s"

    # 5) VNS
    NAME="VNS_K${K}"
    echo -n "[$NAME] "
    START=$(date +%s)
    ./build/emili "$INST" baoimrt "$K" vns best ifirstk tmaxiter 1 nangswap \
        tmaxiter 2 bangshake 2 accng improve rnds "$SEED" > "benchmark/${NAME}.txt" 2>&1
    T=$(( $(date +%s) - START ))
    F=$(grep "Objective function value:" "benchmark/${NAME}.txt" | awk '{print $NF}' | tail -1)
    echo "f=$F  time=${T}s"
    
    echo ""
done

# Tabla resumen
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
    OK_COUNT=$(grep -c "OK$" "$f" 2>/dev/null || echo "0")
    VIOL_COUNT=$(grep -c "VIOL$" "$f" 2>/dev/null || echo "0")
    printf "%-20s %3s %12s %8s %8s %8s %s/%s\n" \
        "$ALGO" "$K" "${OBJ:-N/A}" "${CI:-N/A}" "${HI:-N/A}" "${V95:-N/A}" "${OK_COUNT:-0}" "$(( OK_COUNT + VIOL_COUNT ))"
done
echo "=============================================="
