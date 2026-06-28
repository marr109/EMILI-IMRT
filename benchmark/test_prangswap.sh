#!/usr/bin/env bash
echo "=== ILS K=3 variando prangswap ==="
for p in 1 2 3 4; do
    printf "prangswap=%-2s ... " "$p"
    ./build/emili instances/MEDIUM_test baoimrt 3 ils best ifirstk tmaxiter 1 nangswap tmaxiter 3 prangswap "$p" improve rnds 42 > "benchmark/ils_prangswap_$p.txt" 2>&1
    F=$(awk '/Objective function value:/{print $NF}' "benchmark/ils_prangswap_$p.txt" | tail -1)
    A=$(grep "Found solution:" "benchmark/ils_prangswap_$p.txt" | head -1 | sed 's/.*angles=\[//;s/\].*//')
    echo "f=$F  angles=[$A]"
done
echo "=== FIN ==="
