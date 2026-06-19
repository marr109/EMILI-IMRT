#include "imrt_fmo.h"

#ifdef WITH_OSQP

#include <osqp.h>
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
    const OSQPFloat INF = (OSQPFloat)1e30;
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
 * solve — build a compact QP for the K active angles only, then call OSQP.
 *
 * Variable layout (compact):  z = [ x (K*per) | u (n_ptv) | v (n_oar) ]
 *   where K*per = active_angles.size() * n_dimlets_per_angle
 *
 * Constraint rows:
 *   G1 [0 .. n_act-1]           box on x  (0 <= x <= max_intensity)
 *   G2 [n_act .. n_act+n_ptv-1] box on u  (u >= 0)
 *   G3 [G2_end .. G3_end-1]     box on v  (v >= 0)
 *   G4 [G3_end .. G4_end-1]     D_ptv*x + u >= Dmin
 *   G5 [G4_end .. G5_end-1]     D_oar*x - v <= Dmax
 *---------------------------------------------------------------------------*/
std::pair<std::vector<double>, double>
ImrtFmoSolver::solve(const std::vector<int>& active_angles)
{
    if (!ready_) return {std::vector<double>(inst_.n_dimlets, 0.0), 1e30};

    const int per  = inst_.n_dimlets_per_angle;
    const int K    = (int)active_angles.size();
    const int n_act = K * per;  // number of active beamlet variables

    // Map local beamlet index j -> global dimlet id
    // local j = angle_idx * per + beamlet_within_angle
    auto global_id = [&](int j) -> int {
        return active_angles[j / per] * per + (j % per);
    };

    const bool use_ptv_over = (inst_.w_ptv_over > 0.0);

    // ── QP dimensions ─────────────────────────────────────────────────────
    // Variables: z = [x (n_act) | u (n_ptv) | v (n_oar) | w (n_ptv, if enabled)]
    // Constraints:
    //   G1 [0..n_act-1]        box on x
    //   G2 [G2..G2+n_ptv-1]   box on u
    //   G3 [G3..G3+n_oar-1]   box on v
    //   Gw [Gw..Gw+n_ptv-1]   box on w  (only if use_ptv_over)
    //   G4 [G4..G4+n_ptv-1]   D_ptv*x + u >= Dmin
    //   G5 [G5..G5+n_oar-1]   D_oar*x - v <= Dmax
    //   G6 [G6..G6+n_ptv-1]   D_ptv*x - w <= Dmax_ptv  (only if use_ptv_over)
    const int n_w    = use_ptv_over ? n_ptv_ : 0;
    const int n_vars = n_act + n_ptv_ + n_oar_ + n_w;
    const int G2     = n_act;
    const int G3     = G2 + n_ptv_;
    const int Gw     = G3 + n_oar_;
    const int G4     = Gw + n_w;
    const int G5     = G4 + n_ptv_;
    const int G6     = G5 + n_oar_;
    const int n_cons = G6 + n_w;

    const OSQPFloat INF = (OSQPFloat)1e30;

    // ── P matrix (upper-triangular CSC): diagonal on u, v, [w] blocks ─────
    const int P_nnz = n_ptv_ + n_oar_ + n_w;
    std::vector<OSQPInt>   P_p(n_vars + 1, 0);
    std::vector<OSQPInt>   P_i(P_nnz);
    std::vector<OSQPFloat> P_x(P_nnz);

    // u diagonal
    for (int k = 0; k < n_ptv_; ++k) {
        P_p[n_act + k + 1] = (OSQPInt)(k + 1);
        P_i[k] = (OSQPInt)(n_act + k);
        P_x[k] = (OSQPFloat)(2.0 * inst_.w_under);
    }
    // v diagonal
    for (int k = 0; k < n_oar_; ++k) {
        P_p[n_act + n_ptv_ + k + 1] = (OSQPInt)(n_ptv_ + k + 1);
        P_i[n_ptv_ + k] = (OSQPInt)(n_act + n_ptv_ + k);
        P_x[n_ptv_ + k] = (OSQPFloat)(2.0 * inst_.w_over);
    }
    // w diagonal (PTV overdose penalty)
    if (use_ptv_over) {
        for (int k = 0; k < n_ptv_; ++k) {
            P_p[n_act + n_ptv_ + n_oar_ + k + 1] = (OSQPInt)(n_ptv_ + n_oar_ + k + 1);
            P_i[n_ptv_ + n_oar_ + k] = (OSQPInt)(n_act + n_ptv_ + n_oar_ + k);
            P_x[n_ptv_ + n_oar_ + k] = (OSQPFloat)(2.0 * inst_.w_ptv_over);
        }
    }

    // ── A matrix (CSC, column by column) ──────────────────────────────────
    std::vector<OSQPInt>   A_p, A_i;
    std::vector<OSQPFloat> A_x;
    A_p.reserve(n_vars + 1);
    A_p.push_back(0);

    // x columns (active beamlets): G1 + G4 + G5 + [G6]
    for (int j = 0; j < n_act; ++j) {
        int gj = global_id(j);

        std::vector<std::pair<int,double>> col;
        col.push_back({j, 1.0});  // G1 diagonal
        for (auto& e : ptv_dose_[gj]) col.push_back({G4 + e.first,  e.second});
        for (auto& e : oar_dose_[gj]) col.push_back({G5 + e.first,  e.second});
        if (use_ptv_over)
            for (auto& e : ptv_dose_[gj]) col.push_back({G6 + e.first, e.second});
        std::sort(col.begin(), col.end());

        for (int ci = 0; ci < (int)col.size(); ) {
            int    row = col[ci].first;
            double val = col[ci].second;
            int ci2 = ci + 1;
            while (ci2 < (int)col.size() && col[ci2].first == row) {
                val += col[ci2].second; ++ci2;
            }
            if (val != 0.0) {
                A_i.push_back((OSQPInt)row);
                A_x.push_back((OSQPFloat)val);
            }
            ci = ci2;
        }
        A_p.push_back((OSQPInt)A_i.size());
    }

    // u columns: G2 (box) + G4 (dose constraint)
    for (int k = 0; k < n_ptv_; ++k) {
        A_i.push_back((OSQPInt)(G2 + k));  A_x.push_back( 1.0f);
        A_i.push_back((OSQPInt)(G4 + k));  A_x.push_back( 1.0f);
        A_p.push_back((OSQPInt)A_i.size());
    }
    // v columns: G3 (box) + G5 (dose constraint)
    for (int k = 0; k < n_oar_; ++k) {
        A_i.push_back((OSQPInt)(G3 + k));  A_x.push_back( 1.0f);
        A_i.push_back((OSQPInt)(G5 + k));  A_x.push_back(-1.0f);
        A_p.push_back((OSQPInt)A_i.size());
    }
    // w columns: Gw (box) + G6 (overdose constraint)
    if (use_ptv_over) {
        for (int k = 0; k < n_ptv_; ++k) {
            A_i.push_back((OSQPInt)(Gw + k));  A_x.push_back( 1.0f);
            A_i.push_back((OSQPInt)(G6 + k));  A_x.push_back(-1.0f);
            A_p.push_back((OSQPInt)A_i.size());
        }
    }

    // ── Bounds ────────────────────────────────────────────────────────────
    std::vector<OSQPFloat> l_data(n_cons, 0.0f);
    std::vector<OSQPFloat> u_data(n_cons, INF);

    // G1: x bounds
    for (int j = 0; j < n_act; ++j)
        u_data[j] = (OSQPFloat)inst_.max_intensity;
    // G4 / G5: PTV/OAR dose bounds
    for (int r = 0; r < n_ptv_; ++r) {
        l_data[G4 + r] = (OSQPFloat)l_fixed_[r];
        u_data[G4 + r] = (OSQPFloat)u_fixed_[r];
    }
    for (int r = 0; r < n_oar_; ++r) {
        l_data[G5 + r] = (OSQPFloat)l_fixed_[n_ptv_ + r];
        u_data[G5 + r] = (OSQPFloat)u_fixed_[n_ptv_ + r];
    }
    // G6: PTV max-dose bounds (D_ptv*x - w <= Dmax_ptv → l=-INF, u=Dmax_ptv)
    if (use_ptv_over) {
        for (int r = 0; r < n_ptv_; ++r) {
            l_data[G6 + r] = -INF;
            u_data[G6 + r] = (OSQPFloat)u_ptv_max_[r];
        }
    }

    // ── OSQP setup and solve ──────────────────────────────────────────────
    std::vector<OSQPFloat> q_data(n_vars, 0.0f);

    OSQPCscMatrix P_mat, A_mat;
    OSQPCscMatrix_set_data(&P_mat, n_vars, n_vars, P_nnz,
                           P_x.data(), P_i.data(), P_p.data());
    OSQPCscMatrix_set_data(&A_mat, n_cons, n_vars, (OSQPInt)A_i.size(),
                           A_x.data(), A_i.data(), A_p.data());

    OSQPSettings* settings = OSQPSettings_new();
    osqp_set_default_settings(settings);
    settings->verbose       = 0;
    settings->warm_starting = 0;  // different structure each call
    settings->eps_abs       = (OSQPFloat)1e-6;
    settings->eps_rel       = (OSQPFloat)1e-6;
    settings->polishing     = 1;
    settings->max_iter      = 10000;

    OSQPSolver* solver = nullptr;
    OSQPInt ret = osqp_setup(&solver, &P_mat, q_data.data(), &A_mat,
                             l_data.data(), u_data.data(),
                             (OSQPInt)n_cons, (OSQPInt)n_vars, settings);
    OSQPSettings_free(settings);

    if (ret != 0 || !solver) {
        std::cerr << "[FMO] OSQP setup failed (code=" << ret << ")\n";
        return {std::vector<double>(inst_.n_dimlets, 0.0), 1e30};
    }

    osqp_solve(solver);

    // ── Extract results and map back to full beamlet vector ───────────────
    std::vector<double> x_full(inst_.n_dimlets, 0.0);
    if (solver->solution && solver->solution->x) {
        for (int j = 0; j < n_act; ++j)
            x_full[global_id(j)] = std::max(0.0, (double)solver->solution->x[j]);
    }

    double f = (solver->info->status_val > 0) ? (double)solver->info->obj_val : 1e30;
    if (f < 0.0) f = 0.0;

    osqp_cleanup(solver);
    return {std::move(x_full), f};
}

} // namespace imrt
} // namespace emili

#endif // WITH_OSQP
