#!/usr/bin/env python3
"""
plot_bao_results.py
===================
Genera gráficas de convergencia y análisis de resultados BAO+FMO.

Uso:
    python3 plot_bao_results.py                  # usa CSVs por defecto
    python3 plot_bao_results.py k3.csv k4.csv    # archivos específicos
"""

import sys
import os
import pandas as pd
import numpy as np
import matplotlib
matplotlib.use('Agg')   # sin display (genera archivos PNG)
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
from matplotlib.gridspec import GridSpec

# ── Configuración visual ───────────────────────────────────────────────────────
plt.rcParams.update({
    'font.family': 'DejaVu Sans',
    'font.size': 11,
    'axes.titlesize': 13,
    'axes.labelsize': 12,
    'legend.fontsize': 10,
    'figure.dpi': 150,
})

COLORS = {
    'k3': '#2196F3',   # azul
    'k4': '#F44336',   # rojo
    'best': '#4CAF50', # verde
    'pert': '#FF9800', # naranja (puntos de perturbación ILS)
    'grid': '#E0E0E0',
}


def load_csv(path):
    df = pd.read_csv(path)
    df['angles_list'] = df['angles_deg'].str.split(';').apply(
        lambda x: [int(a) for a in x]
    )
    df['n_angles'] = df['angles_list'].apply(len)
    df['best_so_far'] = df['objective'].cummin()
    df['improvement'] = df['best_so_far'].diff().fillna(0) < 0
    return df


def detect_ils_restarts(df, K, n_candidates):
    """
    Detecta los puntos de reinicio ILS estimando el tamaño del vecindario.
    Vecindario = K × (n_candidates - K).  Los reinicios ocurren después
    de cada sweep completo + la evaluación de perturbación.
    """
    sweep_size = K * (n_candidates - K) + 1  # +1 para la solución inicial
    restarts = []
    i = sweep_size  # primer reinicio después del primer sweep
    while i < len(df):
        restarts.append(i)    # índice de la evaluación perturba
        i += sweep_size + 1   # sweep + 1 evaluación perturba
    return restarts


# ══════════════════════════════════════════════════════════════════════════════
# FIGURA 1 — Convergencia principal
# ══════════════════════════════════════════════════════════════════════════════

def fig_convergence(dfs, labels, colors, restart_lists, outfile):
    fig, axes = plt.subplots(1, 2, figsize=(14, 6))
    fig.suptitle('BAO+FMO — Curvas de Convergencia (instancia PROSTATE, 36 ángulos candidatos)',
                 fontweight='bold', y=1.01)

    for ax, df, label, color, restarts in zip(
            axes, dfs, labels, colors, restart_lists):

        # Todas las evaluaciones
        ax.scatter(df['eval'], df['objective'] / 1e6, color=color,
                   alpha=0.25, s=12, label='Evaluaciones')

        # Mejor hasta ahora (línea gruesa)
        ax.plot(df['eval'], df['best_so_far'] / 1e6,
                color=color, linewidth=2.5, label='Mejor acumulado')

        # Marcar reinicios ILS (perturbación)
        for r in restarts:
            if r < len(df):
                ax.axvline(x=df['eval'].iloc[r], color=COLORS['pert'],
                           linestyle='--', alpha=0.6, linewidth=1.2)

        # Marcar mejoras reales
        mejoras = df[df['improvement']]
        ax.scatter(mejoras['eval'], mejoras['best_so_far'] / 1e6,
                   color=COLORS['best'], zorder=5, s=60,
                   marker='*', label=f'Mejora ({len(mejoras)} veces)')

        # Anotación del mejor resultado
        best_val = df['best_so_far'].iloc[-1]
        best_eval = df[df['objective'] == best_val]['eval'].iloc[0]
        ax.annotate(f'f*={best_val/1e6:.3f}M',
                    xy=(best_eval, best_val/1e6),
                    xytext=(best_eval + len(df)*0.05, best_val/1e6 - 0.005),
                    fontsize=9, color=COLORS['best'],
                    arrowprops=dict(arrowstyle='->', color=COLORS['best'],
                                   lw=1.2))

        ax.set_xlabel('Evaluación FMO (#)')
        ax.set_ylabel('Objetivo f (×10⁶)')
        ax.set_title(f'{label}')
        ax.legend(loc='upper right')
        ax.grid(True, color=COLORS['grid'], linewidth=0.8)
        ax.yaxis.set_major_formatter(plt.FuncFormatter(lambda v, _: f'{v:.2f}'))

        # Línea de inicio del segundo y tercer restart
        pert_patch = mpatches.Patch(color=COLORS['pert'], alpha=0.6,
                                    label='Reinicio ILS (perturbación)')
        handles, lbls = ax.get_legend_handles_labels()
        ax.legend(handles + [pert_patch], lbls + ['Reinicio ILS'],
                  loc='upper right', fontsize=9)

    plt.tight_layout()
    plt.savefig(outfile, bbox_inches='tight')
    print(f'  → {outfile}')
    plt.close()


# ══════════════════════════════════════════════════════════════════════════════
# FIGURA 2 — Distribución del objetivo y comparativa K=3 vs K=4
# ══════════════════════════════════════════════════════════════════════════════

def fig_comparison(dfs, labels, colors, outfile):
    fig = plt.figure(figsize=(14, 10))
    gs = GridSpec(2, 2, figure=fig, hspace=0.4, wspace=0.35)
    fig.suptitle('Análisis comparativo BAO — K=3 vs K=4',
                 fontweight='bold', fontsize=14)

    # ── Histograma de valores f explorados ─────────────────────────────────
    ax_hist = fig.add_subplot(gs[0, :])
    for df, label, color in zip(dfs, labels, colors):
        ax_hist.hist(df['objective'] / 1e6, bins=30, alpha=0.55,
                     color=color, label=label, edgecolor='white', linewidth=0.5)
        ax_hist.axvline(df['best_so_far'].iloc[-1] / 1e6,
                        color=color, linestyle='--', linewidth=2,
                        label=f'Mejor {label}: {df["best_so_far"].iloc[-1]/1e6:.4f}M')

    ax_hist.set_xlabel('Valor objetivo f (×10⁶)')
    ax_hist.set_ylabel('Frecuencia')
    ax_hist.set_title('Distribución de todos los valores evaluados')
    ax_hist.legend()
    ax_hist.grid(True, color=COLORS['grid'])

    # ── Box plot de mejora relativa por restart ─────────────────────────────
    ax_box = fig.add_subplot(gs[1, 0])
    data_box, tick_labels = [], []
    for df, label, color in zip(dfs, labels, colors):
        vals = df['objective'].values
        n = len(vals)
        # Dividir en tercios (aproximadamente un restart por tercio)
        thirds = np.array_split(vals, 3)
        for i, t in enumerate(thirds):
            data_box.append(t / 1e6)
            tick_labels.append(f'{label}\nR{i+1}')

    bp = ax_box.boxplot(data_box, patch_artist=True, notch=False)
    k3_col = colors[0]; k4_col = colors[1]
    box_colors = [k3_col]*3 + [k4_col]*3
    for patch, c in zip(bp['boxes'], box_colors):
        patch.set_facecolor(c); patch.set_alpha(0.5)
    ax_box.set_xticklabels(tick_labels, fontsize=8)
    ax_box.set_ylabel('Objetivo f (×10⁶)')
    ax_box.set_title('Distribución por restart ILS')
    ax_box.grid(True, color=COLORS['grid'], axis='y')

    # ── Mejora acumulada normalizada ────────────────────────────────────────
    ax_norm = fig.add_subplot(gs[1, 1])
    for df, label, color in zip(dfs, labels, colors):
        f0 = df['objective'].iloc[0]
        fstar = df['best_so_far'].iloc[-1]
        normalized = (f0 - df['best_so_far']) / (f0 - fstar + 1e-9) * 100
        ax_norm.plot(df['eval'], normalized, color=color, linewidth=2.5,
                     label=label)

    ax_norm.set_xlabel('Evaluación FMO (#)')
    ax_norm.set_ylabel('Mejora relativa (%)')
    ax_norm.set_title('Progreso de mejora\n(0% = inicio, 100% = mejor encontrado)')
    ax_norm.legend()
    ax_norm.grid(True, color=COLORS['grid'])
    ax_norm.set_ylim(-5, 105)

    plt.savefig(outfile, bbox_inches='tight')
    print(f'  → {outfile}')
    plt.close()


# ══════════════════════════════════════════════════════════════════════════════
# FIGURA 3 — Análisis de ángulos seleccionados
# ══════════════════════════════════════════════════════════════════════════════

def fig_angles(dfs, labels, colors, n_candidates, outfile):
    fig, axes = plt.subplots(1, 2, figsize=(14, 5))
    fig.suptitle('Análisis de ángulos evaluados — frecuencia y calidad',
                 fontweight='bold')

    candidate_angles = list(range(0, 360, 360 // n_candidates))[:n_candidates]

    for ax, df, label, color in zip(axes, dfs, labels, colors):
        # Contar apariciones de cada ángulo
        angle_counts = {a: 0 for a in candidate_angles}
        angle_best   = {a: float('inf') for a in candidate_angles}

        for _, row in df.iterrows():
            for a in row['angles_list']:
                angle_counts[a] = angle_counts.get(a, 0) + 1
                angle_best[a]   = min(angle_best.get(a, float('inf')),
                                      row['objective'])

        angles = sorted(angle_counts.keys())
        counts = [angle_counts[a] for a in angles]
        bests  = [angle_best[a] / 1e6 for a in angles]

        # Normalizar bests para colorear
        b_min, b_max = min(bests), max(bests)
        norm_b = [(b - b_min) / (b_max - b_min + 1e-9) for b in bests]
        bar_colors = [plt.cm.RdYlGn(1 - nb) for nb in norm_b]

        bars = ax.bar(range(len(angles)), counts, color=bar_colors,
                      edgecolor='white', linewidth=0.5)

        # Marcar el mejor conjunto final
        best_row = df.loc[df['objective'].idxmin()]
        best_angles = best_row['angles_list']
        for i, a in enumerate(angles):
            if a in best_angles:
                ax.bar(i, counts[i], color='none',
                       edgecolor='black', linewidth=2.5)

        ax.set_xticks(range(len(angles)))
        ax.set_xticklabels([f'{a}°' for a in angles],
                           rotation=45, ha='right', fontsize=8)
        ax.set_xlabel('Ángulo gantry (°)')
        ax.set_ylabel('Nº de veces evaluado')
        ax.set_title(f'{label}\n(borde negro = en mejor solución)')
        ax.grid(True, color=COLORS['grid'], axis='y')

        # Colorbar proxy
        sm = plt.cm.ScalarMappable(
            cmap='RdYlGn',
            norm=plt.Normalize(vmin=b_min, vmax=b_max))
        sm.set_array([])
        cbar = plt.colorbar(sm, ax=ax, shrink=0.8)
        cbar.set_label('Mejor f en ese ángulo (×10⁶)', fontsize=9)

    plt.tight_layout()
    plt.savefig(outfile, bbox_inches='tight')
    print(f'  → {outfile}')
    plt.close()


# ══════════════════════════════════════════════════════════════════════════════
# FIGURA 4 — Mapa de calor: combinaciones de ángulos para K=3
# ══════════════════════════════════════════════════════════════════════════════

def fig_heatmap(df, label, n_candidates, outfile):
    """Matriz de calor: eje X = ángulo A, eje Y = ángulo B,
    color = mejor f cuando ambos aparecen juntos."""
    candidate_angles = list(range(0, 360, 360 // n_candidates))[:n_candidates]
    n = len(candidate_angles)
    idx = {a: i for i, a in enumerate(candidate_angles)}

    heat = np.full((n, n), np.nan)
    count = np.zeros((n, n), dtype=int)

    for _, row in df.iterrows():
        angs = row['angles_list']
        f = row['objective']
        for i in range(len(angs)):
            for j in range(i + 1, len(angs)):
                a, b = idx.get(angs[i]), idx.get(angs[j])
                if a is None or b is None:
                    continue
                if np.isnan(heat[a, b]) or f < heat[a, b]:
                    heat[a, b] = f
                    heat[b, a] = f
                count[a, b] += 1
                count[b, a] += 1

    fig, ax = plt.subplots(figsize=(12, 10))
    im = ax.imshow(heat / 1e6, cmap='RdYlGn_r', aspect='auto')
    plt.colorbar(im, ax=ax, label='Mejor f cuando ambos ángulos activos (×10⁶)')

    tick_labels = [f'{a}°' for a in candidate_angles]
    ax.set_xticks(range(n)); ax.set_xticklabels(tick_labels, rotation=45, ha='right', fontsize=8)
    ax.set_yticks(range(n)); ax.set_yticklabels(tick_labels, fontsize=8)
    ax.set_title(f'Mapa de calor de pares de ángulos — {label}\n'
                 f'(verde = mejor f, rojo = peor f cuando ese par aparece juntos)',
                 fontweight='bold')

    # Marcar celdas donde hay datos
    for i in range(n):
        for j in range(n):
            if count[i, j] > 0 and not np.isnan(heat[i, j]):
                ax.text(j, i, str(count[i, j]), ha='center', va='center',
                        fontsize=6, color='black', alpha=0.6)

    plt.tight_layout()
    plt.savefig(outfile, bbox_inches='tight', dpi=120)
    print(f'  → {outfile}')
    plt.close()


# ══════════════════════════════════════════════════════════════════════════════
# MAIN
# ══════════════════════════════════════════════════════════════════════════════

def main():
    if len(sys.argv) >= 3:
        f3, f4 = sys.argv[1], sys.argv[2]
    else:
        f3 = 'results_36ang_k3.csv'
        f4 = 'results_36ang_k4.csv'

    print(f'\nCargando {f3} y {f4} ...')
    df3 = load_csv(f3)
    df4 = load_csv(f4)

    K3, K4 = df3['n_angles'].iloc[0], df4['n_angles'].iloc[0]
    N_CAND = 36  # ángulos candidatos en la instancia

    r3 = detect_ils_restarts(df3, K3, N_CAND)
    r4 = detect_ils_restarts(df4, K4, N_CAND)

    print(f'\nEstadísticas:')
    print(f'  K=3: {len(df3)} evaluaciones, mejor f={df3["best_so_far"].iloc[-1]:,.2f}, '
          f'reinicios ILS estimados en evals {r3}')
    print(f'  K=4: {len(df4)} evaluaciones, mejor f={df4["best_so_far"].iloc[-1]:,.2f}, '
          f'reinicios ILS estimados en evals {r4}')

    label3 = f'K=3 (ángulos activos) | 36 candidatos | mejor=[{",".join(map(str, df3.loc[df3["objective"].idxmin(), "angles_list"]))}°]'
    label4 = f'K=4 (ángulos activos) | 36 candidatos | mejor=[{",".join(map(str, df4.loc[df4["objective"].idxmin(), "angles_list"]))}°]'

    print('\nGenerando gráficas ...')
    out_dir = 'plots'
    os.makedirs(out_dir, exist_ok=True)

    fig_convergence([df3, df4], [label3, label4],
                    [COLORS['k3'], COLORS['k4']], [r3, r4],
                    f'{out_dir}/01_convergencia.png')

    fig_comparison([df3, df4], [f'K={K3}', f'K={K4}'],
                   [COLORS['k3'], COLORS['k4']],
                   f'{out_dir}/02_comparativa.png')

    fig_angles([df3, df4], [f'K={K3}', f'K={K4}'],
               [COLORS['k3'], COLORS['k4']], N_CAND,
               f'{out_dir}/03_angulos_frecuencia.png')

    fig_heatmap(df3, f'K={K3}', N_CAND,
                f'{out_dir}/04_heatmap_pares_k3.png')
    fig_heatmap(df4, f'K={K4}', N_CAND,
                f'{out_dir}/04_heatmap_pares_k4.png')

    print(f'\nListo. Gráficas en ./{out_dir}/')


if __name__ == '__main__':
    main()
