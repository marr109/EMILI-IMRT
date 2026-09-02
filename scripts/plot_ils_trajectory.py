#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Grafica una trayectoria de ILS resaltando en qué evaluaciones ocurrió una
perturbación (AngleShiftMultiPerturbation) en vez de un paso normal de
búsqueda local (AngleShiftNeighborhood).

Distingue ambas por la magnitud del cambio: la búsqueda local siempre mueve
un ángulo por exactamente ±step; la perturbación lo mueve por una magnitud
aleatoria en (step, 2*step) — nunca step exacto (ver
AngleShiftMultiPerturbation, imrt/imrt_bao.cpp). Comparando cada fila contra
la anterior (mismo slot, cuánto cambió), esa diferencia de magnitud es
suficiente para clasificar cada evaluación sin necesitar ninguna columna
extra en el CSV.

CLI example:
  python scripts/plot_ils_trajectory.py \
      --csv experiments/ils/nangshift10/grid5/data/first/seed01/trajectory.csv \
      --step 10 \
      --output experiments/ils/nangshift10/grid5/data/first/seed01/ils_perturbations.png
"""

import argparse
import csv
import os
import sys

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from plot_trajectory import read_trajectory, running_best


def circular_delta(a, b, n_angles=360):
    d = abs(a - b) % n_angles
    return min(d, n_angles - d)


def detect_perturbations(evals, objectives, angle_sets, step, reject_repeated=True):
    """Simula la semántica real de ILS con First Improvement — no solo el
    rebase dentro de una ronda de búsqueda local, sino también la decisión
    de aceptar/rechazar el resultado de cada iteración externa
    (BaoImproveAccept, imrt_bao.cpp), porque con `rejectrepeated` activo el
    punto desde el que se perturba en la iteración siguiente puede REVERTIR
    a un punto anterior si el resultado de la ronda no mejoró o repitió una
    solución ya visitada — y si no se modela ese revert, la primera
    perturbación de esa ráfaga parece (incorrectamente) cambiar varios
    slots a la vez en vez de uno solo.

    Máquina de estados con dos fases:
      "search": recorre vecinos ±step de `base` (ronda de búsqueda local
        vigente), rebasando cada vez que encuentra una mejora — igual que
        FirstImprovementSearch.
      "perturbing": encadena movimientos de magnitud != step a partir de
        `pert_chain` (arranca en s_p, el punto ACEPTADO por el ILS, no en
        el último base de búsqueda local si esa ronda fue rechazada).

    La transición search -> perturbing resuelve el accept/reject de la
    ronda que acaba de terminar contra s_p (con el mismo criterio de
    BaoImproveAccept, incluyendo el registro de visitados si
    reject_repeated=True) ANTES de clasificar la fila que disparó la
    transición como el primer paso de la perturbación.

    Solo válido para `first` (Best no rebasa a mitad de ronda).
    """
    s_p, s_p_obj, s_p_eval = angle_sets[0], objectives[0], evals[0]
    visited = set()
    base, base_obj, base_eval = s_p, s_p_obj, s_p_eval
    phase = "search"
    pert_chain, pert_chain_obj, pert_chain_eval = None, None, None
    records = []

    for i in range(1, len(angle_sets)):
        cur, cur_obj = angle_sets[i], objectives[i]

        if phase == "search":
            diffs = [j for j in range(len(cur)) if cur[j] != base[j]]
            if len(diffs) == 1 and circular_delta(cur[diffs[0]], base[diffs[0]]) == step:
                if cur_obj < base_obj:
                    base, base_obj, base_eval = cur, cur_obj, evals[i]
                continue

            # Ronda terminada: resuelve accept/reject de `base` (candidato)
            # contra s_p, igual que BaoImproveAccept::accept.
            candidate, candidate_obj, candidate_eval = base, base_obj, base_eval
            visited.add(tuple(s_p))
            if candidate_obj < s_p_obj and not (reject_repeated and tuple(candidate) in visited):
                visited.add(tuple(candidate))
                s_p, s_p_obj, s_p_eval = candidate, candidate_obj, candidate_eval
            pert_chain, pert_chain_obj, pert_chain_eval = s_p, s_p_obj, s_p_eval
            phase = "perturbing"
            # sigue abajo: clasifica esta misma fila ya en fase "perturbing"

        # phase == "perturbing"
        diffs = [j for j in range(len(cur)) if cur[j] != pert_chain[j]]
        if len(diffs) == 1 and circular_delta(cur[diffs[0]], pert_chain[diffs[0]]) == step:
            # La búsqueda local ya retomó: esta fila es un vecino normal de
            # la base perturbada — vuelve a fase "search" y la reprocesa.
            phase = "search"
            base, base_obj, base_eval = pert_chain, pert_chain_obj, pert_chain_eval
            if cur_obj < base_obj:
                base, base_obj, base_eval = cur, cur_obj, evals[i]
            continue

        slot = diffs[0] if len(diffs) == 1 else None
        records.append({
            "eval_antes": pert_chain_eval,
            "eval_despues": evals[i],
            "angles_antes": ";".join(str(a) for a in pert_chain),
            "angles_despues": ";".join(str(a) for a in cur),
            "slots_distintos": len(diffs),
            "slot_movido": slot if slot is not None else "",
            "delta_deg": circular_delta(cur[slot], pert_chain[slot]) if slot is not None else "",
            "obj_antes": pert_chain_obj,
            "obj_despues": cur_obj,
        })
        pert_chain, pert_chain_obj, pert_chain_eval = cur, cur_obj, evals[i]

    return records


def group_bursts(records):
    """Numera cada perturbación detectada dentro de su ráfaga consecutiva
    (evals correlativos = misma perturbación de numSteps movimientos)."""
    burst_id = 0
    step_in_burst = 0
    prev_eval = None
    for r in records:
        if prev_eval is not None and r["eval_despues"] == prev_eval + 1:
            step_in_burst += 1
        else:
            burst_id += 1
            step_in_burst = 1
        r["rafaga"] = burst_id
        r["paso_en_rafaga"] = step_in_burst
        prev_eval = r["eval_despues"]
    return records


def main(argv=None):
    p = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--csv", required=True)
    p.add_argument("--step", type=int, required=True,
                    help="step de nangshift usado en esta corrida (10 o 5)")
    p.add_argument("--output", required=True)
    p.add_argument("--title", default=None)
    p.add_argument("--perturbations-csv", default=None,
                    help="Si se pasa, escribe el detalle antes/después de cada "
                         "perturbación detectada (para auditar la clasificación).")
    p.add_argument("--no-reject-repeated", action="store_true",
                    help="Pasar si la corrida NO usó `baoimprove rejectrepeated` "
                         "(por defecto se asume que sí, que es lo que usan las "
                         "corridas actuales de ILS).")
    args = p.parse_args(argv)

    evals, objectives, angle_sets = read_trajectory(args.csv)
    if not evals:
        raise SystemExit("error: empty trajectory CSV: {0}".format(args.csv))

    best = running_best(objectives)
    records = group_bursts(detect_perturbations(
        evals, objectives, angle_sets, args.step,
        reject_repeated=not args.no_reject_repeated))
    pert_set = {r["eval_despues"] for r in records}
    pert_objs = [o for e, o in zip(evals, objectives) if e in pert_set]
    pert_xs = [e for e in evals if e in pert_set]

    if args.perturbations_csv:
        out_dir = os.path.dirname(args.perturbations_csv) or "."
        os.makedirs(out_dir, exist_ok=True)
        fieldnames = ["rafaga", "paso_en_rafaga", "eval_antes", "eval_despues",
                      "angles_antes", "angles_despues", "slots_distintos",
                      "slot_movido", "delta_deg", "obj_antes", "obj_despues"]
        with open(args.perturbations_csv, "w", newline="") as f:
            w = csv.DictWriter(f, fieldnames=fieldnames)
            w.writeheader()
            for r in records:
                w.writerow(r)
        print("perturbations CSV written: {0} ({1} filas)".format(
            args.perturbations_csv, len(records)))

    fig, ax = plt.subplots(figsize=(12, 6))
    ax.plot(evals, objectives, color="#9aa5b1", linewidth=0.7, alpha=0.5, label="evaluado")
    ax.step(evals, best, where="post", color="#2f6f9f", linewidth=2, label="mejor hasta el momento")

    for e in pert_xs:
        ax.axvline(e, color="#d1495b", alpha=0.25, linewidth=1, zorder=1)
    if pert_xs:
        ax.scatter(pert_xs, pert_objs, color="#d1495b", s=28, zorder=3,
                   label="perturbación ({0})".format(len(pert_xs)))

    ax.set_xlabel("evaluación")
    ax.set_ylabel("FMO objective")
    ax.set_title(args.title or "Trayectoria ILS — perturbaciones resaltadas")
    ax.legend(loc="upper right")
    ax.grid(alpha=0.3)

    fig.tight_layout()
    out_dir = os.path.dirname(args.output) or "."
    os.makedirs(out_dir, exist_ok=True)
    fig.savefig(args.output, dpi=150)
    plt.close(fig)
    print("ILS trajectory plot written: {0} ({1} perturbaciones detectadas de {2} evals)".format(
        args.output, len(pert_xs), len(evals)))


if __name__ == "__main__":
    sys.exit(main())
