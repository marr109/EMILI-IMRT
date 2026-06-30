#!/usr/bin/env python3
"""Wilcoxon signed-rank test — manual implementation, no dependencies."""
import re, os, math

def get_f_star(filepath):
    try:
        with open(filepath) as f:
            for line in f:
                m = re.search(r'Objective function value:\s*([\d.]+)', line)
                if m: return float(m.group(1))
    except: pass
    return None

def wilcoxon_paired(tuned, base):
    """One-sided Wilcoxon: H1 = tuned < base."""
    n = len(tuned)
    diffs = [t - b for t, b in zip(tuned, base)]
    abs_diffs = [abs(d) for d in diffs]
    # Rank (handle ties with average)
    indexed = sorted(enumerate(abs_diffs), key=lambda x: x[1])
    ranks = [0] * n
    i = 0
    while i < n:
        j = i
        while j < n and indexed[j][1] == indexed[i][1]:
            j += 1
        avg_rank = (i + j + 1) / 2  # 1-based average rank
        for k in range(i, j):
            ranks[indexed[k][0]] = avg_rank
        i = j
    # W = sum of ranks for negative differences
    W = sum(r for r, d in zip(ranks, diffs) if d < 0)
    expected = n*(n+1)/4
    # Normal approximation
    # Variance with ties
    tie_correction = 0
    i = 0
    while i < n:
        j = i
        while j < n and indexed[j][1] == indexed[i][1]:
            j += 1
        t = j - i
        if t > 1:
            tie_correction += t*(t-1)*(t+1)
        i = j
    var_W = n*(n+1)*(2*n+1)/24 - tie_correction/48
    z = (W - expected) / math.sqrt(var_W)
    # Normal CDF approximation
    p = 0.5 * math.erfc(z / math.sqrt(2))  # upper tail: P(Z >= z)
    return W, expected, z, p, diffs

algos = {'ils': 'ILS (pgreedy D=2)', 'vns': 'VNS (irandomk)', 'tabu': 'Tabu (tenure=7)'}

print("=" * 70)
print("WILCOXON SIGNED-RANK TEST — H1: tuned < baseline (one-sided)")
print("=" * 70)

for code, name in algos.items():
    tuned, base = [], []
    for seed in range(42, 52):
        tv = get_f_star(f'resultados/{code}_tuned_s{seed}.txt')
        bv = get_f_star(f'resultados/{code}_baseline_s{seed}.txt')
        if tv and bv:
            tuned.append(tv); base.append(bv)
    
    if len(tuned) < 5:
        print(f"\n{name}: insuficientes ({len(tuned)} pairs)")
        continue
    
    W, exp, z, p, diffs = wilcoxon_paired(tuned, base)
    mean_t = sum(tuned)/len(tuned)
    mean_b = sum(base)/len(base)
    imp = (1 - mean_t/mean_b)*100
    all_neg = all(d < 0 for d in diffs)
    
    print(f"\n{name}  (n={len(tuned)})")
    print(f"  Tuned μ  = {mean_t:.0f}")
    print(f"  Base  μ  = {mean_b:.0f}")
    print(f"  Mejora   = {imp:.1f}%")
    print(f"  Todos tuned < base: {'SI' if all_neg else 'NO'}")
    print(f"  W = {W:.1f} (expected {exp:.1f})")
    print(f"  z = {z:.3f}")
    print(f"  p = {p:.8f}")
    print(f"  p < 0.05: {'YES ✅' if p < 0.05 else 'NO'}")
    print(f"  p < 0.01: {'YES ✅' if p < 0.01 else 'NO'}")
    print(f"  p < 0.001: {'YES ✅' if p < 0.001 else 'NO'}")
