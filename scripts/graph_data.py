#!/usr/bin/env python3
"""Generate graph data for the EMILI IMRT paper from validation results."""
import re, os, glob, json

def get_metrics(filepath):
    with open(filepath) as f:
        text = f.read()
    f_star = re.findall(r'Objective function value:\s*([\d.]+)', text)
    v95 = re.findall(r'V95% of Rx\s*=\s*([\d.]+)', text)
    hi = re.findall(r'Homogeneity Index.*=\s*([\d.]+)', text)
    ci = re.findall(r'Conformity Index.*=\s*([\d.]+)', text)
    angles = re.findall(r'angles=\[([^\]]+)\]', text)
    return {
        'f': float(f_star[-1]) if f_star else None,
        'v95': float(v95[-1]) if v95 else None,
        'hi': float(hi[-1]) if hi else None,
        'ci': float(ci[-1]) if ci else None,
        'angles': angles[-1] if angles else None
    }

# ===================== GRAPH 1: Boxplot f* data =====================
print("=" * 60)
print("GRAPH 1: Boxplot f* (tuned vs baseline, PROSTATE K=4)")
print("=" * 60)
for algo in ['ils', 'vns', 'tabu']:
    tuned_vals = []
    base_vals = []
    for seed in range(42, 52):
        tf = f'resultados/{algo}_tuned_s{seed}.txt'
        bf = f'resultados/{algo}_baseline_s{seed}.txt'
        if os.path.exists(tf):
            m = get_metrics(tf)
            if m['f']: tuned_vals.append(m['f'])
        if os.path.exists(bf):
            m = get_metrics(bf)
            if m['f']: base_vals.append(m['f'])
    print(f"\n{algo.upper()}_tuned = {tuned_vals}")
    print(f"{algo.upper()}_base   = {base_vals}")

# ===================== GRAPH 2: V95% bars =====================
print("\n" + "=" * 60)
print("GRAPH 2: V95% comparison (mean ± std)")
print("=" * 60)
for algo in ['ils', 'vns', 'tabu']:
    tuned_v95 = []; base_v95 = []
    for seed in range(42, 52):
        tf = f'resultados/{algo}_tuned_s{seed}.txt'
        bf = f'resultados/{algo}_baseline_s{seed}.txt'
        if os.path.exists(tf):
            m = get_metrics(tf)
            if m['v95']: tuned_v95.append(m['v95'])
        if os.path.exists(bf):
            m = get_metrics(bf)
            if m['v95']: base_v95.append(m['v95'])
    if tuned_v95 and base_v95:
        import statistics
        print(f"\n{algo.upper()}: base={statistics.mean(base_v95):.1f}% ± {statistics.stdev(base_v95):.1f}  →  tuned={statistics.mean(tuned_v95):.1f}% ± {statistics.stdev(tuned_v95):.1f}")

# ===================== GRAPH 3: DVH OK% bars =====================
print("\n" + "=" * 60)
print("GRAPH 3: DVH constraints OK%")
print("=" * 60)
for algo in ['ils', 'vns', 'tabu']:
    for variant in ['tuned', 'baseline']:
        ok_counts = []
        for seed in range(42, 52):
            f = f'resultados/{algo}_{variant}_s{seed}.txt'
            if os.path.exists(f):
                with open(f) as fh:
                    text = fh.read()
                ok = len(re.findall(r'\bOK\b', text))
                viol = len(re.findall(r'\bVIOL\b', text))
                if ok + viol > 0:
                    ok_counts.append(ok)
        if ok_counts:
            import statistics
            print(f"{algo}_{variant}: OK={statistics.mean(ok_counts):.1f}/{statistics.mean(ok_counts)+4:.0f}")

# ===================== GRAPH 4: irace convergence =====================
print("\n" + "=" * 60)
print("GRAPH 4: irace convergence (extract from irace logs)")
print("=" * 60)
for algo, path in [('ils', 'irace_prostate'), ('vns', 'irace_vns'), ('tabu', 'irace_tabu')]:
    logfile = f'{path}/results/irace_output.txt'
    if os.path.exists(logfile):
        # Extract mean best per iteration
        iters = re.findall(r'# (\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}).*End of iteration (\d+)', open(logfile).read())
        bests = re.findall(r'Best-so-far configuration:\s+\d+\s+mean value:\s+([\d.]+)', open(logfile).read())
        print(f"\n{algo.upper()} iterations: {[(i[1], float(b)) for i, b in zip(iters, bests)]}")

# ===================== GRAPH 5: Angle frequency =====================
print("\n" + "=" * 60)
print("GRAPH 5: Angle selection frequency (all tuned seeds)")
print("=" * 60)
for algo in ['ils', 'vns', 'tabu']:
    angle_counts = {}
    total_seeds = 0
    for seed in range(42, 52):
        f = f'resultados/{algo}_tuned_s{seed}.txt'
        if os.path.exists(f):
            m = get_metrics(f)
            if m['angles']:
                total_seeds += 1
                for a in m['angles'].split(','):
                    a = a.strip().replace('deg', '')
                    angle_counts[a] = angle_counts.get(a, 0) + 1
    if angle_counts:
        print(f"\n{algo.upper()} angle frequency ({total_seeds} seeds):")
        for a in sorted(angle_counts.keys(), key=lambda x: int(x)):
            print(f"  {a}°: {angle_counts[a]}/{total_seeds}")

# ===================== GRAPH 6: rVNS ablation =====================
print("\n" + "=" * 60)
print("GRAPH 6: rVNS ablation — GVNS vs rVNS")
print("=" * 60)
rvns_f = get_metrics('resultados/rvns_s42.txt') if os.path.exists('resultados/rvns_s42.txt') else None
gvns_f = get_metrics('resultados/vns_tuned_s42.txt') if os.path.exists('resultados/vns_tuned_s42.txt') else None
if rvns_f and gvns_f:
    print(f"\nGVNS (con LS):  f*={gvns_f['f']:.0f}, V95%={gvns_f['v95']:.1f}%")
    print(f"rVNS (sin LS):  f*={rvns_f['f']:.0f}, V95%={rvns_f['v95']:.1f}%")
    print(f"Δ_LS (aporte LS) = {gvns_f['f'] - rvns_f['f']:.0f} ({abs(gvns_f['f']-rvns_f['f'])/gvns_f['f']*100:.1f}%)")
