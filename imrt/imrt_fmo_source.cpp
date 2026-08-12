#include "imrt_fmo_source.h"

namespace emili {
namespace imrt {

ImrtInstanceFmoSource::ImrtInstanceFmoSource(const ImrtInstance& inst)
    : inst_(inst)
{
    precompute();
}

void ImrtInstanceFmoSource::precompute()
{
    const int nb = inst_.n_dimlets;

    std::vector<int> ptv_orgs, oar_orgs;
    for (int o = 0; o < (int)inst_.organs.size(); ++o) {
        if (inst_.organs[o].is_ptv) ptv_orgs.push_back(o);
        else                        oar_orgs.push_back(o);
    }

    std::vector<int> ptv_row_off(ptv_orgs.size());
    std::vector<int> oar_row_off(oar_orgs.size());
    int n_ptv = 0, n_oar = 0;
    for (int i = 0; i < (int)ptv_orgs.size(); ++i) {
        ptv_row_off[i] = n_ptv;
        n_ptv += inst_.organs[ptv_orgs[i]].n_boxets;
    }
    for (int i = 0; i < (int)oar_orgs.size(); ++i) {
        oar_row_off[i] = n_oar;
        n_oar += inst_.organs[oar_orgs[i]].n_boxets;
    }

    ptv_dose_.resize(nb);
    oar_dose_.resize(nb);
    for (int i = 0; i < (int)ptv_orgs.size(); ++i) {
        int rbase = ptv_row_off[i];
        for (const DoseEntry& e : inst_.organs[ptv_orgs[i]].entries)
            ptv_dose_[e.dimlet_id].push_back({rbase + e.boxet_id, e.dose_rate});
    }
    for (int i = 0; i < (int)oar_orgs.size(); ++i) {
        int rbase = oar_row_off[i];
        for (const DoseEntry& e : inst_.organs[oar_orgs[i]].entries)
            oar_dose_[e.dimlet_id].push_back({rbase + e.boxet_id, e.dose_rate});
    }

    // dmax_ptv is always 1.07*Dmin here (not gated by w_ptv_over): the AMPL
    // model (ampl_gurobi/fmo.mod) absorbs the "disabled" case through the
    // zero-weight slack w rather than through an unbounded dmax_ptv, unlike
    // the old OSQP box-constraint formulation this used to feed.
    ptv_organs_.reserve(ptv_orgs.size());
    for (int i = 0; i < (int)ptv_orgs.size(); ++i) {
        const OrganData& o = inst_.organs[ptv_orgs[i]];
        FmoOrganRef ref;
        ref.is_ptv    = true;
        ref.n_boxets  = o.n_boxets;
        ref.dmin      = o.Dmin;
        ref.dmax      = 0.0;
        ref.dmax_ptv  = 1.07 * o.Dmin;
        ptv_organs_.push_back(ref);
    }
    oar_organs_.reserve(oar_orgs.size());
    for (int i = 0; i < (int)oar_orgs.size(); ++i) {
        const OrganData& o = inst_.organs[oar_orgs[i]];
        FmoOrganRef ref;
        ref.is_ptv    = false;
        ref.n_boxets  = o.n_boxets;
        ref.dmin      = 0.0;
        ref.dmax      = o.Dmax;
        ref.dmax_ptv  = 0.0;
        oar_organs_.push_back(ref);
    }
}

std::vector<int> ImrtInstanceFmoSource::activeDimletIds(const std::vector<int>& active_angles) const
{
    std::vector<int> ids;
    ids.reserve(active_angles.size() * (size_t)inst_.n_dimlets_per_angle);
    for (int a : active_angles)
        for (int local = 0; local < inst_.n_dimlets_per_angle; ++local)
            ids.push_back(inst_.globalDimletIndex(a, local));
    return ids;
}

} // namespace imrt
} // namespace emili
