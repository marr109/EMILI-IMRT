#!/usr/bin/env python3
"""Generate all graphs for the EMILI IMRT paper."""
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np
import re, os

os.makedirs('/project/docs/figures', exist_ok=True)

# ========== DATA ==========
ils_tuned  = [4280.55, 4217.67, 4235.16, 4217.98, 4217.98, 4322.73, 4217.67, 4260.42, 4179.08, 4322.73]
ils_base   = [8762.89, 8995.26, 8590.95, 9612.84, 9482.63, 9443.41, 8834.07, 8925.48, 8846.70]
vns_tuned  = [4792.46, 4893.89, 4694.22, 4885.32, 4629.48, 4728.05, 4708.43, 4666.77, 4817.96, 4738.00]
vns_base   = [8896.15, 9034.82, 8629.99, 8505.24, 8949.32, 8134.36, 8436.79, 8480.74, 8416.32]
tabu_tuned = [1800.29]*10
tabu_base  = [9420.47]*9

# V95% data
v95_data = {
    'ILS':  (86.4, 1.1, 96.3, 0.2),
    'VNS':  (87.5, 1.1, 95.5, 0.5),
    'Tabu': (86.2, 0.0, 98.4, 0.0),
}

# irace convergence
irace_ils  = [(17,65742),(19,70257),(44,50568),(80,53792),(112,50754),(181,53030),(222,49103),(253,47337),(276,45876),(291,45992),(301,47337),(309,46250),(315,47384),(320,47337),(325,46454)]
irace_vns  = [(16,55896),(22,61229),(41,63895),(68,62786),(134,50891),(163,53770),(198,51849),(243,53527),(261,54621),(274,52440),(288,53028),(294,53467),(301,52985),(305,52582),(324,52217),(325,51275),(327,51275),(328,51275)]
irace_tabu = [(16,50110),(19,57371),(56,61001),(98,57175),(131,57371),(157,52949),(200,54749),(242,53040),(255,50149),(279,50031),(284,48687),(292,47549),(295,47642),(297,48687),(303,47763),(305,48677),(313,48677),(316,48677),(319,48677),(320,48677)]

# Time data
time_data = {
    'ILS':  (3656, 424),
    'VNS':  (1338, 2199),
    'Tabu': (720, 773),
}

# ========== GRAPH 1: Boxplot f* ==========
fig, ax = plt.subplots(figsize=(10, 6))
algos = ['ILS', 'VNS', 'Tabu']
tuned_data = [ils_tuned, vns_tuned, tabu_tuned]
base_data = [ils_base, vns_base, tabu_base]
x = np.arange(len(algos))
w = 0.35
bp1 = ax.boxplot(tuned_data, positions=x-w/2, widths=w*0.8, patch_artist=True,
                  boxprops=dict(facecolor='#2ecc71'), medianprops=dict(color='darkgreen'))
bp2 = ax.boxplot(base_data, positions=x+w/2, widths=w*0.8, patch_artist=True,
                  boxprops=dict(facecolor='#e74c3c'), medianprops=dict(color='darkred'))
ax.set_xticks(x)
ax.set_xticklabels(algos)
ax.set_ylabel('f* (objective value)')
ax.set_title('PROSTATE K=4 — f* tuned vs baseline (10 seeds)')
ax.legend([bp1['boxes'][0], bp2['boxes'][0]], ['Tuned (irace)', 'Baseline (manual)'])
ax.grid(axis='y', alpha=0.3)
plt.tight_layout()
plt.savefig('/project/docs/figures/boxplot_fstar.png', dpi=150)
plt.close()

# ========== GRAPH 2: V95% bars ==========
fig, ax = plt.subplots(figsize=(8, 5))
x = np.arange(len(algos))
w = 0.35
base_v95 = [v95_data[a][0] for a in algos]
base_err = [v95_data[a][1] for a in algos]
tuned_v95 = [v95_data[a][2] for a in algos]
tuned_err = [v95_data[a][3] for a in algos]
ax.bar(x-w/2, base_v95, w, yerr=base_err, color='#e74c3c', label='Baseline', capsize=5)
ax.bar(x+w/2, tuned_v95, w, yerr=tuned_err, color='#2ecc71', label='Tuned (irace)', capsize=5)
ax.set_xticks(x)
ax.set_xticklabels(algos)
ax.set_ylabel('V95%')
ax.set_title('Cobertura PTV — V95% (μ ± σ, 10 seeds)')
ax.legend()
ax.set_ylim(80, 100)
ax.grid(axis='y', alpha=0.3)
plt.tight_layout()
plt.savefig('/project/docs/figures/v95_bars.png', dpi=150)
plt.close()

# ========== GRAPH 3: irace convergence ==========
fig, ax = plt.subplots(figsize=(10, 6))
for label, data, color in [('ILS', irace_ils, '#3498db'), ('VNS', irace_vns, '#e67e22'), ('Tabu', irace_tabu, '#9b59b6')]:
    t = [d[0] for d in data]
    f = [d[1] for d in data]
    ax.plot(t, f, 'o-', label=label, color=color, markersize=4, linewidth=1.5)
ax.set_xlabel('Wall time (minutes)')
ax.set_ylabel('Mean best f*')
ax.set_title('Convergencia de irace — PROSTATE_tiny')
ax.legend()
ax.grid(alpha=0.3)
plt.tight_layout()
plt.savefig('/project/docs/figures/irace_convergence.png', dpi=150)
plt.close()

# ========== GRAPH 4: Time comparison ==========
fig, ax = plt.subplots(figsize=(8, 5))
x = np.arange(len(algos))
tuned_times = [time_data[a][0]/60 for a in algos]
base_times = [time_data[a][1]/60 for a in algos]
ax.bar(x-w/2, base_times, w, color='#e74c3c', label='Baseline')
ax.bar(x+w/2, tuned_times, w, color='#2ecc71', label='Tuned')
for i, (tt, bt) in enumerate(zip(tuned_times, base_times)):
    ratio = tt/bt
    ax.annotate(f'{ratio:.1f}×', (i+w/2, tt), textcoords="offset points", xytext=(0,5), ha='center', fontsize=9)
ax.set_xticks(x)
ax.set_xticklabels(algos)
ax.set_ylabel('Tiempo (minutos)')
ax.set_title('Tiempo de ejecución — tuned vs baseline (PROSTATE real)')
ax.legend()
ax.grid(axis='y', alpha=0.3)
plt.tight_layout()
plt.savefig('/project/docs/figures/time_comparison.png', dpi=150)
plt.close()

# ========== GRAPH 5: Cross-val comparison ==========
fig, ax = plt.subplots(figsize=(8, 5))
algos = ['ILS', 'VNS', 'Tabu']
sampled_f  = [4280, 4792, 1800]
sampled2_f = [3699, 4281, 1431]
x = np.arange(len(algos))
w = 0.35
ax.bar(x-w/2, sampled_f, w, color='#3498db', label='PROSTATE_sampled')
ax.bar(x+w/2, sampled2_f, w, color='#1abc9c', label='PROSTATE_sampled2')
ax.set_xticks(x)
ax.set_xticklabels(algos)
ax.set_ylabel('f*')
ax.set_title('Validación cruzada — tuned en dos instancias')
ax.legend()
ax.grid(axis='y', alpha=0.3)
plt.tight_layout()
plt.savefig('/project/docs/figures/crossval.png', dpi=150)
plt.close()

# ========== GRAPH 6: rVNS ablation ==========
fig, ax = plt.subplots(figsize=(6, 4))
ax.bar(['GVNS (con LS)', 'rVNS (sin LS)'], [4792, 4878], color=['#2ecc71', '#f39c12'])
ax.set_ylabel('f*')
ax.set_title('Ablación — aporte del Local Search en VNS')
ax.text(0, 4792, '4792', ha='center', va='bottom')
ax.text(1, 4878, '4878', ha='center', va='bottom')
ax.annotate('Δ = 86 (1.8%)', xy=(0.5, 4835), fontsize=11, ha='center',
            bbox=dict(boxstyle='round', facecolor='lightyellow'))
ax.grid(axis='y', alpha=0.3)
plt.tight_layout()
plt.savefig('/project/docs/figures/rvns_ablation.png', dpi=150)
plt.close()

# ========== GRAPH 7: DVH constraints ==========
fig, ax = plt.subplots(figsize=(8, 5))
algos = ['ILS', 'VNS', 'Tabu']
base_ok = [33, 33, 33]
tuned_ok = [50, 50, 50]
x = np.arange(len(algos))
w = 0.35
ax.bar(x-w/2, base_ok, w, color='#e74c3c', label='Baseline')
ax.bar(x+w/2, tuned_ok, w, color='#2ecc71', label='Tuned')
ax.set_xticks(x)
ax.set_xticklabels(algos)
ax.set_ylabel('% constraints OK')
ax.set_title('DVH Clinical Constraints — % satisfechos')
ax.legend()
ax.set_ylim(0, 60)
ax.grid(axis='y', alpha=0.3)
plt.tight_layout()
plt.savefig('/project/docs/figures/dvh_constraints.png', dpi=150)
plt.close()

print("✅ 7 graphs saved to docs/figures/")
for f in sorted(os.listdir('/project/docs/figures')):
    print(f"  {f}")
