#include "imrt_fmo.h"

#include <algorithm>
#include <cstring>
#include <iostream>

namespace emili {
namespace imrt {

/*---------------------------------------------------------------------------*
 * Constructor — precomputes dose index, organ split, and fixed bounds.
 * No OSQP workspace is built here; the QP is assembled fresh per solve().
 *---------------------------------------------------------------------------*/
ImrtFmoSolver::ImrtFmoSolver(const ImrtInstance& inst)
    : inst_(inst), ready_(false), n_ptv_(0), n_oar_(0)
{
    precompute();
}

void ImrtFmoSolver::precompute()
{
    const ImrtInstance& inst = inst_;
    const int nb = inst.n_dimlets;

    // ── Organ split ────────────────────────────────────────────────────────
    for (int o = 0; o < (int)inst.organs.size(); ++o) {
        if (inst.organs[o].is_ptv) ptv_orgs_.push_back(o);
        else                       oar_orgs_.push_back(o);
    }

    ptv_row_off_.resize(ptv_orgs_.size());
    oar_row_off_.resize(oar_orgs_.size());
    for (int i = 0; i < (int)ptv_orgs_.size(); ++i) {
        ptv_row_off_[i] = n_ptv_;
        n_ptv_ += inst.organs[ptv_orgs_[i]].n_boxets;
    }
    
    for (int i = 0; i < (int)oar_orgs_.size(); ++i) {
        oar_row_off_[i] = n_oar_;
        n_oar_ += inst.organs[oar_orgs_[i]].n_boxets;
    }

    // ── Per-beamlet dose index ─────────────────────────────────────────────
    // ptv_dose_[j] = list of (boxet_row_within_G4, dose_rate)
    // oar_dose_[j] = list of (boxet_row_within_G5, dose_rate)
    ptv_dose_.resize(nb);
    oar_dose_.resize(nb);

    for (int i = 0; i < (int)ptv_orgs_.size(); ++i) {
        int rbase = ptv_row_off_[i];
        for (const DoseEntry& e : inst.organs[ptv_orgs_[i]].entries)
            ptv_dose_[e.dimlet_id].push_back({rbase + e.boxet_id, e.dose_rate});
    }
    for (int i = 0; i < (int)oar_orgs_.size(); ++i) {
        int rbase = oar_row_off_[i];
        for (const DoseEntry& e : inst.organs[oar_orgs_[i]].entries)
            oar_dose_[e.dimlet_id].push_back({rbase + e.boxet_id, e.dose_rate});
    }

    // ── Fixed bounds for PTV (G4) and OAR (G5) blocks ─────────────────────
    // Stored as (n_ptv + n_oar) entries: l_fixed_[G4 rows | G5 rows]
    const double INF = 1e30;
    l_fixed_.assign(n_ptv_ + n_oar_, 0.0);
    u_fixed_.assign(n_ptv_ + n_oar_, INF);

    for (int i = 0; i < (int)ptv_orgs_.size(); ++i) {
        double Dmin = inst.organs[ptv_orgs_[i]].Dmin;
        for (int b = 0; b < inst.organs[ptv_orgs_[i]].n_boxets; ++b) {
            int r = ptv_row_off_[i] + b;
            l_fixed_[r] = Dmin;
            u_fixed_[r] = INF;
        }
    }
    for (int i = 0; i < (int)oar_orgs_.size(); ++i) {
        double Dmax = inst.organs[oar_orgs_[i]].Dmax;
        for (int b = 0; b < inst.organs[oar_orgs_[i]].n_boxets; ++b) {
            int r = n_ptv_ + oar_row_off_[i] + b;
            l_fixed_[r] = -INF;
            u_fixed_[r] = Dmax;
        }
    }

    // ── Dmax_ptv per PTV voxel = 1.07 × Dmin (for overdose penalty) ──────────
    u_ptv_max_.assign(n_ptv_, 1e30);
    if (inst_.w_ptv_over > 0.0) {
        for (int i = 0; i < (int)ptv_orgs_.size(); ++i) {
            double dmax_ptv = 1.07 * inst_.organs[ptv_orgs_[i]].Dmin;
            for (int b = 0; b < inst_.organs[ptv_orgs_[i]].n_boxets; ++b)
                u_ptv_max_[ptv_row_off_[i] + b] = dmax_ptv;
        }
    }

    ready_ = true;
}

/*---------------------------------------------------------------------------*
 * solve — REMOVED on this branch.
 *
 * The exact QP solve (build a compact QP for the K active angles, call
 * OSQP) has been removed. This stub fails loudly instead of silently
 * returning a wrong answer: it logs an error and returns a worst-case
 * sentinel objective (x = 0, f = 1e30) so any caller comparing objective
 * values treats this as the worst possible outcome.
 *
 * See ampl_gurobi/ for the AMPL+Gurobi replacement, and the `develop`
 * branch for the preserved OSQP implementation.
 *---------------------------------------------------------------------------*/
std::pair<std::vector<double>, double>
ImrtFmoSolver::solve(const std::vector<int>& /*active_angles*/)
{
    std::cerr << "[FMO] ERROR: FMO solver removed from this branch — "
                 "see ampl_gurobi/ for the AMPL+Gurobi replacement.\n";
    return {std::vector<double>(inst_.n_dimlets, 0.0), 1e30};
}

} // namespace imrt
} // namespace emili
