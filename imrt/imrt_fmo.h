#ifndef IMRT_FMO_H
#define IMRT_FMO_H

#include "imrt_fmo_source.h"
#include <vector>
#include <utility>
#include <memory>

namespace ampl { class AMPL; }

namespace emili {
namespace imrt {

/**
 * ImrtFmoSolver
 *
 * Solves the FMO QP for a set of active angles via AMPL + Gurobi, reading
 * the shared model at ampl_gurobi/fmo.mod (kept as the single source of
 * truth for the objective/constraints instead of duplicating it here):
 *
 *   min   w_under * ||u||^2  +  w_over * ||v||^2  [+  w_ptv_over * ||w||^2]
 *   s.t.  D_ptv * x + u  >=  Dmin          (PTV underdose slack)
 *         D_oar * x - v  <=  Dmax          (OAR overdose slack)
 *         D_ptv * x - w  <=  1.07*Dmin     (PTV overdose slack, if w_ptv_over>0)
 *         u, v, w  >=  0
 *         0  <=  x_j  <=  M
 *
 * The QP is built at each solve() call using only the K active angles'
 * beamlets (K x per_angle variables instead of n_candidates x per_angle).
 *
 * Depends on IFmoDataSource rather than a concrete instance format, so this
 * solver stays agnostic to on-disk layout details such as CerrFmoSource's
 * variable beamlet count per angle.
 *
 * Keeps a persistent ampl::AMPL member (environment + model read once in
 * the constructor) since BAO calls solve() potentially thousands of times;
 * only the per-solve data (active dimlets, sparse dose sets) is refreshed.
 */
class ImrtFmoSolver {
public:
    explicit ImrtFmoSolver(const IFmoDataSource& source);
    ~ImrtFmoSolver();

    // Solve FMO for the given active angle indices (0-based, into the BAO
    // angle catalog -- same indexing convention the data source expects).
    // Returns { x (length source.n_dimlets(), zero for inactive), objective f* }.
    std::pair<std::vector<double>, double>
    solve(const std::vector<int>& active_angles);

    int nBeamlets() const { return source_.n_dimlets(); }
    bool isReady()  const { return ready_; }

private:
    const IFmoDataSource& source_;
    bool ready_;

    int n_ptv_, n_oar_;
    std::vector<double> dmin_;      // per PTV boxet row, length n_ptv_
    std::vector<double> dmax_;      // per OAR boxet row, length n_oar_
    std::vector<double> dmax_ptv_;  // per PTV boxet row, length n_ptv_

    std::unique_ptr<ampl::AMPL> ampl_;

    void precompute();
    void initAmpl();
};

} // namespace imrt
} // namespace emili

#endif // IMRT_FMO_H
