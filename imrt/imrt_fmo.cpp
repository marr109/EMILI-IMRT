#include "imrt_fmo.h"

#include <ampl/ampl.h>

#include <cstdlib>
#include <iostream>

#ifndef EMILI_REPO_ROOT
#define EMILI_REPO_ROOT "."
#endif

namespace emili {
namespace imrt {

namespace {

std::string envOr(const char* var, const std::string& fallback)
{
    const char* v = std::getenv(var);
    return (v && *v) ? std::string(v) : fallback;
}

std::string repoPath(const std::string& rel)
{
    return std::string(EMILI_REPO_ROOT) + "/" + rel;
}

} // namespace

/*---------------------------------------------------------------------------*
 * Constructor / destructor
 *---------------------------------------------------------------------------*/
ImrtFmoSolver::ImrtFmoSolver(const IFmoDataSource& source)
    : source_(source), ready_(false), n_ptv_(0), n_oar_(0)
{
    precompute();
    try {
        initAmpl();
        ready_ = true;
    } catch (const std::exception& e) {
        std::cerr << "[FMO] ERROR: AMPL/Gurobi initialization failed: " << e.what() << "\n";
        ready_ = false;
    }
}

// Out-of-line so ~unique_ptr<ampl::AMPL> only needs the complete type here,
// not wherever ImrtFmoSolver is used as a member (imrt_bao.h stays free of
// the AMPL headers).
ImrtFmoSolver::~ImrtFmoSolver() = default;

/*---------------------------------------------------------------------------*
 * precompute — organ boundaries (constant across solves for a given source)
 *---------------------------------------------------------------------------*/
void ImrtFmoSolver::precompute()
{
    n_ptv_ = 0;
    n_oar_ = 0;
    dmin_.clear();
    dmax_.clear();
    dmax_ptv_.clear();
    ptv_bounds_.clear();
    oar_bounds_.clear();

    for (const FmoOrganRef& o : source_.ptvOrgans()) {
        ptv_bounds_.push_back({o.name, n_ptv_, o.n_boxets});
        n_ptv_ += o.n_boxets;
        for (int b = 0; b < o.n_boxets; ++b) {
            dmin_.push_back(o.dmin);
            dmax_ptv_.push_back(o.dmax_ptv);
        }
    }
    for (const FmoOrganRef& o : source_.oarOrgans()) {
        oar_bounds_.push_back({o.name, n_oar_, o.n_boxets});
        n_oar_ += o.n_boxets;
        for (int b = 0; b < o.n_boxets; ++b)
            dmax_.push_back(o.dmax);
    }
}

/*---------------------------------------------------------------------------*
 * printSolveDims — advisor-requested sanity check (see git history / project
 * notes): print each organ's boxet count (fixed) and how many sparse dose
 * entries feed this specific solve (varies with the active-angle set), so a
 * silently-stale or repeated matrix is visible immediately instead of
 * hiding inside an otherwise-plausible objective value.
 *---------------------------------------------------------------------------*/
void ImrtFmoSolver::printSolveDims(int n_active_beamlets,
                                    const std::vector<int>& ptv_nnz,
                                    const std::vector<int>& oar_nnz) const
{
    std::cout << "  [FMO] beamlets_activos=" << n_active_beamlets << "\n";
    for (size_t i = 0; i < ptv_bounds_.size(); ++i)
        std::cout << "    PTV " << ptv_bounds_[i].name
                   << ": boxets=" << ptv_bounds_[i].n_boxets
                   << " entradas_dosis=" << ptv_nnz[i] << "\n";
    for (size_t i = 0; i < oar_bounds_.size(); ++i)
        std::cout << "    OAR " << oar_bounds_[i].name
                   << ": boxets=" << oar_bounds_[i].n_boxets
                   << " entradas_dosis=" << oar_nnz[i] << "\n";
    // Gurobi runs as a child process writing straight to the inherited
    // stdout fd; without an explicit flush here, this block-buffered
    // std::cout output can appear AFTER that child's output in the
    // terminal even though it was written first -- exactly backwards from
    // what the "print before solving" sanity check is meant to show.
    std::cout.flush();
}

/*---------------------------------------------------------------------------*
 * initAmpl — one-time environment/model setup + the static (per-source,
 * not per-solve) parameters.
 *---------------------------------------------------------------------------*/
void ImrtFmoSolver::initAmpl()
{
    std::string bin_dir = envOr("EMILI_AMPL_BIN_DIR",
        repoPath("ampl_gurobi/.venv/lib/python3.9/site-packages/ampl_module_base/bin"));
    std::string gurobi_bin = envOr("EMILI_GUROBI_BIN",
        repoPath("ampl_gurobi/.venv/lib/python3.9/site-packages/ampl_module_gurobi/bin/gurobi"));

    ampl::Environment env(bin_dir);
    ampl_.reset(new ampl::AMPL(env));
    ampl_->setOption("solver", gurobi_bin);
    ampl_->read(repoPath("ampl_gurobi/fmo.mod"));

    ampl_->getParameter("n_ptv").set(n_ptv_);
    ampl_->getParameter("n_oar").set(n_oar_);
    ampl_->getParameter("max_intensity").set(source_.max_intensity());
    ampl_->getParameter("w_under").set(source_.w_under());
    ampl_->getParameter("w_over").set(source_.w_over());
    ampl_->getParameter("w_ptv_over").set(source_.w_ptv_over());

    // dmin/dmax/dmax_ptv are indexed by PTV_B/OAR_B := 0..n-1, the same
    // ascending row order dmin_/dmax_/dmax_ptv_ were built in, so a plain
    // positional setValues matches AMPL's iteration order for these ranges.
    if (n_ptv_ > 0) {
        ampl_->getParameter("dmin").setValues(ampl::Args(dmin_.data()), dmin_.size());
        ampl_->getParameter("dmax_ptv").setValues(ampl::Args(dmax_ptv_.data()), dmax_ptv_.size());
    }
    if (n_oar_ > 0) {
        ampl_->getParameter("dmax").setValues(ampl::Args(dmax_.data()), dmax_.size());
    }
}

/*---------------------------------------------------------------------------*
 * Nombres de órgano, mismo orden que precompute() usó para construir
 * ptv_bounds_/oar_bounds_ (y por lo tanto el mismo orden en que solve()
 * agrega ptv_underdose_sq/oar_overdose_sq).
 *---------------------------------------------------------------------------*/
std::vector<std::string> ImrtFmoSolver::ptvOrganNames() const
{
    std::vector<std::string> names;
    names.reserve(ptv_bounds_.size());
    for (const auto& b : ptv_bounds_) names.push_back(b.name);
    return names;
}

std::vector<std::string> ImrtFmoSolver::oarOrganNames() const
{
    std::vector<std::string> names;
    names.reserve(oar_bounds_.size());
    for (const auto& b : oar_bounds_) names.push_back(b.name);
    return names;
}

/*---------------------------------------------------------------------------*
 * solve — refresh the per-solve data (active dimlets, sparse dose sets) and
 * resolve with Gurobi.
 *---------------------------------------------------------------------------*/
FmoResult ImrtFmoSolver::solve(const std::vector<int>& active_angles)
{
    if (!ready_)
        return {std::vector<double>(source_.n_dimlets(), 0.0), 1e30,
                std::vector<double>(ptv_bounds_.size(), 0.0),
                std::vector<double>(oar_bounds_.size(), 0.0)};

    std::vector<int> active = source_.activeDimletIds(active_angles);

    std::vector<double> active_d(active.begin(), active.end());

    std::vector<ampl::Tuple> ptv_tuples, oar_tuples;
    std::vector<double> ptv_vals, oar_vals;
    std::vector<int> ptv_nnz(ptv_bounds_.size(), 0), oar_nnz(oar_bounds_.size(), 0);

    auto organOf = [](const std::vector<OrganBounds>& bounds, int row) -> int {
        for (size_t i = 0; i < bounds.size(); ++i)
            if (row >= bounds[i].row_off && row < bounds[i].row_off + bounds[i].n_boxets)
                return (int)i;
        return -1;
    };

    for (int j : active) {
        for (const auto& e : source_.ptvDoseFor(j)) {
            ptv_tuples.emplace_back(ampl::Variant((double)e.first), ampl::Variant((double)j));
            ptv_vals.push_back(e.second);
            int oi = organOf(ptv_bounds_, e.first);
            if (oi >= 0) ++ptv_nnz[oi];
        }
        for (const auto& e : source_.oarDoseFor(j)) {
            oar_tuples.emplace_back(ampl::Variant((double)e.first), ampl::Variant((double)j));
            oar_vals.push_back(e.second);
            int oi = organOf(oar_bounds_, e.first);
            if (oi >= 0) ++oar_nnz[oi];
        }
    }

    if (verbose_)
        printSolveDims((int)active.size(), ptv_nnz, oar_nnz);

    try {
        // Reset before reassigning: narrowing DIMLETS/PTV_DOSE/OAR_DOSE via `let`
        // while d_ptv/d_oar still hold values for now-removed indices makes AMPL
        // raise "invalid subscripts discarded" as a hard error (not a warning) on
        // that assignment -- which broke every solve() after the first. `reset
        // data` wipes the set and its dependent params atomically with no
        // narrowing step in between, so there's nothing left to discard.
        ampl_->eval("reset data DIMLETS, PTV_DOSE, OAR_DOSE, d_ptv, d_oar;");

        ampl_->getSet("DIMLETS").setValues(ampl::Args(active_d.data()), active_d.size());
        ampl_->getSet("PTV_DOSE").setValues(ptv_tuples.data(), ptv_tuples.size());
        ampl_->getSet("OAR_DOSE").setValues(oar_tuples.data(), oar_tuples.size());

        if (!ptv_tuples.empty())
            ampl_->getParameter("d_ptv").setValues(ptv_tuples.data(), ampl::Args(ptv_vals.data()), ptv_tuples.size());
        if (!oar_tuples.empty())
            ampl_->getParameter("d_oar").setValues(oar_tuples.data(), ampl::Args(oar_vals.data()), oar_tuples.size());

        ampl_->solve();

        double f = ampl_->getObjective("fmo_objective").value();

        std::vector<double> x_full(source_.n_dimlets(), 0.0);
        ampl::Variable xvar = ampl_->getVariable("x");
        for (int j : active)
            x_full[j] = xvar.get(ampl::Variant((double)j)).value();

        // Desglose por órgano de lo que el objetivo agregado esconde: suma
        // de u_b^2 (subdosis PTV) y v_b^2 (sobredosis OAR) sobre los boxets
        // de cada órgano. Mismas variables que ya arma fmo.mod, solo que acá
        // se leen y se agregan por órgano en vez de sumarlas todas en una.
        ampl::Variable uvar = ampl_->getVariable("u");
        std::vector<double> ptv_underdose_sq(ptv_bounds_.size(), 0.0);
        for (size_t oi = 0; oi < ptv_bounds_.size(); ++oi) {
            const OrganBounds& b = ptv_bounds_[oi];
            double acc = 0.0;
            for (int row = b.row_off; row < b.row_off + b.n_boxets; ++row) {
                double u = uvar.get(ampl::Variant((double)row)).value();
                acc += u * u;
            }
            ptv_underdose_sq[oi] = acc;
        }

        ampl::Variable vvar = ampl_->getVariable("v");
        std::vector<double> oar_overdose_sq(oar_bounds_.size(), 0.0);
        for (size_t oi = 0; oi < oar_bounds_.size(); ++oi) {
            const OrganBounds& b = oar_bounds_[oi];
            double acc = 0.0;
            for (int row = b.row_off; row < b.row_off + b.n_boxets; ++row) {
                double v = vvar.get(ampl::Variant((double)row)).value();
                acc += v * v;
            }
            oar_overdose_sq[oi] = acc;
        }

        return {x_full, f, ptv_underdose_sq, oar_overdose_sq};
    } catch (const std::exception& e) {
        std::cerr << "[FMO] ERROR: AMPL/Gurobi solve failed: " << e.what() << "\n";
        return {std::vector<double>(source_.n_dimlets(), 0.0), 1e30,
                std::vector<double>(ptv_bounds_.size(), 0.0),
                std::vector<double>(oar_bounds_.size(), 0.0)};
    }
}

} // namespace imrt
} // namespace emili
