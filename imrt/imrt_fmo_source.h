#ifndef IMRT_FMO_SOURCE_H
#define IMRT_FMO_SOURCE_H

#include "imrt_instance.h"
#include <vector>
#include <utility>

namespace emili {
namespace imrt {

/*---------------------------------------------------------------------------*
 * IFmoDataSource
 *
 * Everything ImrtFmoSolver needs to build the FMO QP, decoupled from the
 * concrete on-disk instance format. ImrtInstance's globalDimletIndex()
 * assumes a fixed beamlet count per angle, which does not hold for the
 * CERR-exported dataset (instances/CERR_Prostate has a variable beamlet
 * count per angle) -- this interface lets ImrtFmoSolver stay agnostic to
 * that assumption instead of forcing every format through it.
 *---------------------------------------------------------------------------*/
struct FmoOrganRef {
    bool   is_ptv;
    int    n_boxets;
    double dmin;       // PTV prescription (Gy); unused for OAR
    double dmax;       // OAR tolerance (Gy); unused for PTV
    double dmax_ptv;   // PTV overdose ceiling (e.g. 1.07*dmin); unused for OAR
};

class IFmoDataSource {
public:
    virtual ~IFmoDataSource() {}

    virtual int    n_dimlets()     const = 0;
    virtual double max_intensity() const = 0;
    virtual double w_under()       const = 0;
    virtual double w_over()        const = 0;
    virtual double w_ptv_over()    const = 0;

    // In boxet-block order: row r of ptvDoseFor()/oarDoseFor() falls within
    // the r-th organ's [0, n_boxets) slice of these lists, concatenated.
    virtual const std::vector<FmoOrganRef>& ptvOrgans() const = 0;
    virtual const std::vector<FmoOrganRef>& oarOrgans() const = 0;

    // Global dimlet ids that are active for this set of active angle indices.
    virtual std::vector<int> activeDimletIds(const std::vector<int>& active_angles) const = 0;

    // Sparse dose entries touching a given global dimlet: (boxet row within the
    // concatenated PTV/OAR block, dose_rate). Empty if the dimlet touches nothing.
    virtual const std::vector<std::pair<int,double>>& ptvDoseFor(int global_dimlet_id) const = 0;
    virtual const std::vector<std::pair<int,double>>& oarDoseFor(int global_dimlet_id) const = 0;
};

/*---------------------------------------------------------------------------*
 * ImrtInstanceFmoSource
 *
 * Adapts the existing fixed-per-angle ImrtInstance (CORT/old format) to
 * IFmoDataSource. Owns its own copy of the instance so a BaoProblem can hold
 * this source independently of the caller's instance lifetime -- the same
 * ownership shape the pre-refactor ImrtFmoSolver relied on (it held a
 * reference into BaoProblem's owned ImrtInstance copy).
 *
 * Zero behavior change versus the dose index the old ImrtFmoSolver::
 * precompute() built for the OSQP path -- this just repackages that logic.
 *---------------------------------------------------------------------------*/
class ImrtInstanceFmoSource : public IFmoDataSource {
public:
    explicit ImrtInstanceFmoSource(const ImrtInstance& inst);

    int    n_dimlets()     const override { return inst_.n_dimlets; }
    double max_intensity() const override { return inst_.max_intensity; }
    double w_under()       const override { return inst_.w_under; }
    double w_over()        const override { return inst_.w_over; }
    double w_ptv_over()    const override { return inst_.w_ptv_over; }

    const std::vector<FmoOrganRef>& ptvOrgans() const override { return ptv_organs_; }
    const std::vector<FmoOrganRef>& oarOrgans() const override { return oar_organs_; }

    std::vector<int> activeDimletIds(const std::vector<int>& active_angles) const override;

    const std::vector<std::pair<int,double>>& ptvDoseFor(int global_dimlet_id) const override {
        return ptv_dose_[global_dimlet_id];
    }
    const std::vector<std::pair<int,double>>& oarDoseFor(int global_dimlet_id) const override {
        return oar_dose_[global_dimlet_id];
    }

    const ImrtInstance& instance() const { return inst_; }

private:
    ImrtInstance inst_;
    std::vector<FmoOrganRef> ptv_organs_;
    std::vector<FmoOrganRef> oar_organs_;
    std::vector<std::vector<std::pair<int,double>>> ptv_dose_;
    std::vector<std::vector<std::pair<int,double>>> oar_dose_;

    void precompute();
};

} // namespace imrt
} // namespace emili

#endif // IMRT_FMO_SOURCE_H
