#ifndef IMRT_FMO_H
#define IMRT_FMO_H

#ifdef WITH_OSQP

#include "imrt_instance.h"
#include <vector>
#include <utility>

#include <osqp.h>

namespace emili {
namespace imrt {

/**
 * ImrtFmoSolver
 *
 * Solves the continuous quadratic FMO sub-problem exactly using OSQP.
 *
 * QP formulation  (z = [x_active; u_ptv; v_oar]):
 *
 *   min   w_under * ||u||^2  +  w_over * ||v||^2  [+  w_ptv_over * ||w||^2]
 *   s.t.  D_ptv * x + u  >=  Dmin          (PTV underdose slack)
 *         D_oar * x - v  <=  Dmax          (OAR overdose slack)
 *         D_ptv * x - w  <=  1.07*Dmin     (PTV overdose slack, if w_ptv_over>0)
 *         u, v, w  >=  0
 *         0  <=  x_j  <=  M
 *
 * The QP is built at each solve() call using only the K active angles'
 * beamlets (K × per_angle variables instead of n_candidates × per_angle).
 * This keeps the problem size O(K) regardless of how many candidate angles
 * are in the instance, enabling large candidate pools with no extra cost.
 *
 * Constructor pre-computes the per-beamlet dose index so that solve() can
 * assemble the compact CSC matrices quickly.
 */
class ImrtFmoSolver {
public:
    explicit ImrtFmoSolver(const ImrtInstance& inst);
    ~ImrtFmoSolver() = default;

    // Solve FMO for the given active angle indices (0-based into inst.angles).
    // Returns { x (length inst.n_dimlets, zero for inactive), objective f* }.
    std::pair<std::vector<double>, double>
    solve(const std::vector<int>& active_angles);

    int nBeamlets() const { return inst_.n_dimlets; }
    bool isReady()  const { return ready_; }

private:
    const ImrtInstance& inst_;
    bool ready_;

    // Organ split (computed once in constructor)
    std::vector<int> ptv_orgs_;
    std::vector<int> oar_orgs_;
    int n_ptv_;  // total PTV boxets
    int n_oar_;  // total OAR boxets
    std::vector<int> ptv_row_off_;  // per-PTV-organ offset in G4 block
    std::vector<int> oar_row_off_;  // per-OAR-organ offset in G5 block

    // Per-beamlet dose index: ptv_dose_[j] = list of (G4_row, dose_rate)
    std::vector<std::vector<std::pair<int,double>>> ptv_dose_;
    std::vector<std::vector<std::pair<int,double>>> oar_dose_;

    // PTV/OAR bounds (constant across solves)
    std::vector<double> l_fixed_;     // lower bounds (n_ptv+n_oar rows)
    std::vector<double> u_fixed_;     // upper bounds (n_ptv+n_oar rows)
    std::vector<double> u_ptv_max_;   // Dmax_ptv per PTV voxel (= 1.07*Dmin)

    void precompute();
};

} // namespace imrt
} // namespace emili

#endif // WITH_OSQP
#endif // IMRT_FMO_H
