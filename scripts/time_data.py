#!/usr/bin/env python3
"""Extract time data for temporal graphs."""
import re, os, glob

# ===================== TIME per seed (from result files) =====================
print("=" * 60)
print("TIME DATA: Tuned vs Baseline (seconds per run)")
print("=" * 60)

for algo in ['ils', 'vns', 'tabu']:
    for variant in ['tuned', 'baseline']:
        times = []
        for seed in range(42, 52):
            f = f'resultados/{algo}_{variant}_s{seed}.txt'
            if os.path.exists(f):
                with open(f) as fh:
                    text = fh.read()
                t = re.findall(r'time\s*:\s*([\d.]+)', text)
                if t:
                    times.append(float(t[-1]))
        if times:
            import statistics
            print(f"{algo}_{variant}: mean={statistics.mean(times):.0f}s, range=[{min(times):.0f}-{max(times):.0f}], n={len(times)}")

# ===================== irace convergence over TIME =====================
print("\n" + "=" * 60)
print("IRACE CONVERGENCE: Iteration + Wall Time + Mean Best")
print("=" * 60)

for algo, path in [('ils', 'irace_prostate'), ('vns', 'irace_vns'), ('tabu', 'irace_tabu')]:
    logfile = f'{path}/results/irace_output.txt'
    if os.path.exists(logfile):
        text = open(logfile).read()
        # Extract timeUsed and best-so-far per iteration
        time_used = re.findall(r'timeUsed:\s*(\d+)', text)
        best_vals = re.findall(r'Best-so-far configuration:\s+\d+\s+mean value:\s+([\d.]+)', text)
        iters = re.findall(r'End of iteration (\d+)', text)
        
        print(f"\n{algo.upper()}:")
        for i in range(min(len(time_used), len(best_vals), len(iters))):
            t = int(time_used[i])
            h, m = t//3600, (t%3600)//60
            print(f"  iter {iters[i]:>2}: {h}h{m:02d}m  mean_best={best_vals[i]}")

# ===================== CROSS-VAL times =====================
print("\n" + "=" * 60)
print("CROSS-VAL TIME (PROSTATE_sampled2)")
print("=" * 60)
for f in sorted(glob.glob('resultados/xval_*.txt')):
    try:
        with open(f) as fh:
            text = fh.read()
        t = re.findall(r'time\s*:\s*([\d.]+)', text)
        obj = re.findall(r'Objective function value:\s*([\d.]+)', text)
        name = os.path.basename(f).replace('.txt','')
        if t and obj:
            print(f"  {name}: time={float(t[-1]):.0f}s  f*={obj[-1]}")
    except:
        pass
