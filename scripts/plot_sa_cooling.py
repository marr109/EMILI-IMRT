#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Diagnostico del enfriamiento de SA sobre las trayectorias de un piloto.

Tres paneles compartiendo el eje de iteracion:

  1. Temperatura. Reconstruida del esquema lineal que aplica emili::Metropolis,
     T_k = max(T_end, T_start - beta*(k-1)), donde k numera las llamadas a
     accept() y por lo tanto las filas del CSV. Marca la iteracion en que el
     esquema toca T_end: a partir de ahi la busqueda es descenso casi puro.

  2. Tasa de aceptacion en ventana movil. Un candidato se considera aceptado
     cuando el siguiente dista un solo angulo de el: el candidato k+1 se genera
     desde el incumbente, asi que coincide en tres de los cuatro angulos con el
     candidato k solo si ese k paso a ser el incumbente. Es una aproximacion --
     el CSV registra candidatos, no incumbentes-- util como senal relativa.

  3. Objetivo del candidato y mejor valor acumulado.

Las corridas que degeneran en revisita (mayoria de aciertos de cache) se dibujan
en gris: no aportan iteraciones nuevas, solo repiten evaluaciones ya hechas.
"""
import argparse, csv, glob, os, re, sys

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt


def load(path):
    objs, ang, cached = [], [], []
    with open(path) as f:
        for r in csv.DictReader(f):
            objs.append(float(r["objective"]))
            ang.append(frozenset(r["angles_deg"].split(";")))
            cached.append(r["cached"].strip().lower() == "true")
    return objs, ang, cached


def accept_rate(ang, window):
    """Fraccion de candidatos aparentemente aceptados, en ventana movil."""
    acc = [1 if len(ang[i] ^ ang[i - 1]) <= 2 else 0 for i in range(1, len(ang))]
    out = []
    for i in range(len(acc)):
        lo = max(0, i - window // 2)
        hi = min(len(acc), i + window // 2 + 1)
        out.append(100.0 * sum(acc[lo:hi]) / (hi - lo))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--data", required=True, help="directorio con seedNN/trajectory.csv")
    ap.add_argument("--t-start", type=float, default=1300.0)
    ap.add_argument("--t-end", type=float, default=80.0)
    ap.add_argument("--beta", type=float, default=11.0)
    ap.add_argument("--window", type=int, default=15)
    ap.add_argument("--max-iter", type=int, default=200,
                    help="corta el eje x; las corridas degeneradas llegan a decenas de miles")
    ap.add_argument("--layout", choices=("vertical", "apaisado"), default="vertical",
                    help="apaisado dispone los paneles en fila, para diapositiva")
    ap.add_argument("--title", default="")
    ap.add_argument("--output", required=True)
    a = ap.parse_args()

    runs = sorted(glob.glob(os.path.join(a.data, "seed*/trajectory.csv")))
    if not runs:
        print("sin trayectorias en %s" % a.data, file=sys.stderr)
        return 1

    if a.layout == "apaisado":
        # En una diapositiva el alto disponible es la mitad del ancho: tres paneles
        # apilados dejan cada uno demasiado bajo para leer sus ejes.
        fig, axes = plt.subplots(1, 3, figsize=(15, 4.6))
        ax_t, ax_a, ax_o = axes
        for ax in axes:
            ax.set_xlabel("Iteración de SA")
    else:
        fig, axes = plt.subplots(3, 1, figsize=(11, 11), sharex=True,
                                 gridspec_kw={"height_ratios": [1, 1.2, 1.4]})
        ax_t, ax_a, ax_o = axes

    k_end = int((a.t_start - a.t_end) / a.beta) + 1
    ks = list(range(1, a.max_iter + 1))
    ax_t.plot(ks, [max(a.t_end, a.t_start - a.beta * (k - 1)) for k in ks],
              color="#d62728", lw=2.2, label="T = max(%g, %g - %g(k-1))" % (a.t_end, a.t_start, a.beta))
    for ax in axes:
        ax.axvline(k_end, color="#d62728", ls="--", lw=1.2, alpha=0.55)
    ax_t.annotate("T alcanza %g en k=%d" % (a.t_end, k_end), xy=(k_end, a.t_start * 0.55),
                  xytext=(k_end + 6, a.t_start * 0.62), fontsize=9, color="#d62728")
    ax_t.set_ylabel("Temperatura")
    ax_t.legend(loc="upper right", fontsize=9)
    ax_t.grid(alpha=0.25)

    n_deg = 0
    for path in runs:
        seed = re.search(r"seed(\d+)", path).group(1)
        objs, ang, cached = load(path)
        degenerate = sum(cached) > 0.5 * len(cached)
        n_deg += degenerate
        style = dict(color="#999999", lw=0.9, alpha=0.55, zorder=1) if degenerate \
                else dict(lw=1.1, alpha=0.8, zorder=2)

        rate = accept_rate(ang, a.window)[:a.max_iter]
        ax_a.plot(range(2, 2 + len(rate)), rate, **style)

        cut = objs[:a.max_iter]
        ax_o.plot(range(1, 1 + len(cut)), cut, **style)
        best, run_best = [], None
        for v in cut:
            run_best = v if run_best is None or v < run_best else run_best
            best.append(run_best)
        ax_o.plot(range(1, 1 + len(best)), best,
                  color=("#999999" if degenerate else "#1f77b4"),
                  lw=1.8 if not degenerate else 0.9, alpha=0.9 if not degenerate else 0.5)

    ax_a.set_ylabel("Aceptación aparente\n(ventana %d, %%)" % a.window)
    ax_a.grid(alpha=0.25)
    ax_o.set_ylabel("Objetivo FMO")
    if a.layout != "apaisado":
        ax_o.set_xlabel("Iteración de SA (llamadas a accept)")
    ax_o.grid(alpha=0.25)

    note = "%d corridas" % len(runs)
    if n_deg:
        note += " -- %d en gris degeneran en revisita (mayoría de aciertos de caché)" % n_deg
    ax_o.text(0.99, 0.02, note, transform=ax_o.transAxes, ha="right", fontsize=8.5, color="#555555")

    if a.title:
        fig.suptitle(a.title, fontsize=13)
        fig.tight_layout(rect=[0, 0, 1, 0.97])
    else:
        fig.tight_layout()
    fig.savefig(a.output, dpi=150)
    print("escrito: %s" % a.output)
    return 0


if __name__ == "__main__":
    sys.exit(main())
