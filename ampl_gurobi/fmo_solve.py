"""Solve the FMO QP for a subset of angles from instances/CERR_Prostate with AMPL + Gurobi.

CLINICAL_CONFIG below is a PLACEHOLDER, explicitly authorized for pipeline
validation only. No instance_config.txt, VOILIST, or CERR patient plan in
this repo documents the real prescription (PTV Dmin) or OAR tolerance
(Dmax) for this specific patient -- replace these once the real CERR
treatment plan is available.
"""

from __future__ import annotations

import argparse
import time
from pathlib import Path

import numpy as np
from amplpy import AMPL

from cerr_instance import load_cerr_instance

REPO_ROOT = Path(__file__).resolve().parent.parent
INSTANCE_ROOT = REPO_ROOT / "instances" / "CERR_Prostate"
MODEL_PATH = Path(__file__).resolve().parent / "fmo.mod"

PTV_NAMES = ["PTVHD", "PTVLD"]
OAR_NAMES = ["BLADDER", "RECTUM"]

CLINICAL_CONFIG = {
    "ptv_dmin": {"PTVHD": 65.0, "PTVLD": 65.0},
    "oar_dmax": {"BLADDER": 50.0, "RECTUM": 50.0},
    "w_under": 1.0,
    "w_over": 0.5,
    "w_ptv_over": 0.0,
    "max_intensity": 15000.0,
}


def build_ampl(inst, config: dict) -> AMPL:
    ptv_offsets, n_ptv, dmin = {}, 0, []
    for name in PTV_NAMES:
        ptv_offsets[name] = n_ptv
        organ = inst.organs[name]
        n_ptv += organ.n_boxets
        dmin.extend([config["ptv_dmin"][name]] * organ.n_boxets)

    oar_offsets, n_oar, dmax = {}, 0, []
    for name in OAR_NAMES:
        oar_offsets[name] = n_oar
        organ = inst.organs[name]
        n_oar += organ.n_boxets
        dmax.extend([config["oar_dmax"][name]] * organ.n_boxets)

    ptv_b = np.concatenate([inst.organs[n].boxet_id + ptv_offsets[n] for n in PTV_NAMES])
    ptv_j = np.concatenate([inst.organs[n].dimlet_id for n in PTV_NAMES])
    ptv_d = np.concatenate([inst.organs[n].dose_rate for n in PTV_NAMES])

    oar_b = np.concatenate([inst.organs[n].boxet_id + oar_offsets[n] for n in OAR_NAMES])
    oar_j = np.concatenate([inst.organs[n].dimlet_id for n in OAR_NAMES])
    oar_d = np.concatenate([inst.organs[n].dose_rate for n in OAR_NAMES])

    active_dimlets = inst.active_dimlet_ids()

    ampl = AMPL()
    ampl.read(str(MODEL_PATH))

    ampl.get_parameter("n_ptv").set(n_ptv)
    ampl.get_parameter("n_oar").set(n_oar)
    ampl.get_set("DIMLETS").set_values(active_dimlets.tolist())

    ampl.get_parameter("max_intensity").set(config["max_intensity"])
    ampl.get_parameter("w_under").set(config["w_under"])
    ampl.get_parameter("w_over").set(config["w_over"])
    ampl.get_parameter("w_ptv_over").set(config["w_ptv_over"])

    ampl.get_parameter("dmin").set_values({b: v for b, v in enumerate(dmin)})
    ampl.get_parameter("dmax").set_values({b: v for b, v in enumerate(dmax)})
    ampl.get_parameter("dmax_ptv").set_values({b: 1.07 * v for b, v in enumerate(dmin)})

    ampl.get_set("PTV_DOSE").set_values(list(zip(ptv_b.tolist(), ptv_j.tolist())))
    ampl.get_set("OAR_DOSE").set_values(list(zip(oar_b.tolist(), oar_j.tolist())))
    ampl.get_parameter("d_ptv").set_values(
        {(int(b), int(j)): float(d) for b, j, d in zip(ptv_b, ptv_j, ptv_d)}
    )
    ampl.get_parameter("d_oar").set_values(
        {(int(b), int(j)): float(d) for b, j, d in zip(oar_b, oar_j, oar_d)}
    )

    return ampl, n_ptv, n_oar


def main():
    parser = argparse.ArgumentParser(description="Solve FMO QP on CERR_Prostate with AMPL+Gurobi")
    parser.add_argument("--angles", type=int, nargs="+", default=[0, 90, 180, 270])
    parser.add_argument("--solver", default="gurobi")
    args = parser.parse_args()

    inst = load_cerr_instance(
        INSTANCE_ROOT,
        organ_names=PTV_NAMES + OAR_NAMES,
        active_angles=args.angles,
    )

    ampl, n_ptv, n_oar = build_ampl(inst, CLINICAL_CONFIG)

    t0 = time.perf_counter()
    ampl.solve(solver=args.solver)
    elapsed = time.perf_counter() - t0

    n_act = len(inst.active_dimlet_ids())
    print(f"angles          : {args.angles}")
    print(f"solver          : {args.solver}")
    print(f"solve_result    : {ampl.solve_result}")
    print(f"objective       : {ampl.get_objective('fmo_objective').value():.6f}")
    print(f"solve time      : {elapsed:.3f}s")
    print(f"n_vars (x)      : {n_act}")
    print(f"n_ptv_boxets    : {n_ptv}")
    print(f"n_oar_boxets    : {n_oar}")


if __name__ == "__main__":
    main()
